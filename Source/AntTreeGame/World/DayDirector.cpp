#include "World/DayDirector.h"
#include "AntTreeGame.h"
#include "Ant/AntPawn.h"
#include "Ant/AntMovementComponent.h"
#include "Tree/TreeActor.h"
#include "Colony/TrailNetwork.h"
#include "Colony/NestActor.h"
#include "World/WindSystem.h"
#include "Engine/DirectionalLight.h"
#include "Components/LightComponent.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "Game/LeaflineGameInstance.h"

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

void ADayDirector::Init(ATreeActor* InTree, ATrailNetwork* InTrails, ANestActor* InNest, AWindSystem* InWind)
{
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

	Tree->Params.LowestLeafyHeight = Tuning.LeafyHeightDay1 + (Day - 1) * Tuning.LeafyHeightPerDay;
	Tree->Grow(Seed);
	Nest->SetActorTransform(Tree->GetNestTransform(0));
	if (Wind) Wind->StartDay(Day, Seed);
	PendingPossess = 0.f;

	Stats = FDayStats();
	Stats.Day = Day;
	Phase = EDayPhase::Shift;
	ShiftsStarted = 0;
	Current = INDEX_NONE;
	UE_LOG(LogLeafline, Log, TEXT("Day %d starts: seed %d, %d workers, lowest leafy branch at %.0f m"),
		Day, Seed, Roster.Num(), Tree->Params.LowestLeafyHeight / 100.f);
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
		if (PC) PC->Possess(Ant);
		UE_LOG(LogLeafline, Log, TEXT("Shift %d of %d: worker %d leaves the nest"), ShiftsStarted, Roster.Num(), i);
		return;
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
		if (Ant->GetAntMovement() && IsNearNest(Ant->GetAntMovement()->GetSurfacePosition()))
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
	UE_LOG(LogLeafline, Log, TEXT("Worker %d sent home carrying %.1f"), i, Ant->GetCarriedMass());
	PossessNext();
}

void ADayDirector::OnBlownOff(AAntPawn* Ant)
{
	const int32 i = FindWorker(Ant);
	if (i == INDEX_NONE) return;
	const EWorkerState Was = Roster[i].State;
	if (Was != EWorkerState::Out && Was != EWorkerState::Returning) return;
	Roster[i].State = EWorkerState::BlownOff;
	Stats.BlownOff += 1;
	UE_LOG(LogLeafline, Log, TEXT("Worker %d blown off (was %s); it walks home empty-handed"), i, *UEnum::GetValueAsString(Was));
	// Let the player watch the fall for a moment before the next worker comes out.
	if (Ant->IsPlayerControlled() && Was == EWorkerState::Out) PendingPossess = 2.5f;
	CheckAllHome();
}

void ADayDirector::OnAntReachedNest(AAntPawn* Ant)
{
	const int32 i = FindWorker(Ant);
	if (i == INDEX_NONE || Roster[i].State == EWorkerState::Home) return;
	if (Ant->IsCarrying())
	{
		const float Mass = Ant->GetCarriedMass();
		Stats.MassHauled += Mass;
		Stats.Fragments += 1;
		Stats.LargestFragment = FMath::Max(Stats.LargestFragment, Mass);
		if (Nest) Nest->PlayDeposit(Mass);
		if (ULeaflineGameInstance* GI = Colony(this)) GI->AddHaul(Mass);
		UE_LOG(LogLeafline, Log, TEXT("Worker %d home with %.1f (today %.1f from %d fragments)"), i, Mass, Stats.MassHauled, Stats.Fragments);
		Ant->ClearFragment();
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
	Ant->Destroy();
	CheckAllHome();
}

void ADayDirector::CheckAllHome()
{
	for (const FWorker& W : Roster)
	{
		if (W.State != EWorkerState::Home && W.State != EWorkerState::BlownOff) return;
	}
	if (Phase != EDayPhase::Shift) return;
	Phase = EDayPhase::AllHome;
	PhaseTimer = 1.5f;
	if (ULeaflineGameInstance* GI = Colony(this))
	{
		GI->SetDay(Day + 1);
		GI->SaveNow();
	}
	UE_LOG(LogLeafline, Log, TEXT("Day %d: all workers home"), Day);
}

void ADayDirector::CycleWorker()
{
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

void ADayDirector::ConfirmRecap()
{
	if (Phase != EDayPhase::Recap) return;
	StartDay(Day + 1);
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
		if (PendingPossess <= 0.f && Phase == EDayPhase::Shift) PossessNext();
	}
	if (Phase == EDayPhase::AllHome)
	{
		PhaseTimer -= DeltaTime;
		if (PhaseTimer <= 0.f)
		{
			Phase = EDayPhase::Recap;
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
	if (Phase == EDayPhase::AllHome || Phase == EDayPhase::Recap) T = 1.f;
	const float Target = T < 0.5f ? FMath::Lerp(Tuning.SunPitchMorning, Tuning.SunPitchNoon, T * 2.f)
	                              : FMath::Lerp(Tuning.SunPitchNoon, Tuning.SunPitchEvening, (T - 0.5f) * 2.f);
	SunPitch = bSnap ? Target : FMath::FInterpTo(SunPitch, Target, Dt, 0.6f);
	float& YawOffset = SpareFloat;
	YawOffset = bSnap ? T * 70.f : FMath::FInterpTo(YawOffset, T * 70.f, Dt, 0.6f);
	Sun->SetActorRotation(FRotator(SunPitch, SunYaw + YawOffset, 0.f));
}
