#include "Game/LeaflineShots.h"
#include "Ant/AntCamera.h"
#include "Ant/AntPawn.h"
#include "Ant/AntMovementComponent.h"
#include "AntTreeGame.h"
#include "Tree/TreeActor.h"
#include "UI/LeaflineHUD.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "ProceduralMeshComponent.h"

namespace
{
	double Ease(double T)
	{
		T = FMath::Clamp(T, 0.0, 1.0);
		return T * T * T * (T * (T * 6.0 - 15.0) + 10.0);
	}

	FVector Tangent(const FVector& V, const FVector& N)
	{
		FVector T = FVector::VectorPlaneProject(V, N).GetSafeNormal();
		if (T.IsNearlyZero()) T = FVector::VectorPlaneProject(FVector::UpVector, N).GetSafeNormal();
		if (T.IsNearlyZero()) T = FVector::VectorPlaneProject(FVector::ForwardVector, N).GetSafeNormal();
		return T;
	}

	FVector BranchPoint(const FTreeBranch& Branch, double Distance, FVector& Axis, double& Radius)
	{
		int32 I = 0;
		while (I + 2 < Branch.Samples.Num() && Branch.SampleDist[I + 1] < Distance) ++I;
		const double Length = Branch.SampleDist[I + 1] - Branch.SampleDist[I];
		const double T = Length > 0.001 ? FMath::Clamp((Distance - Branch.SampleDist[I]) / Length, 0.0, 1.0) : 0.0;
		Axis = (Branch.Samples[I + 1] - Branch.Samples[I]).GetSafeNormal();
		Radius = FMath::Lerp((double)Branch.SampleRadius[I], (double)Branch.SampleRadius[I + 1], T);
		return FMath::Lerp(Branch.Samples[I], Branch.Samples[I + 1], T);
	}

	struct FVideoGroveVolume
	{
		FBox Bounds = FBox(ForceInit);
		int32 Tree = 0;
		bool bCrown = false;
	};

	struct FVideoGroveGeometry
	{
		TArray<FVector> Bases;
		TArray<FVideoGroveVolume> Volumes;
	};

	FVideoGroveGeometry VideoGroveGeometry(ATreeActor* Tree)
	{
		// Read generated geometry: no tree actor changes and no replay of BuildGrove's random stream.
		FVideoGroveGeometry Result;
		TArray<int32> Counts;
		TArray<UProceduralMeshComponent*> Meshes;
		Tree->GetComponents(Meshes);
		for (UProceduralMeshComponent* Mesh : Meshes)
		{
			if (Mesh->GetFName() != FName(TEXT("GroveMesh"))) continue;
			for (int32 S = 0; S < Mesh->GetNumSections(); ++S)
			{
				const FProcMeshSection* Section = Mesh->GetProcMeshSection(S);
				if (!Section) continue;
				for (const FProcMeshVertex& Vertex : Section->ProcVertexBuffer)
				{
					const FVector P = Mesh->GetComponentTransform().TransformPosition(Vertex.Position);
					if (P.Z > Tree->GetField().GroundZ + 100.0) continue;
					int32 Cluster = INDEX_NONE;
					for (int32 I = 0; I < Result.Bases.Num(); ++I)
						if (FVector::DistSquared2D(P, Result.Bases[I]) < FMath::Square(4000.0)) { Cluster = I; break; }
					if (Cluster == INDEX_NONE) { Result.Bases.Add(P); Counts.Add(1); }
					else { ++Counts[Cluster]; Result.Bases[Cluster] += (P - Result.Bases[Cluster]) / Counts[Cluster]; }
				}
			}
		}

		// Bin whole triangles by tree and height. Every trunk/limb/blade is enclosed, including triangles crossing bins.
		// These conservative boxes test crown geometry, unlike a fixed radius around a ground-level trunk base.
		TMap<int32, int32> Bins;
		for (UProceduralMeshComponent* Mesh : Meshes)
		{
			const bool bCrown = Mesh->GetFName() == FName(TEXT("GroveLeafMesh"));
			if (!bCrown && Mesh->GetFName() != FName(TEXT("GroveMesh"))) continue;
			for (int32 S = 0; S < Mesh->GetNumSections(); ++S)
			{
				const FProcMeshSection* Section = Mesh->GetProcMeshSection(S);
				if (!Section) continue;
				TArray<FVector> Positions;
				Positions.Reserve(Section->ProcVertexBuffer.Num());
				for (const FProcMeshVertex& Vertex : Section->ProcVertexBuffer)
					Positions.Add(Mesh->GetComponentTransform().TransformPosition(Vertex.Position));
				for (int32 I = 0; I + 2 < Section->ProcIndexBuffer.Num(); I += 3)
				{
					const FVector A = Positions[Section->ProcIndexBuffer[I]], B = Positions[Section->ProcIndexBuffer[I + 1]], C = Positions[Section->ProcIndexBuffer[I + 2]];
					const FVector Centre = (A + B + C) / 3.0;
					int32 Owner = 0;
					double Best = TNumericLimits<double>::Max();
					for (int32 J = 0; J < Result.Bases.Num(); ++J)
					{
						const double D2 = FVector::DistSquared2D(Centre, Result.Bases[J]);
						if (D2 < Best) { Best = D2; Owner = J; }
					}
					const int32 Band = FMath::Clamp(FMath::FloorToInt((Centre.Z - Tree->GetField().GroundZ) / 2000.0), 0, 63);
					const int32 Key = (Owner * 64 + Band) * 2 + (bCrown ? 1 : 0);
					int32* Found = Bins.Find(Key);
					int32 Index;
					if (Found) Index = *Found;
					else
					{
						Index = Result.Volumes.AddDefaulted();
						Result.Volumes[Index].Tree = Owner;
						Result.Volumes[Index].bCrown = bCrown;
						Bins.Add(Key, Index);
					}
					Result.Volumes[Index].Bounds += A;
					Result.Volumes[Index].Bounds += B;
					Result.Volumes[Index].Bounds += C;
				}
			}
		}
		for (FVideoGroveVolume& Volume : Result.Volumes) Volume.Bounds = Volume.Bounds.ExpandBy(250.0); // sway margin
		return Result;
	}

