// Wind exposure map (docs/briefs/exposure-overlay.md).
#include "World/ExposureOverlay.h"
#include "AntTreeGame.h"
#include "Ant/AntMovementComponent.h"
#include "Ant/AntPawn.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "Tree/TreeActor.h"
#include "UObject/ConstructorHelpers.h"
#include "World/WindSystem.h"

namespace
{
	/** The overlay last found or spawned (checked against the world before use). */
	TWeakObjectPtr<AExposureOverlay> GExposureOverlay;

	constexpr int32 ExposureSpotSides = 8;
	constexpr int32 ExposureMaxRingPoints = 1024;

	/** A unit vector perpendicular to T. */
	FVector ExposurePerpendicular(const FVector& T)
	{
		const FVector Ref = FMath::Abs(T.Z) < 0.9 ? FVector::UpVector : FVector::ForwardVector;
		return (Ref - T * FVector::DotProduct(Ref, T)).GetSafeNormal();
	}
}

AExposureOverlay::AExposureOverlay()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Spots"));
	Mesh->SetupAttachment(RootComponent);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
	Mesh->SetAffectDistanceFieldLighting(false);
	Mesh->SetVisibleInRayTracing(false);
	Mesh->SetVisibility(false);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> VertexColorMat(TEXT("/Engine/EngineDebugMaterials/VertexColorMaterial.VertexColorMaterial"));
	if (VertexColorMat.Succeeded()) FallbackMaterial = VertexColorMat.Object;
}

