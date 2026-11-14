#include "Game/LeaflineGameMode.h"
#include "AntTreeGame.h"
#include "Tree/TreeActor.h"
#include "Ant/AntPawn.h"
#include "Colony/TrailNetwork.h"
#include "Colony/NestActor.h"
#include "World/DayDirector.h"
#include "World/WindSystem.h"
#include "Colony/LineSystem.h"
#include "World/BirdSystem.h"
#include "Game/LeaflinePlayerController.h"
#include "UI/LeaflineHUD.h"
#include "GameFramework/PlayerController.h"
#include "Game/LeaflineGameInstance.h"

ALeaflineGameMode::ALeaflineGameMode()
{
	DefaultPawnClass = AAntPawn::StaticClass();
	PlayerControllerClass = ALeaflinePlayerController::StaticClass();
	HUDClass = ALeaflineHUD::StaticClass();
	TreeClass = ATreeActor::StaticClass();
	DirectorClass = ADayDirector::StaticClass();
}

void ALeaflineGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	// Everything the first worker depends on exists before any pawn does. The director grows the tree on day start.
	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	UWorld* World = GetWorld();
	Tree = World->SpawnActor<ATreeActor>(TreeClass ? *TreeClass : ATreeActor::StaticClass(), FTransform::Identity, Params);
	Trails = World->SpawnActor<ATrailNetwork>(ATrailNetwork::StaticClass(), FTransform::Identity, Params);
	Nest = World->SpawnActor<ANestActor>(ANestActor::StaticClass(), FTransform::Identity, Params);
	Wind = World->SpawnActor<AWindSystem>(AWindSystem::StaticClass(), FTransform::Identity, Params);
	Director = World->SpawnActor<ADayDirector>(DirectorClass ? *DirectorClass : ADayDirector::StaticClass(), FTransform::Identity, Params);
	Lines = World->SpawnActor<ALineSystem>(ALineSystem::StaticClass(), FTransform::Identity, Params);
	if (Director)
	{
		if (DefaultPawnClass && DefaultPawnClass->IsChildOf(AAntPawn::StaticClass())) Director->WorkerClass = *DefaultPawnClass;
		Director->Init(Tree, Trails, Nest, Wind, Lines);
	}
	if (Lines) Lines->Init(Tree, Trails, Nest, Wind, Director);
	// The bird (docs/briefs/bird.md): stepped by the line system, so it exists before the first StartDay.
	if (ABirdSystem* Bird = World->SpawnActor<ABirdSystem>(ABirdSystem::StaticClass(), FTransform::Identity, Params))
	{
		Bird->Init(Tree, Wind, Director, Lines);
		if (Lines) Lines->SetBird(Bird);
	}
	UE_LOG(LogLeafline, Log, TEXT("LeaflineGameMode: InitGame on %s"), *MapName);
}

void ALeaflineGameMode::RestartPlayer(AController* NewPlayer)
{
	if (!NewPlayer || NewPlayer->IsPendingKillPending() || !Director) return;
	Director->SetPlayer(Cast<APlayerController>(NewPlayer));
	if (!bDayStarted)
	{
		bDayStarted = true;
		const ULeaflineGameInstance* GI = Cast<ULeaflineGameInstance>(GetGameInstance());
		Director->StartDay(GI ? GI->GetDay() : 1);
		Director->ShowTitle();
	}
}

void ALeaflineGameMode::NewDay(int32 Seed)
{
	if (!Director) return;
	Director->StartDay(Director->GetDay(), Seed);
	UE_LOG(LogLeafline, Log, TEXT("New day: seed %d"), Seed);
}