	void VideoSampleRail(const TArray<FVector>& Points, bool bSmooth, TArray<FVector>& Out)
	{
		Out.Reset();
		if (Points.IsEmpty()) return;
		Out.Add(Points[0]);
		for (int32 I = 0; I + 1 < Points.Num(); ++I)
		{
			const FVector P0 = Points[FMath::Max(I - 1, 0)], P1 = Points[I], P2 = Points[I + 1], P3 = Points[FMath::Min(I + 2, Points.Num() - 1)];
			const double Length = FVector::Dist(P0, P1) + FVector::Dist(P1, P2) + FVector::Dist(P2, P3);
			const FVector M1 = (P2 - P0).GetSafeNormal() * (FMath::Min(FVector::Dist(P0, P1), FVector::Dist(P1, P2)) * 0.7);
			const FVector M2 = (P3 - P1).GetSafeNormal() * (FMath::Min(FVector::Dist(P1, P2), FVector::Dist(P2, P3)) * 0.7);
			const int32 Steps = FMath::Max(2, FMath::CeilToInt(Length / 100.0));
			for (int32 J = 1; J <= Steps; ++J)
			{
				const double T = (double)J / Steps;
				Out.Add(bSmooth ? FMath::CubicInterp(P1, M1, P2, M2, T) : FMath::Lerp(P1, P2, T));
			}
		}
	}

	bool VideoGroveInView(const FBox& Bounds, const FVector& Camera, const FVector& View, const FVector& Right,
		const FVector& Up, double TanHalfH, double TanHalfV, double Far, double& OutGap)
	{
		const FVector Centre = Bounds.GetCenter() - Camera, Extent = Bounds.GetExtent();
		const FVector Planes[] = { View, -View, View * TanHalfH + Right, View * TanHalfH - Right,
			View * TanHalfV + Up, View * TanHalfV - Up };
		const double Offsets[] = { -12.0, Far, 0.0, 0.0, 0.0, 0.0 };
		OutGap = -TNumericLimits<double>::Max();
		for (int32 I = 0; I < (int32)UE_ARRAY_COUNT(Planes); ++I)
		{
			const double Outside = -(FVector::DotProduct(Planes[I], Centre) + Offsets[I] + FVector::DotProduct(Planes[I].GetAbs(), Extent));
			OutGap = FMath::Max(OutGap, Outside / Planes[I].Size());
		}
		return OutGap <= 0.0;
	}

}

ALeaflineShots::ALeaflineShots()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	PrimaryActorTick.bTickEvenWhenPaused = true;
}

ALeaflineShots* ALeaflineShots::Get(UWorld* World)
{
	if (!World || !World->IsGameWorld()) return nullptr;
	for (TActorIterator<ALeaflineShots> It(World); It; ++It) return *It;
	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	return World->SpawnActor<ALeaflineShots>(Params);
}

AAntCamera* ALeaflineShots::FindCamera() const
{
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	return PC ? Cast<AAntCamera>(PC->GetViewTarget()) : nullptr;
}

void ALeaflineShots::SetCinematic(bool bOn)
{
	if (bCinematic == bOn) return;
	if (bOn && GEngine) bSavedScreenMessages = GEngine->bEnableOnScreenDebugMessagesDisplay;
	bCinematic = bOn;
	if (GEngine) GEngine->bEnableOnScreenDebugMessagesDisplay = bOn ? false : bSavedScreenMessages;
	ApplyCinematic();
	UE_LOG(LogLeafline, Log, TEXT("Cinematic %s"), bOn ? TEXT("on") : TEXT("off"));
}

void ALeaflineShots::ApplyCinematic()
{
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (ALeaflineHUD* HUD = Cast<ALeaflineHUD>(PC->GetHUD())) HUD->bHideAll = bCinematic;
		if (bCinematic) PC->bShowMouseCursor = false;
	}
	if (AAntCamera* Camera = FindCamera())
		if (Camera->IsCinematic() != bCinematic) Camera->SetCinematic(bCinematic);
}

bool ALeaflineShots::Play(const FString& Name, float Seconds)
{
	if (!IsShotName(Name) || !FMath::IsFinite(Seconds) || Seconds < 2.f || Seconds > 120.f) return false;
	Stop();
	return StartShot(Name, Seconds);
}

bool ALeaflineShots::IsShotName(const FString& Name)
{
	return Name == TEXT("orbit") || Name == TEXT("crane") || Name == TEXT("dolly") || Name == TEXT("reveal") || Name == TEXT("follow");
}

bool ALeaflineShots::PlayList(const TArray<FString>& Names)
{
	if (Names.IsEmpty() || Names.Num() > 16) return false;
	for (const FString& Name : Names) if (!IsShotName(Name)) return false;
	Stop();
	Queue = Names;
	const FString First = Queue[0];
	Queue.RemoveAt(0);
	if (StartShot(First, 8.f)) return true;
	Stop();
	return false;
}

