#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Colony/ColonyTypes.h"
#include "DayDirector.generated.h"

class ATreeActor;
class ATrailNetwork;
class ANestActor;
class AAntPawn;
class APlayerController;
class ADirectionalLight;
class AWindSystem;

/**
 * Runs a day: grows the tree, sends workers out one shift at a time, brings them home, shows the recap and starts
 * the next day. Owns the worker roster.
 */
UCLASS()
class ANTTREEGAME_API ADayDirector : public AActor
{
	GENERATED_BODY()

public:
	ADayDirector();

	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day")
	FDayTuning Tuning;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day")
	TSubclassOf<AAntPawn> WorkerClass;

	void Init(ATreeActor* InTree, ATrailNetwork* InTrails, ANestActor* InNest, AWindSystem* InWind);
	void SetPlayer(APlayerController* InPlayer) { Player = InPlayer; }

	/** Grows the day's tree (seed from the day number unless overridden) and sends out the first worker. */
	void StartDay(int32 InDay, int32 SeedOverride = -1);

	/** The controlled ant goes home on its own; control shifts to the next worker. */
	void SendHome(AAntPawn* Ant);
	/** Wind knocked a worker off; if it was the controlled one, control moves on after a moment. */
	void OnBlownOff(AAntPawn* Ant);
	/** An ant reached the nest: deposit what it carries. */
	void OnAntReachedNest(AAntPawn* Ant);
	/** Switch the camera between ants that are out or returning. */
	void CycleWorker();
	/** Leave the recap screen for the next day. */
	void ConfirmRecap() { MenuConfirm(); }

	/** Show the title over the first worker; play starts on confirm. */
	void ShowTitle();
	/** Menu input for the title, recap, end card and nest screens. */
	void MenuConfirm();
	void MenuNavigate(int32 Direction);
	void MenuBack();
	bool IsMenuPhase() const { return Phase == EDayPhase::Title || Phase == EDayPhase::Recap || Phase == EDayPhase::EndCard || Phase == EDayPhase::Nest; }
	/** Selected row on the nest screen: 0..4 upgrades, 5 = start the next day. */
	int32 GetNestSelection() const { return NestSelection; }

	bool IsNearNest(const FVector& P) const;

	EDayPhase GetPhase() const { return Phase; }
	const FDayStats& GetStats() const { return Stats; }
	int32 GetDay() const { return Day; }
	int32 GetSeed() const { return Seed; }
	int32 NumWorkers() const { return Roster.Num(); }
	EWorkerState GetWorkerState(int32 Index) const { return Roster.IsValidIndex(Index) ? Roster[Index].State : EWorkerState::Waiting; }
	int32 GetCurrentWorker() const { return Current; }
	int32 NumWaiting() const;
	ATreeActor* GetTree() const { return Tree; }
	ATrailNetwork* GetTrails() const { return Trails; }
	ANestActor* GetNest() const { return Nest; }
	AWindSystem* GetWind() const { return Wind; }

	static ADayDirector* Get(const UObject* WorldContext);

private:
	struct FWorker
	{
		TWeakObjectPtr<AAntPawn> Pawn;
		EWorkerState State = EWorkerState::Waiting;
	};

	AAntPawn* SpawnWorker(int32 Index);
	/** Control the next waiting worker, or else follow a returning one. */
	void PossessNext();
	int32 FindWorker(const AAntPawn* Ant) const;
	void CheckAllHome();
	void UpdateSun(float Dt, bool bSnap);

	UPROPERTY(Transient) TObjectPtr<ATreeActor> Tree;
	UPROPERTY(Transient) TObjectPtr<ATrailNetwork> Trails;
	UPROPERTY(Transient) TObjectPtr<ANestActor> Nest;
	UPROPERTY(Transient) TObjectPtr<AWindSystem> Wind;
	TWeakObjectPtr<APlayerController> Player;
	TWeakObjectPtr<ADirectionalLight> Sun;

	TArray<FWorker> Roster;
	int32 Current = INDEX_NONE;
	EDayPhase Phase = EDayPhase::Starting;
	FDayStats Stats;
	int32 Day = 1;
	int32 Seed = 0;
	int32 ShiftsStarted = 0;
	float PhaseTimer = 0.f;
	float SunPitch = -30.f;
	float SunYaw = -40.f;
	float SpareFloat = 0.f;
	int32 SpareInt = 0;
	float PendingPossess = 0.f;
	int32 NestSelection = 0;
	bool bEndCardPending = false;
};
