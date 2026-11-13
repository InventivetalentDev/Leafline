#include "World/DayDirector.h"
#include "AntTreeGame.h"
#include "Ant/AntPawn.h"
#include "Ant/AntMovementComponent.h"
#include "Tree/TreeActor.h"
#include "Colony/TrailNetwork.h"
#include "Colony/NestActor.h"
#include "World/WindSystem.h"
#include "Audio/LeafSound.h"
#include "Engine/DirectionalLight.h"
#include "Components/LightComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "Game/LeaflineGameInstance.h"
#include "Game/LeaflinePlayerController.h"
#include "Colony/LineSystem.h"
#include "Ant/AntCamera.h"

namespace
{
	ULeaflineGameInstance* Colony(const AActor* A)
	{
		return A ? Cast<ULeaflineGameInstance>(A->GetGameInstance()) : nullptr;
	}
}

ADayDirector::ADayDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	WorkerClass = AAntPawn::StaticClass();
}

ADayDirector* ADayDirector::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World) return nullptr;
	for (TActorIterator<ADayDirector> It(const_cast<UWorld*>(World)); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

void ADayDirector::Init(ATreeActor* InTree, ATrailNetwork* InTrails, ANestActor* InNest, AWindSystem* InWind, ALineSystem* InLines)
{
	Lines = InLines;
	Wind = InWind;
	Tree = InTree;
	Trails = InTrails;
	Nest = InNest;
	for (TActorIterator<ADirectionalLight> It(GetWorld()); It; ++It)
	{
		Sun = *It;
		break;
	}
	if (Sun.IsValid())
	{
		SunYaw = Sun->GetActorRotation().Yaw;
		if (ULightComponent* Light = Sun->GetLightComponent()) Light->SetMobility(EComponentMobility::Movable);
	}
	ApplyLook();
}

void ADayDirector::ApplyLook()
{
	UWorld* World = GetWorld();
	if (!World) return;
	const FLookTuning& L = Look;
	if (Sun.IsValid())
	{
		if (UDirectionalLightComponent* S = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
		{
			S->SetIntensity(L.SunIntensity);
			S->bUseTemperature = true;
			S->SetLightSourceAngle(L.SunSourceAngle);
			S->SetEnableLightShaftBloom(L.ShaftBloomScale > 0.f);
			S->SetBloomScale(L.ShaftBloomScale);
			S->SetBloomThreshold(L.ShaftBloomThreshold);
			S->SetVolumetricScatteringIntensity(L.SunVolumetricScattering);
			SunTemperatureApplied = 0.f; // UpdateSun sets it for the current shift
		}
	}
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		for (UActorComponent* C : It->GetComponents())
		{
			if (USkyLightComponent* Sky = Cast<USkyLightComponent>(C))
			{
				Sky->SetIntensity(L.SkyLightIntensity);
				Sky->SetLightColor(L.SkyLightColor);
			}
			else if (USkyAtmosphereComponent* Atmo = Cast<USkyAtmosphereComponent>(C))
			{
				Atmo->SetAerialPespectiveViewDistanceScale(L.AerialPerspectiveScale);
				Atmo->SetMieScatteringScale(L.MieScatteringScale);
				Atmo->SetMieAnisotropy(L.MieAnisotropy);
			}
			else if (UExponentialHeightFogComponent* Fog = Cast<UExponentialHeightFogComponent>(C))
			{
				Fog->SetFogDensity(L.FogDensity);
				Fog->SetFogHeightFalloff(L.FogHeightFalloff);
				Fog->SetStartDistance(L.FogStartDistance);
				Fog->SetDirectionalInscatteringColor(L.FogSunGlowColor);
				Fog->SetDirectionalInscatteringExponent(L.FogSunGlowExponent);
				Fog->SetVolumetricFog(L.bVolumetricFog);
				Fog->SetVolumetricFogExtinctionScale(L.VolumetricFogExtinction);
				Fog->SetVolumetricFogDistance(L.VolumetricFogDistance);
				Fog->SetVolumetricFogScatteringDistribution(L.VolumetricFogScattering);
				Fog->SetSecondFogDensity(L.GroundHazeDensity);
				Fog->SetSecondFogHeightFalloff(L.GroundHazeFalloff);
				Fog->SetSecondFogHeightOffset(L.GroundHazeHeightOffset);
				if (L.FogBaseHeight > -100000.f)
				{
					FVector P = It->GetActorLocation();
					P.Z = L.FogBaseHeight;
					Fog->SetMobility(EComponentMobility::Movable);
					It->SetActorLocation(P);
				}
			}
		}
	}
}

int32 ADayDirector::NumWaiting() const
{
	int32 N = 0;
	for (const FWorker& W : Roster) N += W.State == EWorkerState::Waiting ? 1 : 0;
	return N;
}

int32 ADayDirector::FindWorker(const AAntPawn* Ant) const
{
	for (int32 i = 0; i < Roster.Num(); ++i)
	{
		if (Roster[i].Pawn.Get() == Ant) return i;
	}
	return INDEX_NONE;
}

void ADayDirector::SetOneTree(bool bOn)
{
	PendingOneTree = bOn ? 1 : 0;
	UE_LOG(LogLeafline, Log, TEXT("One tree per stage %s (from the next day)"), bOn ? TEXT("on") : TEXT("off"));
}

FString ADayDirector::GetMoveCardText() const
{
	return FString::Printf(TEXT("The colony moves to %s"), *TreeName);
}

float ADayDirector::GetLowestLeafyHeight() const
{
	if (!Tree) return 3000.f;
	const float FromParams = FMath::Max(Tree->Params.LowestLeafyHeight, Tree->Params.LimbStartHeight);
	if (!Tuning.bOneTree) return FromParams;
	// One tree per stage: the sparse low zone puts the first leaves well above LowestLeafyHeight, so this is the lowest leaf
	// that can still be cut, found once per grown tree (seed and leaf count).
	static int32 CachedSeed = -1;
	static int32 CachedCount = -1;
	static float CachedZ = 0.f;
	const FLeafSet& Leaves = Tree->GetLeaves();
	if (CachedSeed != Tree->GetSeed() || CachedCount != Leaves.Num())
	{
		float Z = TNumericLimits<float>::Max();
		for (int32 i = 0; i < Leaves.Num(); ++i)
		{
			if (Leaves.Get(i).CanCut()) Z = FMath::Min(Z, (float)Leaves.Get(i).Base.Z);
		}
		CachedZ = Z < TNumericLimits<float>::Max() ? Z : FromParams;
		CachedSeed = Tree->GetSeed();
		CachedCount = Leaves.Num();
	}
	return CachedZ;
}

bool ADayDirector::IsMoveDue() const
{
	const ULeaflineGameInstance* GI = Colony(this);
	return Tuning.bOneTree && GI && GI->GetStage() > Stage;
}

void ADayDirector::DebugMoveNow()
{
	if (!Tuning.bOneTree)
	{
		UE_LOG(LogLeafline, Warning, TEXT("Leaf.Move: one tree per stage is off (Leaf.OneTree 1)"));
		return;
	}
	ULeaflineGameInstance* GI = Colony(this);
	if (GI && GI->GetStage() > Stage)
	{
		// A milestone move is already scheduled for tomorrow: take that one today instead of skipping a tree.
		GI->SetTree(GI->GetStage(), GI->GetTreeSeed(), Day);
	}
	else
	{
		bDebugMovePending = true;
	}
	UE_LOG(LogLeafline, Log, TEXT("Leaf.Move: day %d restarts on tree %d"), Day, Stage + 2);
	StartDay(Day);
	if (bMovedToday) Phase = EDayPhase::Move;
}

namespace
{
	/** The tree shapes, in the order the colony meets them: the old giant, a spreading giant, a tall spire, a leaning elder. */
	FTreeParams ShapeParams(const FTreeParams& Base, int32 Shape, FString& OutName)
	{
		FTreeParams P = Base;
		switch (Shape)
		{
		case 1:
			OutName = TEXT("a spreading giant");
			P.TrunkHeight *= 0.8f;
			P.TrunkRadius *= 1.1f;
			P.NumLimbs = 18;
			P.LimbLengthMax *= 1.45f;
			P.LimbLengthMin *= 1.3f;
			P.LimbPitchMinDeg = 4.f;
			P.LimbPitchMaxDeg = 26.f;
			break;
		case 2:
			OutName = TEXT("a tall spire");
			P.TrunkHeight *= 1.2f;
			P.TrunkRadius *= 0.85f;
			P.NumLimbs = 28;
			P.LimbLengthMax *= 0.65f;
			P.LimbLengthMin *= 0.8f;
			P.LimbPitchMinDeg = 28.f;
			P.LimbPitchMaxDeg = 58.f;
			break;
		case 3:
			OutName = TEXT("a leaning elder");
			P.TrunkLeanDeg = 9.f;
			P.TrunkWobble = 0.09f;
			P.FlareMul = 1.05f;
			P.NumRoots = 6;
			P.RootLength *= 1.3f;
			P.LimbPitchMinDeg = 8.f;
			P.LimbPitchMaxDeg = 36.f;
			break;
		default:
			OutName = TEXT("the old giant");
			break;
		}
		return P;
	}

	/** Stage the colony's milestones put it in (one tree on): it moves at the second and the third. */
	int32 StageForMilestones(int32 Reached)
	{
		return Reached >= 3 ? 2 : (Reached >= 2 ? 1 : 0);
	}
}

const FStageTuning& ADayDirector::GetStageTuning(int32 InStage) const
{
	static const FStageTuning Default;
	if (Tuning.Stages.Num() == 0) return Default;
	return Tuning.Stages[FMath::Clamp(InStage, 0, Tuning.Stages.Num() - 1)];
}

int32 ADayDirector::SeedForStage(int32 InStage) const
{
	// Never 0: a stored seed of 0 means "no tree yet".
	return 1 + (int32)((HashCombine(GetTypeHash(Tuning.BaseSeed), GetTypeHash(InStage * 7919 + 4242)) & 0x7fffffff) % 999999);
}

void ADayDirector::StartDay(int32 InDay, int32 SeedOverride)
{
	if (!Tree || !Trails || !Nest) return;
	const double StartTime = FPlatformTime::Seconds();
	const bool bFromNest = Phase == EDayPhase::Nest;
	Day = FMath::Max(1, InDay);

	for (FWorker& W : Roster)
	{
		if (W.Pawn.IsValid()) W.Pawn->Destroy();
	}
	Roster.Reset();
	ULeaflineGameInstance* GI = Colony(this);
	Roster.SetNum(FMath::Max(1, GI ? GI->GetWorkersPerDay(Tuning.WorkersPerDay) : Tuning.WorkersPerDay));
	if (GI) GI->SetDay(Day);
	// The day's modes are latched now: milestone sets, the save at day end and the stage choice below all use them.
	if (PendingOneTree >= 0)
	{
		const bool bWasOn = Tuning.bOneTree;
		Tuning.bOneTree = PendingOneTree != 0;
		PendingOneTree = -1;
		// Leaving one tree per stage forgets the kept tree, so turning it on again later starts a fresh one for the stage
		// (not an old tree with a stale day count).
		if (bWasOn && !Tuning.bOneTree && GI) GI->ClearTreeState();
	}
	if (GI) GI->SetOneTree(Tuning.bOneTree);
	if (GI) GI->SetRecruitsEnabled(Tuning.bRecruits);

	if (!bHaveBaseTreeParams)
	{
		BaseTreeParams = Tree->Params;
		bHaveBaseTreeParams = true;
	}
	bool bGrew = false;
	bMovedToday = false;
	if (!Tuning.bOneTree)
	{
		// A new tree every day. Each day's tree has a character: day 1 is the classic shape, later days take turns being
		// broad and spreading, tall and narrow, or leaning with more roots. Every tree also has one broken limb low down.
		Seed = SeedOverride >= 0 ? SeedOverride : (int32)((HashCombine(GetTypeHash(Tuning.BaseSeed), GetTypeHash(Day)) & 0x7fffffff) % 1000000);
		Trails->ClearAll();
		// A kept tree from earlier one-tree days is forgotten, so turning the flag on later starts a fresh tree.
		if (GI && GI->GetTreeSeed() != 0) GI->ClearTreeState();
		const int32 Shape = Tuning.TreeShapeOverride >= 0 ? Tuning.TreeShapeOverride % 4 : (Day <= 1 ? 0 : 1 + (Day - 2) % 3);
		FTreeParams P = ShapeParams(BaseTreeParams, Shape, TreeName);
		P.LowestLeafyHeight = Tuning.LeafyHeightDay1 + (Day - 1) * Tuning.LeafyHeightPerDay;
		Tree->Params = P;
		Tree->Grow(Seed);
		bGrew = true;
		GrownTreeSeed = -1;
		GrownStage = -1;
		Stage = 0;
		StageStartDay = Day;
		if (Wind) Wind->StartDay(Day, Seed);
	}
	else
	{
		// One tree per stage: the save names the stage and its tree. Seed 0 means none yet (a fresh colony, or a save from
		// before this change): it starts on a fresh tree for the stage its milestones put it in.
		Stage = GI ? GI->GetStage() : Stage;
		int32 TreeSeed = GI ? GI->GetTreeSeed() : GrownTreeSeed;
		StageStartDay = GI ? GI->GetStageStartDay() : StageStartDay;
		const bool bExplicitMove = bDebugMovePending;
		if (bDebugMovePending)
		{
			bDebugMovePending = false;
			++Stage;
			TreeSeed = 0;
		}
		if (SeedOverride >= 0)
		{
			TreeSeed = SeedOverride;
			if (GI) GI->SetTree(Stage, TreeSeed, Day);
			StageStartDay = Day;
			GrownTreeSeed = -1;
		}
		if (TreeSeed <= 0)
		{
			if (GI && !bExplicitMove) Stage = FMath::Max(Stage, StageForMilestones(GI->GetReachedMilestoneCount()));
			TreeSeed = SeedForStage(Stage);
			StageStartDay = Day;
			if (GI) GI->SetTree(Stage, TreeSeed, Day); // written with the day's haul at day end
		}
		Seed = TreeSeed;
		bMovedToday = Stage > 0 && Day == StageStartDay;

		const FStageTuning& ST = GetStageTuning(Stage);
		const int32 Shape = Tuning.TreeShapeOverride >= 0 ? Tuning.TreeShapeOverride % 4 : Stage % 4;
		FTreeParams P = ShapeParams(BaseTreeParams, Shape, TreeName);
		P.TrunkHeight *= ST.TreeHeightMul;
		P.LowestLeafyHeight = ST.LowestLeafyHeight;
		P.RichnessMul = ST.RichnessMul;
		P.SparseSpacingMul = ST.SparseSpacingMul;
		P.SparseBlendHeight = ST.SparseBlendHeight;
		P.SparseTwigDrop = ST.SparseTwigDrop;
		if (GrownTreeSeed != TreeSeed || GrownStage != Stage || (GI && GrownGeneration != GI->GetTreeGeneration()))
		{
			Tree->Params = P;
			const bool bSaved = GI && GI->GetTreeSeed() == TreeSeed && GI->GetSavedBites().Num() > 0;
			if (bSaved) Tree->GrowWithBites(TreeSeed, GI->GetSavedBites(), GI->GetSavedLeafCount());
			else Tree->Grow(TreeSeed);
			GrownTreeSeed = TreeSeed;
			GrownStage = Stage;
			GrownGeneration = GI ? GI->GetTreeGeneration() : -1;
			bGrew = true;
		}
		// Yesterday's routes stay one more day, faint and ridable; a move drops them (SetTree).
		if (GI && GI->GetYesterdayDay() == Day - 1 && GI->GetYesterdayTrails().Num() > 0) Trails->LoadYesterday(GI->GetYesterdayTrails());
		else Trails->ClearAll();
		if (Wind)
		{
			// The wind changes direction every day; within a stage only its strength rises, a little each day.
			const int32 WindSeed = (int32)((HashCombine(GetTypeHash(TreeSeed), GetTypeHash(Day)) & 0x7fffffff) % 1000000);
			const float Scale = ST.GustMul * (1.f + Tuning.GustPerDayOnTree * (GetDayOnTree() - 1));
			// The direction turns 70 to 290 degrees from the day before, so the sheltered side always moves. Rebuilt from
			// the tree's first day, so it does not depend on the session.
			float Yaw = FRandomStream(TreeSeed * 13 + 5).FRandRange(0.f, 360.f);
			for (int32 d = StageStartDay + 1; d <= Day; ++d)
			{
				Yaw += FRandomStream((int32)((HashCombine(GetTypeHash(TreeSeed), GetTypeHash(d * 31 + 7)) & 0x7fffffff))).FRandRange(70.f, 290.f);
			}
			Wind->StartDay(Day, WindSeed, Scale, GetDayOnTree(), FMath::Fmod(Yaw, 360.f));
		}
	}
	if (bGrew)
	{
		Nest->SetActorTransform(Tree->GetNestTransform(0));
		Nest->SetMoundSize(Tree->Params.NestMoundRadius, Tree->Params.NestMoundHeight);
	}
	PendingPossess = 0.f;
	bSkippingHarvest = false;
	HarvestView = 0;
	if (Lines) Lines->StartDay();
	if (ALeaflinePlayerController* PC = Cast<ALeaflinePlayerController>(Player.Get()))
	{
		if (AAntCamera* Cam = PC->GetAntCamera()) Cam->SetOverview(false);
	}

	Stats = FDayStats();
	Stats.Day = Day;
	CutZSum = 0.0;
	CutCount = 0;
	CutZMin = 0.f;
	Phase = EDayPhase::Shift;
	ShiftsStarted = 0;
	Current = INDEX_NONE;
	if (Tuning.bOneTree)
	{
		UE_LOG(LogLeafline, Log, TEXT("Day %d starts: tree %d (%s, seed %d), day %d on this tree, %s in %.0f ms, %d workers, lowest leafy branch at %.0f m, %d pieces left, %d yesterday's routes"),
			Day, Stage + 1, *TreeName, Seed, GetDayOnTree(), bGrew ? TEXT("grown") : TEXT("kept"), (FPlatformTime::Seconds() - StartTime) * 1000.0,
			Roster.Num(), Tree->Params.LowestLeafyHeight / 100.f, Tree->CountPiecesLeft(), Trails->NumYesterdayPaths());
	}
	else
	{
		UE_LOG(LogLeafline, Log, TEXT("Day %d starts: seed %d, %s, %d workers, lowest leafy branch at %.0f m"),
			Day, Seed, *TreeName, Roster.Num(), Tree->Params.LowestLeafyHeight / 100.f);
	}
	PossessNext();
	UpdateSun(0.f, true);
	// A day on a new tree opens with the move card (after the title at launch, see MenuConfirm).
	if (bMovedToday && bFromNest)
	{
		Phase = EDayPhase::Move;
		UE_LOG(LogLeafline, Log, TEXT("Move card: %s"), *GetMoveCardText());
	}
}

AAntPawn* ADayDirector::SpawnWorker(int32 Index)
{
	const FTransform NestXf = Tree->GetNestTransform(0);
	FTransform SpawnXf = NestXf;
	SpawnXf.AddToTranslation(FVector(0.0, 0.0, 40.0));
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags |= RF_Transient;
	AAntPawn* Ant = GetWorld()->SpawnActor<AAntPawn>(WorkerClass ? *WorkerClass : AAntPawn::StaticClass(), SpawnXf, Params);
	if (!Ant) return nullptr;
	Ant->SetWorker(Index, Trails->StartPath(NestXf.GetLocation(), FVector::UpVector));
	// Ants tick after the line system, so one handed over to its fixed clock in a frame was not also moved by its
	// own tick earlier in that frame.
	if (Lines)
	{
		Ant->AddTickPrerequisiteActor(Lines);
		if (Ant->GetAntMovement()) Ant->GetAntMovement()->AddTickPrerequisiteActor(Lines);
	}
	if (ULeaflineGameInstance* GI = Colony(this)) GI->ApplyUpgradesToAnt(Ant);
	Roster[Index].Pawn = Ant;
	return Ant;
}

void ADayDirector::PossessNext()
{
	APlayerController* PC = Player.Get();
	for (int32 i = 0; i < Roster.Num(); ++i)
	{
		if (Roster[i].State != EWorkerState::Waiting) continue;
		AAntPawn* Ant = SpawnWorker(i);
		if (!Ant) continue;
		Roster[i].State = EWorkerState::Out;
		Current = i;
		++ShiftsStarted;
		if (ShiftsStarted > 1) LeafSound::Play(this, TEXT("shift_whoosh"), 0.5f);
		if (PC) PC->Possess(Ant);
		UE_LOG(LogLeafline, Log, TEXT("Shift %d of %d: worker %d leaves the nest"), ShiftsStarted, Roster.Num(), i);
		return;
	}
	// A worker left standing when the player switched away must not be forgotten.
	for (int32 i = 0; i < Roster.Num(); ++i)
	{
		AAntPawn* Idle = Roster[i].Pawn.Get();
		if (Roster[i].State == EWorkerState::Out && Idle && !Idle->IsPlayerControlled())
		{
			Current = i;
			if (PC) PC->Possess(Idle);
			UE_LOG(LogLeafline, Log, TEXT("Back to worker %d, still out"), i);
			return;
		}
	}
	for (int32 i = 0; i < Roster.Num(); ++i)
	{
		const EWorkerState St = Roster[i].State;
		if ((St == EWorkerState::Returning || St == EWorkerState::BlownOff) && Roster[i].Pawn.IsValid() && !Roster[i].Pawn->IsVanishing())
		{
			Current = i;
			if (PC) PC->Possess(Roster[i].Pawn.Get());
			UE_LOG(LogLeafline, Log, TEXT("No workers left in the nest: following worker %d home"), i);
			return;
		}
	}
	Current = INDEX_NONE;
}

void ADayDirector::SendHome(AAntPawn* Ant)
{
	if (Phase != EDayPhase::Shift || !Ant) return;
	const int32 i = FindWorker(Ant);
	if (i == INDEX_NONE || Roster[i].State != EWorkerState::Out || Ant->IsCutting()) return;
	if (Ant->Cut.bSendHomeNeedsFragment && !Ant->IsCarrying())
	{
		// An empty worker can always end its trip at the nest, so a day without cuts can still finish.
		const bool bRealTrip = Trails && Ant->GetPathIndex() != INDEX_NONE && Trails->GetPoints(Ant->GetPathIndex()).Num() > 30;
		if (bRealTrip && Ant->GetAntMovement() && IsNearNest(Ant->GetAntMovement()->GetSurfacePosition()))
		{
			UE_LOG(LogLeafline, Log, TEXT("Worker %d ends its trip at the nest empty-handed"), i);
			OnAntReachedNest(Ant);
			return;
		}
		UE_LOG(LogLeafline, Log, TEXT("Send home ignored: worker %d carries nothing (walk to the nest and press it there)"), i);
		return;
	}
	Roster[i].State = EWorkerState::Returning;
	Ant->StartReturn();
	if (Lines)
	{
		if (Ant->IsCarrying()) Lines->AddLine(Ant, false);
		// From now on it walks on the fixed sim clock, like the recruits: the harvest then runs the same at any speed.
		Lines->AddDrivenAnt(Ant);
	}
	LeafSound::Play(this, TEXT("send_home"), 0.6f);
	UE_LOG(LogLeafline, Log, TEXT("Worker %d sent home carrying %.1f"), i, Ant->GetCarriedMass());
	// Stay with it for a moment as it turns for home, so it is clear the worker is on its way; then the next comes out.
	PendingPossess = 1.4f;
	PendingFrom = Ant;
}

void ADayDirector::OnBlownOff(AAntPawn* Ant)
{
	const int32 i = FindWorker(Ant);
	if (i == INDEX_NONE) return;
	const EWorkerState Was = Roster[i].State;
	if (Was != EWorkerState::Out && Was != EWorkerState::Returning) return;
	Roster[i].State = EWorkerState::BlownOff;
	Stats.BlownOff += 1;
	// A scout sent home with a piece started a line; that piece is now lost to the wind.
	if (Lines && Was == EWorkerState::Returning) Lines->OnScoutPieceResolved(i, false, 0.f);
	if (Lines) Lines->AddDrivenAnt(Ant);
	UE_LOG(LogLeafline, Log, TEXT("Worker %d blown off (was %s); it walks home empty-handed"), i, *UEnum::GetValueAsString(Was));
	// Let the player watch the fall for a moment before the next worker comes out.
	if (Ant->IsPlayerControlled() && Was == EWorkerState::Out)
	{
		PendingPossess = 2.5f;
		PendingFrom = Ant;
	}
}

void ADayDirector::OnAntReachedNest(AAntPawn* Ant)
{
	const int32 i = FindWorker(Ant);
	if (i == INDEX_NONE || Roster[i].State == EWorkerState::Home) return;
	if (Ant->IsCarrying())
	{
		const float Mass = Ant->GetCarriedValue();
		// Carried all the way home by the scout itself: that also lays a line (a sent-home scout laid it already).
		if (Lines && Roster[i].State == EWorkerState::Out) Lines->AddLine(Ant, true);
		CreditHaul(Mass, false);
		if (Lines)
		{
			Lines->RecordDelivery(Nest ? Nest->GetActorLocation() : Ant->GetActorLocation(), Mass, INDEX_NONE);
			Lines->OnScoutPieceResolved(i, true, Mass);
		}
		else
		{
			if (Nest) Nest->PlayDeposit(Mass);
			LeafSound::Play(this, TEXT("deposit"), 0.7f, FMath::Clamp(1.15f - 0.06f * Mass, 0.8f, 1.2f));
		}
		UE_LOG(LogLeafline, Log, TEXT("Worker %d home with %.1f (today %.1f from %d fragments)"), i, Mass, Stats.MassHauled, Stats.Fragments);
		// The piece stays on the ant for its last moment: it drops into the entrance as the ant goes in (DepositAndVanish).
	}
	else
	{
		UE_LOG(LogLeafline, Log, TEXT("Worker %d home empty-handed"), i);
	}
	Roster[i].State = EWorkerState::Home;
	Stats.TrailLaid = Trails ? Trails->DrawnLength() : 0.f;

	const bool bWasControlled = Ant->IsPlayerControlled();
	if (bWasControlled)
	{
		PossessNext();
		if (Ant->IsPlayerControlled() && Player.IsValid()) Player->UnPossess();
	}
	// The ant walks into the entrance and disappears, then destroys itself.
	Ant->DepositAndVanish(Nest ? Nest->GetActorLocation() + FVector(0.f, 0.f, 6.f) : Ant->GetActorLocation());
}

void ADayDirector::CreditHaul(float Mass, bool bByRecruit)
{
	Stats.MassHauled += Mass;
	Stats.Fragments += 1;
	Stats.LargestFragment = FMath::Max(Stats.LargestFragment, Mass);
	if (bByRecruit) Stats.RecruitPieces += 1;
	else Stats.ScoutMass += Mass;
	if (ULeaflineGameInstance* GI = Colony(this))
	{
		const int32 Before = GI->GetReachedMilestoneCount();
		GI->AddHaul(Mass);
		if (GI->GetReachedMilestoneCount() > Before)
		{
			if (GI->GetReachedMilestoneCount() >= LeaflineUpgrades::MilestoneCount) bEndCardPending = true;
			LeafSound::Play(this, TEXT("milestone"), 0.8f);
			UE_LOG(LogLeafline, Log, TEXT("Colony milestone %d reached (%.1f hauled in all)"), GI->GetReachedMilestoneCount(), GI->GetTotalHauled());
		}
	}
}

void ADayDirector::NoteCut(int32 Leaf)
{
	if (!Tree || Leaf < 0 || Leaf >= Tree->GetLeaves().Num()) return;
	const float Z = Tree->GetLeaves().Get(Leaf).Base.Z;
	CutZMin = CutCount == 0 ? Z : FMath::Min(CutZMin, Z);
	CutZSum += Z;
	++CutCount;
}

void ADayDirector::CheckAllHome()
{
	UpdateDayFlow();
}

void ADayDirector::UpdateDayFlow()
{
	if (Phase != EDayPhase::Shift && Phase != EDayPhase::Harvest) return;
	bool bScoutsOut = false;    // waiting in the nest or out under control
	bool bScoutsMoving = false; // walking home along their route
	for (const FWorker& W : Roster)
	{
		bScoutsOut |= W.State == EWorkerState::Waiting || W.State == EWorkerState::Out;
		// Every ant home: a blown-off scout still walking back counts too (RescueStalledScouts bounds the wait).
		bScoutsMoving |= W.State == EWorkerState::Returning ||
			(W.State == EWorkerState::BlownOff && W.Pawn.IsValid() && !W.Pawn->IsVanishing());
	}
	// Recruits latch their on/off state at day start, and any still out are always waited for.
	const bool bRecruitWork = Lines && Lines->HasWork();
	if (Phase == EDayPhase::Shift)
	{
		// Let the player watch the last send-home or fall for its moment before anything changes.
		if (bScoutsOut || PendingPossess > 0.f) return;
		if (bRecruitWork)
		{
			EnterHarvest();
			return;
		}
		if (!bScoutsMoving) EndDay();
		// Only scouts walking home are left: their walk plays at the harvest's speed and can be skipped.
		else if (Tuning.bFastForwardHome) EnterHarvest();
		return;
	}
	if (!bRecruitWork && !bScoutsMoving) EndDay();
}

void ADayDirector::RescueStalledScouts()
{
	// Sim time: fallen scouts walk on the line system's fixed clock, so the waits mean the same at any harvest speed.
	const double Now = Lines ? Lines->GetSimTime() : (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0);
	// When each blown-off scout fell: however it moves, it is counted home after two minutes, so the day always ends.
	static TMap<TWeakObjectPtr<AAntPawn>, double> FellAt;
	for (auto It = FellAt.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid()) It.RemoveCurrent();
	}
	for (int32 i = 0; i < Roster.Num(); ++i)
	{
		FWorker& W = Roster[i];
		AAntPawn* Ant = W.Pawn.Get();
		if (W.State != EWorkerState::BlownOff || !Ant || Ant->IsVanishing() || !Ant->GetAntMovement()) continue;
		const UAntMovementComponent* M = Ant->GetAntMovement();
		const double Fell = FellAt.FindOrAdd(Ant, Now);
		if (Now - Fell > 120.0)
		{
			UE_LOG(LogLeafline, Log, TEXT("Worker %d still not home two minutes after its fall (at %s); counted home"), i, *M->GetSurfacePosition().ToCompactString());
			const bool bWasControlled = Ant->IsPlayerControlled();
			if (bWasControlled && Player.IsValid()) Player->UnPossess();
			Ant->Destroy();
			W.State = EWorkerState::Home;
			if (bWasControlled) PossessNext();
			continue;
		}
		if (M->GetState() == EAntState::Falling)
		{
			W.ProgressTime = Now;
			W.ProgressPos = M->GetSurfacePosition();
			continue;
		}
		if (W.ProgressTime < 0.0 || FVector::DistSquared(M->GetSurfacePosition(), W.ProgressPos) > 40.0 * 40.0)
		{
			W.ProgressPos = M->GetSurfacePosition();
			W.ProgressTime = Now;
		}
		else if (Now - W.ProgressTime > 8.0)
		{
			UE_LOG(LogLeafline, Log, TEXT("Worker %d stalled on its walk home after a fall (at %s); counted home"), i, *M->GetSurfacePosition().ToCompactString());
			const bool bWasControlled = Ant->IsPlayerControlled();
			if (bWasControlled && Player.IsValid()) Player->UnPossess();
			Ant->Destroy();
			W.State = EWorkerState::Home;
			if (bWasControlled) PossessNext();
		}
	}
}

