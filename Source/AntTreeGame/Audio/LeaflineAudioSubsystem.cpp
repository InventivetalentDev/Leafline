#include "Audio/LeaflineAudioSubsystem.h"
#include "AntTreeGame.h"
#include "Components/AudioComponent.h"
#include "HAL/UnrealMemory.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Sound/SoundWaveProcedural.h"
#include "UObject/UObjectGlobals.h"

struct FLeaflinePCMData
{
	TArray<uint8> Samples;
	int32 Channels = 0;
};

namespace
{
	constexpr int32 LeaflineSampleRate = 48000;

	uint16 ReadLE16(const uint8* P) { return uint16(P[0]) | (uint16(P[1]) << 8); }
	uint32 ReadLE32(const uint8* P)
	{
		return uint32(P[0]) | (uint32(P[1]) << 8) | (uint32(P[2]) << 16) | (uint32(P[3]) << 24);
	}
	bool WaveTag(const uint8* P, const ANSICHAR* Tag) { return FMemory::Memcmp(P, Tag, 4) == 0; }

	/** Bounds checked RIFF chunks, including odd-size padding and chunks before/after fmt/data. */
	bool DecodeLeaflineWave(const TArray<uint8>& File, FLeaflinePCMData& Out)
	{
		if (File.Num() < 12) return false;
		const uint8* Bytes = File.GetData();
		if (!WaveTag(Bytes, "RIFF") || !WaveTag(Bytes + 8, "WAVE")) return false;
		const uint64 End = 8ull + ReadLE32(Bytes + 4);
		if (End < 12 || End > uint64(File.Num())) return false;
		bool bFormat = false;
		int32 DataOffset = INDEX_NONE;
		int32 DataSize = 0;
		uint64 Offset = 12;
		while (Offset + 8 <= End)
		{
			const uint8* Header = Bytes + Offset;
			const uint32 Size = ReadLE32(Header + 4);
			const uint64 Payload = Offset + 8;
			const uint64 Next = Payload + Size + (Size & 1u);
			if (Next > End) return false;
			if (WaveTag(Header, "fmt "))
			{
				if (bFormat || Size < 16) return false;
				const uint8* Format = Bytes + Payload;
				const uint16 Channels = ReadLE16(Format + 2);
				if (ReadLE16(Format) != 1 || (Channels != 1 && Channels != 2) ||
					ReadLE32(Format + 4) != LeaflineSampleRate || ReadLE16(Format + 14) != 16 ||
					ReadLE16(Format + 12) != Channels * 2 || ReadLE32(Format + 8) != LeaflineSampleRate * Channels * 2) return false;
				Out.Channels = Channels;
				bFormat = true;
			}
			else if (WaveTag(Header, "data"))
			{
				if (DataOffset != INDEX_NONE) return false;
				DataOffset = static_cast<int32>(Payload);
				DataSize = static_cast<int32>(Size);
			}
			Offset = Next;
		}
		if (Offset != End || !bFormat || DataOffset == INDEX_NONE || DataSize <= 0 || DataSize % (Out.Channels * 2) != 0) return false;
		Out.Samples.Append(Bytes + DataOffset, DataSize);
		return true;
	}
}

void ULeaflineAudioLoop::SetVolume(float Volume)
{
	if (IsValid(Component) && FMath::IsFinite(Volume)) Component->SetVolumeMultiplier(FMath::Max(0.f, Volume));
}

void ULeaflineAudioLoop::SetPitch(float Pitch)
{
	if (IsValid(Component) && FMath::IsFinite(Pitch) && Pitch > 0.f) Component->SetPitchMultiplier(FMath::Clamp(Pitch, 0.125f, 4.f));
}

void ULeaflineAudioLoop::Stop()
{
	UAudioComponent* Playing = Component;
	Component = nullptr;
	Wave = nullptr;
	if (IsValid(Playing))
	{
		Playing->OnAudioFinishedNative.RemoveAll(this);
		Playing->OnAudioPlaybackPercentNative.RemoveAll(this);
		Playing->Stop();
		Playing->DestroyComponent();
	}
	// Leave the wave's underflow delegate intact: the render thread may still be using it.
	if (Owner.IsValid()) Owner->Release(this);
	Owner.Reset();
}

void ULeaflineAudioLoop::OnFinished(UAudioComponent*) { Stop(); }

void ULeaflineAudioLoop::OnPlaybackPercent(const UAudioComponent*, const USoundWave*, float Percent)
{
	// Procedural waves emit silence after EOF. Stop after actual playback, not a game-time duration timer.
	if (Percent >= 1.f) Stop();
}

bool ULeaflineAudioSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void ULeaflineAudioSubsystem::Deinitialize()
{
	bDeinitializing = true;
	StopAll();
	Cache.Empty();
	Super::Deinitialize();
}

void ULeaflineAudioSubsystem::Release(ULeaflineAudioLoop* Voice) { Active.RemoveSwap(Voice); }

void ULeaflineAudioSubsystem::StopAll()
{
	while (Active.Num() > 0) Active.Last()->Stop();
}

