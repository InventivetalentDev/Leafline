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
		FLinearColor(0.095f, 0.055f, 0.030f),  // brown
		FLinearColor(0.150f, 0.110f, 0.045f),  // dull ochre
		FLinearColor(0.075f, 0.085f, 0.030f),  // olive
		FLinearColor(0.055f, 0.036f, 0.024f),  // dark brown
		FLinearColor(0.018f, 0.050f, 0.030f),  // dark olive-teal grass
		FLinearColor(0.045f, 0.034f, 0.026f),  // twigs
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
		constexpr int32 Stations = 5;
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

	// The clearing: past the farthest root tip and the nest spots, with their margins.
	float Reach = 0.f;
	for (const FTreeBranch& Br : Sk.Branches)
	{
		if (Br.Level != -1) continue;
		for (const FVector& S : Br.Samples) Reach = FMath::Max(Reach, float(FVector2D::Distance(FVector2D(S), FVector2D(Trunk))));
	}
	float Clear = FMath::Max(T.ClearRadius, Reach + T.RootMargin);
	for (const FVector& N : Sk.NestSpots) Clear = FMath::Max(Clear, float(FVector2D::Distance(FVector2D(N), FVector2D(Trunk))) + T.NestClear);
	const float FlatEnd = Clear + T.FlatRing;

	FRandomStream R(Sk.Seed * 104729 + 9161);
	TArray<FBuf> Bufs;
	Bufs.SetNum(NumSectors * NumMats);
	int32 NLeaves = 0, NFlat = 0, NTufts = 0, NBlades = 0, NTwigs = 0;

	auto Place = [&](const FVector2D& Rel, int32 Mat) -> FBuf& { return Bufs[SectorOf(Rel) * NumMats + Mat]; };
	auto LeafMat = [&]() -> int32
	{
		const float u = R.FRand();
		return u < 0.35f ? MBrown : u < 0.55f ? MOchre : u < 0.80f ? MOlive : MDarkBrown;
	};

	// 1. Fallen leaves, in clusters with bare gaps. Flat in the ring, curled and tilted farther out.
	auto Leaves = [&](int32 Count, float RMin, float RSpan, float Pow, float Sigma)
	{
		int32 Made = 0, Guard = 0;
		while (Made < Count && Guard++ < Count * 40)
		{
			const float Rc = RMin + RSpan * FMath::Pow(R.FRand(), Pow);
			const float Ac = R.FRand() * 2.f * PI;
			const int32 InCluster = 8 + R.RandRange(0, 18);
			for (int32 i = 0; i < InCluster && Made < Count; ++i)
			{
				const FVector2D Off(Gauss(R) * Sigma, Gauss(R) * Sigma);
				const FVector2D Rel = FVector2D(FMath::Cos(Ac), FMath::Sin(Ac)) * Rc + Off;
				const float D = Rel.Size();
				const bool bFrag = R.FRand() < T.FragmentFraction;
				const float Scale = 1.f + 1.4f * FMath::Clamp(D / T.LitterRadius, 0.f, 1.f);
				const float L = bFrag ? R.FRandRange(8.f, 24.f) : FMath::Lerp(T.LeafLengthMin, T.LeafLengthMax, FMath::Pow(R.FRand(), 1.3f)) * Scale;
				const float W = L * R.FRandRange(0.38f, 0.55f);
				if (D - 0.6f * L < Clear || D > T.LitterRadius) continue;
				bool bNearNest = false;
				for (const FVector& N : Sk.NestSpots)
				{
					if (FVector2D::Distance(FVector2D(N) - FVector2D(Trunk), Rel) < T.NestClear + L) { bNearNest = true; break; }
				}
				if (bNearNest) continue;
				const float Yaw = R.FRand() * 2.f * PI;
				const bool bFlat = D - 0.6f * L < FlatEnd || R.FRand() < 0.55f;
				const float Layer = float(R.RandRange(0, 2));
				float Tilt = 0.f, TiltDir = 0.f, Curl = 0.f;
				float Lift = T.FlatLift + Layer * 0.1f;
				if (bFlat)
				{
					Curl = R.FRandRange(0.f, 0.25f);
					if (D - 0.6f * L >= FlatEnd) Curl = R.FRandRange(0.f, 0.15f) * W * 0.2f;
				}
				else
				{
					Tilt = FMath::DegreesToRadians(R.FRandRange(4.f, 24.f));
					TiltDir = R.FRand() * 2.f * PI;
					Curl = R.FRandRange(0.02f, 0.10f) * W;
				}
				const FVector C(Trunk.X + Rel.X, Trunk.Y + Rel.Y, Z0 + Lift);
				AddLeaf(Place(Rel, LeafMat()), C, Yaw, L, W, Tilt, TiltDir, Curl, Z0 + T.FlatLift);
				++Made;
				++NLeaves;
				if (bFlat && D + 0.6f * L < FlatEnd) ++NFlat;
			}
		}
	};
	Leaves(T.RingLeaves, Clear - 200.f, T.FlatRing + 1600.f, 1.5f, 220.f);
	Leaves(T.OuterLeaves, FlatEnd, T.LitterRadius - FlatEnd, 1.4f, 450.f);

	// 2. Grass: patches of tufts of three to five curved blades; a few tall, far ones to break the flat horizon.
	const float GrassStart = Clear + 1200.f;
	auto Tuft = [&](const FVector2D& Rel, float HeightScale)
	{
		const int32 Blades = R.RandRange(3, 5);
		const float Out = FMath::Atan2(Rel.Y, Rel.X);
		FBuf& B = Place(Rel, MGrass);
		for (int32 b = 0; b < Blades; ++b)
		{
			const FVector2D Foot = Rel + FVector2D(R.FRandRange(-5.f, 5.f), R.FRandRange(-5.f, 5.f)) * HeightScale;
			const float H = R.FRandRange(T.BladeHeightMin, T.BladeHeightMax) * HeightScale;
			const float Yaw = R.FRand() * 2.f * PI;
			AddBlade(B, FVector(Trunk.X + Foot.X, Trunk.Y + Foot.Y, Z0 - 0.5f), Yaw, H, FMath::Max(3.f, 0.045f * H), R.FRandRange(0.25f, 0.7f), Z0);
			++NBlades;
		}
		(void)Out;
		++NTufts;
	};
	{
		const int32 PerPatch = 9;
		int32 Made = 0, Guard = 0;
		while (Made < T.Tufts && Guard++ < T.Tufts * 10)
		{
			const float Rc = GrassStart + (T.LitterRadius * 1.1f - GrassStart) * FMath::Pow(R.FRand(), 1.2f);
			const float Ac = R.FRand() * 2.f * PI;
			for (int32 i = 0; i < PerPatch && Made < T.Tufts; ++i)
			{
				const FVector2D Rel = FVector2D(FMath::Cos(Ac), FMath::Sin(Ac)) * Rc + FVector2D(Gauss(R), Gauss(R)) * 320.f;
				if (Rel.Size() < GrassStart) continue;
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
	for (int32 i = 0, Made = 0; i < T.Twigs * 6 && Made < T.Twigs; ++i)
	{
		const float Rr = FlatEnd + (T.LitterRadius - FlatEnd) * FMath::Pow(R.FRand(), 1.2f);
		const float A = R.FRand() * 2.f * PI;
		const FVector2D Rel = FVector2D(FMath::Cos(A), FMath::Sin(A)) * Rr;
		const float Len = R.FRandRange(40.f, 120.f) * (1.f + Rr / T.LitterRadius);
		AddTwig(Place(Rel, MTwig), FVector(Trunk.X + Rel.X, Trunk.Y + Rel.Y, Z0), R.FRand() * 2.f * PI, Len, R.FRandRange(0.8f, 1.6f) * (1.f + Rr / T.LitterRadius),
			R.FRandRange(-0.5f, 0.5f), R.FRandRange(0.f, 0.12f));
		++Made;
		++NTwigs;
	}

	// Materials and components: one component per sector (so far sectors cull), one section per colour.
	if (Mats.Num() == 0 && BasicMaterial)
	{
		for (int32 m = 0; m < NumMats; ++m)
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BasicMaterial, this);
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
	UE_LOG(LogLeafline, Log, TEXT("Forest floor: seed %d, clearing %.0f cm (root reach %.0f), %d leaves (%d flat in the ring), %d tufts (%d blades), %d twigs, %lld triangles, built in %.1f ms"),
		Sk.Seed, Clear, Reach, NLeaves, NFlat, NTufts, NBlades, NTwigs, Tris, (FPlatformTime::Seconds() - Start) * 1000.0);
}
