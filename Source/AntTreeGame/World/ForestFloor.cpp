#include "World/ForestFloor.h"
#include "AntTreeGame.h"
#include "Tree/TreeActor.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

namespace
{
	constexpr int32 NumSectors = 8;
	constexpr int32 NumMats = 6;
	enum EMat { MBrown, MOchre, MOlive, MDarkBrown, MGrass, MTwig };
	// Dull on purpose: brown already means "cannot be cut" on the tree. Grass is darker than any leaf on the tree.
	const FLinearColor MatColors[NumMats] = {
		FLinearColor(0.050f, 0.028f, 0.015f),  // brown
		FLinearColor(0.082f, 0.058f, 0.024f),  // dull ochre (sunlit it lifts a lot)
		FLinearColor(0.038f, 0.044f, 0.016f),  // olive
		FLinearColor(0.028f, 0.019f, 0.012f),  // dark brown
		FLinearColor(0.012f, 0.034f, 0.022f),  // dark olive-teal grass
		FLinearColor(0.030f, 0.023f, 0.017f),  // twigs
	};

	struct FBuf
	{
		TArray<FVector> V, N;
		TArray<int32> T;
	};

	/** UE front faces: (b - a) x (c - a) points away from the viewer, so a triangle facing along n is wound the other way. */
	void AddTri(FBuf& B, int32 A, int32 Bi, int32 C, const FVector& Facing)
	{
		if ((FVector::CrossProduct(B.V[Bi] - B.V[A], B.V[C] - B.V[A]) | Facing) > 0.0) B.T.Append({ A, C, Bi });
		else B.T.Append({ A, Bi, C });
	}

	double Gauss(FRandomStream& R)
	{
		return FMath::Sqrt(-2.0 * FMath::Loge(FMath::Max(R.FRand(), 1e-6f))) * FMath::Cos(2.0 * PI * R.FRand());
	}

	/** A strip of stations, each with `Across` points, as a two-sided or one-sided surface with a per-point normal. */
	void AddGrid(FBuf& B, const TArray<FVector>& P, int32 Stations, int32 Across, const FVector& Nrm, bool bTwoSided)
	{
		for (int32 Side = 0; Side < (bTwoSided ? 2 : 1); ++Side)
		{
			const FVector Nr = Side ? -Nrm : Nrm;
			const int32 Base = B.V.Num();
			for (const FVector& V : P) { B.V.Add(V); B.N.Add(Nr); }
			for (int32 k = 0; k + 1 < Stations; ++k)
			{
				for (int32 j = 0; j + 1 < Across; ++j)
				{
					const int32 A = Base + k * Across + j, Bb = A + 1, C = A + Across, D = C + 1;
					AddTri(B, A, Bb, C, Nr);
					AddTri(B, Bb, D, C, Nr);
				}
			}
		}
	}

	/** A pointed leaf lying on the ground at Centre (its stem end), pitched up by Tilt toward direction TiltDir. */
	void AddLeaf(FBuf& B, const FVector& Centre, float Yaw, float L, float W, float Tilt, float TiltDir, float Curl, float MinZ)
	{
		const int32 Stations = L < 30.f ? 5 : 14;   // small fragments stay coarse: the triangle budget goes to the big leaves
		const FVector F(FMath::Cos(Yaw), FMath::Sin(Yaw), 0.f);
		const FVector Rt(-F.Y, F.X, 0.f);
		const FVector Axis(FMath::Sin(TiltDir), -FMath::Cos(TiltDir), 0.f);
		const FQuat Q = FQuat(Axis, Tilt);
		TArray<FVector> P;
		P.Reserve(Stations * 3);
		for (int32 k = 0; k < Stations; ++k)
		{
			const float t = (k + 0.5f) / Stations;               // never exactly the stem point
			const float U = L * (k == Stations - 1 ? 1.f : t);
			const float Half = 0.5f * W * FMath::Pow(FMath::Sin(PI * FMath::Pow(FMath::Min(U / L, 1.f), 0.75f)), 0.9f);
			for (int32 j = 0; j < 3; ++j)
			{
				const float Vv = (j - 1) * Half;
				const float H = j == 1 ? 0.f : Curl * (0.4f + 0.6f * (Half / (0.5f * W)));
				FVector Pt = Centre + Q.RotateVector(F * U + Rt * Vv + FVector(0, 0, H));
				Pt.Z = FMath::Max(Pt.Z, MinZ);
				P.Add(Pt);
			}
		}
		AddGrid(B, P, Stations, 3, Q.RotateVector(FVector::UpVector), Tilt > 0.01f || Curl > 0.5f);
	}