AExposureOverlay* AExposureOverlay::Get(UWorld* World)
{
	if (!World || !World->IsGameWorld()) return nullptr;
	if (AExposureOverlay* Cached = GExposureOverlay.Get())
	{
		if (Cached->GetWorld() == World && !Cached->IsActorBeingDestroyed()) return Cached;
	}
	for (TActorIterator<AExposureOverlay> It(World); It; ++It)
	{
		if (It->IsActorBeingDestroyed()) continue;
		GExposureOverlay = *It;
		return *It;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags |= RF_Transient;
	AExposureOverlay* Spawned = World->SpawnActor<AExposureOverlay>(AExposureOverlay::StaticClass(), FTransform::Identity, Params);
	GExposureOverlay = Spawned;
	return Spawned;
}

int32 AExposureOverlay::BandOf(float Exposure)
{
	// Same thresholds as the HUD's "Here: sheltered / partly exposed / exposed" (UI/LeaflineHUD.cpp).
	return Exposure < 0.25f ? 0 : (Exposure < 0.6f ? 1 : 2);
}

FLinearColor AExposureOverlay::BandColor(int32 Band)
{
	switch (Band)
	{
	case 0: return FLinearColor(0.6f, 0.95f, 0.5f);
	case 1: return FLinearColor(1.f, 0.8f, 0.35f);
	default: return FLinearColor(0.85f, 0.35f, 0.25f); // the HUD's StaminaLockedColor
	}
}

bool AExposureOverlay::CanFade() const
{
	return SpotMaterial != nullptr && Tuning.FadeTime > 0.f;
}

UMaterialInterface* AExposureOverlay::MaterialInUse() const
{
	return SpotMaterial ? SpotMaterial.Get() : FallbackMaterial.Get();
}

void AExposureOverlay::SetShown(bool bShow)
{
	if (bShow == bShown) return;
	bShown = bShow;
	const double Now = FPlatformTime::Seconds();
	if (bShow)
	{
		// Shown again during the fade-out: the values are a fraction of a second old, keep them.
		bClearOnShow = Fade <= 0.f;
		bShowPending = true;
		ShowStartTime = Now;
		PositionSeconds = 0.0;
		bFillLogged = false;
		NextLookupTime = 0.0;
		RebuildTimer = 0.f;
		Mesh->SetVisibility(true);
		SetActorTickEnabled(true);
	}
	else
	{
		UE_LOG(LogLeafline, Log, TEXT("Exposure map hidden (%d spots, shown %.1f s)"), Spots.Num(), Now - ShowStartTime);
		if (!CanFade()) FinishHide();
	}
}

void AExposureOverlay::FinishHide()
{
	Fade = 0.f;
	Mesh->SetVisibility(false);
	Mesh->ClearAllMeshSections();
	MeshVerts.Reset();
	MeshColors.Reset();
	bMeshDirty = true;
	SetActorTickEnabled(false);
}

bool AExposureOverlay::ResolveWorld(ATreeActor*& OutTree, AWindSystem*& OutWind)
{
	const double Now = FPlatformTime::Seconds();
	if ((!TreeRef.IsValid() || !WindRef.IsValid()) && Now >= NextLookupTime)
	{
		NextLookupTime = Now + 1.0;
		if (!TreeRef.IsValid()) TreeRef = ATreeActor::Get(this);
		if (!WindRef.IsValid()) WindRef = AWindSystem::Get(this);
	}
	OutTree = TreeRef.Get();
	OutWind = WindRef.Get();
	return OutTree && OutWind && OutTree->GetField().IsValid();
}

void AExposureOverlay::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	const float Target = bShown ? 1.f : 0.f;
	const float NewFade = CanFade() ? FMath::FInterpConstantTo(Fade, Target, DeltaTime, 1.f / Tuning.FadeTime) : Target;
	const bool bFadeChanged = NewFade != Fade;
	Fade = NewFade;

	if (!bShown)
	{
		// Fading out: no sampling, only the alpha.
		if (bFadeChanged) ApplyFade();
		if (Fade <= 0.f) FinishHide();
		return;
	}

	const double TickStart = FPlatformTime::Seconds();
	int32 NumPositions = 0, NumExposures = 0;
	double PosEnd = TickStart, ExpEnd = TickStart;

	ATreeActor* Tree = nullptr;
	AWindSystem* Wind = nullptr;
	const AAntPawn* Ant = Cast<AAntPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
	const UAntMovementComponent* Move = Ant ? Ant->GetAntMovement() : nullptr;
	if (ResolveWorld(Tree, Wind) && Move)
	{
		const FVector Centre = Move->GetSurfacePosition();
		FTreeKey Key;
		Key.Seed = Tree->GetSeed();
		Key.Branches = Tree->GetSkeleton().Branches.Num();
		Key.Leaves = Tree->GetLeaves().Num();

		if (Key != TreeKey && bSampled)
		{
			UE_LOG(LogLeafline, Log, TEXT("Exposure map: the tree changed, sampling again"));
			ResetSpots();
		}
		TreeKey = Key;

		const double Budget = FMath::Max(Tuning.WorkBudgetMs, 0.1f) * 1e-3;
		if (!bSampled)
		{
			ResetSpots();
			SampleCentre = Centre;
			bSampled = true;
			GenerateCandidates(*Tree, Centre, nullptr);
		}
		else if (bShowPending)
		{
			Recentre(Centre, bClearOnShow);
		}
		else if (FVector::DistSquared(Centre, SampleCentre) > FMath::Square((double)Tuning.RecentreDistance))
		{
			Recentre(Centre, false);
		}
		bShowPending = false;

		// Positions take half the budget while spots wait for a value, all of it otherwise.
		// (Candidate generation above adds its own time to PositionSeconds and counts against this tick's budget.)
		const double PosBudget = NextPending < Pending.Num() ? Budget * 0.5 : Budget;
		const double ProjectStart = FPlatformTime::Seconds();
		NumPositions = ProcessCandidates(*Tree, TickStart, PosBudget);
		PosEnd = FPlatformTime::Seconds();
		PositionSeconds += PosEnd - ProjectStart;

		NumExposures = ProcessExposures(*Wind, DeltaTime, TickStart, Budget);
		ExpEnd = FPlatformTime::Seconds();

		if (!bFillLogged && !bPassActive && NextPending >= Pending.Num())
		{
			bFillLogged = true;
			LogFill();
		}
	}

	RebuildTimer -= DeltaTime;
	if (bMeshDirty && RebuildTimer <= 0.f)
	{
		RebuildMesh();
		RebuildTimer = Tuning.RebuildInterval;
	}
	else if (bFadeChanged)
	{
		ApplyFade();
	}

	const double TickEnd = FPlatformTime::Seconds();
	if (TickEnd - TickStart > 0.003 && TickEnd - LastWarnTime > 1.0)
	{
		LastWarnTime = TickEnd;
		UE_LOG(LogLeafline, Warning, TEXT("Exposure map: one tick took %.1f ms (%d positions %.1f ms, %d exposures %.1f ms, mesh %.1f ms)"),
			(TickEnd - TickStart) * 1000.0, NumPositions, (PosEnd - TickStart) * 1000.0, NumExposures, (ExpEnd - PosEnd) * 1000.0,
			(TickEnd - FMath::Max(ExpEnd, PosEnd)) * 1000.0);
	}
}

