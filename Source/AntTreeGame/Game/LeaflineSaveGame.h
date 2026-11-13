#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Tree/TreeTypes.h"
#include "Colony/ColonyTypes.h"
#include "LeaflineSaveGame.generated.h"

/** One colony, saved at user index zero; -LeafSlot selects a test slot. */
UCLASS()
class ANTTREEGAME_API ULeaflineSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	ULeaflineSaveGame();
	static constexpr int32 CurrentVersion = 3;

	/** Tagged save serialisation skips values equal to the class default, so files from versions 1 and 2 (whose default
	 *  was their own version) hold no Version at all and load as 0. A new colony sets CurrentVersion explicitly. */
	UPROPERTY(SaveGame) int32 Version = 0;
	UPROPERTY(SaveGame) float LeafMass = 0.f;
	UPROPERTY(SaveGame) float TotalHauled = 0.f;
	UPROPERTY(SaveGame) float BestDayHaul = 0.f;
	/** ELeaflineUpgrade order, levels 0..6. */
	UPROPERTY(SaveGame) TArray<int32> UpgradeLevels;
	UPROPERTY(SaveGame) int32 Day = 1;
	UPROPERTY(SaveGame) int32 BaseSeed = 1112;
	/** Set once the end card has been shown. Until then, a colony past the last milestone shows it at the next recap. */
	UPROPERTY(SaveGame) bool bEndCardSeen = false;

	// One tree per stage (version 3). Written at day end together with the day's haul.
	/** The colony's tree, 0 for the first. */
	UPROPERTY(SaveGame) int32 Stage = 0;
	/** Seed of the stage's tree; 0 means no tree grown yet (a fresh colony or an older save). */
	UPROPERTY(SaveGame) int32 TreeSeed = 0;
	/** First day on this tree. */
	UPROPERTY(SaveGame) int32 StageStartDay = 1;
	/** The tree's leaf count when Bitten was saved; the bites are dropped if a regrown tree has another count. */
	UPROPERTY(SaveGame) int32 TreeLeafCount = 0;
	UPROPERTY(SaveGame) TArray<FSavedLeaf> Bitten;
	/** Routes of the day YesterdayDay, shown faint on the next day only. */
	UPROPERTY(SaveGame) TArray<FSavedTrail> Yesterday;
	UPROPERTY(SaveGame) int32 YesterdayDay = 0;

	/** Repairs malformed values after loading the supported version. */
	void Sanitize();
};