void ADayDirector::EnterHarvest()
{
	Phase = EDayPhase::Harvest;
	bSkippingHarvest = false;
	HarvestView = 0;
	bHarvestHomeOnly = !(Lines && Lines->HasWork());
	if (Lines)
	{
		Lines->SetSpeedFactor(Lines->Tuning.HarvestSpeed);
		// Scouts still walking home now run on the same fast clock as the recruits.
		for (const FWorker& W : Roster)
		{
			if ((W.State == EWorkerState::Returning || W.State == EWorkerState::BlownOff) && W.Pawn.IsValid()) Lines->AddDrivenAnt(W.Pawn.Get());
		}
		UE_LOG(LogLeafline, Log, TEXT("Harvest, day %d: %d recruits out, %d trips left, %d live lines, x%.0f%s"),
			Day, Lines->NumOut(), Lines->TripsLeft(), Lines->NumLiveLines(), Lines->GetSpeedFactor(),
			bHarvestHomeOnly ? TEXT(" (only scouts walking home)") : TEXT(""));
	}
	LeafSound::Play(this, TEXT("shift_whoosh"), 0.5f);
}

void ADayDirector::SkipHarvest()
{
	if (Phase != EDayPhase::Harvest || bSkippingHarvest || !Lines) return;
	bSkippingHarvest = true;
	Lines->SetSpeedFactor(Lines->Tuning.SkipSpeed);
	LeafSound::Play(this, TEXT("ui_confirm"), 0.6f);
	UE_LOG(LogLeafline, Log, TEXT("Harvest skipped: running the rest at x%.0f (%d recruits out, %d trips left)"),
		Lines->GetSpeedFactor(), Lines->NumOut(), Lines->TripsLeft());
}

