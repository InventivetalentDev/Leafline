#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "LeaflineSaveGame.generated.h"

/** One colony, saved in the Leafline slot at user index zero. */
UCLASS()
class ANTTREEGAME_API ULeaflineSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	ULeaflineSaveGame();
	static constexpr int32 CurrentVersion = 1;

	UPROPERTY(SaveGame) int32 Version = CurrentVersion;
	UPROPERTY(SaveGame) float LeafMass = 0.f;
	UPROPERTY(SaveGame) float TotalHauled = 0.f;
	/** ELeaflineUpgrade order, levels 0..3. */
	UPROPERTY(SaveGame) TArray<int32> UpgradeLevels;
	UPROPERTY(SaveGame) int32 Day = 1;
	UPROPERTY(SaveGame) int32 BaseSeed = 1112;
	/** Set once the end card has been shown. Until then, a colony past the last milestone shows it at the next recap. */
	UPROPERTY(SaveGame) bool bEndCardSeen = false;

	/** Repairs malformed values after loading the supported version. */
	void Sanitize();
};