bool ALeaflineShots::StartShot(const FString& Name, float Seconds)
{
	AAntCamera* Camera = FindCamera();
	ATreeActor* Tree = ATreeActor::Get(this);
	AAntPawn* Ant = Camera ? Camera->GetTarget() : nullptr;
	if (!Camera || !Ant || !Ant->GetAntMovement() || !Tree || !Tree->GetField().IsValid())
	{
		UE_LOG(LogLeafline, Warning, TEXT("Shot: start a day and wait for the player camera first."));
		return false;
	}
	const bool bContinuing = ActiveCamera.Get() == Camera;
	ShotAnt = Ant;
	ShotTree = Tree;
	ShotName = Name;
	Duration = Seconds;
	Elapsed = 0.0;
	// Grove analysis may take a frame: begin the reveal clock on its first driven tick after planning.
	LastRealTime = Name == TEXT("reveal") ? -1.0 : GetWorld()->GetRealTimeSeconds();
	EntryView = Camera->GetActorTransform();
	EntryFOV = Camera->GetFieldOfView();
	if (!bContinuing)
	{
		ReturnView = EntryView;
		ReturnFOV = EntryFOV;
		History.Reset();
		History.Add(ReturnView.GetLocation());
	}
	bReturning = false;
	if (Tree->GetField().Distance(EntryView.GetLocation()) < 12.0)
	{
		UE_LOG(LogLeafline, Warning, TEXT("Shot: player camera is inside wood; turn it clear before recording (exact return would be unsafe)."));
		return false;
	}
	Anchor = Ant->GetAntMovement()->GetSurfacePosition();
	Normal = Ant->GetAntMovement()->GetSurfaceNormal().GetSafeNormal();
	Forward = Tangent(Ant->GetAntMovement()->GetBodyForward(), Normal);
	Right = FVector::CrossProduct(Normal, Forward).GetSafeNormal();
	Boom = Camera->Tuning.BoomLength;
	TreeBounds = FBox(ForceInit);
	for (const FTreeBranch& Branch : Tree->GetSkeleton().Branches) TreeBounds += Branch.Bounds;
	TreeBounds = TreeBounds.ExpandBy(250.0); // leaf blades extend past the skeleton's branch bounds
	Crown = TreeBounds.GetCenter();
	Crown.Z = FMath::Lerp(TreeBounds.Min.Z, TreeBounds.Max.Z, 0.78);
	const FTransform NestTransform = Tree->GetNestTransform();
	Nest = NestTransform.GetLocation();
	// GetNestTransform faces the worker toward the trunk; the reveal pulls in the opposite direction.
	NestOut = -NestTransform.GetRotation().GetForwardVector().GetSafeNormal2D();
	if (NestOut.IsNearlyZero()) NestOut = FVector::ForwardVector;
	if (ShotName == TEXT("reveal")) PrepareReveal();

	DollyBranch = INDEX_NONE;
	Tree->GetField().Distance(Anchor, &DollyBranch);
	const TArray<FTreeBranch>& Branches = Tree->GetSkeleton().Branches;
	if (Branches.IsValidIndex(DollyBranch) && Branches[DollyBranch].Samples.Num() >= 2)
	{
		const FTreeBranch& Branch = Branches[DollyBranch];
		double Best = TNumericLimits<double>::Max(), At = 0.0;
		for (int32 I = 0; I + 1 < Branch.Samples.Num(); ++I)
		{
			const FVector A = Branch.Samples[I], Delta = Branch.Samples[I + 1] - A;
			const double T = FMath::Clamp(FVector::DotProduct(Anchor - A, Delta) / FMath::Max(Delta.SizeSquared(), 0.001), 0.0, 1.0);
			const double D2 = FVector::DistSquared(Anchor, A + Delta * T);
			if (D2 < Best)
			{
				Best = D2;
				At = FMath::Lerp((double)Branch.SampleDist[I], (double)Branch.SampleDist[I + 1], T);
			}
		}
		FVector Axis;
		double Radius;
		const FVector Centre = BranchPoint(Branch, At, Axis, Radius);
		DollyRadial = Tangent(Anchor - Centre, Axis);
		double Sign = FVector::DotProduct(Forward, Axis) >= 0.0 ? 1.0 : -1.0;
		if ((Sign > 0.0 ? At : Branch.Length() - At) < Boom * 1.8) Sign = -Sign;
		DollyFrom = FMath::Clamp(At - Sign * Boom * 1.8, 0.0, (double)Branch.Length());
		DollyTo = FMath::Clamp(At - Sign * Boom, 0.0, (double)Branch.Length());
	}
	else DollyBranch = INDEX_NONE; // ground/nest: use a low straight approach
	ChasePosition = Anchor + Normal * Boom * 0.7 - Forward * Boom * 1.6;
	ChaseFocus = Anchor + Normal * 10.0;
	ChaseUp = Normal;
	LastSafePosition = EntryView.GetLocation();
	if (!ClearPosition(LastSafePosition))
	{
		UE_LOG(LogLeafline, Warning, TEXT("Shot: no safe starting camera position."));
		return false;
	}
	MinClearance = TNumericLimits<double>::Max();
	HeldFrames = 0;
	ClippedFrames = 0;
	ActiveCamera = Camera;
	if (!bContinuing) Camera->BeginExternalDrive();
	// Run before the camera and the camera manager, so both read this frame's pose.
	Camera->AddTickPrerequisiteActor(this);
	UE_LOG(LogLeafline, Log, TEXT("Shot %s starts: %.2f real seconds; seed %d"), *ShotName, Duration, Tree->GetSeed());
	return true;
}

