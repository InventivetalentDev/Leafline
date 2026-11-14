// Console commands for the in-world wind cues (World/WindCues.*, docs/briefs/in-world-cues.md).
#include "AntTreeGame.h"
#include "World/WindCues.h"
#include "World/WindSystem.h"
#include "Ant/AntCamera.h"
#include "Game/LeaflinePlayerController.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

namespace
{
	FAutoConsoleCommandWithWorldAndArgs CmdCues(
		TEXT("Leaf.Cues"),
		TEXT("Leaf.Cues [0|1]: in-world wind cues (gust front, the ant's air, grip dust) on (default) or off (the old streaks only); no argument toggles. Tuning: Leaf.Tune wind Cue*."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld*)
		{
			WindCues::SetEnabled(Args.Num() > 0 ? FCString::Atoi(*Args[0]) != 0 : !WindCues::IsEnabled());
			UE_LOG(LogLeafline, Log, TEXT("Leaf.Cues %d"), WindCues::IsEnabled() ? 1 : 0);
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdCuesStat(
		TEXT("Leaf.CuesStat"),
		TEXT("Leaf.CuesStat [seconds]: log the wind cues' game-thread cost (average and worst frame) over the next seconds (default 10)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld*)
		{
			const float Seconds = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 10.f;
			WindCues::StartStat(Seconds);
			UE_LOG(LogLeafline, Log, TEXT("Leaf.CuesStat %.1f"), Seconds);
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdCuesLog(
		TEXT("Leaf.CuesLog"),
		TEXT("Leaf.CuesLog [0|1]: log the wind phase, where the gust front is, the air at the ant and its calm pocket every 0.5 s; no argument toggles."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld*)
		{
			WindCues::SetLogging(Args.Num() > 0 ? FCString::Atoi(*Args[0]) != 0 : !WindCues::IsLogging());
			UE_LOG(LogLeafline, Log, TEXT("Leaf.CuesLog %d"), WindCues::IsLogging() ? 1 : 0);
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdWindAim(
		TEXT("Leaf.WindAim"),
		TEXT("Leaf.WindAim <deg> <pitch>: aim the camera relative to the wind (0 looks downwind, 180 upwind into the coming gust, 90 or -90 across it), for repeatable wind screenshots."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ALeaflinePlayerController* PC = World ? Cast<ALeaflinePlayerController>(World->GetFirstPlayerController()) : nullptr;
			const AWindSystem* Wind = AWindSystem::Get(World);
			if (!PC || !PC->GetAntCamera() || !Wind || Args.Num() < 2) return;
			const float WindYaw = Wind->GetDirection().Rotation().Yaw;
			const float Yaw = WindYaw + FCString::Atof(*Args[0]);
			PC->GetAntCamera()->DebugAim(Yaw, FCString::Atof(*Args[1]));
			UE_LOG(LogLeafline, Log, TEXT("Leaf.WindAim %s %s: wind toward yaw %.0f, camera yaw %.0f"), *Args[0], *Args[1], WindYaw, Yaw);
		}));
}
