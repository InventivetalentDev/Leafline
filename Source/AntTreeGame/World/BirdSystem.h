#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BirdSystem.generated.h"

class ATreeActor;
class AWindSystem;
class ADayDirector;
class ALineSystem;
class AAntPawn;
class UStaticMeshComponent;
class UStaticMesh;
class UMaterialInstanceDynamic;
class UMaterialInterface;

UENUM()
enum class EBirdState : uint8
{
	Off,      // not today, the shift is over, or the switch is off
	Circling, // gliding on the upwind side, looking for a target
	Diving,   // called; comes in from upwind at its target (WarnSeconds)
	Away      // out of sight after a pass (or gone for the day when its swoops are used up)
};

/** The bird's numbers (docs/briefs/bird.md). Schedule values are sim seconds; Look values only move the visible bird. */
USTRUCT(BlueprintType)
struct FBirdTuning
{
	GENERATED_BODY()

	/** The whole feature (Leaf.Bird). Off: no bird, no schedule, no HUD line, no hint, no drop action. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Switch") bool bBird = true;
	/** Tier 2, dropped pieces as bait (Leaf.Bait). First to cut. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Switch") bool bBait = true;

	/** With recruits off the bird first comes on this day (with recruits on: the first day the pool is above zero). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Schedule") int32 FirstBirdDay = 3;
	/** 2, not the brief's starting 3: with 3 the end card came a day later in twenty AutoPlay days (hold240 holdgrip, 14 Nov). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Schedule") int32 MaxSwoopsPerDay = 2;
	/** Swoops on the first bird day (of the session; nothing is saved). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Schedule") int32 FirstDaySwoops = 1;
	/** Circling time before it looks for a target, and the seeded jitter on it (fraction, +-). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Schedule") float SwoopInterval = 40.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Schedule") float SwoopJitter = 0.3f;
	/** No target: it looks again after this long. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Schedule") float LookAgain = 4.f;
	/** From the call to the pass. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Schedule") float WarnSeconds = 2.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Schedule") float AwaySeconds = 30.f;
	/** Away after taking a dropped piece. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Schedule") float BaitAwaySeconds = 45.f;
	/** Other laden ants this close to the target (cm) face the same test at the pass. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Schedule") float HitRadius = 120.f;
	/** Most ants knocked off in one pass. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Schedule") int32 MaxHitsPerSwoop = 2;

	/** Size of the visible bird: SM_Bird as imported (real size, 1 m wingspan, 62 cm long) times this; the placeholder is
	 *  sized like it. 5: a 5 m wingspan, about 25 times life size like the ants. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look") float BirdScale = 5.f;
	/** Gap between the bark and the bird's underside at the pass (cm); it also stays PieceClearance above the carried piece. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look") float PassHeight = 80.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look") float PieceClearance = 30.f;
	/** Circle: radius, how far upwind of the trunk its centre starts, lowest height (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look") float CircleRadius = 2200.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look") float CircleDistance = 4000.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look") float MinCircleHeight = 1500.f;
	/** Air kept between the circling bird and any branch (cm; leaves stick out about 200 cm from their twigs). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look") float ClearMargin = 450.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look") float GlideSpeed = 900.f;
	/** Straight run in toward the target's bark, and the climb away after the pass (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look") float ApproachDistance = 1800.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look") float ExitDistance = 2400.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look") float ExitSeconds = 1.6f;
	/** After the exit it flies off and is hidden after this long. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look") float LeaveSeconds = 4.f;
	/** Wing beats per second while flapping (FlapPhase on M_Bird). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look") float FlapHz = 3.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look") FLinearColor PlaceholderColor = FLinearColor(0.05f, 0.045f, 0.04f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound") float CallVolume = 0.9f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sound") float PassVolume = 0.8f;
};

/** What a pass did to one ant. */
enum class EBirdOutcome : uint8 { Knocked, Held, Missed, Gone };

/** A piece dropped on the bark (tier 2). */
struct FBirdBait
{
	FVector Pos = FVector::ZeroVector;
	FVector Normal = FVector::UpVector;
	float Diameter = 50.f;
	float Exposure = 0.f;
	TWeakObjectPtr<UStaticMeshComponent> Mesh;
};

/**
 * The bird (docs/briefs/bird.md). Its schedule, choice of target and the hit run in ALineSystem's fixed sim steps
 * (Advance), seeded from the tree seed and the day, on its own random stream; only during the Shift. The visible bird in
 * Tick only follows that schedule. A hit goes through AAntPawn::BlowOff with the bird as the cause.
 */
UCLASS()
class ANTTREEGAME_API ABirdSystem : public AActor
{
	GENERATED_BODY()

public:
	ABirdSystem();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bird")
	FBirdTuning Tuning;

