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

void ADayDirector::StartDay(int32 InDay, int32 SeedOverride)
{
	if (!Tree || !Trails || !Nest) return;
	Day = FMath::Max(1, InDay);
	Seed = SeedOverride >= 0 ? SeedOverride : (int32)((HashCombine(GetTypeHash(Tuning.BaseSeed), GetTypeHash(Day)) & 0x7fffffff) % 1000000);

	for (FWorker& W : Roster)
	{
		if (W.Pawn.IsValid()) W.Pawn->Destroy();
	}
	Roster.Reset();
	ULeaflineGameInstance* GI = Colony(this);
	Roster.SetNum(FMath::Max(1, GI ? GI->GetWorkersPerDay(Tuning.WorkersPerDay) : Tuning.WorkersPerDay));
	if (GI) GI->SetDay(Day);
	Trails->ClearAll();

	// Each day's tree has a character: day 1 is the classic shape, later days take turns being broad and spreading,
	// tall and narrow, or leaning with more roots. Every tree also has one broken limb low down (bBrokenLimb).
	if (!bHaveBaseTreeParams)
	{
		BaseTreeParams = Tree->Params;
		bHaveBaseTreeParams = true;
	}
	FTreeParams P = BaseTreeParams;
	const int32 Shape = Tuning.TreeShapeOverride >= 0 ? Tuning.TreeShapeOverride % 4 : (Day <= 1 ? 0 : 1 + (Day - 2) % 3);
	switch (Shape)
	{
	case 1:
		TreeName = TEXT("a spreading giant");
		P.TrunkHeight *= 0.8f;
		P.TrunkRadius *= 1.1f;
		P.NumLimbs = 18;
		P.LimbLengthMax *= 1.45f;
		P.LimbLengthMin *= 1.3f;
		P.LimbPitchMinDeg = 4.f;
		P.LimbPitchMaxDeg = 26.f;
		break;
	case 2:
		TreeName = TEXT("a tall spire");
		P.TrunkHeight *= 1.2f;
		P.TrunkRadius *= 0.85f;
		P.NumLimbs = 28;
		P.LimbLengthMax *= 0.65f;
		P.LimbLengthMin *= 0.8f;
		P.LimbPitchMinDeg = 28.f;
		P.LimbPitchMaxDeg = 58.f;
		break;
	case 3:
		TreeName = TEXT("a leaning elder");
		P.TrunkLeanDeg = 9.f;
		P.TrunkWobble = 0.09f;
		P.FlareMul = 1.05f;
		P.NumRoots = 6;
		P.RootLength *= 1.3f;
		P.LimbPitchMinDeg = 8.f;
		P.LimbPitchMaxDeg = 36.f;
		break;
	default:
		TreeName = TEXT("the old giant");
		break;
	}
	P.LowestLeafyHeight = Tuning.LeafyHeightDay1 + (Day - 1) * Tuning.LeafyHeightPerDay;
	Tree->Params = P;
	Tree->Grow(Seed);
	Nest->SetActorTransform(Tree->GetNestTransform(0));
	Nest->SetMoundSize(Tree->Params.NestMoundRadius, Tree->Params.NestMoundHeight);
	if (Wind) Wind->StartDay(Day, Seed);
	PendingPossess = 0.f;
	bSkippingHarvest = false;
	HarvestView = 0;
	if (GI) GI->SetRecruitsEnabled(Tuning.bRecruits);
	if (Lines) Lines->StartDay();
	if (ALeaflinePlayerController* PC = Cast<ALeaflinePlayerController>(Player.Get()))
	{
		if (AAntCamera* Cam = PC->GetAntCamera()) Cam->SetOverview(false);
	}

	Stats = FDayStats();
	Stats.Day = Day;
	Phase = EDayPhase::Shift;
	ShiftsStarted = 0;
	Current = INDEX_NONE;
	UE_LOG(LogLeafline, Log, TEXT("Day %d starts: seed %d, %s, %d workers, lowest leafy branch at %.0f m"),
		Day, Seed, *TreeName, Roster.Num(), Tree->Params.LowestLeafyHeight / 100.f);
	PossessNext();
	UpdateSun(0.f, true);
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
		if (Roster[i].State == EWorkerState::Returning && Roster[i].Pawn.IsValid())
		{
			Current = i;
			if (PC) PC->Possess(Roster[i].Pawn.Get());
			UE_LOG(LogLeafline, Log, TEXT("No workers left in the nest: following returning worker %d"), i);
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
	if (Lines && Ant->IsCarrying()) Lines->AddLine(Ant, false);
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
		bScoutsMoving |= W.State == EWorkerState::Returning;
	}
	const bool bRecruitWork = Lines && Lines->IsActive() && Lines->HasWork();
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
		return;
	}
	if (!bRecruitWork && !bScoutsMoving) EndDay();
}

void ADayDirector::EnterHarvest()
{
	Phase = EDayPhase::Harvest;
	bSkippingHarvest = false;
	HarvestView = 0;
	if (Lines)
	{
		Lines->SetSpeedFactor(Lines->Tuning.HarvestSpeed);
		// Scouts still walking home now run on the same fast clock as the recruits.
		for (const FWorker& W : Roster)
		{
			if ((W.State == EWorkerState::Returning || W.State == EWorkerState::BlownOff) && W.Pawn.IsValid()) Lines->AddDrivenAnt(W.Pawn.Get());
		}
		UE_LOG(LogLeafline, Log, TEXT("Harvest, day %d: %d recruits out, %d trips left, %d live lines, x%.0f"),
			Day, Lines->NumOut(), Lines->TripsLeft(), Lines->NumLiveLines(), Lines->GetSpeedFactor());
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
	if (Phase != EDayPhase::Harvest)
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
	const FTransform NestXf = Tree->GetNestTransform(0);
	const FVector Away = NestXf.GetRotation().GetForwardVector().GetSafeNormal2D();
	const FVector NestPos = Nest->GetActorLocation();
	const FVector Loc = NestPos + Away * 520.0 + FVector(0.0, 0.0, 70.0);
	const FVector LookAt = NestPos - Away * 250.0 + FVector(0.0, 0.0, 650.0);
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
		GI->SetDay(Day + 1);
		GI->SaveNow();
	}
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
				if (Lines) Lines->AddLine(Ant, false);
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
		Phase = EDayPhase::Shift;
		LeafSound::Play(this, TEXT("ui_confirm"), 0.6f);
		UE_LOG(LogLeafline, Log, TEXT("Title: play starts on day %d"), Day);
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