void ADayDirector::CycleHarvestView()
{
	if (Phase != EDayPhase::Harvest || !Lines) return;
	const int32 Views = 1 + Lines->NumLiveLines();
	HarvestView = (HarvestView + 1) % FMath::Max(Views, 1);
	LeafSound::Play(this, TEXT("ui_move"), 0.5f);
	UE_LOG(LogLeafline, Log, TEXT("Harvest view %d of %d"), HarvestView, Views - 1);
}

void ADayDirector::UpdateHarvestCamera(float Dt)
{
	ALeaflinePlayerController* PC = Cast<ALeaflinePlayerController>(Player.Get());
	AAntCamera* Cam = PC ? PC->GetAntCamera() : nullptr;
	if (!Cam || !Tree || !Nest) return;
	if (Phase == EDayPhase::Move)
	{
		// The move card: the whole new tree from out on the ground, looking up its trunk.
		const FVector NestPos = Nest->GetActorLocation();
		FVector Away = (NestPos - Tree->GetSkeleton().TrunkBase).GetSafeNormal2D();
		if (Away.IsNearlyZero()) Away = FVector::ForwardVector;
		const float H = Tree->Params.TrunkHeight;
		const FVector Base = Tree->GetSkeleton().TrunkBase;
		Cam->SetOverview(true, Base + Away * (H * 0.55f) + FVector(0.0, 0.0, H * 0.12f), Base + FVector(0.0, 0.0, H * 0.4f));
		return;
	}
	if (Phase != EDayPhase::Harvest)
	{
		Cam->SetOverview(false);
		return;
	}
	// Only scouts walking home: view 0 keeps following the one under control (PossessNext hands over a returning
	// worker), so the fast-forward shows its walk.
	if (bHarvestHomeOnly && HarvestView == 0 && Player.IsValid() && Player->GetPawn())
	{
		Cam->SetOverview(false);
		return;
	}
	// View k > 0: the k-th live line's patch end. Lines can die while watched; fall back to the nest.
	const FColonyLine* Watched = nullptr;
	if (HarvestView > 0 && Lines)
	{
		int32 k = 0;
		for (const FColonyLine& L : Lines->GetLines())
		{
			if (!L.IsLive()) continue;
			if (++k == HarvestView)
			{
				Watched = &L;
				break;
			}
		}
		if (!Watched) HarvestView = 0;
	}
	if (Watched)
	{
		// Beside the end of the line, looking at it, a little above the bark.
		const FTrailPoint& End = Watched->Route.Last();
		const FTrailPoint& Before = Watched->Route[FMath::Max(Watched->Route.Num() - 25, 0)];
		FVector Back = (Before.Pos - End.Pos).GetSafeNormal();
		if (Back.IsNearlyZero()) Back = -End.Normal;
		const FVector Loc = End.Pos + End.Normal * 260.0 + Back * 220.0 + FVector(0.0, 0.0, 80.0);
		Cam->SetOverview(true, Loc, End.Pos);
		return;
	}
	// At the nest, low, looking up the trunk where every line comes down: the file of ants walks toward the camera.
	const FVector NestPos = Nest->GetActorLocation();
	FVector Away = (NestPos - Tree->GetSkeleton().TrunkBase).GetSafeNormal2D();
	if (Away.IsNearlyZero()) Away = FVector::ForwardVector;
	// Far enough back that the nest entrance (where the "+N" pops rise) sits in the lower part of the frame and the
	// lower trunk, where the lines converge, fills the rest.
	const FVector Loc = NestPos + Away * 650.0 + FVector(0.0, 0.0, 120.0);
	const FVector LookAt = NestPos - Away * 200.0 + FVector(0.0, 0.0, 380.0);
	Cam->SetOverview(true, Loc, LookAt);
}