void AExposureOverlay::LogFill() const
{
	int32 Count[3] = { 0, 0, 0 };
	for (const FSpot& S : Spots)
	{
		if (S.Band >= 0 && S.Band <= 2) ++Count[S.Band];
	}
	UE_LOG(LogLeafline, Log, TEXT("Exposure map: %d spots within %.0f m (%d sheltered, %d partly, %d exposed), positions in %.1f ms, filled in %.2f s"),
		Spots.Num(), Tuning.Radius / 100.f, Count[0], Count[1], Count[2], PositionSeconds * 1000.0, FPlatformTime::Seconds() - ShowStartTime);
}

// --- Sampling ---

void AExposureOverlay::ResetSpots()
{
	Spots.Reset();
	Candidates.Reset();
	NextCandidate = 0;
	Pending.Reset();
	NextPending = 0;
	RefreshCursor = 0;
	RefreshCarry = 0.f;
	bPassActive = false;
	bCovered = false;
	bSampled = false;
	bMeshDirty = true;
	RebuildHash();
}

void AExposureOverlay::Recentre(const FVector& NewCentre, bool bClearValues)
{
	ATreeActor* Tree = TreeRef.Get();
	if (!Tree) return;

	const double R2 = FMath::Square((double)Tuning.Radius);
	TArray<FSpot> Kept;
	Kept.Reserve(Spots.Num());
	for (const FSpot& S : Spots)
	{
		if (FVector::DistSquared(S.P, NewCentre) > R2) continue;
		FSpot& K = Kept.Add_GetRef(S);
		if (bClearValues)
		{
			K.Exposure = -1.f;
			K.Band = -1;
		}
	}
	if (bClearValues || Kept.Num() != Spots.Num()) bMeshDirty = true;
	Spots = MoveTemp(Kept);
	RebuildHash();

	Pending.Reset();
	NextPending = 0;
	for (int32 i = 0; i < Spots.Num(); ++i)
	{
		if (Spots[i].Band < 0) Pending.Add(i);
	}
	Pending.Sort([this, &NewCentre](int32 A, int32 B)
	{
		return FVector::DistSquared(Spots[A].P, NewCentre) < FVector::DistSquared(Spots[B].P, NewCentre);
	});
	RefreshCursor = 0;

	// Kept spots cover what the last completed pass sampled; only the rest of the new sphere needs candidates.
	const bool bUseCover = bCovered;
	const FVector Cover = CoveredCentre;
	SampleCentre = NewCentre;
	bSampled = true;
	GenerateCandidates(*Tree, NewCentre, bUseCover ? &Cover : nullptr);
}

