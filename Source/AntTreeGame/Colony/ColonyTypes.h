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
	Nest     // upgrade screen between days
};

/** One recorded point of a worker's path. */
struct FTrailPoint
{
	FVector Pos = FVector::ZeroVector;
	FVector Normal = FVector::UpVector;
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
};
