// Console commands for the bird (World/BirdSystem.*, docs/briefs/bird.md). Leaf.Tune bird is in LeafDebugCommands.cpp.
#include "AntTreeGame.h"
#include "World/BirdSystem.h"
#include "Ant/AntPawn.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"

namespace
{
	ABirdSystem* BirdOf(UWorld* World)
	{
		ABirdSystem* B = World ? ABirdSystem::Get(World) : nullptr;
		if (!B) UE_LOG(LogLeafline, Warning, TEXT("Bird commands need a running game"));
		return B;
	}

	FAutoConsoleCommandWithWorldAndArgs CmdBird(
		TEXT("Leaf.Bird"),
		TEXT("Leaf.Bird 0|1: the bird (off: no bird, no schedule, no HUD line, no hint, no drop action). No argument toggles."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ABirdSystem* B = BirdOf(World)) B->SetBirdOn(Args.Num() > 0 ? FCString::Atoi(*Args[0]) != 0 : !B->IsBirdOn());
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdBait(
		TEXT("Leaf.Bait"),
		TEXT("Leaf.Bait 0|1: dropped pieces as bait (the drop action and the bird preferring a piece on exposed bark). No argument toggles."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ABirdSystem* B = BirdOf(World)) B->SetBaitOn(Args.Num() > 0 ? FCString::Atoi(*Args[0]) != 0 : !B->Tuning.bBait);
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdBirdNow(
		TEXT("Leaf.BirdNow"),
		TEXT("Leaf.BirdNow: the bird picks its target and dives now (during a shift; ignores the schedule and the day's swoop count)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (ABirdSystem* B = BirdOf(World)) B->DiveNow();
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdBirdInfo(
		TEXT("Leaf.BirdInfo"),
		TEXT("Leaf.BirdInfo: log the bird's state, today's swoops and their outcomes."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (ABirdSystem* B = BirdOf(World)) B->LogInfo();
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdDrop(
		TEXT("Leaf.Drop"),
		TEXT("Leaf.Drop: the controlled ant drops its piece as bait, as if the drop button were pressed (only while the bird is about)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			ABirdSystem* B = BirdOf(World);
			APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
			AAntPawn* Ant = PC ? Cast<AAntPawn>(PC->GetPawn()) : nullptr;
			if (B && !B->DropPiece(Ant))
			{
				UE_LOG(LogLeafline, Log, TEXT("Leaf.Drop: not now (needs bait on, the bird circling or diving, and the controlled ant carrying a piece)"));
			}
		}));
}