void AExposureOverlay::GenerateCandidates(const ATreeActor& Tree, const FVector& Centre, const FVector* Covered)
{
	const double StartTime = FPlatformTime::Seconds();
	Candidates.Reset();
	NextCandidate = 0;
	bPassActive = true;
	bCovered = false;

	const FTreeSkeleton& Skeleton = Tree.GetSkeleton();
	const FTreeField& Field = Tree.GetField();
	const double Spacing = FMath::Max(Tuning.Spacing, 5.f);
	const double Radius = FMath::Max(Tuning.Radius, 10.f);
	// A projected point moves at most Spacing from its ring point (farther ones are dropped), so look that much wider.
	const double Reach = Radius + Spacing;
	const double Reach2 = Reach * Reach;
	const double CoverR = Radius - Spacing;
	const double CoverR2 = CoverR > 0.0 ? CoverR * CoverR : -1.0;
	const double Dedup = 0.6 * Spacing;
	const float MinRadius = FMath::Max(Tuning.MinBranchRadius, 0.5f);

	TArray<FVector> Tangents, Ups;
	for (int32 b = 0; b < Skeleton.Branches.Num(); ++b)
	{
		const FTreeBranch& Branch = Skeleton.Branches[b];
		const int32 NumSamples = Branch.Samples.Num();
		if (NumSamples < 2 || Branch.SampleRadius.Num() != NumSamples || Branch.SampleDist.Num() != NumSamples) continue;
		if (Branch.Bounds.IsValid && Branch.Bounds.ComputeSquaredDistanceToPoint(Centre) > Reach2) continue;

		// A frame per sample, carried along the branch (parallel transport) so the rings line up into even rows.
		Tangents.SetNumUninitialized(NumSamples);
		Ups.SetNumUninitialized(NumSamples);
		for (int32 i = 0; i < NumSamples; ++i)
		{
			const FVector D = Branch.Samples[FMath::Min(i + 1, NumSamples - 1)] - Branch.Samples[FMath::Max(i - 1, 0)];
			Tangents[i] = D.GetSafeNormal();
			if (Tangents[i].IsNearlyZero()) Tangents[i] = i > 0 ? Tangents[i - 1] : FVector::UpVector;
		}
		Ups[0] = ExposurePerpendicular(Tangents[0]);
		for (int32 i = 1; i < NumSamples; ++i)
		{
			FVector U = Ups[i - 1] - Tangents[i] * FVector::DotProduct(Ups[i - 1], Tangents[i]);
			Ups[i] = U.Normalize() ? U : ExposurePerpendicular(Tangents[i]);
		}

		const double Length = Branch.SampleDist.Last();
		int32 Seg = 0;
		for (int32 k = 0; k * Spacing <= Length; ++k)
		{
			const double S = k * Spacing;
			while (Seg < NumSamples - 2 && Branch.SampleDist[Seg + 1] < S) ++Seg;
			const double D0 = Branch.SampleDist[Seg], D1 = Branch.SampleDist[Seg + 1];
			const double A = D1 > D0 ? FMath::Clamp((S - D0) / (D1 - D0), 0.0, 1.0) : 0.0;
			const float RingRadius = FMath::Lerp(Branch.SampleRadius[Seg], Branch.SampleRadius[Seg + 1], (float)A);
			if (RingRadius < MinRadius) continue;
			const FVector C = FMath::Lerp(Branch.Samples[Seg], Branch.Samples[Seg + 1], A);
			if (FVector::DistSquared(C, Centre) > FMath::Square(Reach + RingRadius)) continue;

			FVector T = FMath::Lerp(Tangents[Seg], Tangents[Seg + 1], A);
			if (!T.Normalize()) T = Tangents[Seg];
			FVector U = FMath::Lerp(Ups[Seg], Ups[Seg + 1], A);
			U -= T * FVector::DotProduct(U, T);
			if (!U.Normalize()) U = ExposurePerpendicular(T);
			const FVector V = FVector::CrossProduct(T, U);

			// Sized from the circumference so spots are as far apart around the trunk as along it; the sphere test
			// below keeps the work bounded on the wide trunk.
			const int32 NumAround = FMath::Clamp(FMath::CeilToInt32(2.0 * PI * RingRadius / Spacing), 6, ExposureMaxRingPoints);
			const double Phase = (k & 1) ? 0.5 : 0.0; // stagger alternate rings
			const float Size = FMath::Clamp(0.5f * RingRadius, 0.35f * Tuning.SpotRadius, Tuning.SpotRadius);
			for (int32 j = 0; j < NumAround; ++j)
			{
				const double Angle = 2.0 * PI * (j + Phase) / NumAround;
				const FVector P = C + (U * FMath::Cos(Angle) + V * FMath::Sin(Angle)) * RingRadius;
				if (P.Z < Field.GroundZ - 1.0) continue; // sunk trunk and the buried half of the roots
				const double D2 = FVector::DistSquared(P, Centre);
				if (D2 > Reach2) continue;
				if (Covered && FVector::DistSquared(P, *Covered) < CoverR2) continue;
				if (HasSpotNear(P, Dedup)) continue;
				FCandidate& Cand = Candidates.AddDefaulted_GetRef();
				Cand.P = P;
				Cand.Size = Size;
				Cand.Dist2 = D2;
			}
		}
	}
	Candidates.Sort([](const FCandidate& A, const FCandidate& B) { return A.Dist2 < B.Dist2; });
	PositionSeconds += FPlatformTime::Seconds() - StartTime;
}