	/** A curved, tapering blade of grass (a flat ribbon, two-sided). */
	void AddBlade(FBuf& B, const FVector& Base, float Yaw, float H, float W, float Lean, float Z0)
	{
		constexpr int32 Stations = 5;
		const FVector D(FMath::Cos(Yaw), FMath::Sin(Yaw), 0.f);
		const FVector Across(-D.Y, D.X, 0.f);
		TArray<FVector> P;
		FVector Prev = Base;
		for (int32 k = 0; k < Stations; ++k)
		{
			const float s = float(k) / (Stations - 1);
			const FVector C = Base + D * (H * Lean * s * s) + FVector(0, 0, H * (s - 0.35f * Lean * s * s));
			const float Half = 0.5f * W * (k == Stations - 1 ? 0.f : FMath::Pow(1.f - s, 0.8f));
			P.Add(C - Across * Half);
			P.Add(C + Across * Half);
		}
		const FVector Tangent = (P[2 * (Stations - 1)] - P[0]).GetSafeNormal();
		const FVector Nrm = FVector::CrossProduct(Across, Tangent).GetSafeNormal();
		(void)Prev; (void)Z0;
		AddGrid(B, P, Stations, 2, Nrm, true);
	}

	/** A thin tapered stick lying on the ground: a three-sided prism along a gentle bend. */
	void AddTwig(FBuf& B, const FVector& Start, float Yaw, float Len, float Rad, float Bend, float Pitch)
	{
		constexpr int32 Segs = 4;
		TArray<FVector> Centres;
		for (int32 k = 0; k <= Segs; ++k)
		{
			const float s = float(k) / Segs;
			const float A = Yaw + Bend * (s - 0.5f);
			Centres.Add(Start + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * (Len * s) + FVector(0, 0, Rad * (1.f - s * 0.6f) + Len * s * Pitch));
		}
		const int32 Base = B.V.Num();
		for (int32 k = 0; k <= Segs; ++k)
		{
			const float s = float(k) / Segs;
			const float R = Rad * (1.f - 0.8f * s);
			const FVector Dir = (Centres[FMath::Min(k + 1, Segs)] - Centres[FMath::Max(k - 1, 0)]).GetSafeNormal();
			const FVector Side = FVector::CrossProduct(Dir, FVector::UpVector).GetSafeNormal();
			const FVector Up = FVector::CrossProduct(Side, Dir).GetSafeNormal();
			for (int32 j = 0; j < 3; ++j)
			{
				const float Ang = 2.f * PI * j / 3.f + 0.5f;
				const FVector Out = Up * FMath::Cos(Ang) + Side * FMath::Sin(Ang);
				B.V.Add(Centres[k] + Out * R);
				B.N.Add(Out);
			}
		}
		for (int32 k = 0; k < Segs; ++k)
		{
			for (int32 j = 0; j < 3; ++j)
			{
				const int32 A = Base + k * 3 + j, Bb = Base + k * 3 + (j + 1) % 3, C = A + 3, D = Bb + 3;
				const FVector Facing = B.N[A] + B.N[Bb];
				AddTri(B, A, Bb, C, Facing);
				AddTri(B, Bb, D, C, Facing);
			}
		}
	}

	int32 SectorOf(const FVector2D& Rel)
	{
		const float A = FMath::Atan2(Rel.Y, Rel.X) + PI;
		return FMath::Clamp(int32(A / (2.f * PI) * NumSectors), 0, NumSectors - 1);
	}

	FAutoConsoleCommandWithWorldAndArgs CmdFloor(
		TEXT("Leaf.Floor"),
		TEXT("Leaf.Floor <0|1>: hide or show the forest floor (leaf litter, grass tufts and twigs round the clearing). No argument logs its state."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (!World) return;
			for (TActorIterator<AForestFloor> It(World); It; ++It)
			{
				if (Args.Num() > 0) It->SetFloorEnabled(FCString::Atoi(*Args[0]) != 0);
				UE_LOG(LogLeafline, Log, TEXT("Leaf.Floor: %s"), It->IsFloorEnabled() ? TEXT("on") : TEXT("off"));
			}
		}));
}

AForestFloor::AForestFloor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.5f;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMat(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (BasicMat.Succeeded()) BasicMaterial = BasicMat.Object;
}

