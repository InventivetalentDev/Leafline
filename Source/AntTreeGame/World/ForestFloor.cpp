#include "World/ForestFloor.h"
#include "AntTreeGame.h"
#include "Tree/TreeActor.h"
#include "World/GroundRelief.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Game/LeaflinePlayerController.h"
#include "Ant/AntCamera.h"

namespace
{
	constexpr int32 NumSectors = 8;
	constexpr int32 NumMats = 6;
	enum EMat { MBrown, MOchre, MOlive, MDarkBrown, MGrass, MTwig };
	// Dull on purpose: brown already means "cannot be cut" on the tree. Grass is darker than any leaf on the tree.
	const FLinearColor MatColors[NumMats] = {
		FLinearColor(0.040f, 0.029f, 0.020f),  // brown (greyer: the matte material turned 0.05/0.028/0.015 rust-orange in direct sun)
		FLinearColor(0.052f, 0.046f, 0.024f),  // dull tan-ochre (matte M_Litter lifts it in sun; the first 0.082/0.058/0.024 read orange)
		FLinearColor(0.038f, 0.044f, 0.016f),  // olive
		FLinearColor(0.036f, 0.025f, 0.017f),  // dark brown
		FLinearColor(0.028f, 0.070f, 0.032f),  // dark olive-green grass (still far darker than the tree's leaves)
		FLinearColor(0.030f, 0.023f, 0.017f),  // twigs
	};

	struct FBuf
	{
		TArray<FVector> V, N;
		TArray<int32> T;
		/** Grass only (M_Grass): UV0 = (height fraction 0 base .. 1 tip, per-blade random phase), UV1.x = blade height in metres. */
		TArray<FVector2D> UV0, UV1;
	};

	bool IsFiniteVec(const FVector& V) { return FMath::IsFinite(V.X) && FMath::IsFinite(V.Y) && FMath::IsFinite(V.Z); }
	bool IsFiniteVec(const FVector2D& V) { return FMath::IsFinite(V.X) && FMath::IsFinite(V.Y); }