int32 AExposureOverlay::ProcessCandidates(const ATreeActor& Tree, double StartTime, double Budget)
{
	if (!bPassActive) return 0;
	const FTreeField& Field = Tree.GetField();
	const double Spacing = FMath::Max(Tuning.Spacing, 5.f);
	const double Radius2 = FMath::Square((double)Tuning.Radius);
	const double Dedup = 0.6 * Spacing;
	const int32 MaxCount = FMath::Max(Tuning.PositionsPerFrame, 1);
	int32 Done = 0;
	while (NextCandidate < Candidates.Num() && Done < MaxCount && FPlatformTime::Seconds() - StartTime < Budget)
	{
		const FCandidate Cand = Candidates[NextCandidate++];
		// A spot kept since the candidates were generated already sits here (siblings on a thin ring): free to reject,
		// so it does not count against PositionsPerFrame.
		if (HasSpotNear(Cand.P, Dedup)) continue;
		++Done;
		FVector P = Cand.P, N;
		if (!Field.Project(P, N, 0.0)) continue;
		// Slid a long way: it landed on another branch, whose own rings cover that spot.
		if (FVector::DistSquared(P, Cand.P) > Spacing * Spacing) continue;
		if (FVector::DistSquared(P, SampleCentre) > Radius2) continue;
		if (HasSpotNear(P, Dedup)) continue; // where branches blend into each other and the trunk
		int32 Nearest = INDEX_NONE;
		Field.Distance(P, &Nearest);
		if (Nearest == INDEX_NONE) continue; // the soil, not bark
		FSpot& Spot = Spots.AddDefaulted_GetRef();
		Spot.P = P;
		Spot.N = N;
		Spot.Size = Cand.Size;
		AddToHash(Spots.Num() - 1);
		Pending.Add(Spots.Num() - 1);
	}
	if (NextCandidate >= Candidates.Num())
	{
		Candidates.Reset();
		NextCandidate = 0;
		bPassActive = false;
		bCovered = true;
		CoveredCentre = SampleCentre;
	}
	return Done;
}

void AExposureOverlay::Evaluate(const AWindSystem& Wind, FSpot& Spot)
{
	Spot.Exposure = Wind.GetExposure(Spot.P, Spot.N);
	const int8 Band = (int8)BandOf(Spot.Exposure);
	if (Band != Spot.Band) bMeshDirty = true;
	Spot.Band = Band;
}

