#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "LeaflineGameMode.generated.h"

class ATreeActor;
class ATrailNetwork;
class ANestActor;
class ADayDirector;

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
	TSubclassOf<ADayDirector> DirectorClass;

	/** Regrows the tree from a seed for the current day and starts it over. */
	void NewDay(int32 Seed);

	ATreeActor* GetTree() const { return Tree; }
	ADayDirector* GetDirector() const { return Director; }

private:
	UPROPERTY(Transient) TObjectPtr<ATreeActor> Tree;
	UPROPERTY(Transient) TObjectPtr<ATrailNetwork> Trails;
	UPROPERTY(Transient) TObjectPtr<ANestActor> Nest;
	UPROPERTY(Transient) TObjectPtr<ADayDirector> Director;
	UPROPERTY(Transient) TObjectPtr<class AWindSystem> Wind;
	bool bDayStarted = false;
};
