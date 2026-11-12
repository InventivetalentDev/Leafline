#include "Game/LeaflineGameMode.h"
#include "AntTreeGame.h"
#include "Tree/TreeActor.h"
#include "Ant/AntPawn.h"
#include "Ant/AntMovementComponent.h"
#include "Game/LeaflinePlayerController.h"
#include "UI/LeaflineHUD.h"
#include "EngineUtils.h"

ALeaflineGameMode::ALeaflineGameMode()
{
	DefaultPawnClass = AAntPawn::StaticClass();
	PlayerControllerClass = ALeaflinePlayerController::StaticClass();
	HUDClass = ALeaflineHUD::StaticClass();
	TreeClass = ATreeActor::StaticClass();
}

void ALeaflineGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	// The pawn exists before BeginPlay, so the tree it walks on is created here.
	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Tree = GetWorld()->SpawnActor<ATreeActor>(TreeClass ? *TreeClass : ATreeActor::StaticClass(), FTransform::Identity, Params);
	if (Tree)
	{
		Tree->Grow(StartSeed);
	}
	UE_LOG(LogLeafline, Log, TEXT("LeaflineGameMode: InitGame on %s, seed %d"), *MapName, StartSeed);
}

void ALeaflineGameMode::RestartPlayer(AController* NewPlayer)
{
	if (!NewPlayer || NewPlayer->IsPendingKillPending()) return;
	FTransform Spawn = Tree ? Tree->GetNestTransform(0) : FTransform::Identity;
	Spawn.AddToTranslation(FVector(0.0, 0.0, 40.0));
	RestartPlayerAtTransform(NewPlayer, Spawn);
}

void ALeaflineGameMode::NewDay(int32 Seed)
{
	if (!Tree) return;
	Tree->Grow(Seed);
	const FTransform Nest = Tree->GetNestTransform(0);
	for (TActorIterator<AAntPawn> It(GetWorld()); It; ++It)
	{
		if (UAntMovementComponent* M = It->GetAntMovement())
		{
			M->TeleportTo(Nest.GetLocation() + FVector(0.0, 0.0, 40.0), Nest.GetRotation().GetForwardVector());
		}
	}
	UE_LOG(LogLeafline, Log, TEXT("New day: seed %d"), Seed);
}
