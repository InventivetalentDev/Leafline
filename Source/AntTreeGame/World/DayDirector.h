#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Colony/ColonyTypes.h"
#include "Tree/TreeTypes.h"
#include "DayDirector.generated.h"

class ATreeActor;
class ATrailNetwork;
class ANestActor;
class AAntPawn;
class APlayerController;
class ADirectionalLight;
class AWindSystem;
class ALineSystem;

/**
 * The look of the day: sun warmth through the shifts, sky haze, height fog and light shafts. Applied to the level's
 * sun, sky light, sky atmosphere and height fog when the game starts and whenever Leaf.Tune look changes a value.
 */
USTRUCT(BlueprintType)
struct FLookTuning
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sun") float SunIntensity = 8.f;
	/** Colour temperature (K) at the first, middle and last shift; lower is warmer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sun") float SunTemperatureMorning = 4600.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sun") float SunTemperatureNoon = 5600.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sun") float SunTemperatureEvening = 3800.f;
	/** Apparent size of the sun (degrees): larger gives softer shadow edges. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sun") float SunSourceAngle = 1.f;
	/** Screen-space rays around the sun when it is in view (cheap, works without ray tracing). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sun") float ShaftBloomScale = 0.6f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sun") float ShaftBloomThreshold = 4.f;
	/** How strongly the sun lights the volumetric fog (light shafts through the leaves). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sun") float SunVolumetricScattering = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky") float SkyLightIntensity = 1.f;
	/** Aerial perspective distance scale: above 1 hazes distant things sooner (the world is at ant scale). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky") float AerialPerspectiveScale = 4.f;
	/** Haze in the sky (Mie scattering) and how tightly it glows around the sun. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky") float MieScatteringScale = 0.003996f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sky") float MieAnisotropy = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog") float FogDensity = 0.06f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog") float FogHeightFalloff = 0.25f;
	/** Height of the fog's base (cm); below -100000 leaves the level's value. With the falloff, the fog thins upward:
	 *  the ground far below hazes over while the canopy keeps enough for light shafts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog") float FogBaseHeight = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog") float FogStartDistance = 0.f;
	/** Sun-coloured glow in the fog looking toward the sun; black turns it off. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog") FLinearColor FogSunGlowColor = FLinearColor(0.6f, 0.42f, 0.22f, 1.f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog") float FogSunGlowExponent = 6.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog") bool bVolumetricFog = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog") float VolumetricFogExtinction = 0.6f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog") float VolumetricFogDistance = 9000.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog") float VolumetricFogScattering = 0.7f;
	/** A second, lower fog layer: ground haze. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog") float GroundHazeDensity = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog") float GroundHazeFalloff = 0.2f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fog") float GroundHazeHeightOffset = 0.f;
};

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
	FLookTuning Look;

	/** Push Look to the level's sun, sky light, atmosphere and fog. */
	void ApplyLook();

	/** The day's tree, e.g. "a spreading giant". */
	const FString& GetTreeName() const { return TreeName; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day")
	TSubclassOf<AAntPawn> WorkerClass;

	void Init(ATreeActor* InTree, ATrailNetwork* InTrails, ANestActor* InNest, AWindSystem* InWind, ALineSystem* InLines);
	void SetPlayer(APlayerController* InPlayer) { Player = InPlayer; }

	/** Grows the day's tree (seed from the day number unless overridden) and sends out the first worker. */
	void StartDay(int32 InDay, int32 SeedOverride = -1);

	/** The controlled ant goes home on its own; control shifts to the next worker. */
	void SendHome(AAntPawn* Ant);
	/** Wind knocked a worker off; if it was the controlled one, control moves on after a moment. */
	void OnBlownOff(AAntPawn* Ant);
	/** An ant reached the nest: deposit what it carries. */
	void OnAntReachedNest(AAntPawn* Ant);
	/** A piece reached the nest (scout or recruit): today's stats, the colony's haul, milestones. */
	void CreditHaul(float Mass, bool bByRecruit);
	/** A leaf was bitten (scout or recruit): today's cut heights (logged at day end). */
	void NoteCut(int32 Leaf);
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
	bool IsMenuPhase() const { return Phase == EDayPhase::Title || Phase == EDayPhase::Recap || Phase == EDayPhase::EndCard || Phase == EDayPhase::Nest || Phase == EDayPhase::Move; }
	/** Selected row on the nest screen: 0..4 upgrades, 5 = start the next day. */
	int32 GetNestSelection() const { return NestSelection; }
	/** Whether an upgrade's row is on the nest screen (Brood is hidden while recruits are off). */
	bool IsNestRowVisible(int32 Upgrade) const;
	// One tree per stage (FDayTuning::bOneTree, Leaf.OneTree): the tree is kept from day to day and the colony moves to a
	// new tree at the second and third milestones.
	bool IsOneTree() const { return Tuning.bOneTree; }
	/** Takes effect from the next day (the day's mode is latched at dawn, so a day is always saved the way it started). */
	void SetOneTree(bool bOn);
	/** The colony's tree, 0 for the first (one tree on; 0 with it off). */
	int32 GetStage() const { return Stage; }
	/** 1 on a tree's first day (always 1 with one tree off: a new tree every day). */
	int32 GetDayOnTree() const { return FMath::Max(1, Day - StageStartDay + 1); }
	/** Today's tree is new: the colony moved to it this morning (one tree on, not the first tree). */
	bool HasMovedToday() const { return bMovedToday; }
	/** "The colony moves to a tall spire" (the move card). */
	FString GetMoveCardText() const;
	/** Where today's leaves start (cm): with one tree on, the lowest leaf that could be cut when the tree was grown; else the
	 *  lowest leafy branch. */
	float GetLowestLeafyHeight() const;
	/** The next day starts on a new tree (set at day end, when the second or third milestone was reached). */
	bool IsMoveDue() const;
	/** Debug (Leaf.Move): restart today on the next tree. */
	void DebugMoveNow();
	/** Debug: the next StartDay grows the tree again from the save (the launch path: seed, saved bites, one mesh build). */
	void DebugForceRegrow() { GrownTreeSeed = -1; }
	const FStageTuning& GetStageTuning(int32 InStage) const;

	/** Recruits on: FDayTuning::bRecruits (Leaf.Recruits). */
	bool AreRecruitsOn() const { return Tuning.bRecruits; }
	void SetRecruitsOn(bool bOn);

	// Harvest: every scout is home or on its way and recruits still have trips; the rest of the day plays fast.
	bool IsHarvest() const { return Phase == EDayPhase::Harvest; }
	bool IsSkippingHarvest() const { return bSkippingHarvest; }
	/** Finish the harvest at skip speed (same simulation, more steps per frame). */
	void SkipHarvest();
	/** Harvest camera: 0 = at the nest looking up the trunk, k = the patch end of the k-th live line. Tab steps. */
	void CycleHarvestView();
	int32 GetHarvestView() const { return HarvestView; }
	/** Debug (Leaf.Harvest): end every shift now and go straight to the harvest. */
	void ForceHarvest();
	/** Debug: the scout under control if it is out, else the next one called out at once (no pause). */
	AAntPawn* DebugTakeNextScout();
	/** The mouse moved onto a nest row (0..upgrade count, the last is "next day"). */
	void SetNestSelection(int32 Index);

	bool IsNearNest(const FVector& P) const;

	EDayPhase GetPhase() const { return Phase; }
	const FDayStats& GetStats() const { return Stats; }
	int32 GetDay() const { return Day; }
	int32 GetSeed() const { return Seed; }
	int32 NumWorkers() const { return Roster.Num(); }
	/** Workers sent out so far today (1 during the first shift). */
	int32 GetShiftsStarted() const { return ShiftsStarted; }
	EWorkerState GetWorkerState(int32 Index) const { return Roster.IsValidIndex(Index) ? Roster[Index].State : EWorkerState::Waiting; }
	int32 GetCurrentWorker() const { return Current; }
	int32 NumWaiting() const;
	ATreeActor* GetTree() const { return Tree; }
	ATrailNetwork* GetTrails() const { return Trails; }
	ANestActor* GetNest() const { return Nest; }
	AWindSystem* GetWind() const { return Wind; }
	ALineSystem* GetLineSystem() const { return Lines; }

	static ADayDirector* Get(const UObject* WorldContext);

private:
	struct FWorker
	{
		TWeakObjectPtr<AAntPawn> Pawn;
		EWorkerState State = EWorkerState::Waiting;
		// Blown off and walking home over the ground: where it last made headway, and when (world seconds).
		FVector ProgressPos = FVector::ZeroVector;
		double ProgressTime = -1.0;
	};
	/** A blown-off scout that stalls on its walk home (against a root) is counted home, so the day can end. */
	void RescueStalledScouts();

	AAntPawn* SpawnWorker(int32 Index);
	/** Control the next waiting worker, or else follow a returning one. */
	void PossessNext();
	int32 FindWorker(const AAntPawn* Ant) const;
	void CheckAllHome();
	/** Day-end and harvest transitions, evaluated every frame during shifts and the harvest. */
	void UpdateDayFlow();
	void EnterHarvest();
	void EndDay();
	/** Where the harvest camera should be for the current view. */
	void UpdateHarvestCamera(float Dt);
	void UpdateSun(float Dt, bool bSnap);
	/** Seed of a stage's tree (never 0). */
	int32 SeedForStage(int32 InStage) const;

	UPROPERTY(Transient) TObjectPtr<ATreeActor> Tree;
	UPROPERTY(Transient) TObjectPtr<ATrailNetwork> Trails;
	UPROPERTY(Transient) TObjectPtr<ANestActor> Nest;
	UPROPERTY(Transient) TObjectPtr<AWindSystem> Wind;
	UPROPERTY(Transient) TObjectPtr<ALineSystem> Lines;
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
	float SunTemperatureApplied = 0.f;
	FString TreeName;
	FTreeParams BaseTreeParams;
	bool bHaveBaseTreeParams = false;
	float SpareFloat2 = 6500.f;
	float SpareFloat = 0.f;
	int32 SpareInt = 0;
	float PendingPossess = 0.f;
	TWeakObjectPtr<AAntPawn> PendingFrom;
	int32 NestSelection = 0;
	bool bEndCardPending = false;
	bool bSkippingHarvest = false;
	int32 HarvestView = 0;
	int32 Stage = 0;
	int32 StageStartDay = 1;
	bool bMovedToday = false;
	bool bDebugMovePending = false;
	/** Leaf.OneTree waiting for the next dawn: -1 none, else 0 or 1. */
	int32 PendingOneTree = -1;
	double CutZSum = 0.0;
	int32 CutCount = 0;
	float CutZMin = 0.f;
	/** Seed and stage of the tree currently grown (one tree on); -1 when it must be grown. */
	int32 GrownTreeSeed = -1;
	int32 GrownStage = -1;
	int32 GrownGeneration = -1;
};