void ALeaflineShots::PrepareReveal()
{
	const double GroundZ = ShotTree->GetField().GroundZ;
	const double Height = FMath::Max(TreeBounds.Max.Z - GroundZ, 1000.0);
	RevealFocus = TreeBounds.GetCenter();
	RevealFocus.Z = GroundZ + Height * 0.5;
	const double Distance = FMath::Clamp(Height * 0.75, 5000.0, 26000.0);
	RevealCrownBounds = FBox(ForceInit);
	const FLeafSet& Leaves = ShotTree->GetLeaves();
	for (int32 I = 0; I < Leaves.Num(); ++I)
	{
		const FTreeLeaf& L = Leaves.Get(I);
		RevealCrownBounds += L.Base;
		const FVector HalfWidth = L.Side * (L.Width * 0.5);
		RevealCrownBounds += L.Origin - HalfWidth;
		RevealCrownBounds += L.Origin + HalfWidth;
		RevealCrownBounds += L.Origin + L.Dir * L.Length - HalfWidth;
		RevealCrownBounds += L.Origin + L.Dir * L.Length + HalfWidth;
	}
	if (RevealCrownBounds.IsValid) RevealCrownBounds = RevealCrownBounds.ExpandBy(250.0);
	RevealRail.Reset();
	bRevealFallback = false;
	RevealMaxRoll = 0.f;
	RevealStoppedFrames = 0;
	bRevealHoldLogged = false;
	FVector FallbackEnd = RevealFocus + NestOut * Distance;
	FallbackEnd.Z = GroundZ + Height / 3.0;
	float FallbackFOV = 110.f;
	int32 FallbackBlockers = MAX_int32, FallbackHeroHits = MAX_int32;
	double FallbackGap = -TNumericLimits<double>::Max(), FallbackTurn = 180.0;
	int32 FallbackCandidate = 0;
	FString SelectedRoute;
	const FVideoGroveGeometry Grove = VideoGroveGeometry(ShotTree.Get());
	int32 BestBlockers = MAX_int32, BestHeroHits = MAX_int32;
	double BestGap = -TNumericLimits<double>::Max(), BestTurn = 180.0;
	int32 Chosen = 0;
	float BestSpan = 0.f;
	for (int32 I = 0; I < 12; ++I)
	{
		const double Degrees = -75.0 + 150.0 * I / 11.0;
		const FVector Away = NestOut.RotateAngleAxis(Degrees, FVector::UpVector);
		FVector End = RevealFocus + Away * Distance;
		End.Z = GroundZ + Height / 3.0;
		const FVector View = (RevealFocus - End).GetSafeNormal();
		const FVector ViewRight = FVector::CrossProduct(FVector::UpVector, View).GetSafeNormal();
		const FVector ViewUp = FVector::CrossProduct(View, ViewRight).GetSafeNormal();
		FVector Top = RevealFocus, Bottom = RevealFocus;
		Top.Z = GroundZ + Height;
		Bottom.Z = GroundZ;
		const FVector ToTop = Top - End, ToBottom = Bottom - End;
		const double ProjectedHeight = FVector::DotProduct(ToTop, ViewUp) / FMath::Max(FVector::DotProduct(ToTop, View), 1.0) -
			FVector::DotProduct(ToBottom, ViewUp) / FMath::Max(FVector::DotProduct(ToBottom, View), 1.0);
		// Solve the lens for the ground-to-crown span, rather than fitting horizontal branch bounds.
		const double TanHalfV = ProjectedHeight / (2.0 * 0.70);
		const double TanHalfH = TanHalfV * 16.0 / 9.0;
		const float FOV = (float)FMath::RadiansToDegrees(2.0 * FMath::Atan(TanHalfH));
		// Include the crown's farthest depth, so an occluder above the midpoint is not mistaken for background.
		const double Far = FVector::DotProduct(TreeBounds.GetCenter() - End, View) +
			FVector::DotProduct(TreeBounds.GetExtent(), View.GetAbs());
		TArray<uint8> Blocked;
		Blocked.Init(0, FMath::Max(Grove.Bases.Num(), 1));
		int32 HeroHits = 0;
		double Gap = ShotTree->Params.GroveMaxDistance;
		// A tighter cone is used only to break blocker-count ties in favour of an unobscured hero silhouette.
		const double HeroHalfH = FMath::Min(TanHalfH, FMath::Max(200.0, FVector::DotProduct(TreeBounds.GetExtent(), ViewRight.GetAbs())) / Far * 1.1);
		for (const FVideoGroveVolume& Volume : Grove.Volumes)
		{
			const FVector ToBox = Volume.Bounds.GetCenter() - End;
			const double Along = FVector::DotProduct(ToBox, View), Radius = FVector::DotProduct(Volume.Bounds.GetExtent(), View.GetAbs());
			if (Along + Radius < 12.0 || Along - Radius > Far) continue; // only geometry between lens and main tree
			double VolumeGap;
			if (VideoGroveInView(Volume.Bounds, End, View, ViewRight, ViewUp, TanHalfH, TanHalfV, Far, VolumeGap))
			{
				Blocked[Volume.Tree] |= Volume.bCrown ? 2 : 1;
				double HeroGap;
				if (VideoGroveInView(Volume.Bounds, End, View, ViewRight, ViewUp, HeroHalfH, TanHalfV * 0.85, Far, HeroGap)) ++HeroHits;
			}
			Gap = FMath::Min(Gap, VolumeGap);
		}
		int32 Blockers = 0, Trunks = 0, Crowns = 0;
		for (uint8 Flags : Blocked)
		{
			if (Flags) ++Blockers;
			if (Flags & 1) ++Trunks;
			if (Flags & 2) ++Crowns;
		}
		if (Blockers < FallbackBlockers || (Blockers == FallbackBlockers && (HeroHits < FallbackHeroHits ||
			(HeroHits == FallbackHeroHits && (Gap > FallbackGap + 1.0 ||
			(FMath::Abs(Gap - FallbackGap) <= 1.0 && FMath::Abs(Degrees) < FallbackTurn))))))
		{
			FallbackBlockers = Blockers;
			FallbackHeroHits = HeroHits;
			FallbackGap = Gap;
			FallbackTurn = FMath::Abs(Degrees);
			FallbackCandidate = I + 1;
			FallbackEnd = End;
			FallbackFOV = FOV;
		}
		TArray<FVector> CandidateRail;
		FString Route;
		const bool bCleanPath = BuildRevealRail(End, CandidateRail, Route);
		UE_LOG(LogLeafline, Log, TEXT("Reveal candidate %d/12: azimuth offset %.1f, foreground trees %d (wood %d, crowns %d), hero volumes %d, cone gap %.0f cm, path %s (%s)"),
			I + 1, Degrees, Blockers, Trunks, Crowns, HeroHits, Gap, bCleanPath ? TEXT("clear") : TEXT("blocked"), *Route);
		const bool bBetter = Blockers < BestBlockers || (Blockers == BestBlockers && (HeroHits < BestHeroHits ||
			(HeroHits == BestHeroHits && (Gap > BestGap + 1.0 || (FMath::Abs(Gap - BestGap) <= 1.0 && FMath::Abs(Degrees) < BestTurn)))));
		if (bCleanPath && bBetter)
		{
			RevealRail = MoveTemp(CandidateRail);
			SelectedRoute = Route;
			BestBlockers = Blockers;
			BestHeroHits = HeroHits;
			BestGap = Gap;
			BestTurn = FMath::Abs(Degrees);
			Chosen = I + 1;
			RevealEnd = End;
			RevealFOV = FOV;
			BestSpan = (float)(ProjectedHeight / (2.0 * TanHalfV));
		}
	}
	if (RevealRail.IsEmpty())
	{
		bRevealFallback = true;
		const double BlendTime = FMath::Min(0.8, Duration * 0.2);
		// Freeze the previous reveal's six-second (75%-duration) composition, before its animated return.
		const double SixT = Ease((Duration * 0.75 - BlendTime) / (Duration - 2.0 * BlendTime));
		const FVector Start = Nest + NestOut * 180.0 + FVector::UpVector * 65.0;
		RevealEnd = FMath::Lerp(Start, FallbackEnd, SixT);
		RevealFOV = FMath::Lerp(65.f, FallbackFOV, (float)SixT);
		if (!BuildRevealRail(RevealEnd, RevealRail, SelectedRoute))
		{
			// An unreachable prefix must never be pushed through wood. Hold the incoming safe position instead.
			RevealRail.Reset();
			RevealRail.Add(EntryView.GetLocation());
			RevealEnd = EntryView.GetLocation();
			RevealFOV = EntryFOV;
			SelectedRoute = TEXT("entry hold: six-second pose unreachable");
		}
		BestBlockers = FallbackBlockers;
		BestHeroHits = FallbackHeroHits;
		Chosen = FallbackCandidate;
	}
	RevealDistances.Reset();
	RevealDistances.Add(0.0);
	for (int32 I = 1; I < RevealRail.Num(); ++I)
		RevealDistances.Add(RevealDistances.Last() + FVector::Dist(RevealRail[I - 1], RevealRail[I]));
	ValidateRevealRail(RevealRail, &RevealMinPlannedClearance);
	const FVector FinalView = (RevealFocus - RevealEnd).GetSafeNormal();
	const FVector FinalUp = FVector::VectorPlaneProject(FVector::UpVector, FinalView).GetSafeNormal();
	FVector ToTop = RevealFocus - RevealEnd, ToBottom = ToTop;
	ToTop.Z += Height * 0.5;
	ToBottom.Z -= Height * 0.5;
	BestSpan = (float)((FVector::DotProduct(ToTop, FinalUp) / FMath::Max(FVector::DotProduct(ToTop, FinalView), 1.0) -
		FVector::DotProduct(ToBottom, FinalUp) / FMath::Max(FVector::DotProduct(ToBottom, FinalView), 1.0)) /
		(2.0 * FMath::Tan(FMath::DegreesToRadians(RevealFOV * 0.5)) * 9.0 / 16.0));
	UE_LOG(LogLeafline, Log, TEXT("Reveal plan: height %.0f cm, pullback %.0f cm (cap 26000), camera height %.0f cm (1/3), frame height %.3f, FOV %.1f, grove bases %d, volumes %d, candidate %d/12, foreground blockers %d, hero volumes %d, end %s, focus %s"),
		Height, Distance, RevealEnd.Z - GroundZ, BestSpan, RevealFOV, Grove.Bases.Num(), Grove.Volumes.Num(), Chosen, BestBlockers, BestHeroHits,
		*RevealEnd.ToCompactString(), *RevealFocus.ToCompactString());
	UE_LOG(LogLeafline, Log, TEXT("Reveal rail: %s; %d samples, planned clearance %.2f cm, fallback-six %d; move ends at %.2f s, hold until %.2f s, no animated return"),
		*SelectedRoute, RevealRail.Num(), RevealMinPlannedClearance, bRevealFallback ? 1 : 0, Duration * 0.75f, Duration);
}