void ADayDirector::EndDay()
{
	Phase = EDayPhase::AllHome;
	PhaseTimer = 1.5f;
	bSkippingHarvest = false;
	Stats.TrailLaid = Trails ? Trails->DrawnLength() : Stats.TrailLaid;
	if (Lines)
	{
		Lines->SetSpeedFactor(1.f);
		Stats.RecruitLost = Lines->GetRecruitLosses();
	}
	if (ULeaflineGameInstance* GI = Colony(this))
	{
		Stats.PreviousBest = GI->GetBestDayHaul();
		const bool bBest = GI->RecordDayHaul(Stats.MassHauled);
		Stats.bNewBest = bBest && Stats.PreviousBest > 0.f;
		if (Tuning.bOneTree && Tree && Trails && GI->GetTreeGeneration() != GrownGeneration)
		{
			UE_LOG(LogLeafline, Warning, TEXT("The colony's tree changed during the day (a reset or reload): today's tree is not saved"));
		}
		else if (Tuning.bOneTree && Tree && Trails)
		{
			// The day's bites and routes go into the same write as the day's haul: quitting mid-day restarts the day from
			// dawn with neither. Purchases on the nest screen save later and carry this snapshot.
			TArray<FSavedLeaf> Bites;
			Tree->ExportBites(Bites);
			TArray<FSavedTrail> Routes;
			Trails->ExportToday(Routes);
			GI->SetTree(Stage, Seed, StageStartDay);
			GI->SetTreeState(Bites, Tree->GetLeaves().Num(), Routes, Day);
			// The second and third milestones move the colony: the next day starts on a new tree. At most one move a day;
			// if both milestones land on one day, the second move follows the day after.
			const int32 Target = StageForMilestones(GI->GetReachedMilestoneCount());
			if (Target > Stage)
			{
				GI->SetTree(Stage + 1, SeedForStage(Stage + 1), Day + 1);
				UE_LOG(LogLeafline, Log, TEXT("Colony moves to tree %d tomorrow (%d milestones reached)"), Stage + 2, GI->GetReachedMilestoneCount());
			}
			UE_LOG(LogLeafline, Log, TEXT("Tree saved: %d bitten leaves of %d, %d routes, %d pieces left"), Bites.Num(),
				Tree->GetLeaves().Num(), Routes.Num(), Tree->CountPiecesLeft());
		}
		GI->SetDay(Day + 1);
		GI->SaveNow();
	}
	UE_LOG(LogLeafline, Log, TEXT("Day %d cuts: %d at mean height %.1f m (lowest %.1f m), tree %d day %d"), Day, CutCount,
		CutCount > 0 ? CutZSum / CutCount / 100.0 : 0.0, CutZMin / 100.f, Stage + 1, GetDayOnTree());
	UE_LOG(LogLeafline, Log, TEXT("Day %d: all workers home (hauled %.1f, recruits %d pieces home and %d lost, %d lines, %d fallback cuts%s)"),
		Day, Stats.MassHauled, Stats.RecruitPieces, Stats.RecruitLost, Lines ? Lines->GetLines().Num() : 0,
		Lines ? Lines->GetFallbackCount() : 0, Stats.bNewBest ? TEXT(", best day yet") : TEXT(""));
}

