#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "LeaflineAudioSubsystem.generated.h"

class UAudioComponent;
class USoundWave;
class USoundWaveProcedural;
class ULeaflineAudioSubsystem;
struct FLeaflinePCMData;

/** World-owned playback handle. Retain with UPROPERTY or TWeakObjectPtr; Stop is idempotent. Game thread only. */
UCLASS()
class ANTTREEGAME_API ULeaflineAudioLoop : public UObject
{
	GENERATED_BODY()

public:
	void SetVolume(float Volume);
	/** Positive pitches are clamped to 0.125..4. */
	void SetPitch(float Pitch);
	void Stop();

private:
	friend class ULeaflineAudioSubsystem;
	void OnFinished(UAudioComponent* FinishedComponent);
	void OnPlaybackPercent(const UAudioComponent* PlayingComponent, const USoundWave* PlayingWave, float Percent);

	UPROPERTY(Transient) TObjectPtr<UAudioComponent> Component;
	UPROPERTY(Transient) TObjectPtr<USoundWaveProcedural> Wave;
	TWeakObjectPtr<ULeaflineAudioSubsystem> Owner;
};

/** 2D audio from Content/Leafline/AudioRaw. World lifetime isolates PIE sessions and stops audio on travel. */
UCLASS()
class ANTTREEGAME_API ULeaflineAudioSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Sound is a file name without .wav. All public methods run on the game thread. */
	void PlayOneShot(FName Sound, float Volume = 1.f, float Pitch = 1.f);
	/** Returns nullptr on load/playback failure. A loop may start at volume zero. */
	ULeaflineAudioLoop* StartLoop(FName Sound, float Volume = 1.f);
	void StopAll();
	virtual void Deinitialize() override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	friend class ULeaflineAudioLoop;
	ULeaflineAudioLoop* CreateVoice(FName Sound, float Volume, float Pitch, bool bLooping);
	TSharedPtr<const FLeaflinePCMData, ESPMode::ThreadSafe> LoadPCM(FName Sound);
	void Release(ULeaflineAudioLoop* Voice);

	UPROPERTY(Transient) TArray<TObjectPtr<ULeaflineAudioLoop>> Active;
	TMap<FName, TSharedPtr<const FLeaflinePCMData, ESPMode::ThreadSafe>> Cache;
	bool bDeinitializing = false;
};