bool ALeaflineShots::ValidateRevealRail(const TArray<FVector>& Rail, double* OutMinClearance) const
{
	double Minimum = TNumericLimits<double>::Max();
	if (OutMinClearance) *OutMinClearance = 0.0;
	if (Rail.IsEmpty() || !ShotTree.IsValid()) return false;
	const FTreeField& Field = ShotTree->GetField();
	for (int32 I = 0; I < Rail.Num(); ++I)
	{
		Minimum = FMath::Min(Minimum, Field.Distance(Rail[I]));
		if (OutMinClearance) *OutMinClearance = Minimum;
		if (Minimum < 13.0 || (RevealCrownBounds.IsValid && RevealCrownBounds.IsInsideOrOn(Rail[I]))) return false;
		if (I == 0) continue;
		double Hit;
		if (Field.March(Rail[I - 1], Rail[I], 12.0, Hit)) return false;
		if (RevealCrownBounds.IsValid && FMath::LineBoxIntersection(RevealCrownBounds, Rail[I - 1], Rail[I], Rail[I] - Rail[I - 1])) return false;
	}
	return true;
}

bool ALeaflineShots::BuildRevealRail(const FVector& End, TArray<FVector>& OutRail, FString& OutRoute) const
{
	const double GroundZ = ShotTree->GetField().GroundZ;
	const FVector Start = Nest + NestOut * 180.0 + FVector::UpVector * 65.0;
	FVector Centre = RevealCrownBounds.IsValid ? RevealCrownBounds.GetCenter() : TreeBounds.GetCenter();
	Centre.Z = 0.0;
	const double Radius = (RevealCrownBounds.IsValid ? RevealCrownBounds.GetExtent().Size2D() : TreeBounds.GetExtent().Size2D()) + 750.0;
	const double TravelZ = FMath::Max(GroundZ + 150.0, FMath::Min(GroundZ + 500.0,
		RevealCrownBounds.IsValid ? RevealCrownBounds.Min.Z - 300.0 : GroundZ + 500.0));
	FVector FromDir = (Start - Centre).GetSafeNormal2D(), ToDir = (End - Centre).GetSafeNormal2D();
	if (FromDir.IsNearlyZero()) FromDir = NestOut;
	if (ToDir.IsNearlyZero()) ToDir = NestOut;
	const double Turn = FMath::RadiansToDegrees(FMath::Atan2(FVector::CrossProduct(FromDir, ToDir).Z, FVector::DotProduct(FromDir, ToDir)));
	auto Try = [this, &OutRail, &OutRoute](const TArray<FVector>& Points, const FString& Name)
	{
		VideoSampleRail(Points, true, OutRail);
		if (ValidateRevealRail(OutRail)) { OutRoute = Name + TEXT(" / smooth"); return true; }
		VideoSampleRail(Points, false, OutRail);
		if (ValidateRevealRail(OutRail)) { OutRoute = Name + TEXT(" / linear"); return true; }
		return false;
	};
	for (int32 Entrance = 0; Entrance < 3; ++Entrance)
	{
		TArray<FVector> Points;
		Points.Add(EntryView.GetLocation());
		if (Entrance == 1)
		{
			Points.Add(EntryView.GetLocation() + NestOut * 400.0 + FVector::UpVector * 400.0);
			Points.Add(Start + NestOut * 400.0 + FVector::UpVector * 400.0);
		}
		else if (Entrance == 2)
		{
			Points.Add(EntryView.GetLocation() + FVector::UpVector * 800.0);
			Points.Add(Start + FVector::UpVector * 800.0);
		}
		Points.Add(Start);
		TArray<FVector> Direct = Points;
		Direct.Add(End);
		if (Try(Direct, FString::Printf(TEXT("direct, entrance %d"), Entrance))) return true;

		// Pull outside the crown below its lowest leaves, arc round its horizontal envelope, then rise/pull back.
		const int32 ArcSteps = FMath::Max(1, FMath::CeilToInt(FMath::Abs(Turn) / 15.0));
		for (int32 I = 0; I <= ArcSteps; ++I)
		{
			FVector P = Centre + FromDir.RotateAngleAxis(Turn * I / ArcSteps, FVector::UpVector) * Radius;
			P.Z = TravelZ;
			if (FVector::DistSquared(Points.Last(), P) > 0.01) Points.Add(P);
		}
		if (FVector::DistSquared(Points.Last(), End) > 0.01) Points.Add(End);
		if (Try(Points, FString::Printf(TEXT("crown arc radius %.0f cm, entrance %d"), Radius, Entrance))) return true;
	}
	OutRoute = TEXT("no complete field/crown-clear path");
	OutRail.Reset();
	return false;
}