void ADayDirector::ForceHarvest()
{
	if (Phase != EDayPhase::Shift) return;
	for (int32 i = 0; i < Roster.Num(); ++i)
	{
		FWorker& W = Roster[i];
		if (W.State == EWorkerState::Waiting)
		{
			W.State = EWorkerState::Home;
		}
		else if (W.State == EWorkerState::Out)
		{
			AAntPawn* Ant = W.Pawn.Get();
			if (Ant && Ant->IsCarrying() && !Ant->IsCutting())
			{
				W.State = EWorkerState::Returning;
				Ant->StartReturn();
				if (Lines)
				{
					Lines->AddLine(Ant, false);
					Lines->AddDrivenAnt(Ant);
				}
			}
			else
			{
				if (Ant) Ant->Destroy();
				W.State = EWorkerState::Home;
			}
		}
	}
	PendingPossess = 0.f;
	UE_LOG(LogLeafline, Log, TEXT("Leaf.Harvest: shifts ended early"));
	UpdateDayFlow();
}

AAntPawn* ADayDirector::DebugTakeNextScout()
{
	if (Phase != EDayPhase::Shift) return nullptr;
	if (Roster.IsValidIndex(Current) && Roster[Current].State == EWorkerState::Out && Roster[Current].Pawn.IsValid())
	{
		return Roster[Current].Pawn.Get();
	}
	PendingPossess = 0.f;
	PossessNext();
	if (Roster.IsValidIndex(Current) && Roster[Current].State == EWorkerState::Out) return Roster[Current].Pawn.Get();
	return nullptr;
}