	void Init(ATreeActor* InTree, AWindSystem* InWind, ADayDirector* InDirector, ALineSystem* InLines);

	/** New day (ALineSystem::StartDay): today's plan from the day, the tree seed, the recruit switch and the pool. */
	void StartDay(int32 InDay, int32 InSeed, bool bInRecruitsOn, int32 InPool);
	/** One fixed sim step (ALineSystem::StepAnts). Runs only while the director is in the Shift. */
	void Advance(float Dt);

	/** Leaf.Bird / Leaf.Bait. Turning the bird on mid-day starts today's schedule from now. */
	void SetBirdOn(bool bOn);
	void SetBaitOn(bool bOn);
	bool IsBirdOn() const { return Tuning.bBird; }
	bool IsBaitOn() const { return Tuning.bBird && Tuning.bBait; }
	/** Leaf.BirdNow: at the next fixed step, look for a target and dive (ignores the schedule; counts as one of the day's
	 *  swoops, but also works when they are used up). */
	void DiveNow();
	/** Leaf.BirdInfo. */
	void LogInfo() const;

	/** Tier 2: the drop action. The piece is put down at the next fixed step (if still allowed then). False if not allowed now. */
	bool DropPiece(AAntPawn* Ant);
	/** The drop prompt and action: bait on, the bird circling or diving, the ant carrying and in a state that can drop. */
	bool CanDrop(const AAntPawn* Ant) const;

	EBirdState GetState() const { return State; }
	/** Circling or diving today (what the HUD line shows). */
	bool IsPresent() const { return bActiveToday && (State == EBirdState::Circling || State == EBirdState::Diving); }
	bool IsDiving() const { return bActiveToday && State == EBirdState::Diving; }
	bool IsDivingAt(const AAntPawn* Ant) const;
	bool IsActiveToday() const { return bActiveToday; }
	/** The first bird day (from the day and the pool: FirstBirdDay with recruits off, the first Brood level's pool with
	 *  recruits on): FirstDaySwoops, and the hint. */
	bool IsFirstBirdDay() const { return bActiveToday && bFirstBirdDay; }
	/** Pieces the bird knocked off today (all ants), and how many of those were recruits' (they also count as recruit losses). */
	int32 GetPiecesTaken() const { return PiecesTaken; }
	int32 GetRecruitPiecesTaken() const { return RecruitPiecesTaken; }
	int32 GetSwoops() const { return Swoops; }

	static ABirdSystem* Get(const UObject* WorldContext);

private:
	void EnterCircling(bool bFromAway);
	void StartDive(AAntPawn* Ant, int32 BaitIndex);
	void Pass();
	/** The way in to the pass is blocked by wood (the target moved round a branch): the bird breaks off, a miss. */
	void BreakOff();
	/** Requests from the console and the drop button, taken at the start of a fixed step. */
	void ConsumeRequests(bool bShift);
	void DoDiveNow();
	void DoDrop(AAntPawn* Ant);
	/** The pass point and the approach corridor toward it (sim): chosen at the dive's start, checked as the target moves. */
	void ChooseApproach();
	bool IsCorridorClear(const FVector& PassPoint, const FVector& Dir) const;
	FVector PassPoint() const;
	EBirdOutcome TestAnt(AAntPawn* Ant, float& OutExposure);
	/** Laden ants a gust could blow off, scouts by roster index then recruits by pool index (a fixed order). */
	void GatherLaden(TArray<AAntPawn*>& Out) const;
	/** Best target now: exposed bait first (tier 2), else the most exposed laden ant. False if none. */
	bool ChooseTarget(AAntPawn*& OutAnt, int32& OutBait, float& OutExposure, int32& OutLaden) const;
	void GoOff(const TCHAR* Why);
	void ClearBaits();
	bool InShift() const;
	/** "worker 1 (yours)", "recruit 4". */
	FString Describe(const AAntPawn* Ant) const;
	void PlaySound(const TCHAR* Name, const TCHAR* Fallback, float Volume, const FVector& At) const;