void ALeaflineShots::TickReveal()
{
	AAntCamera* Camera = ActiveCamera.Get();
	// Reach the composition by six seconds of an eight-second shot, then hold it through the end.
	const double Phase = Ease(Elapsed / (Duration * 0.75));
	const double Along = RevealDistances.Last() * Phase;
	FVector Position = RevealRail.Last();
	for (int32 I = 1; I < RevealRail.Num(); ++I)
	{
		if (RevealDistances[I] < Along) continue;
		const double Length = RevealDistances[I] - RevealDistances[I - 1];
		Position = FMath::Lerp(RevealRail[I - 1], RevealRail[I], Length > 0.0001 ? (Along - RevealDistances[I - 1]) / Length : 1.0);
		break;
	}
	const FTreeField& Field = ShotTree->GetField();
	if (Field.Distance(Position) < 13.0 || (RevealCrownBounds.IsValid && RevealCrownBounds.IsInsideOrOn(Position)))
	{
		// The rail was prevalidated. If the world changed, stop at the last safe point; never project or slide it.
		Position = LastSafePosition;
		++RevealStoppedFrames;
	}
	LastSafePosition = Position;
	History.Add(Position);
	MinClearance = FMath::Min(MinClearance, Field.Distance(Position));
	const FVector LookAt = FMath::Lerp(Nest + FVector::UpVector * 15.0, RevealFocus, Ease(Phase / 0.55));
	FVector Direction = (LookAt - Position).GetSafeNormal();
	if (Direction.IsNearlyZero()) Direction = EntryView.GetRotation().GetForwardVector();
	const double Intro = Ease(Elapsed / FMath::Min(0.8, Duration * 0.2));
	const FVector EntryForward = EntryView.GetRotation().GetForwardVector();
	Direction = FQuat::Slerp(FQuat::Identity, FQuat::FindBetweenNormals(EntryForward, Direction), Intro).RotateVector(EntryForward);
	// Interpolate direction only. Reconstruct against world up every frame, including entrance and the end hold.
	FRotator Rotation = Direction.Rotation();
	Rotation.Pitch = FMath::Clamp(Rotation.Pitch, -85.0, 85.0);
	Rotation.Roll = 0.0;
	const float FOV = FMath::Lerp(EntryFOV, FMath::Lerp(65.f, RevealFOV, (float)Phase), (float)Intro);
	Camera->DriveExternal(FTransform(Rotation, Position), FOV, LookAt);
	RevealMaxRoll = FMath::Max(RevealMaxRoll, (float)FMath::Abs(Camera->GetActorRotation().GetNormalized().Roll));
	if (!bRevealHoldLogged && Elapsed >= Duration * 0.75)
	{
		bRevealHoldLogged = true;
		UE_LOG(LogLeafline, Log, TEXT("Reveal hold: %.2f s, clearance %.2f cm, roll %.5f deg, FOV %.2f, position %s"),
			Elapsed, Field.Distance(Position), Camera->GetActorRotation().GetNormalized().Roll, FOV, *Position.ToCompactString());
	}
	if (Elapsed < Duration) return;
	if (Queue.IsEmpty()) Stop(); // exact player view restored at the boundary, never animated through the reveal
	else
	{
		UE_LOG(LogLeafline, Log, TEXT("Shot reveal segment ends: min clearance %.2f cm, max roll %.5f deg, stopped frames %d; continuing list"),
			MinClearance, RevealMaxRoll, RevealStoppedFrames);
		const FString Next = Queue[0];
		Queue.RemoveAt(0);
		if (!StartShot(Next, 8.f)) Stop();
	}
}

bool ALeaflineShots::ClearPosition(FVector& Position) const
{
	if (!ShotTree.IsValid()) return false;
	const FTreeField& Field = ShotTree->GetField();
	constexpr double Clearance = 12.0; // normal camera probe radius; also permits its 15 cm ground clearance
	if (Field.Distance(Position) >= Clearance) return true;
	FVector N;
	return Field.Project(Position, N, Clearance + 2.0) && Field.Distance(Position) >= Clearance;
}