bool ADayDirector::IsNestRowVisible(int32 Upgrade) const
{
	return Upgrade != static_cast<int32>(ELeaflineUpgrade::Brood) || AreRecruitsOn();
}

void ADayDirector::SetRecruitsOn(bool bOn)
{
	Tuning.bRecruits = bOn;
	if (ULeaflineGameInstance* GI = Colony(this)) GI->SetRecruitsEnabled(bOn);
	if (!IsNestRowVisible(NestSelection)) NestSelection = LeaflineUpgrades::Count;
	UE_LOG(LogLeafline, Log, TEXT("Recruits %s (the pool changes from the next day)"), bOn ? TEXT("on") : TEXT("off"));
}

void ADayDirector::CycleWorker()
{
	if (Phase == EDayPhase::Harvest)
	{
		CycleHarvestView();
		return;
	}
	if (Phase != EDayPhase::Shift || Roster.Num() == 0 || !Player.IsValid()) return;
	for (int32 Step = 1; Step <= Roster.Num(); ++Step)
	{
		const int32 i = (FMath::Max(Current, 0) + Step) % Roster.Num();
		const FWorker& W = Roster[i];
		if ((W.State == EWorkerState::Out || W.State == EWorkerState::Returning) && W.Pawn.IsValid() && i != Current)
		{
			Current = i;
			Player->Possess(W.Pawn.Get());
			UE_LOG(LogLeafline, Log, TEXT("Camera switched to worker %d"), i);
			return;
		}
	}
}

