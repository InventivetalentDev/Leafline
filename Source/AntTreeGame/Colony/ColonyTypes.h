#pragma once

#include "CoreMinimal.h"
#include "ColonyTypes.generated.h"

UENUM(BlueprintType)
enum class EWorkerState : uint8
{
	Waiting,   // in the nest, not yet out today
	Out,       // being controlled (or walked off by the player)
	Returning, // replaying its path home, unattended
	Home,      // back in the nest
	BlownOff   // fell; walking home empty-handed
};

UENUM(BlueprintType)
enum class EDayPhase : uint8
{
	Starting,
	Title,   // title screen over the first worker at the nest
	Shift,
	Harvest, // every scout is home or on its way; recruits finish their trips at a faster sim speed (skippable)
	AllHome,
	Recap,
	EndCard, // shown once, after the day the colony reaches its third milestone
	Nest,    // upgrade screen between days
	Move     // card at the start of a day on a new tree (one tree per stage): "The colony moves to a tall spire"
};

/** One recorded point of a worker's path. */
struct FTrailPoint
{
	FVector Pos = FVector::ZeroVector;
	FVector Normal = FVector::UpVector;
};

/** A recorded route, for the colony save (yesterday's trails). */
USTRUCT()
struct FSavedTrail
{
	GENERATED_BODY()

	UPROPERTY(SaveGame) TArray<FVector> Points;
	UPROPERTY(SaveGame) TArray<FVector> Normals;
};

/** One colony stage's tree (one tree per stage). Index 0 is the first tree. */
USTRUCT(BlueprintType)
struct FStageTuning
{
	GENERATED_BODY()

	/** Multiplier on the shape's trunk height. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage") float TreeHeightMul = 1.f;
	/** Lowest leafy branch (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage") float LowestLeafyHeight = 2000.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage") float RichnessMul = 1.f;
	/** Multiplier on the wind's peak force. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage") float GustMul = 1.f;
	/** Leaf spacing multiplier at the lowest leafy branch, easing to 1 over SparseBlendHeight (FTreeParams). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage") float SparseSpacingMul = 12.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage") float SparseBlendHeight = 5500.f;
	/** Share of the spacing thinning also applied as whole low twigs dropped (FTreeParams::SparseTwigDrop). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage") float SparseTwigDrop = 1.f;

	FStageTuning() = default;
	FStageTuning(float InHeightMul, float InLeafy, float InRichness, float InGust, float InSparse, float InBlend)
		: TreeHeightMul(InHeightMul), LowestLeafyHeight(InLeafy), RichnessMul(InRichness), GustMul(InGust),
		  SparseSpacingMul(InSparse), SparseBlendHeight(InBlend) {}
};

/** Today's numbers, shown on the recap screen. */
USTRUCT(BlueprintType)
struct FDayStats
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Day") int32 Day = 1;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Day") float MassHauled = 0.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Day") int32 Fragments = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Day") int32 BlownOff = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Day") float TrailLaid = 0.f; // cm
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Day") float LargestFragment = 0.f;
	/** Pieces recruits brought home, and lost to the wind. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Day") int32 RecruitPieces = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Day") int32 RecruitLost = 0;
	/** Mass hauled by scouts themselves (the rest came from recruits). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Day") float ScoutMass = 0.f;
	/** Set at day end: the best day before this one, and whether this day beat it. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Day") float PreviousBest = 0.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Day") bool bNewBest = false;
};

/** Cutting and carrying numbers. Fragment size s is 0..1. */
USTRUCT(BlueprintType)
struct FCutTuning
{
	GENERATED_BODY()

	/** How close the ant's mouth must be to a leaf (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cut") float Reach = 80.f;
	/** Fragment size grown per second of holding (before upgrades). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cut") float CutRate = 0.28f;
	/** Releasing below this size cancels the cut. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cut") float MinSize = 0.08f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cut") float MassMin = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cut") float MassMax = 5.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cut") float DiameterMin = 30.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cut") float DiameterMax = 100.f;
	/** Share of a leaf one full-size cut removes; leaves run out after about three cuts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cut") float LeafUsePerCut = 0.38f;
	/** Sending home only works with a fragment, so a trip is never wasted by accident. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cut") bool bSendHomeNeedsFragment = true;
};

/** Day structure numbers. */
USTRUCT(BlueprintType)
struct FDayTuning
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day") int32 WorkersPerDay = 3;
	/** Path sample spacing (cm): half an ant length. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day") float PathSpacing = 10.f;
	/** How close the ant must be to a trail to ride it (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day") float RideReach = 60.f;
	/** Ground distance from the nest that counts as home (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day") float NestRadius = 220.f;
	/** Sun pitch (degrees below horizon) for the first and last shift of a day. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day") float SunPitchMorning = -22.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day") float SunPitchNoon = -58.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day") float SunPitchEvening = -14.f;
	/** Tree shape for the day: -1 by day (day 1 classic, then spreading, spire, leaning in turn), 0..3 to force one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day") int32 TreeShapeOverride = -1;
	/** Lowest leafy branch on day 1 and its rise per day (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day") float LeafyHeightDay1 = 2000.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day") float LeafyHeightPerDay = 700.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day") int32 BaseSeed = 1112;
	/** The line: recruits follow scouts' lines and strip their patches. Off: no recruits, no harvest, no Brood row, and
	 *  the smaller "by hand" milestones. Leaf.Recruits 0|1 toggles it while playing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day") bool bRecruits = true;
	/** One tree per colony stage (docs/briefs/one-tree.md): the tree and its bites are kept from day to day and the colony
	 *  moves at the second and third milestones. Off: a new tree every day with the rising leafy height, as before.
	 *  Leaf.OneTree 0|1 toggles it (from the next day). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day") bool bOneTree = true;
	/** When the last scout heads home and no recruits have trips, its walk home plays at the harvest's speed and can be
	 *  skipped (playtest: "fast-forward for 'worker x heads home'"). Off: it walks home at normal speed, as before. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day") bool bFastForwardHome = true;
	/** Leaf richness against height (FTreeParams::RichnessHeightScale, from the next tree growth; Leaf.Tune day
	 *  RichnessHeightScale): 0 the linear curve over the crown, as before. 40 (m), the proposed value: the lowest leaves
	 *  stay about as they are (x1.0-1.2 on tree 1), a layer or two up is worth about 1.5x (45 m) and 1.8x (65 m), high
	 *  leaves 2-2.5x, so a line laid higher pays (playtest, 14 Nov). AutoPlay reaches the end card after day 12 instead
	 *  of 16 (60: day 14, gentler; 25: day 11). Leaf.ReloadColony regrows the saved tree with a new value. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day") float RichnessHeightScale = 0.f;
	/** One tree on: fractional rise of the wind's peak force per day on the same tree. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day") float GustPerDayOnTree = 0.06f;
	/** One tree on: the trees in order; the last repeats. Shapes go old giant, spreading giant, tall spire, leaning elder. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day") TArray<FStageTuning> Stages = {
		FStageTuning(1.0f, 2000.f, 1.0f, 1.0f, 12.f, 5500.f),
		FStageTuning(1.4f, 2600.f, 1.3f, 1.2f, 8.f, 4500.f),
		FStageTuning(1.15f, 3200.f, 1.6f, 1.4f, 8.f, 4500.f),
	};
};