	// Visible bird.
	void UpdateVisual(float Dt);
	void ComputeCircle(float Height);
	void BeginVisualDive();
	void BeginVisualExit();
	void UpdateExitTangent(const FVector& Normal);
	FVector TargetSurfacePos(FVector* OutNormal = nullptr) const;
	float BirdHalfSpan() const;
	/** From the bird's origin (the shoulder line) down to its belly, at the current scale. */
	float BirdBelow() const;
	/** How far a wing tip falls below the shoulder line at full flap (cm, at the current scale). */
	float BirdTipFall() const;
	float PassGap() const;
	void SetVisible(bool bVisible);

	UPROPERTY(VisibleAnywhere, Category = "Bird") TObjectPtr<UStaticMeshComponent> BirdMesh;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> BirdMID;
	UPROPERTY() TObjectPtr<UMaterialInterface> BasicMaterial;
	UPROPERTY() TObjectPtr<UStaticMesh> SphereMesh;
	UPROPERTY() TObjectPtr<UStaticMesh> CylinderMesh;
	UPROPERTY(Transient) TObjectPtr<ATreeActor> Tree;
	UPROPERTY(Transient) TObjectPtr<AWindSystem> Wind;
	UPROPERTY(Transient) TObjectPtr<ADayDirector> Director;
	UPROPERTY(Transient) TObjectPtr<ALineSystem> Lines;

	// Schedule (sim).
	FRandomStream Rand;
	EBirdState State = EBirdState::Off;
	double StateTime = 0.0;
	double NextLook = 0.0;
	double AwayFor = 0.0;
	double Clock = 0.0;
	int32 Day = 1;
	int32 Seed = 0;
	bool bRecruitsOn = true;
	int32 Pool = 0;
	bool bActiveToday = false;
	bool bFirstBirdDay = false;
	bool bLoggedNoTarget = false;
	int32 SwoopsLeft = 0;
	int32 Swoops = 0;
	int32 PiecesTaken = 0;
	int32 RecruitPiecesTaken = 0;
	int32 Held = 0;
	int32 Missed = 0;
	int32 BaitsTaken = 0;
	bool bDaySummaryLogged = false;
	TWeakObjectPtr<AAntPawn> Target;
	int32 TargetBait = INDEX_NONE;
	FVector TargetLastPos = FVector::ZeroVector;
	FVector TargetLastNormal = FVector::UpVector;
	TArray<FBirdBait> Baits;
	bool bHasCallSound = false;
	bool bHasPassSound = false;
	/** Count dives, passes and break-offs, so the visible bird reacts to each once even if several steps run in a frame. */
	int32 DiveSerial = 0;
	int32 PassSerial = 0;
	int32 BreakSerial = 0;
	int32 DiveSteps = 0;
	bool bDiveRequested = false;
	TWeakObjectPtr<AAntPawn> DropRequest;
	/** Sim: how the bird comes in to its pass (from the target's bark out along this), and how far out it pulls up. */
	FVector ApproachDir = FVector::ForwardVector;
	float DiveGap = 80.f;

	// Visible bird (real time).
	bool bVisShown = false;
	uint8 VisMode = 0;
	float VisModeTime = 0.f;
	int32 VisDiveSerial = 0;
	int32 VisPassSerial = 0;
	int32 VisBreakSerial = 0;
	FVector VisPos = FVector::ZeroVector;
	FVector VisVel = FVector::ZeroVector;
	FQuat VisRot = FQuat::Identity;
	FVector CircleCenter = FVector::ZeroVector;
	float CircleR = 2000.f;
	float CircleHeight = -1.f;
	float CircleAngle = 0.f;
	float CircleCheckTimer = 0.f;
	float CircleSign = 1.f;
	FVector DiveFrom = FVector::ZeroVector;
	FVector DiveFromVel = FVector::ZeroVector;
	FVector DiveCtrl = FVector::ZeroVector;
	FVector ExitFrom = FVector::ZeroVector;
	FVector ExitDir = FVector::UpVector;
	FVector ExitNormal = FVector::UpVector;
	FVector ExitTangent = FVector::UpVector;
	FVector LeaveDir = FVector::UpVector;
	/** SM_Bird's own box (cm, as imported, without the bounds extension the flap needs); the placeholder's stand-in. */
	FBox ModelBox = FBox(FVector(-36.4f, -49.9f, -9.f), FVector(25.8f, 49.9f, 5.2f));
	float FlapPhase = 0.f;
	float FlapAmount = 0.f;
	bool bPlaceholder = true;
};