void ADayDirector::ShowTitle()
{
	Phase = EDayPhase::Title;
}

void ADayDirector::MenuConfirm()
{
	switch (Phase)
	{
	case EDayPhase::Title:
	case EDayPhase::Move:
		// At launch on a tree's first day (not the first tree), the move card follows the title.
		if (Phase == EDayPhase::Title && bMovedToday)
		{
			Phase = EDayPhase::Move;
			LeafSound::Play(this, TEXT("ui_confirm"), 0.6f);
			UE_LOG(LogLeafline, Log, TEXT("Move card: %s"), *GetMoveCardText());
			break;
		}
		UE_LOG(LogLeafline, Log, TEXT("%s: play starts on day %d"), Phase == EDayPhase::Move ? TEXT("Move card") : TEXT("Title"), Day);
		Phase = EDayPhase::Shift;
		LeafSound::Play(this, TEXT("ui_confirm"), 0.6f);
		// The world keeps running behind the title: workers may have been blown off and walked home meanwhile.
		if (!Player.IsValid() || !Player->GetPawn()) PossessNext();
		CheckAllHome();
		break;
	case EDayPhase::Recap:
	{
		ULeaflineGameInstance* GI = Colony(this);
		const bool bEndCard = bEndCardPending || (GI && GI->IsEndCardDue());
		Phase = bEndCard ? EDayPhase::EndCard : EDayPhase::Nest;
		bEndCardPending = false;
		if (bEndCard && GI) GI->MarkEndCardSeen();
		NestSelection = LeaflineUpgrades::Count; // start on "next day", so a quick double press never spends
		LeafSound::Play(this, TEXT("ui_confirm"), 0.6f);
		break;
	}
	case EDayPhase::EndCard:
		Phase = EDayPhase::Nest;
		LeafSound::Play(this, TEXT("ui_confirm"), 0.6f);
		break;
	case EDayPhase::Nest:
		if (NestSelection >= LeaflineUpgrades::Count)
		{
			LeafSound::Play(this, TEXT("ui_confirm"), 0.6f);
			StartDay(Day + 1);
		}
		else if (ULeaflineGameInstance* GI = Colony(this))
		{
			if (GI->Buy(NestSelection))
			{
				LeafSound::Play(this, TEXT("upgrade"), 0.75f);
				UE_LOG(LogLeafline, Log, TEXT("Nest: bought %s level %d"), LeaflineUpgrades::Get(NestSelection)->Name, GI->GetUpgradeLevel(NestSelection));
			}
			else
			{
				LeafSound::Play(this, TEXT("ui_deny"), 0.6f);
			}
		}
		break;
	default:
		break;
	}
}