TSharedPtr<const FLeaflinePCMData, ESPMode::ThreadSafe> ULeaflineAudioSubsystem::LoadPCM(FName Sound)
{
	if (const auto* Found = Cache.Find(Sound)) return *Found;
	const FString Name = Sound.ToString().ToLower();
	if (Sound.IsNone()) return nullptr;
	for (TCHAR C : Name)
	{
		if (!((C >= 'a' && C <= 'z') || (C >= '0' && C <= '9') || C == '_' || C == '-'))
		{
			UE_LOG(LogLeafline, Warning, TEXT("Invalid sound name '%s': use a file name without an extension."), *Name);
			return nullptr;
		}
	}
	const FString Path = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Leafline/AudioRaw"), Name + TEXT(".wavraw"));
	TArray<uint8> File;
	TSharedPtr<FLeaflinePCMData, ESPMode::ThreadSafe> PCM = MakeShared<FLeaflinePCMData, ESPMode::ThreadSafe>();
	if (!FFileHelper::LoadFileToArray(File, *Path) || !DecodeLeaflineWave(File, *PCM))
	{
		UE_LOG(LogLeafline, Warning, TEXT("Cannot load '%s': expected RIFF/WAVE 16-bit PCM mono/stereo at 48000 Hz."), *Path);
		return nullptr;
	}
	Cache.Add(Sound, PCM);
	return PCM;
}

void ULeaflineAudioSubsystem::PlayOneShot(FName Sound, float Volume, float Pitch)
{
	CreateVoice(Sound, Volume, Pitch, false);
}

ULeaflineAudioLoop* ULeaflineAudioSubsystem::StartLoop(FName Sound, float Volume)
{
	return CreateVoice(Sound, Volume, 1.f, true);
}

ULeaflineAudioLoop* ULeaflineAudioSubsystem::CreateVoice(FName Sound, float Volume, float Pitch, bool bLooping)
{
	if (bDeinitializing || !FMath::IsFinite(Volume) || !FMath::IsFinite(Pitch) || Pitch <= 0.f) return nullptr;
	const auto PCM = LoadPCM(Sound);
	if (!PCM) return nullptr;
	USoundWaveProcedural* Wave = NewObject<USoundWaveProcedural>(this);
	Wave->SetSampleRate(LeaflineSampleRate);
	Wave->NumChannels = PCM->Channels;
	Wave->SetNumFrames(PCM->Samples.Num() / (PCM->Channels * 2));
	Wave->Duration = float(PCM->Samples.Num()) / (PCM->Channels * 2 * LeaflineSampleRate);
	Wave->bLooping = bLooping;
	Wave->VirtualizationMode = EVirtualizationMode::PlayWhenSilent;
	// The native callback executes inline in GeneratePCMData. Only immutable PCM and its private cursor are captured.
	Wave->OnSoundWaveProceduralUnderflow.BindLambda([PCM, bLooping, Cursor = 0](USoundWaveProcedural* Stream, int32 SamplesNeeded) mutable
	{
		const int32 TargetBytes = FMath::Max(SamplesNeeded * 2, 16384 * PCM->Channels * 2);
		int32 Bytes = FMath::Max(0, TargetBytes - Stream->GetAvailableAudioByteCount());
		if (!bLooping) Bytes = FMath::Min(Bytes, PCM->Samples.Num() - Cursor);
		if (Bytes == 0) return;
		TArray<uint8> Chunk;
		Chunk.SetNumUninitialized(Bytes);
		for (int32 Written = 0; Written < Bytes;)
		{
			const int32 Copy = FMath::Min(Bytes - Written, PCM->Samples.Num() - Cursor);
			FMemory::Memcpy(Chunk.GetData() + Written, PCM->Samples.GetData() + Cursor, Copy);
			Cursor += Copy;
			Written += Copy;
			if (bLooping && Cursor == PCM->Samples.Num()) Cursor = 0;
		}
		Stream->QueueAudio(Chunk.GetData(), Chunk.Num());
	});
	// Prime before Play; after this call, only the render thread advances the cursor.
	Wave->OnSoundWaveProceduralUnderflow.Execute(Wave, 1024);
	UAudioComponent* Component = UGameplayStatics::CreateSound2D(this, Wave, FMath::Max(0.f, Volume), FMath::Clamp(Pitch, 0.125f, 4.f), 0.f, nullptr, false, false);
	if (!Component)
	{
		UE_LOG(LogLeafline, Warning, TEXT("Cannot create 2D playback for '%s' in this world."), *Sound.ToString());
		return nullptr;
	}
	ULeaflineAudioLoop* Voice = NewObject<ULeaflineAudioLoop>(this);
	Voice->Owner = this;
	Voice->Component = Component;
	Voice->Wave = Wave;
	Active.Add(Voice);
	Component->OnAudioFinishedNative.AddUObject(Voice, &ULeaflineAudioLoop::OnFinished);
	if (!bLooping) Component->OnAudioPlaybackPercentNative.AddUObject(Voice, &ULeaflineAudioLoop::OnPlaybackPercent);
	Component->Play();
	return Voice;
}
