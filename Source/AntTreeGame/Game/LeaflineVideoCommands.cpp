#include "Game/LeaflineShots.h"
#include "AntTreeGame.h"
#include "HAL/IConsoleManager.h"

namespace
{
	void Cinematic(const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() != 1 || (Args[0] != TEXT("0") && Args[0] != TEXT("1")))
		{
			UE_LOG(LogLeafline, Warning, TEXT("Usage: Leaf.Cinematic 0|1"));
			return;
		}
		if (ALeaflineShots* Shots = ALeaflineShots::Get(World)) Shots->SetCinematic(Args[0] == TEXT("1"));
	}

	void Shot(const TArray<FString>& Args, UWorld* World)
	{
		float Seconds = 8.f;
		if (Args.Num() < 1 || Args.Num() > 2 ||
			(Args.Num() == 2 && (!LexTryParseString(Seconds, *Args[1]) || !FMath::IsFinite(Seconds) || Seconds < 2.f || Seconds > 120.f)))
		{
			UE_LOG(LogLeafline, Warning, TEXT("Usage: Leaf.Shot orbit|crane|dolly|reveal|follow [seconds 2..120], or Leaf.Shot stop"));
			return;
		}
		if (ALeaflineShots* Shots = ALeaflineShots::Get(World))
		{
			const FString Name = Args[0].ToLower();
			if (Name == TEXT("stop")) Shots->Stop();
			else if (!Shots->Play(Name, Seconds)) UE_LOG(LogLeafline, Warning, TEXT("Shot did not start (available: orbit, crane, dolly, reveal, follow)."));
		}
	}

	void ShotList(const TArray<FString>& Args, UWorld* World)
	{
		TArray<FString> Names;
		for (const FString& Arg : Args) Names.Add(Arg.ToLower());
		bool bValid = !Names.IsEmpty() && Names.Num() <= 16;
		for (const FString& Name : Names) bValid &= ALeaflineShots::IsShotName(Name);
		if (!bValid)
		{
			UE_LOG(LogLeafline, Warning, TEXT("Usage: Leaf.ShotList <name> <name> ... (1..16 shots, each 8 real seconds; orbit, crane, dolly, reveal, follow)."));
			return;
		}
		if (ALeaflineShots* Shots = ALeaflineShots::Get(World))
			if (!Shots->PlayList(Names)) UE_LOG(LogLeafline, Warning, TEXT("Shot list could not start; start a day first."));
	}

	void Slomo(const TArray<FString>& Args, UWorld* World)
	{
		float Factor = 1.f;
		if (Args.Num() != 1 || !LexTryParseString(Factor, *Args[0]) || !FMath::IsFinite(Factor) || Factor < 0.05f || Factor > 4.f)
		{
			UE_LOG(LogLeafline, Warning, TEXT("Usage: Leaf.Slomo <factor 0.05..4>; 1 = normal."));
			return;
		}
		if (ALeaflineShots* Shots = ALeaflineShots::Get(World)) Shots->SetSlomo(Factor);
	}

	FAutoConsoleCommandWithWorldAndArgs CmdCinematic(TEXT("Leaf.Cinematic"), TEXT("Leaf.Cinematic 0|1: hide all HUD and damp the player camera for recording."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Cinematic));
	FAutoConsoleCommandWithWorldAndArgs CmdShot(TEXT("Leaf.Shot"), TEXT("Leaf.Shot orbit|crane|dolly|reveal|follow [seconds], or stop: an eased move, then restore the player view."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Shot));
	FAutoConsoleCommandWithWorldAndArgs CmdShotList(TEXT("Leaf.ShotList"), TEXT("Leaf.ShotList <name> <name> ...: play up to 16 shots, 8 real seconds each, then return to the player view."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ShotList));
	FAutoConsoleCommandWithWorldAndArgs CmdSlomo(TEXT("Leaf.Slomo"), TEXT("Leaf.Slomo <factor 0.05..4>: set global game time dilation; 1 = normal. No saves are changed."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Slomo));
}