int32 AExposureOverlay::ProcessExposures(const AWindSystem& Wind, float DeltaTime, double StartTime, double Budget)
{
	const int32 MaxCount = FMath::Max(Tuning.SamplesPerFrame, 1);
	int32 Done = 0;
	while (NextPending < Pending.Num() && Done < MaxCount && FPlatformTime::Seconds() - StartTime < Budget)
	{
		const int32 Index = Pending[NextPending++];
		if (Spots.IsValidIndex(Index)) Evaluate(Wind, Spots[Index]);
		++Done;
	}
	if (NextPending >= Pending.Num())
	{
		Pending.Reset();
		NextPending = 0;
	}

	// Filled: refresh round-robin so the colours follow the wind's slow wander.
	if (!bPassActive && Pending.Num() == 0 && Spots.Num() > 0)
	{
		RefreshCarry = FMath::Min(RefreshCarry + Spots.Num() * DeltaTime / FMath::Max(Tuning.RefreshSeconds, 0.1f), (float)MaxCount);
		int32 Want = FMath::Min(FMath::FloorToInt(RefreshCarry), MaxCount - Done);
		while (Want > 0 && FPlatformTime::Seconds() - StartTime < Budget)
		{
			if (RefreshCursor >= Spots.Num()) RefreshCursor = 0;
			Evaluate(Wind, Spots[RefreshCursor++]);
			RefreshCarry -= 1.f;
			--Want;
			++Done;
		}
	}
	return Done;
}

// --- Spatial hash (cell = the de-duplication distance, so a query looks at the 27 cells round a point) ---

FIntVector AExposureOverlay::HashCell(const FVector& P) const
{
	return FIntVector(FMath::FloorToInt32(P.X / HashCellSize), FMath::FloorToInt32(P.Y / HashCellSize), FMath::FloorToInt32(P.Z / HashCellSize));
}

bool AExposureOverlay::HasSpotNear(const FVector& P, double Radius) const
{
	const double R = FMath::Min(Radius, HashCellSize); // the 27 cells reach one cell out
	const double R2 = R * R;
	const FIntVector C = HashCell(P);
	for (int32 X = -1; X <= 1; ++X)
		for (int32 Y = -1; Y <= 1; ++Y)
			for (int32 Z = -1; Z <= 1; ++Z)
			{
				const TArray<int32>* List = Hash.Find(C + FIntVector(X, Y, Z));
				if (!List) continue;
				for (int32 Index : *List)
				{
					if (FVector::DistSquared(Spots[Index].P, P) < R2) return true;
				}
			}
	return false;
}

void AExposureOverlay::AddToHash(int32 Index)
{
	Hash.FindOrAdd(HashCell(Spots[Index].P)).Add(Index);
}

void AExposureOverlay::RebuildHash()
{
	HashCellSize = 0.6 * FMath::Max(Tuning.Spacing, 5.f);
	Hash.Reset();
	for (int32 i = 0; i < Spots.Num(); ++i) AddToHash(i);
}

// --- Mesh ---

