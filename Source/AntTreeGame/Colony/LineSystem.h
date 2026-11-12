#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Colony/ColonyTypes.h"
#include "LineSystem.generated.h"

class ATreeActor;
class ATrailNetwork;
class ANestActor;
class AWindSystem;
class ADayDirector;
class AAntPawn;

/** Recruit, line and harvest numbers. */
USTRUCT(BlueprintType)
struct FLineTuning
{
	GENERATED_BODY()

	/** Most leaves in one patch: the scout's leaf, then the nearest walkable ones (same twig, then neighbouring twigs). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patch") int32 PatchMaxLeaves = 4;
	/** Most pieces (of the line's template size) one patch may hold, the scout's own cut included. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patch") int32 PatchMaxPieces = 10;
	/** Farthest a patch leaf's base may be from the end of the line, walking along the bark (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patch") float PatchMaxWalk = 700.f;
	/** Template size assumed for the patch read-out before the player starts cutting. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Patch") float PreviewTemplate = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recruits") int32 TripsPerRecruit = 3;
	/** Seconds (sim time) between recruits leaving the nest, so several on one line read as a file. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recruits") float DepartSpacing = 1.5f;
	/** Walking speed out along a line, unladen (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recruits") float OutSpeed = 340.f;
	/** Multiplies the laden walking speed on the way home (they walk on the scent line). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recruits") float HomeSpeedScale = 1.5f;
	/** Past the end of the line, a recruit that has not reached its leaf after this long cuts from where it stands. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recruits") float ReachTimeout = 6.f;
	/** Safety net: a recruit out longer than this (sim seconds) is brought home, and it is logged. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Recruits") float RescueAfter = 420.f;

	/** Sim step (s). Recruits, the wind clock and (in the harvest) returning scouts advance in these steps. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harvest") float Step = 1.f / 120.f;
	/** Harvest playback speed, and the speed while skipping. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harvest") float HarvestSpeed = 5.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harvest") float SkipSpeed = 400.f;
	/** Real milliseconds per frame the simulation may use (bounds the skip). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Harvest") float FrameBudgetMs = 12.f;

	/** Trail brightness of a line with a full patch, a stripped one, and a trail with no line. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trail") float GlowFull = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trail") float GlowEmpty = 0.3f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trail") float GlowNoLine = 0.25f;
};

/** One line: a scout's loop-free route from the nest to the leaf it cut, plus the patch around that leaf. */
struct FColonyLine
{
	/** 1-based number shown to the player ("Line 2"). */
	int32 Number = 0;
	/** Scout (roster index) that laid it. */
	int32 Worker = INDEX_NONE;
	/** Nest to the scout's cutting spot. Route[0] is at the nest. */
	TArray<FTrailPoint> Route;
	/** The leaf the scout cut; the patch is anchored to it. */
	int32 CutLeaf = INDEX_NONE;
	/** Branch the scout stood on while cutting (the end of the route). */
	int32 CutBranch = INDEX_NONE;
	/** Fragment size (0..1) the scout cut; recruits on this line cut the same. */
	float Template = 0.5f;
	/** Average value multiplier of the patch's leaves. */
	float Richness = 1.f;
	/** Patch leaves, nearest first; the scout's leaf is first. */
	TArray<int32> Patch;
	/** Approach from the end of the route to each patch leaf's base (parallel to Patch; empty if not built yet). */
	TArray<TArray<FTrailPoint>> Approach;

	/** Pieces when the line started, the scout's own included. */
	int32 PatchTotal = 0;
	/** Pieces of template size still on the patch (not counting ones already reserved by recruits walking out). */
	int32 PatchRemaining = 0;
	int32 PiecesHome = 0;
	int32 PiecesLost = 0;
	float MassHome = 0.f;
	/** Recruits walking out to, working at, or queued for this line. */
	int32 Assigned = 0;
	/** Pieces promised to recruits on their way (taken off PatchRemaining when they leave). */
	int32 Reserved = 0;
	/** The scout's own piece has not reached the nest yet (counts as home or lost when it does). */
	bool bScoutPieceInFlight = false;

