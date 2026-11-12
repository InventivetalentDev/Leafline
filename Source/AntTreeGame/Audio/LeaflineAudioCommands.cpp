#include "Audio/LeaflineAudioSubsystem.h"
#include "AntTreeGame.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CString.h"

namespace
{
	FAutoConsoleCommandWithWorldAndArgs CmdLeafSound(
		TEXT("Leaf.Sound"), TEXT("Leaf.Sound <name> [volume] [pitch]: play a WAV once from Content/Leafline/AudioRaw."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() < 1 || Args.Num() > 3 || (Args.Num() > 1 && !Args[1].IsNumeric()) || (Args.Num() > 2 && !Args[2].IsNumeric()))
			{
				UE_LOG(LogLeafline, Warning, TEXT("Usage: Leaf.Sound <name without .wav> [volume] [positive pitch]"));
				return;
			}
			const float Volume = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 1.f;
			const float Pitch = Args.Num() > 2 ? FCString::Atof(*Args[2]) : 1.f;
			if (!FMath::IsFinite(Volume) || !FMath::IsFinite(Pitch) || Volume < 0.f || Pitch <= 0.f)
			{
				UE_LOG(LogLeafline, Warning, TEXT("Leaf.Sound needs finite volume >= 0 and pitch > 0 (clamped to 0.125..4)."));
				return;
			}
			ULeaflineAudioSubsystem* Audio = World ? World->GetSubsystem<ULeaflineAudioSubsystem>() : nullptr;
			if (!Audio)
			{
				UE_LOG(LogLeafline, Warning, TEXT("Leaf.Sound needs a game or PIE world."));
				return;
			}
			Audio->PlayOneShot(FName(*Args[0]), Volume, Pitch);
		}));
}