bool ALeaflineShots::SweepPosition(FVector& Position) const
{
	if (!ClearPosition(Position)) return false;
	const FTreeField& Field = ShotTree->GetField();
	double Hit;
	if (!Field.March(LastSafePosition, Position, 12.0, Hit)) return true;
	// Clip the move before contact, then slide its remainder along the local bark tangent.
	const FVector HitPosition = FMath::Lerp(LastSafePosition, Position, FMath::Max(0.0, Hit - 0.002));
	FVector Slide = HitPosition + FVector::VectorPlaneProject(Position - HitPosition, Field.Gradient(HitPosition));
	if (!ClearPosition(Slide)) Slide = HitPosition;
	if (Field.March(LastSafePosition, Slide, 12.0, Hit))
		Slide = FMath::Lerp(LastSafePosition, Slide, FMath::Max(0.0, Hit - 0.002));
	Position = Slide;
	return Field.Distance(Position) >= 12.0;
}

void ALeaflineShots::Evaluate(double T, FVector& Position, FVector& LookAt, FVector& Up, float& FOV) const
{
	Up = Normal;
	LookAt = Anchor + Normal * 6.0;
	if (ShotName == TEXT("orbit"))
	{
		const double Angle = 2.0 * UE_PI * T;
		Position = Anchor + Normal * (Boom * 0.75) + (-Forward * FMath::Cos(Angle) + Right * FMath::Sin(Angle)) * Boom;
		FOV = 65.f;
	}
	else if (ShotName == TEXT("crane")) // climb above and away from the ant, and tilt into the crown
	{
		FVector Out = Normal.GetSafeNormal2D();
		if (Out.IsNearlyZero()) Out = -Forward.GetSafeNormal2D();
		if (Out.IsNearlyZero()) Out = FVector::ForwardVector;
		const double Rise = FMath::Clamp(Crown.Z - Anchor.Z, 800.0, 4500.0);
		Position = Anchor + Out * FMath::Lerp((double)Boom, 1400.0, T) + FVector::UpVector * FMath::Lerp(80.0, Rise, T);
		// Establish the ant first. Even a tiny direct blend toward a crown hundreds of metres away loses it.
		const double CrownBlend = Ease((T - 0.15) / 0.85);
		LookAt = FMath::Lerp(Anchor + Normal * 6.0, Crown, CrownBlend);
		Up = FVector::UpVector;
		FOV = 72.f;
	}
	else if (ShotName == TEXT("dolly"))
	{
		Position = Anchor - Forward * FMath::Lerp(Boom * 1.8, (double)Boom, T) + Normal * 45.0;
		if (ShotTree->GetSkeleton().Branches.IsValidIndex(DollyBranch))
		{
			const FTreeBranch& Branch = ShotTree->GetSkeleton().Branches[DollyBranch];
			FVector Axis;
			double Radius;
			const FVector Centre = BranchPoint(Branch, FMath::Lerp(DollyFrom, DollyTo, T), Axis, Radius);
			Position = Centre + Tangent(DollyRadial, Axis) * (Radius + 45.0);
		}
		FOV = 58.f;
	}
	else // follow: smoothed live ant position and facing, even during a trail ride
	{
		Position = ChasePosition;
		LookAt = ChaseFocus;
		Up = ChaseUp;
		FOV = 68.f;
	}
}

void ALeaflineShots::BeginReturn()
{
	bReturning = true;
	AAntCamera* Camera = ActiveCamera.Get();
	ReturnStartView = Camera->GetActorTransform();
	ReturnStartFOV = Camera->GetFieldOfView();
	ReturnPath.Reset();
	ReturnPath.Add(ReturnStartView.GetLocation());
	double Hit;
	if (!ShotTree->GetField().March(ReturnStartView.GetLocation(), ReturnView.GetLocation(), 12.0, Hit))
		ReturnPath.Add(ReturnView.GetLocation());
	else
	{
		// Every history segment was swept clear. Retrace it if a direct return would cut through wood.
		for (int32 I = History.Num() - 1; I >= 0; --I)
			if (FVector::DistSquared(ReturnPath.Last(), History[I]) > 0.0001) ReturnPath.Add(History[I]);
	}
	ReturnDistances.Reset();
	ReturnDistances.Add(0.0);
	for (int32 I = 1; I < ReturnPath.Num(); ++I)
		ReturnDistances.Add(ReturnDistances.Last() + FVector::Dist(ReturnPath[I - 1], ReturnPath[I]));
}

void ALeaflineShots::TickReturn(double T)
{
	const double Alpha = Ease(T);
	const double Distance = ReturnDistances.Last() * Alpha;
	FVector Position = ReturnPath.Last();
	for (int32 I = 1; I < ReturnPath.Num(); ++I)
	{
		if (ReturnDistances[I] < Distance) continue;
		const double Length = ReturnDistances[I] - ReturnDistances[I - 1];
		Position = FMath::Lerp(ReturnPath[I - 1], ReturnPath[I], Length > 0.0001 ? (Distance - ReturnDistances[I - 1]) / Length : 1.0);
		break;
	}
	const FQuat Rotation = FQuat::Slerp(ReturnStartView.GetRotation(), ReturnView.GetRotation(), Alpha);
	MinClearance = FMath::Min(MinClearance, ShotTree->GetField().Distance(Position));
	ActiveCamera->DriveExternal(FTransform(Rotation, Position), FMath::Lerp(ReturnStartFOV, ReturnFOV, (float)Alpha), Position + Rotation.GetForwardVector() * Boom);
}