void AForestFloor::SetFloorEnabled(bool bOn)
{
	bEnabled = bOn;
	SetActorHiddenInGame(!bOn);
}

void AForestFloor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const ATreeActor* Tree = ATreeActor::Get(this);
	if (!Tree || Tree->GetSkeleton().Branches.Num() == 0) return;
	if (Tree->GetSeed() != BuiltSeed) Build(*Tree);
}

void AForestFloor::Clear()
{
	for (UProceduralMeshComponent* C : Sectors)
	{
		if (C) C->DestroyComponent();
	}
	Sectors.Reset();
}

void AForestFloor::Build(const ATreeActor& Tree)
{
	const double Start = FPlatformTime::Seconds();
	Clear();
	const FTreeSkeleton& Sk = Tree.GetSkeleton();
	BuiltSeed = Sk.Seed;
	const FForestFloorTuning& T = Tuning;
	const FVector Trunk = Sk.TrunkBase;
	const float Z0 = Tree.GetField().GroundZ;

	// The bare clearing: a disc round the trunk, discs round the nest spots, corridors from them to the trunk and a band
	// round every root. Clear(P) is the distance outside it (negative inside), with a wavy edge.
	FRandomStream R(Sk.Seed * 104729 + 9161);
	float Reach = 0.f, TrunkR = 0.f;
	TArray<FVector2D> RootPts, NestPts;
	for (const FTreeBranch& Br : Sk.Branches)
	{
		if (Br.Level == 0 && Br.SampleRadius.Num() > 0 && TrunkR == 0.f) TrunkR = Br.SampleRadius[0];
		if (Br.Level != -1) continue;
		for (int32 k = 0; k < Br.Samples.Num(); k += 3)
		{
			const FVector2D S = FVector2D(Br.Samples[k]) - FVector2D(Trunk);
			RootPts.Add(S);
			Reach = FMath::Max(Reach, float(S.Size()));
		}
	}
	for (const FVector& N : Sk.NestSpots) NestPts.Add(FVector2D(N) - FVector2D(Trunk));
	const float TrunkClearR = FMath::Max(T.TrunkClear, TrunkR + 500.f);
	const float PhaseA = R.FRand() * 6.28f, PhaseB = R.FRand() * 6.28f;
	auto SegDist = [](const FVector2D& P, const FVector2D& A, const FVector2D& B)
	{
		const FVector2D AB = B - A;
		const float t = FMath::Clamp(float(FVector2D::DotProduct(P - A, AB) / FMath::Max(AB.SizeSquared(), 1.0)), 0.f, 1.f);
		return float(FVector2D::Distance(P, A + AB * t));
	};
	auto ClearDist = [&](const FVector2D& P) -> float
	{
		float D = float(P.Size()) - TrunkClearR;
		for (const FVector2D& N : NestPts)
		{
			D = FMath::Min(D, float(FVector2D::Distance(P, N)) - T.NestClear);
			D = FMath::Min(D, SegDist(P, N, FVector2D::ZeroVector) - T.CorridorHalf);
		}
		for (const FVector2D& Rp : RootPts) D = FMath::Min(D, float(FVector2D::Distance(P, Rp)) - T.RootBand);
		return D - T.EdgeWave * FMath::Sin(P.X * 0.0021f + PhaseA) * FMath::Sin(P.Y * 0.0027f + PhaseB);
	};
	// Anchors on the clearing's edge, where the litter gathers.
	TArray<FVector2D> Anchors;
	for (int32 k = 0; k < 24; ++k) Anchors.Add(FVector2D(FMath::Cos(k * PI / 12.f), FMath::Sin(k * PI / 12.f)) * TrunkClearR);
	for (const FVector2D& Rp : RootPts) Anchors.Add(Rp);
	for (const FVector2D& N : NestPts)
	{
		for (int32 k = 0; k < 8; ++k) Anchors.Add(N + FVector2D(FMath::Cos(k * PI / 4.f), FMath::Sin(k * PI / 4.f)) * T.NestClear);
		for (int32 k = 1; k < 6; ++k) Anchors.Add(N * (float(k) / 6.f));
	}
	const float FlatEnd = T.FlatRing;  // distance outside the clearing up to which only flat leaves lie

	TArray<FBuf> Bufs;
	Bufs.SetNum(NumSectors * NumMats);
	int32 NLeaves = 0, NFlat = 0, NTufts = 0, NBlades = 0, NTwigs = 0;

	auto Place = [&](const FVector2D& Rel, int32 Mat) -> FBuf& { return Bufs[SectorOf(Rel) * NumMats + Mat]; };
	auto LeafMat = [&]() -> int32
	{
		const float u = R.FRand();
		return u < 0.35f ? MBrown : u < 0.55f ? MOchre : u < 0.80f ? MOlive : MDarkBrown;
	};

	// 1. Fallen leaves, in clusters with bare gaps. Flat near the clearing, curled and tilted farther out.
	auto Leaves = [&](int32 Count, bool bRing, float Sigma)
	{
		int32 Made = 0, Guard = 0;
		while (Made < Count && Guard++ < Count * 60)
		{
			FVector2D Centre;
			if (bRing)
			{
				const float A = R.FRand() * 2.f * PI;
				Centre = Anchors[R.RandRange(0, Anchors.Num() - 1)] + FVector2D(FMath::Cos(A), FMath::Sin(A)) * (FlatEnd + 900.f) * FMath::Pow(R.FRand(), 1.3f);
			}
			else
			{
				const float A = R.FRand() * 2.f * PI;
				Centre = FVector2D(FMath::Cos(A), FMath::Sin(A)) * (T.LitterRadius * FMath::Pow(R.FRand(), 0.7f));
			}
			const int32 InCluster = 8 + R.RandRange(0, 18);
			for (int32 i = 0; i < InCluster && Made < Count; ++i)
			{
				const FVector2D Rel = Centre + FVector2D(Gauss(R), Gauss(R)) * Sigma;
				const float D = float(Rel.Size());
				const bool bFrag = R.FRand() < T.FragmentFraction;
				const float Scale = 1.f + 1.4f * FMath::Clamp(D / T.LitterRadius, 0.f, 1.f);
				const float L = bFrag ? R.FRandRange(8.f, 24.f) : FMath::Lerp(T.LeafLengthMin, T.LeafLengthMax, FMath::Pow(R.FRand(), 1.3f)) * Scale;
				const float W = L * R.FRandRange(0.38f, 0.55f);
				const float Out = ClearDist(Rel) - 0.6f * L;   // the leaf's nearest end stays outside the clearing
				if (Out < 0.f || D > T.LitterRadius) continue;
				const float Yaw = R.FRand() * 2.f * PI;
				const bool bFlat = Out < FlatEnd || R.FRand() < 0.2f;
				const float Layer = float(R.RandRange(0, 2));
				float Tilt = 0.f, TiltDir = 0.f, Curl = 0.f;
				if (bFlat)
				{
					Curl = R.FRandRange(0.f, 0.25f);
				}
				else
				{
					Tilt = FMath::DegreesToRadians(R.FRandRange(6.f, 34.f));
					TiltDir = R.FRand() * 2.f * PI;
					Curl = R.FRandRange(0.05f, 0.16f) * W;
				}
				const FVector C(Trunk.X + Rel.X, Trunk.Y + Rel.Y, Z0 + T.FlatLift + Layer * 0.06f);
				AddLeaf(Place(Rel, LeafMat()), C, Yaw, L, W, Tilt, TiltDir, Curl, Z0 + T.FlatLift);
				++Made;
				++NLeaves;
				if (Out < FlatEnd) ++NFlat;
			}
		}
	};
	Leaves(T.RingLeaves, true, 260.f);
	Leaves(T.OuterLeaves, false, 450.f);

	// 2. Grass: patches of tufts of three to five curved blades, beyond the flat ring; a few tall, far ones to break the
	// flat horizon.
	auto Tuft = [&](const FVector2D& Rel, float HeightScale)
	{
		const int32 Blades = R.RandRange(3, 5);
		FBuf& B = Place(Rel, MGrass);
		for (int32 b = 0; b < Blades; ++b)
		{
			const FVector2D Foot = Rel + FVector2D(R.FRandRange(-5.f, 5.f), R.FRandRange(-5.f, 5.f)) * HeightScale;
			const float H = R.FRandRange(T.BladeHeightMin, T.BladeHeightMax) * HeightScale;
			const float Yaw = R.FRand() * 2.f * PI;
			AddBlade(B, FVector(Trunk.X + Foot.X, Trunk.Y + Foot.Y, Z0 - 0.5f), Yaw, H, FMath::Max(3.f, 0.05f * H), R.FRandRange(0.25f, 0.7f), Z0);
			++NBlades;
		}
		++NTufts;
	};
	{
		int32 Made = 0, Guard = 0;
		while (Made < T.Tufts && Guard++ < T.Tufts * 20)
		{
			const float A = R.FRand() * 2.f * PI;
			const FVector2D Centre = (R.FRand() < 0.7f)
				? Anchors[R.RandRange(0, Anchors.Num() - 1)] + FVector2D(FMath::Cos(A), FMath::Sin(A)) * (FlatEnd + 600.f + 2400.f * FMath::Pow(R.FRand(), 1.2f))
				: FVector2D(FMath::Cos(A), FMath::Sin(A)) * (T.LitterRadius * 1.1f * FMath::Pow(R.FRand(), 0.8f));
			for (int32 i = 0; i < 9 && Made < T.Tufts; ++i)
			{
				const FVector2D Rel = Centre + FVector2D(Gauss(R), Gauss(R)) * 320.f;
				if (ClearDist(Rel) < FlatEnd) continue;
				Tuft(Rel, 1.f);
				++Made;
			}
		}
		for (int32 i = 0; i < T.FarTufts; ++i)
		{
			const float Rr = FMath::Sqrt(FMath::Lerp(FMath::Square(T.LitterRadius), FMath::Square(T.FarRadius), R.FRand()));
			const float A = R.FRand() * 2.f * PI;
			Tuft(FVector2D(FMath::Cos(A), FMath::Sin(A)) * Rr, 1.f + 3.f * Rr / T.FarRadius);
		}
	}

	// 3. Twigs, beyond the flat ring.
	for (int32 i = 0, Made = 0; i < T.Twigs * 20 && Made < T.Twigs; ++i)
	{
		const float Rr = T.LitterRadius * FMath::Pow(R.FRand(), 0.8f);
		const float A = R.FRand() * 2.f * PI;
		const FVector2D Rel = FVector2D(FMath::Cos(A), FMath::Sin(A)) * Rr;
		if (ClearDist(Rel) < FlatEnd) continue;
		const float Len = R.FRandRange(40.f, 120.f) * (1.f + Rr / T.LitterRadius);
		AddTwig(Place(Rel, MTwig), FVector(Trunk.X + Rel.X, Trunk.Y + Rel.Y, Z0), R.FRand() * 2.f * PI, Len, R.FRandRange(0.8f, 1.6f) * (1.f + Rr / T.LitterRadius),
			R.FRandRange(-0.5f, 0.5f), R.FRandRange(0.f, 0.12f));
		++Made;
		++NTwigs;
	}

	// Materials and components: one component per sector (so far sectors cull), one section per colour.
	if (Mats.Num() == 0)
	{
		// M_Litter (matte, built from Tools/materials/build_litter_mcp.py) if it exists; BasicShapeMaterial is the fallback.
		UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Leafline/Materials/M_Litter.M_Litter"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!Base) Base = BasicMaterial;
		UE_LOG(LogLeafline, Log, TEXT("Forest floor material: %s"), Base ? *Base->GetName() : TEXT("none"));
		for (int32 m = 0; Base && m < NumMats; ++m)
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, this);
			MID->SetVectorParameterValue(TEXT("Color"), MatColors[m]);
			Mats.Add(MID);
		}
	}
	int64 Tris = 0;
	for (int32 s = 0; s < NumSectors; ++s)
	{
		UProceduralMeshComponent* C = NewObject<UProceduralMeshComponent>(this, NAME_None, RF_Transient);
		C->SetupAttachment(RootComponent);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(false);
		C->bUseAsyncCooking = false;
		C->SetWorldLocation(FVector::ZeroVector);
		C->RegisterComponent();
		for (int32 m = 0; m < NumMats; ++m)
		{
			const FBuf& B = Bufs[s * NumMats + m];
			if (B.T.Num() == 0) continue;
			C->CreateMeshSection_LinearColor(m, B.V, B.T, B.N, {}, {}, {}, {}, {}, {}, false, false);
			if (m < Mats.Num()) C->SetMaterial(m, Mats[m]);
			Tris += B.T.Num() / 3;
		}
		Sectors.Add(C);
	}
	SetFloorEnabled(bEnabled);
	UE_LOG(LogLeafline, Log, TEXT("Forest floor: seed %d, trunk disc %.0f cm (flared radius %.0f), root reach %.0f, %d leaves (%d flat in the ring), %d tufts (%d blades), %d twigs, %lld triangles, built in %.1f ms"),
		Sk.Seed, TrunkClearR, TrunkR, Reach, NLeaves, NFlat, NTufts, NBlades, NTwigs, Tris, (FPlatformTime::Seconds() - Start) * 1000.0);
}