	bool IsLive() const { return PatchRemaining > 0; }
	/** 0 (stripped) .. 1 (untouched): drives the trail brightness and the HUD bar. */
	float Fullness() const { return PatchTotal > 0 ? FMath::Clamp(float(PatchRemaining + Reserved) / PatchTotal, 0.f, 1.f) : 0.f; }
	/** World position of the end of the line (the scout's cutting spot). */
	FVector EndPos() const { return Route.Num() ? Route.Last().Pos : FVector::ZeroVector; }
};

/** What the patch around a leaf is worth, for the read-out before the player commits. */
struct FPatchPreview
{
	int32 Leaf = INDEX_NONE;
	float Template = 0.f;
	TArray<int32> Leaves;
	int32 Pieces = 0;
	float Richness = 1.f;
};

/** A delivery at the nest, for the floating "+N". Times are real seconds (UWorld::GetRealTimeSeconds). */
struct FDeliveryPop
{
	FVector WorldPos = FVector::ZeroVector;
	float Mass = 0.f;
	double RealTime = 0.0;
	/** 0 for a lone delivery; counts up while deliveries arrive close together (the chime rises with it). */
	int32 Chain = 0;
};

UENUM()
enum class ERecruitState : uint8
{
	Nest,     // in the nest (trips left or not)
	Queued,   // assigned to a line, waiting for its turn to leave
	Out,      // walking the line and the approach to its leaf
	Cutting,  // cutting its piece
	Laden,    // walking home with a piece
	Empty,    // walking home without one (patch stripped on arrival)
	Fallen    // blown off: falling, then walking home over the ground
};

/**
 * The colony's automation: lines laid by scouts, the shared recruit pool that works them, and the fixed-step clock
 * that recruits and the wind run on. The director drives the day; this owns everything recruit-shaped.
 *
 * Every ant this system moves (recruits always, returning scouts during the harvest) and the wind clock advance in
 * fixed steps of Tuning.Step, at SpeedFactor steps' worth of sim time per real second, so a skipped harvest runs the
 * same simulation as a watched one.
 */
UCLASS()
class ANTTREEGAME_API ALineSystem : public AActor
{
	GENERATED_BODY()

public:
	ALineSystem();

	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Line")
	FLineTuning Tuning;

	void Init(ATreeActor* InTree, ATrailNetwork* InTrails, ANestActor* InNest, AWindSystem* InWind, ADayDirector* InDirector);

	/** New day: clears lines and recruits; the pool comes from the colony (Brood) unless overridden (Leaf.Pool). */
	void StartDay();

	/** A scout's piece is on its way home (sent home, or carried into the nest): start a line from its cut. */
	void AddLine(AAntPawn* Scout, bool bPieceAlreadyHome);
	/** The scout's piece reached the nest (true) or was lost to the wind (false). */
	void OnScoutPieceResolved(int32 Worker, bool bHome, float Mass);

	// Recruit callbacks from AAntPawn.
	void OnRecruitBlownOff(AAntPawn* Ant);

	/** Sim speed: 1 during shifts. The director sets HarvestSpeed or SkipSpeed. */
	void SetSpeedFactor(float Factor) { SpeedFactor = FMath::Max(Factor, 0.f); }
	float GetSpeedFactor() const { return SpeedFactor; }
	/** Ants the director hands over for the harvest (returning or blown-off scouts): stepped with the recruits. */
	void AddDrivenAnt(AAntPawn* Ant);

	/** Recruits are on (FDayTuning::bRecruits and the colony flag) and the pool is not empty. */
	bool IsActive() const;
	/** Anything left to do: a recruit out or queued, or one in the nest with trips left and a live line to go to. */
	bool HasWork() const;
	/** No recruit will move again today without a new line. */
	bool IsQuiet() const { return !HasWork(); }

	// Read-outs for the HUD, recap and camera.
	const TArray<FColonyLine>& GetLines() const { return Lines; }
	int32 NumLiveLines() const;
	int32 GetPoolSize() const { return Recruits.Num(); }
	/** Recruits in the nest with trips left and no live line to go to ("4 workers idle: lay a new line"). */
	int32 NumIdle() const;
	int32 NumOut() const;
	/** Trips left across the pool. */
	int32 TripsLeft() const;
	ERecruitState GetRecruitState(int32 Index) const { return Recruits.IsValidIndex(Index) ? Recruits[Index].State : ERecruitState::Nest; }
	/** The patch around a leaf at a template size (cached by leaf and size). False if the leaf is invalid. */
	bool GetPatchPreview(int32 Leaf, float Template, FPatchPreview& Out) const;
	/** Deliveries of the last few seconds (scouts and recruits), oldest first. */
	const TArray<FDeliveryPop>& GetRecentDeliveries() const { return Deliveries; }
	/** Pieces lost to the wind today, by recruits. */
	int32 GetRecruitLosses() const { return RecruitLosses; }
	/** Times a recruit cut from where it stood because it could not reach its leaf (logged, reported). */
	int32 GetFallbackCount() const { return Fallbacks; }