	/** Drops every triangle that touches a non-finite vertex (position, normal or UV) and compacts the vertex arrays, so a NaN
	 *  can never reach a component's bounds (a component with NaN bounds is culled whole: the owner saw no floor on 14 Nov).
	 *  Returns the triangles dropped; OutBadVerts counts the non-finite vertices found. */
	int32 Sanitize(FBuf& B, int32& OutBadVerts)
	{
		const int32 NV = B.V.Num();
		TArray<bool> Ok;
		Ok.Init(true, NV);
		for (int32 i = 0; i < NV; ++i)
		{
			const bool bUV0 = B.UV0.Num() != NV || IsFiniteVec(B.UV0[i]);
			const bool bUV1 = B.UV1.Num() != NV || IsFiniteVec(B.UV1[i]);
			Ok[i] = IsFiniteVec(B.V[i]) && IsFiniteVec(B.N[i]) && bUV0 && bUV1;
			if (!Ok[i]) ++OutBadVerts;
		}
		int32 Dropped = 0;
		for (int32 t = 0; t + 2 < B.T.Num(); t += 3)
		{
			if (!Ok[B.T[t]] || !Ok[B.T[t + 1]] || !Ok[B.T[t + 2]]) ++Dropped;
		}
		if (Dropped == 0 && OutBadVerts == 0) return 0;
		FBuf Out;
		TArray<int32> Remap;
		Remap.Init(INDEX_NONE, NV);
		for (int32 t = 0; t + 2 < B.T.Num(); t += 3)
		{
			if (!Ok[B.T[t]] || !Ok[B.T[t + 1]] || !Ok[B.T[t + 2]]) continue;
			for (int32 k = 0; k < 3; ++k)
			{
				const int32 Old = B.T[t + k];
				if (Remap[Old] == INDEX_NONE)
				{
					Remap[Old] = Out.V.Num();
					Out.V.Add(B.V[Old]);
					Out.N.Add(B.N[Old]);
					if (B.UV0.Num() == NV) Out.UV0.Add(B.UV0[Old]);
					if (B.UV1.Num() == NV) Out.UV1.Add(B.UV1[Old]);
				}
				Out.T.Add(Remap[Old]);
			}
		}
		B = MoveTemp(Out);
		return Dropped;
	}

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
	void AddGrid(FBuf& B, const TArray<FVector>& P, int32 Stations, int32 Across, const FVector& Nrm, bool bTwoSided,
		const TArray<FVector2D>* UV0 = nullptr, const TArray<FVector2D>* UV1 = nullptr)
	{
		for (int32 Side = 0; Side < (bTwoSided ? 2 : 1); ++Side)
		{
			const FVector Nr = Side ? -Nrm : Nrm;
			const int32 Base = B.V.Num();
			for (int32 i = 0; i < P.Num(); ++i)
			{
				B.V.Add(P[i]);
				B.N.Add(Nr);
				if (UV0) B.UV0.Add((*UV0)[i]);
				if (UV1) B.UV1.Add((*UV1)[i]);
			}
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
			// sin(PI) is -8.7e-8 in float at the tip, and pow(negative, 0.9) is NaN (every leaf tip was NaN until 14 Nov evening): clamp at 0.
			const float Half = 0.5f * W * FMath::Pow(FMath::Max(FMath::Sin(PI * FMath::Pow(FMath::Min(U / L, 1.f), 0.75f)), 0.f), 0.9f);
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
	void AddBlade(FBuf& B, const FVector& Base, float Yaw, float H, float W, float Lean, float Z0, float Phase)
	{
		constexpr int32 Stations = 5;
		const FVector D(FMath::Cos(Yaw), FMath::Sin(Yaw), 0.f);
		const FVector Across(-D.Y, D.X, 0.f);
		TArray<FVector> P;
		TArray<FVector2D> UVa, UVb;
		FVector Prev = Base;
		for (int32 k = 0; k < Stations; ++k)
		{
			const float s = float(k) / (Stations - 1);
			const FVector C = Base + D * (H * Lean * s * s) + FVector(0, 0, H * (s - 0.35f * Lean * s * s));
			const float Half = 0.5f * W * (k == Stations - 1 ? 0.f : FMath::Pow(1.f - s, 0.8f));
			P.Add(C - Across * Half);
			P.Add(C + Across * Half);
			for (int32 side = 0; side < 2; ++side) { UVa.Add(FVector2D(s, Phase)); UVb.Add(FVector2D(H / 100.f, 0.f)); }
		}
		const FVector Tangent = (P[2 * (Stations - 1)] - P[0]).GetSafeNormal();
		const FVector Nrm = FVector::CrossProduct(Across, Tangent).GetSafeNormal();
		(void)Prev; (void)Z0;
		AddGrid(B, P, Stations, 2, Nrm, true, &UVa, &UVb);
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

	// Leaf.TopDown: a camera straight above (screenshots of the whole floor). The tree's leaf meshes are hidden meanwhile, or
	// the canopy would hide the ground.
	TWeakObjectPtr<ACameraActor> GTopCam;
	TArray<TWeakObjectPtr<UPrimitiveComponent>> GHiddenLeaves;

	void TopDownOff(UWorld* World)
	{
		for (TWeakObjectPtr<UPrimitiveComponent>& C : GHiddenLeaves)
		{
			if (C.IsValid()) C->SetVisibility(true);
		}
		GHiddenLeaves.Reset();
		if (ALeaflinePlayerController* PC = World ? Cast<ALeaflinePlayerController>(World->GetFirstPlayerController()) : nullptr)
		{
			if (PC->GetAntCamera()) PC->SetViewTarget(PC->GetAntCamera());
		}
		if (GTopCam.IsValid()) GTopCam->Destroy();
		GTopCam.Reset();
	}

	FAutoConsoleCommandWithWorldAndArgs CmdTopDown(
		TEXT("Leaf.TopDown"),
		TEXT("Leaf.TopDown <height cm> [fov] [x y]: a camera straight above (default over the trunk base) with the tree's leaves hidden, for screenshots of the whole floor; image up is +X. Leaf.TopDown 0 goes back to the ant."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (!World) return;
			TopDownOff(World);
			const float H = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 0.f;
			if (H <= 0.f) return;
			ALeaflinePlayerController* PC = Cast<ALeaflinePlayerController>(World->GetFirstPlayerController());
			ATreeActor* Tree = ATreeActor::Get(World);
			if (!PC || !Tree) return;
			TInlineComponentArray<UPrimitiveComponent*> Comps;
			Tree->GetComponents(Comps);
			for (UPrimitiveComponent* C : Comps)
			{
				if (C->GetName().Contains(TEXT("Leaf")) && C->IsVisible()) { C->SetVisibility(false); GHiddenLeaves.Add(C); }
			}
			const FVector Base = Tree->GetSkeleton().TrunkBase;
			const float X = Args.Num() > 2 ? FCString::Atof(*Args[2]) : Base.X;
			const float Y = Args.Num() > 3 ? FCString::Atof(*Args[3]) : Base.Y;
			FActorSpawnParameters P;
			P.ObjectFlags |= RF_Transient;
			ACameraActor* Cam = World->SpawnActor<ACameraActor>(FVector(X, Y, Base.Z + H), FRotator(-90.f, 0.f, 0.f), P);
			if (!Cam) return;
			Cam->GetCameraComponent()->SetFieldOfView(Args.Num() > 1 ? FCString::Atof(*Args[1]) : 60.f);
			GTopCam = Cam;
			PC->SetViewTarget(Cam);
			UE_LOG(LogLeafline, Log, TEXT("Leaf.TopDown: camera at %.0f %.0f height %.0f cm, %d leaf meshes hidden"), X, Y, H, GHiddenLeaves.Num());
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdFloorView(
		TEXT("Leaf.FloorView"),
		TEXT("Leaf.FloorView: log how many near grass tufts lie in a 60 degree cone in front of the ant (along the camera's yaw), by 5 m of distance."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ALeaflinePlayerController* PC = World ? Cast<ALeaflinePlayerController>(World->GetFirstPlayerController()) : nullptr;
			if (!PC || !PC->GetPawn()) return;
			const float Yaw = PC->GetControlRotation().Yaw;
			for (TActorIterator<AForestFloor> It(World); It; ++It) It->LogView(PC->GetPawn()->GetActorLocation(), Args.Num() > 0 ? FCString::Atof(*Args[0]) : Yaw);
		}));

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

namespace
{
	/** The relief stamp the floor was built on (LeafRelief): Leaf.Relief rebuilds it. */
	int32 GFloorReliefStamp = -1;
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
	if (Tree->GetSeed() != BuiltSeed || GFloorReliefStamp != LeafRelief::Stamp()) Build(*Tree);
}

void AForestFloor::LogView(const FVector& From, float YawDeg) const
{
	const FVector2D Dir(FMath::Cos(FMath::DegreesToRadians(YawDeg)), FMath::Sin(FMath::DegreesToRadians(YawDeg)));
	int32 Bins[12] = {};
	for (const FVector2D& P : NearTufts)
	{
		const FVector2D V = P - FVector2D(From);
		const float L = float(V.Size());
		if (L > 1.f && L < 6000.f && FVector2D::DotProduct(V / L, Dir) > 0.5f) ++Bins[int32(L / 500.f)];
	}
	FString Out;
	for (int32 b = 0; b < 12; ++b) Out += FString::Printf(TEXT(" %d-%d m:%d"), b * 5, b * 5 + 5, Bins[b]);
	UE_LOG(LogLeafline, Log, TEXT("Leaf.FloorView from %.0f %.0f yaw %.0f, tufts in a 60 degree cone:%s"), From.X, From.Y, YawDeg, *Out);
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
	GFloorReliefStamp = LeafRelief::Stamp();
	const FForestFloorTuning& T = Tuning;
	const FVector Trunk = Sk.TrunkBase;
	const float Z0 = Tree.GetField().GroundZ;

	// The bare clearing: a disc round the trunk, discs round the nest spots, corridors from them to the trunk and a band
	// round every root. Clear(P) is the distance outside it (negative inside), with a wavy edge.
	FRandomStream R(Sk.Seed * 104729 + 9161);
	float Reach = 0.f, TrunkR = 0.f;
	TArray<FVector> RootPts;   // x y relative to the trunk axis, z the root's radius there
	TArray<FVector2D> NestPts;
	for (const FTreeBranch& Br : Sk.Branches)
	{
		if (Br.Level == 0 && Br.SampleRadius.Num() > 0 && TrunkR == 0.f) TrunkR = Br.SampleRadius[0];
		if (Br.Level != -1) continue;
		for (int32 k = 0; k < Br.Samples.Num(); k += 3)
		{
			const FVector2D S = FVector2D(Br.Samples[k]) - FVector2D(Trunk);
			RootPts.Add(FVector(S.X, S.Y, Br.SampleRadius.IsValidIndex(k) ? Br.SampleRadius[k] : 0.f));
			Reach = FMath::Max(Reach, float(S.Size()));
		}
	}
	for (const FVector& N : Sk.NestSpots) NestPts.Add(FVector2D(N) - FVector2D(Trunk));
	const float TrunkClearR = FMath::Max(T.TrunkClear, TrunkR + 300.f);
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
		for (const FVector& Rp : RootPts) D = FMath::Min(D, float(FVector2D::Distance(P, FVector2D(Rp.X, Rp.Y))) - Rp.Z - T.RootBand);
		return D - T.EdgeWave * FMath::Sin(P.X * 0.0021f + PhaseA) * FMath::Sin(P.Y * 0.0027f + PhaseB);
	};
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
	// Both litter and grass are stratified by angle (NumStrata sectors, an equal share each) so no side of the trunk is
	// favoured; within a sector the clusters are random.
	constexpr int32 NumStrata = 24;
	auto Leaves = [&](int32 Count, bool bRing, float Sigma)
	{
		for (int32 Sector = 0; Sector < NumStrata; ++Sector)
		{
		const int32 Target = (Count * (Sector + 1)) / NumStrata - (Count * Sector) / NumStrata;
		int32 Made = 0, Guard = 0;
		while (Made < Target && Guard++ < Target * 60 + 60)
		{
			FVector2D Centre;
			{
				const float A = (Sector + R.FRand()) * 2.f * PI / NumStrata;
				const float Rr = bRing ? TrunkClearR - 200.f + (FlatEnd + 3500.f) * FMath::Pow(R.FRand(), 1.3f)
									   : T.LitterRadius * FMath::Pow(R.FRand(), 0.7f);
				Centre = FVector2D(FMath::Cos(A), FMath::Sin(A)) * Rr;
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
				// On the visible ground's relief (zero inside its start radius, 90 m by default, and when it is off).
				const float Lift = LeafRelief::Height(Trunk.X + Rel.X, Trunk.Y + Rel.Y);
				const FVector C(Trunk.X + Rel.X, Trunk.Y + Rel.Y, Z0 + Lift + T.FlatLift + Layer * 0.06f);
				AddLeaf(Place(Rel, LeafMat()), C, Yaw, L, W, Tilt, TiltDir, Curl, Z0 + Lift + T.FlatLift);
				++Made;
				++NLeaves;
				if (Out < FlatEnd) ++NFlat;
			}
		}
		}
	};
	Leaves(T.RingLeaves, true, 260.f);
	Leaves(T.OuterLeaves, false, 450.f);

	TArray<FVector2D> TuftPos;
	int32 TryBins[20] = {}, OkBins[20] = {};
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
			const float Lean = R.FRandRange(0.25f, 0.7f);
			const float Phase = R.FRand();
			// The foot on the visible ground's relief (zero inside its start radius, 90 m by default).
			const float Lift = LeafRelief::Height(Trunk.X + Foot.X, Trunk.Y + Foot.Y);
			AddBlade(B, FVector(Trunk.X + Foot.X, Trunk.Y + Foot.Y, Z0 - 0.5f + Lift), Yaw, H, FMath::Max(3.f, 0.022f * H), Lean, Z0, Phase);
			++NBlades;
		}
		++NTufts;
	};
	{
		// Clumps of about nine tufts, per angular sector; the radius law keeps most grass within about 40 m of the trunk
		// axis' clearing edge and thins out to the litter radius.
		const float Rmax = T.LitterRadius * 1.1f;
		for (int32 Sector = 0; Sector < NumStrata; ++Sector)
		{
			const int32 Target = (T.Tufts * (Sector + 1)) / NumStrata - (T.Tufts * Sector) / NumStrata;
			int32 Made = 0, Guard = 0;
			while (Made < Target && Guard++ < Target * 300 + 300)
			{
				const float A = (Sector + R.FRand()) * 2.f * PI / NumStrata;
				const bool bTail = R.FRand() < 0.12f;   // a thin tail out to the litter radius
				const float Rr = bTail ? TrunkClearR + (Rmax - TrunkClearR) * FMath::Pow(R.FRand(), 1.5f)
									   : TrunkClearR + (T.GrassRadius - TrunkClearR) * R.FRand();
				const FVector2D Centre = FVector2D(FMath::Cos(A), FMath::Sin(A)) * Rr;
				if (!bTail)
				{
					// Clumps thin out with the distance from the bare ground (not from the trunk axis), so every side of
					// the clearing, root spokes included, gets its grass right at the edge.
					const float Cd = ClearDist(Centre) - (FlatEnd + 100.f);
					if (Cd < 0.f || R.FRand() > FMath::Exp(-Cd / T.GrassFalloff)) continue;
				}
				for (int32 i = 0; i < 8 && Made < Target; ++i)
				{
					const FVector2D Rel = Centre + FVector2D(Gauss(R), Gauss(R)) * 280.f;
					const int32 Bin = FMath::Clamp(int32(Rel.Size() / 1000.f), 0, 19);
					++TryBins[Bin];
					if (ClearDist(Rel) < FlatEnd + 100.f) continue;
					++OkBins[Bin];
					// seedling height at the edge of the grass, full height 20 m further out
					const float Hs = 0.45f + 0.55f * FMath::Clamp((ClearDist(Rel) - (FlatEnd + 100.f)) / 2000.f, 0.f, 1.f);
					Tuft(Rel, Hs);
					TuftPos.Add(Rel);
					++Made;
				}
			}
		}
		for (int32 i = 0; i < T.FarTufts; ++i)
		{
			const float Rr = FMath::Sqrt(FMath::Lerp(FMath::Square(T.LitterRadius), FMath::Square(T.FarRadius), R.FRand()));
			const float A = (i % NumStrata + R.FRand()) * 2.f * PI / NumStrata;
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
		AddTwig(Place(Rel, MTwig), FVector(Trunk.X + Rel.X, Trunk.Y + Rel.Y, Z0 + LeafRelief::Height(Trunk.X + Rel.X, Trunk.Y + Rel.Y)), R.FRand() * 2.f * PI, Len, R.FRandRange(0.8f, 1.6f) * (1.f + Rr / T.LitterRadius),
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
		// M_Grass (WPO sway from MPC_Wind, Tools/materials/build_grass_mcp.py) for the blades, M_Litter / the basic material if missing.
		// M_Grass2 (backlit foliage shading, dark foot; build_grass_mcp.py MODE build2) first, then M_Grass.
		UMaterialInterface* GrassBase = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Leafline/Materials/M_Grass2.M_Grass2"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!GrassBase) GrassBase = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Leafline/Materials/M_Grass.M_Grass"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		UE_LOG(LogLeafline, Log, TEXT("Forest floor grass material: %s"), GrassBase ? *GrassBase->GetName() : (Base ? *Base->GetName() : TEXT("none")));
		for (int32 m = 0; Base && m < NumMats; ++m)
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(m == MGrass && GrassBase ? GrassBase : Base, this);
			MID->SetVectorParameterValue(TEXT("Color"), MatColors[m]);
			Mats.Add(MID);
		}
	}
	int32 DroppedTris = 0, BadVerts = 0;
	for (FBuf& B : Bufs) DroppedTris += Sanitize(B, BadVerts);
	if (DroppedTris > 0 || BadVerts > 0)
	{
		UE_LOG(LogLeafline, Warning, TEXT("Forest floor: dropped %d triangles with %d non-finite vertices before building the meshes"), DroppedTris, BadVerts);
	}
	else
	{
		UE_LOG(LogLeafline, Log, TEXT("Forest floor: no non-finite vertices"));
	}
	int64 Tris = 0;
	for (int32 s = 0; s < NumSectors; ++s)
	{
		UProceduralMeshComponent* C = NewObject<UProceduralMeshComponent>(this, NAME_None, RF_Transient);
		C->SetupAttachment(RootComponent);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(false);
		C->bUseAsyncCooking = false;
		C->BoundsScale = 1.6f;   // the grass sways by world position offset: its blades leave the mesh bounds a little
		C->SetWorldLocation(FVector::ZeroVector);
		C->RegisterComponent();
		for (int32 m = 0; m < NumMats; ++m)
		{
			const FBuf& B = Bufs[s * NumMats + m];
			if (B.T.Num() == 0) continue;
			C->CreateMeshSection_LinearColor(m, B.V, B.T, B.N, B.UV0, B.UV1, {}, {}, {}, {}, false, false);
			if (m < Mats.Num()) C->SetMaterial(m, Mats[m]);
			Tris += B.T.Num() / 3;
		}
		Sectors.Add(C);
	}
	NearTufts.Reset();
	for (const FVector2D& P : TuftPos) NearTufts.Add(P + FVector2D(Trunk));
	SetFloorEnabled(bEnabled);
	{
		// Evenness evidence: near tufts per octant of the compass, and their distance from the trunk axis.
		int32 Octant[8] = {};
		TArray<float> Dist;
		for (const FVector2D& P : TuftPos)
		{
			Octant[FMath::Clamp(int32((FMath::Atan2(P.Y, P.X) + PI) / (2.f * PI) * 8.f), 0, 7)]++;
			Dist.Add(float(P.Size()) / 100.f);
		}
		Dist.Sort();
		const int32 N = Dist.Num();
		UE_LOG(LogLeafline, Log, TEXT("Forest floor grass: %d near tufts by octant %d %d %d %d %d %d %d %d, distance from the trunk axis p10 %.0f m, median %.0f m, p90 %.0f m"),
			N, Octant[0], Octant[1], Octant[2], Octant[3], Octant[4], Octant[5], Octant[6], Octant[7],
			N ? Dist[N / 10] : 0.f, N ? Dist[N / 2] : 0.f, N ? Dist[N * 9 / 10] : 0.f);
	}
	{
		FString Bins;
		for (int32 b = 0; b < 20; ++b) Bins += FString::Printf(TEXT(" %d:%d/%d"), b * 10, OkBins[b], TryBins[b]);
		UE_LOG(LogLeafline, Log, TEXT("Forest floor grass tries by 10 m of distance (ok/tried):%s"), *Bins);
	}
	if (NestPts.Num() > 0)
	{
		// What the nest camera sees: from the first nest spot, per compass direction (30 degree wedge), how far out grass is
		// first allowed along the ray, and how many near tufts lie within the wedge and 45 m.
		FString Out;
		const FVector2D N0 = NestPts[0];
		for (int32 k = 0; k < 8; ++k)
		{
			const float Ang = k * PI / 4.f;
			const FVector2D Dir(FMath::Cos(Ang), FMath::Sin(Ang));
			float First = -1.f;
			for (float D = 0.f; D < 6000.f; D += 50.f)
			{
				if (ClearDist(N0 + Dir * D) >= FlatEnd + 100.f) { First = D; break; }
			}
			int32 Cnt = 0;
			for (const FVector2D& P : TuftPos)
			{
				const FVector2D V = P - N0;
				const float L = float(V.Size());
				if (L < 4500.f && L > 1.f && FVector2D::DotProduct(V / L, Dir) > 0.866f) ++Cnt;
			}
			Out += FString::Printf(TEXT(" %d deg: first %.0f m, %d tufts;"), int32(k * 45), First / 100.f, Cnt);
		}
		UE_LOG(LogLeafline, Log, TEXT("Forest floor nest probe (nest spot 0 at %.0f m from the axis):%s"), float(N0.Size()) / 100.f, *Out);
	}
	UE_LOG(LogLeafline, Log, TEXT("Forest floor: seed %d, trunk disc %.0f cm (flared radius %.0f), root reach %.0f, %d leaves (%d flat in the ring), %d tufts (%d blades), %d twigs, %lld triangles, built in %.1f ms"),
		Sk.Seed, TrunkClearR, TrunkR, Reach, NLeaves, NFlat, NTufts, NBlades, NTwigs, Tris, (FPlatformTime::Seconds() - Start) * 1000.0);
}
