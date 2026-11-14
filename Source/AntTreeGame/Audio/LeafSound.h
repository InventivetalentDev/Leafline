#pragma once

#include "CoreMinimal.h"
#include "Audio/LeaflineAudioSubsystem.h"
#include "Engine/World.h"

/** Short helpers so gameplay code can play the synthesised sounds in one line. */
namespace LeafSound
{
	inline ULeaflineAudioSubsystem* Get(const UObject* Context)
	{
		UWorld* World = Context ? Context->GetWorld() : nullptr;
		return World ? World->GetSubsystem<ULeaflineAudioSubsystem>() : nullptr;
	}

	inline void Play(const UObject* Context, const TCHAR* Name, float Volume = 1.f, float Pitch = 1.f)
	{
		if (ULeaflineAudioSubsystem* S = Get(Context)) S->PlayOneShot(FName(Name), Volume, Pitch);
	}

	/** Placed in the world at At (fades with distance from the listener). */
	inline void PlayAt(const UObject* Context, const TCHAR* Name, const FVector& At, float Volume = 1.f, float Pitch = 1.f)
	{
		if (ULeaflineAudioSubsystem* S = Get(Context)) S->PlayOneShotAt(FName(Name), At, Volume, Pitch);
	}

	inline ULeaflineAudioLoop* Loop(const UObject* Context, const TCHAR* Name, float Volume)
	{
		ULeaflineAudioSubsystem* S = Get(Context);
		return S ? S->StartLoop(FName(Name), Volume) : nullptr;
	}

	inline void Stop(TObjectPtr<ULeaflineAudioLoop>& Loop)
	{
		if (Loop) Loop->Stop();
		Loop = nullptr;
	}
}