void AExposureOverlay::RebuildMesh()
{
	bMeshDirty = false;
	int32 NumValued = 0;
	for (const FSpot& S : Spots) NumValued += S.Band >= 0 ? 1 : 0;
	MeshVerts.Reset(NumValued * (ExposureSpotSides + 1));
	MeshColors.Reset(NumValued * (ExposureSpotSides + 1));
	if (NumValued == 0)
	{
		Mesh->ClearAllMeshSections();
		return;
	}

	TArray<FVector> Normals;
	TArray<FVector2D> UV;
	TArray<FProcMeshTangent> Tangents;
	TArray<int32> Tris;
	Normals.Reserve(NumValued * (ExposureSpotSides + 1));
	UV.Reserve(NumValued * (ExposureSpotSides + 1));
	Tangents.Reserve(NumValued * (ExposureSpotSides + 1));
	Tris.Reserve(NumValued * ExposureSpotSides * 3);

	FVector2D Rim[ExposureSpotSides];
	for (int32 j = 0; j < ExposureSpotSides; ++j)
	{
		const double Angle = 2.0 * PI * j / ExposureSpotSides;
		Rim[j] = FVector2D(FMath::Cos(Angle), FMath::Sin(Angle));
	}

	const float Alpha = Tuning.SpotAlpha;
	for (const FSpot& S : Spots)
	{
		if (S.Band < 0) continue;
		FLinearColor Color = BandColor(S.Band);
		Color.A = Alpha;
		const FVector Centre = S.P + S.N * Tuning.Lift;
		const FVector U = ExposurePerpendicular(S.N);
		const FVector V = FVector::CrossProduct(S.N, U);
		const FProcMeshTangent Tangent(U, false);
		const int32 Base = MeshVerts.Num();
		MeshVerts.Add(Centre);
		UV.Add(FVector2D(0.0, 0.0));
		for (int32 j = 0; j < ExposureSpotSides; ++j)
		{
			MeshVerts.Add(Centre + (U * Rim[j].X + V * Rim[j].Y) * S.Size);
			UV.Add(Rim[j]);
		}
		for (int32 j = 0; j <= ExposureSpotSides; ++j)
		{
			Normals.Add(S.N);
			Tangents.Add(Tangent);
			MeshColors.Add(Color);
		}
		// UE front faces: (b - a) x (c - a) points away from the viewer, who looks down the normal (as TrailNetwork).
		const FVector Cross = FVector::CrossProduct(MeshVerts[Base + 1] - Centre, MeshVerts[Base + 2] - Centre);
		const bool bFlip = FVector::DotProduct(Cross, S.N) > 0.0;
		for (int32 j = 0; j < ExposureSpotSides; ++j)
		{
			const int32 I1 = Base + 1 + j;
			const int32 I2 = Base + 1 + (j + 1) % ExposureSpotSides;
			if (bFlip) Tris.Append({ Base, I2, I1 });
			else Tris.Append({ Base, I1, I2 });
		}
	}

	TArray<FLinearColor> Faded = MeshColors;
	for (FLinearColor& C : Faded) C.A *= Fade;
	Mesh->CreateMeshSection_LinearColor(0, MeshVerts, Tris, Normals, UV, {}, {}, {}, Faded, Tangents, false, false);
	if (UMaterialInterface* Mat = MaterialInUse()) Mesh->SetMaterial(0, Mat);
}

void AExposureOverlay::ApplyFade()
{
	if (MeshVerts.Num() == 0 || Mesh->GetNumSections() == 0) return;
	TArray<FLinearColor> Faded = MeshColors;
	for (FLinearColor& C : Faded) C.A *= Fade;
	// Positions and colours only; normals, UVs and tangents stay as built. Same sRGB setting as the build.
	Mesh->UpdateMeshSection_LinearColor(0, MeshVerts, {}, {}, {}, {}, {}, Faded, {}, false);
}

// --- Console ---

namespace
{
	void ToggleExposureMap(UWorld* World, const TArray<FString>& Args)
	{
		AExposureOverlay* Overlay = AExposureOverlay::Get(World);
		if (!Overlay)
		{
			UE_LOG(LogLeafline, Warning, TEXT("Leaf.Exposure: only in a game world (Play In Editor or the game)"));
			return;
		}
		const bool bShow = Args.Num() > 0 ? FCString::Atoi(*Args[0]) != 0 : !Overlay->IsShown();
		UE_LOG(LogLeafline, Log, TEXT("Leaf.Exposure %d"), bShow ? 1 : 0);
		Overlay->SetShown(bShow);
	}

	FAutoConsoleCommandWithWorldAndArgs CmdExposure(
		TEXT("Leaf.Exposure"),
		TEXT("Leaf.Exposure [0|1]: show or hide the wind exposure map round the ant (toggles with no argument)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World) { ToggleExposureMap(World, Args); }));
}