void ALeaflineShots::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ApplyCinematic();
	AAntCamera* Camera = ActiveCamera.Get();
	if (!Camera) return;
	if (!ShotAnt.IsValid() || !ShotTree.IsValid() || Camera != FindCamera() || Camera->GetTarget() != ShotAnt.Get())
	{
		Stop();
		return;
	}
	const double Now = GetWorld()->GetRealTimeSeconds();
	const double RealDt = LastRealTime < 0.0 ? 0.0 : FMath::Max(Now - LastRealTime, 0.0);
	const double Dt = FMath::Min(RealDt, 0.1);
	Elapsed += RealDt;
	LastRealTime = Now;
	if (ShotName == TEXT("reveal"))
	{
		TickReveal();
		return;
	}
	if (ShotName == TEXT("follow"))
	{
		const UAntMovementComponent* M = ShotAnt->GetAntMovement();
		const FVector Out = M->GetState() == EAntState::Falling ? FVector::UpVector : M->GetSurfaceNormal();
		const FVector Heading = Tangent(M->GetBodyForward(), Out);
		const FVector Goal = M->GetSurfacePosition() + Out * Boom * 0.7 - Heading * Boom * 1.6;
		ChasePosition = FMath::Lerp(ChasePosition, Goal, 1.0 - FMath::Exp(-Dt / 0.6));
		ChaseFocus = FMath::Lerp(ChaseFocus, M->GetSurfacePosition() + Out * 10.0, 1.0 - FMath::Exp(-Dt / 0.25));
		ChaseUp = FQuat::Slerp(FQuat::Identity, FQuat::FindBetweenNormals(ChaseUp, Out), 1.0 - FMath::Exp(-Dt / 0.8)).RotateVector(ChaseUp).GetSafeNormal();
	}
	// A list transitions straight into its next shot; only its final shot returns to the player.
	const double BlendTime = FMath::Min(0.8, Duration * 0.2);
	const bool bLast = Queue.IsEmpty();
	const double MoveTime = Duration - BlendTime - (bLast ? BlendTime : 0.0);
	if (bLast && Elapsed >= Duration - BlendTime)
	{
		if (!bReturning) BeginReturn();
		TickReturn((Elapsed - Duration + BlendTime) / BlendTime);
		if (Elapsed >= Duration) Stop();
		return;
	}
	FVector Position, LookAt, Up;
	float FOV = ReturnFOV;
	const double MoveT = Ease((Elapsed - BlendTime) / MoveTime);
	Evaluate(MoveT, Position, LookAt, Up, FOV);
	const bool bEntering = Elapsed < BlendTime;
	const double Blend = bEntering ? Ease(Elapsed / BlendTime) : 1.0;
	const FTransform& BlendView = EntryView;
	Position = FMath::Lerp(BlendView.GetLocation(), Position, Blend);
	FOV = FMath::Lerp(bEntering ? EntryFOV : ReturnFOV, FOV, (float)Blend);
	const FVector Wanted = Position;
	if (!SweepPosition(Position))
	{
		Position = LastSafePosition;
		++HeldFrames;
	}
	if (FVector::DistSquared(Wanted, Position) > 1.0) ++ClippedFrames;
	LastSafePosition = Position;
	History.Add(Position);
	MinClearance = FMath::Min(MinClearance, ShotTree->GetField().Distance(Position));
	FVector Direction = (LookAt - Position).GetSafeNormal();
	const double LowerThird = ShotName == TEXT("dolly") ? 1.0 :
		(ShotName == TEXT("crane") ? 1.0 - Ease((MoveT - 0.15) / 0.85) : 0.0);
	if (LowerThird > 0.0 && !Direction.IsNearlyZero())
	{
		// Offset the optical axis after collision correction, keeping the opening ant below centre.
		const FVector ScreenUp = FVector::VectorPlaneProject(Up, Direction).GetSafeNormal();
		const double TanHalfV = FMath::Tan(FMath::DegreesToRadians(FOV * 0.5)) * 9.0 / 16.0;
		Direction = (Direction + ScreenUp * (TanHalfV / 3.0 * LowerThird)).GetSafeNormal();
	}
	FQuat Rotation = Direction.IsNearlyZero() ? Camera->GetActorQuat() : FRotationMatrix::MakeFromXZ(Direction, Up).ToQuat();
	Rotation = FQuat::Slerp(BlendView.GetRotation(), Rotation, Blend);
	Camera->DriveExternal(FTransform(Rotation, Position), FOV, LookAt);
	if (Elapsed >= Duration)
	{
		if (Queue.IsEmpty()) Stop();
		else
		{
			UE_LOG(LogLeafline, Log, TEXT("Shot %s segment ends: min clearance %.2f cm, held %d, clipped %d; continuing list"),
				*ShotName, MinClearance, HeldFrames, ClippedFrames);
			const FString Next = Queue[0];
			Queue.RemoveAt(0);
			if (!StartShot(Next, 8.f)) Stop();
		}
	}
}

void ALeaflineShots::Stop()
{
	Queue.Reset();
	if (AAntCamera* Camera = ActiveCamera.Get())
	{
		if (ShotName == TEXT("reveal"))
			UE_LOG(LogLeafline, Log, TEXT("Reveal playback: max roll %.5f deg, stopped frames %d, fallback-six %d, end %s; hold ends here, restore player view now"),
				RevealMaxRoll, RevealStoppedFrames, bRevealFallback ? 1 : 0, *Camera->GetActorLocation().ToCompactString());
		Camera->EndExternalDrive();
		Camera->RemoveTickPrerequisiteActor(this);
		UE_LOG(LogLeafline, Log, TEXT("Shot %s ends: min clearance %.2f cm, held frames %d, clipped frames %d, return error %.4f cm / %.4f deg, FOV error %.4f"),
			*ShotName, MinClearance, HeldFrames, ClippedFrames, FVector::Dist(Camera->GetActorLocation(), ReturnView.GetLocation()),
			FMath::RadiansToDegrees(Camera->GetActorQuat().AngularDistance(ReturnView.GetRotation())), FMath::Abs(Camera->GetFieldOfView() - ReturnFOV));
	}
	ActiveCamera.Reset();
}

void ALeaflineShots::SetSlomo(float Factor)
{
	if (!bSlomoSet) SavedDilation = UGameplayStatics::GetGlobalTimeDilation(this);
	bSlomoSet = true;
	UGameplayStatics::SetGlobalTimeDilation(this, Factor);
	UE_LOG(LogLeafline, Log, TEXT("Video slomo %.3f (shots run in real seconds)"), UGameplayStatics::GetGlobalTimeDilation(this));
}

void ALeaflineShots::EndPlay(const EEndPlayReason::Type Reason)
{
	Stop();
	SetCinematic(false);
	if (bSlomoSet) UGameplayStatics::SetGlobalTimeDilation(this, SavedDilation);
	Super::EndPlay(Reason);
}
