#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "LeaflineGameMode.generated.h"

class ATreeActor;

UCLASS()
class ANTTREEGAME_API ALeaflineGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ALeaflineGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void RestartPlayer(AController* NewPlayer) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leafline")
	TSubclassOf<ATreeActor> TreeClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leafline")
	int32 StartSeed = 1112;

	/** Regrows the tree from a seed and puts the player's ant at the nest. */
	void NewDay(int32 Seed);

	ATreeActor* GetTree() const { return Tree; }

private:
	UPROPERTY(Transient)
	TObjectPtr<ATreeActor> Tree;
};