void ADayDirector::MenuNavigate(int32 Direction)
{
	if (Phase != EDayPhase::Nest || Direction == 0) return;
	int32 Next = NestSelection;
	do
	{
		Next = FMath::Clamp(Next + (Direction > 0 ? 1 : -1), 0, LeaflineUpgrades::Count);
	} while (!IsNestRowVisible(Next) && Next > 0 && Next < LeaflineUpgrades::Count);
	if (!IsNestRowVisible(Next)) Next = NestSelection; // a hidden first row: stay put
	NestSelection = Next;
	LeafSound::Play(this, TEXT("ui_move"), 0.5f);
}

void ADayDirector::SetNestSelection(int32 Index)
{
	if (Phase != EDayPhase::Nest) return;
	Index = FMath::Clamp(Index, 0, LeaflineUpgrades::Count);
	if (Index == NestSelection || !IsNestRowVisible(Index)) return;
	NestSelection = Index;
	LeafSound::Play(this, TEXT("ui_move"), 0.5f);
}

void ADayDirector::MenuBack()
{
	if (Phase == EDayPhase::Nest)
	{
		LeafSound::Play(this, TEXT("ui_confirm"), 0.6f);
		StartDay(Day + 1);
	}
}

bool ADayDirector::IsNearNest(const FVector& P) const
{
	if (!Nest) return false;
	const FVector N = Nest->GetActorLocation();
	return FVector::DistSquared2D(P, N) < Tuning.NestRadius * Tuning.NestRadius && FMath::Abs(P.Z - N.Z) < 150.0;
}

void ADayDirector::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (PendingPossess > 0.f)
	{
		PendingPossess -= DeltaTime;
		// Only move on if the player is still watching that ant (blown off or sent home; control may have moved already).
		APawn* Watched = Player.IsValid() ? Player->GetPawn() : nullptr;
		if (PendingPossess <= 0.f && Phase == EDayPhase::Shift && (!Watched || Watched == PendingFrom.Get())) PossessNext();
	}
	RescueStalledScouts();
	// Never leave the player without an ant while scouts still wait in the nest (e.g. the pending hand-off was lost
	// because the player switched away, and the ant they switched to was then counted home).
	if (Phase == EDayPhase::Shift && PendingPossess <= 0.f && Player.IsValid() && !Player->GetPawn() && NumWaiting() > 0)
	{
		UE_LOG(LogLeafline, Log, TEXT("No ant under control with scouts waiting: the next one comes out"));
		PossessNext();
	}
	UpdateDayFlow();
	UpdateHarvestCamera(DeltaTime);
	if (Phase == EDayPhase::AllHome)
	{
		PhaseTimer -= DeltaTime;
		if (PhaseTimer <= 0.f)
		{
			Phase = EDayPhase::Recap;
			LeafSound::Play(this, TEXT("day_end"), 0.7f);
			UE_LOG(LogLeafline, Log, TEXT("Recap, day %d: hauled %.1f from %d fragments, %d blown off, %.0f m of trail"),
				Day, Stats.MassHauled, Stats.Fragments, Stats.BlownOff, Stats.TrailLaid / 100.f);
		}
	}
	UpdateSun(DeltaTime, false);
}

void ADayDirector::UpdateSun(float Dt, bool bSnap)
{
	if (!Sun.IsValid()) return;
	// The sun steps from morning to evening with each shift: a visual cue only.
	const int32 N = FMath::Max(Roster.Num(), 1);
	float T = N > 1 ? float(FMath::Clamp(ShiftsStarted - 1, 0, N - 1)) / (N - 1) : 0.f;
	if (Phase == EDayPhase::Harvest || Phase == EDayPhase::AllHome || Phase == EDayPhase::Recap) T = 1.f;
	const float Target = T < 0.5f ? FMath::Lerp(Tuning.SunPitchMorning, Tuning.SunPitchNoon, T * 2.f)
	                              : FMath::Lerp(Tuning.SunPitchNoon, Tuning.SunPitchEvening, (T - 0.5f) * 2.f);
	SunPitch = bSnap ? Target : FMath::FInterpTo(SunPitch, Target, Dt, 0.6f);
	float& YawOffset = SpareFloat;
	YawOffset = bSnap ? T * 70.f : FMath::FInterpTo(YawOffset, T * 70.f, Dt, 0.6f);
	Sun->SetActorRotation(FRotator(SunPitch, SunYaw + YawOffset, 0.f));
	// Warmth follows the same morning-noon-evening curve; only touch the light when it changes noticeably.
	const float Temp = T < 0.5f ? FMath::Lerp(Look.SunTemperatureMorning, Look.SunTemperatureNoon, T * 2.f)
	                            : FMath::Lerp(Look.SunTemperatureNoon, Look.SunTemperatureEvening, (T - 0.5f) * 2.f);
	float& TempShown = SpareFloat2;
	TempShown = bSnap ? Temp : FMath::FInterpTo(TempShown, Temp, Dt, 0.6f);
	if (FMath::Abs(TempShown - SunTemperatureApplied) > 5.f)
	{
		if (ULightComponent* Light = Sun->GetLightComponent()) Light->SetTemperature(TempShown);
		SunTemperatureApplied = TempShown;
	}
}