	/** Record a delivery (also used by the director for scout deposits): stats, "+N" pop, rising chime. */
	void RecordDelivery(const FVector& At, float Mass, int32 Line);

	/** Debug: force the pool size until the next StartDay with -1 (Leaf.Pool). */
	void SetPoolOverride(int32 N) { PoolOverride = N; }

	static ALineSystem* Get(const UObject* WorldContext);

private:
	struct FRecruit
	{
		TWeakObjectPtr<AAntPawn> Pawn;
		ERecruitState State = ERecruitState::Nest;
		int32 TripsLeft = 0;
		int32 Line = INDEX_NONE;
		int32 PatchSlot = INDEX_NONE; // index into the line's Patch / Approach
		double DepartAt = 0.0;        // sim time
		double OutSince = 0.0;        // sim time it left the nest
		double LegSince = -1.0;       // sim time it passed the end of the line (< 0: not yet)
		int32 LegStart = 0;           // route index where the approach begins
		int32 ReturnFrom = 0;         // route index to walk home from (where it stopped to cut)
		double StateSince = 0.0;      // sim time of the last state change
		FVector ProgressPos = FVector::ZeroVector; // fallen: where it last made headway, and when
		double ProgressTime = 0.0;
		TArray<FTrailPoint> Route;    // line route + approach
	};

	void SimStep(float Dt);
	void AssignIdle();
	void UpdateRecruit(int32 Index, float Dt);
	void Depart(int32 Index);
	void ArriveAtLeaf(int32 Index, bool bFallback);
	void SetState(FRecruit& R, ERecruitState NewState);
	/** Walk back down the route from ReturnFrom (Laden with a piece, Empty without). */
	void StartHome(FRecruit& R, bool bLaden);
	void StepAnts(float Dt);
	/** Back in the nest. bQuiet: it vanishes where it is (a stalled walk home) instead of walking into the entrance. */
	void FinishTrip(int32 Index, bool bUsedTrip, bool bQuiet = false);
	void ReleaseReservation(FRecruit& R);
	int32 ChooseLine() const;
	int32 ChoosePatchSlot(const FColonyLine& L) const;
	int32 PiecesOn(int32 Leaf, float Template) const;
	void RecountLine(FColonyLine& L);
	void RecountAll();
	void UpdateTrailGlow();
	int32 FindRecruit(const AAntPawn* Ant) const;
	AAntPawn* SpawnRecruitPawn();

	UPROPERTY(Transient) TObjectPtr<ATreeActor> Tree;
	UPROPERTY(Transient) TObjectPtr<ATrailNetwork> Trails;
	UPROPERTY(Transient) TObjectPtr<ANestActor> Nest;
	UPROPERTY(Transient) TObjectPtr<AWindSystem> Wind;
	UPROPERTY(Transient) TObjectPtr<ADayDirector> Director;

	TArray<FColonyLine> Lines;
	TArray<FRecruit> Recruits;
	TArray<TWeakObjectPtr<AAntPawn>> DrivenAnts;
	TArray<FDeliveryPop> Deliveries;
	/** Reserved pieces per leaf (recruits on their way). */
	TMap<int32, int32> LeafReserved;
	mutable TMap<uint64, FPatchPreview> PreviewCache;

	double SimTime = 0.0;
	double Accumulator = 0.0;
	double LastDepart = -1000.0;
	float SpeedFactor = 1.f;
	int32 PoolOverride = -1;
	float UsePerCut = 0.38f;
	float CutReach = 80.f;
	mutable double PreviewCacheTime = 0.0;
	int32 RecruitLosses = 0;
	int32 Fallbacks = 0;
	bool bGlowDirty = false;
	double LastDeliveryReal = -1000.0;
	int32 DeliveryChain = 0;
};
