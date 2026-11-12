#include "Game/LeaflineGameInstance.h"
#include "AntTreeGame.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CString.h"

namespace
{
	ULeaflineGameInstance* ColonyFor(UWorld* World)
	{
		ULeaflineGameInstance* Colony = World ? World->GetGameInstance<ULeaflineGameInstance>() : nullptr;
		if (!Colony) UE_LOG(LogLeafline, Warning, TEXT("No ULeaflineGameInstance in the command's world."));
		return Colony;
	}

	FAutoConsoleCommandWithWorldAndArgs CmdGive(
		TEXT("Leaf.Give"), TEXT("Leaf.Give <mass>: add positive mass to balance and lifetime haul, then save."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() != 1 || !Args[0].IsNumeric())
			{
				UE_LOG(LogLeafline, Warning, TEXT("Usage: Leaf.Give <positive mass>"));
				return;
			}
			if (ULeaflineGameInstance* Colony = ColonyFor(World))
			{
				if (Colony->AddHaul(FCString::Atof(*Args[0]))) Colony->SaveNow();
				else UE_LOG(LogLeafline, Warning, TEXT("Leaf.Give requires positive finite mass within the balance range."));
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdBuy(
		TEXT("Leaf.Buy"), TEXT("Leaf.Buy <0..4>: Mandibles, Strength, Grip, Stamina, Brood."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			TCHAR* End = nullptr;
			const int64 Index = Args.Num() == 1 ? FCString::Strtoi64(*Args[0], &End, 10) : -1;
			if (!End || End == *Args[0] || *End != 0 || Index < 0 || Index >= LeaflineUpgrades::Count)
			{
				UE_LOG(LogLeafline, Warning, TEXT("Usage: Leaf.Buy <0..4>; Leaf.Upgrades lists the indices."));
				return;
			}
			if (ULeaflineGameInstance* Colony = ColonyFor(World))
				if (!Colony->Buy(static_cast<int32>(Index)))
					UE_LOG(LogLeafline, Warning, TEXT("Purchase failed: unaffordable, max level, or save write failed."));
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdResetSave(
		TEXT("Leaf.ResetSave"), TEXT("Leaf.ResetSave: overwrite the selected -LeafSlot (default Leafline). Active day continues."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() != 0)
			{
				UE_LOG(LogLeafline, Warning, TEXT("Usage: Leaf.ResetSave"));
				return;
			}
			if (ULeaflineGameInstance* Colony = ColonyFor(World)) Colony->ResetSave();
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdUpgrades(
		TEXT("Leaf.Upgrades"), TEXT("Leaf.Upgrades: list upgrade indices, levels, next costs, and effects."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() != 0)
			{
				UE_LOG(LogLeafline, Warning, TEXT("Usage: Leaf.Upgrades"));
				return;
			}
			if (ULeaflineGameInstance* Colony = ColonyFor(World))
			{
				UE_LOG(LogLeafline, Log, TEXT("Mass %.2f; total hauled %.2f; day %d; milestones %d/%d. Next cost 0 = MAX."),
					Colony->GetLeafMass(), Colony->GetTotalHauled(), Colony->GetDay(), Colony->GetReachedMilestoneCount(), LeaflineUpgrades::MilestoneCount);
				UE_LOG(LogLeafline, Log, TEXT("Recruits %s; pool %d; best day %.2f; milestone thresholds %.0f / %.0f / %.0f."),
					Colony->AreRecruitsEnabled() ? TEXT("on") : TEXT("off"), Colony->GetRecruitPool(), Colony->GetBestDayHaul(),
					LeaflineUpgrades::GetMilestoneThreshold(0, Colony->AreRecruitsEnabled()),
					LeaflineUpgrades::GetMilestoneThreshold(1, Colony->AreRecruitsEnabled()),
					LeaflineUpgrades::GetMilestoneThreshold(2, Colony->AreRecruitsEnabled()));
				for (int32 Index = 0; Index < LeaflineUpgrades::Count; ++Index)
				{
					const FLeaflineUpgradeInfo* Info = LeaflineUpgrades::Get(Index);
					const int32 Level = Colony->GetUpgradeLevel(Index);
					UE_LOG(LogLeafline, Log, TEXT("%d %s: level %d/%d, next cost %d, effect %.2f. %s"),
						Index, Info->Name, Level, LeaflineUpgrades::MaxLevel, Colony->GetUpgradeCost(Index), LeaflineUpgrades::GetEffect(Index, Level), Info->Description);
				}
			}
		}));
}
