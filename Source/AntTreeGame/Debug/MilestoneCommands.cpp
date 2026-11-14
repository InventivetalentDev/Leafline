// Leaf.Milestones: try another milestone set without a code change (the climb option: a steeper richness curve hauls more,
// so the milestones may need raising; see docs/economy.md). Session only.
#include "AntTreeGame.h"
#include "Game/LeaflineGameInstance.h"
#include "Game/Upgrades.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

namespace
{
	FAutoConsoleCommandWithWorldAndArgs CmdMilestones(
		TEXT("Leaf.Milestones"),
		TEXT("Leaf.Milestones [first second third | 0]: replace the milestone set of the default game (one tree per stage, recruits on) for this session; 0 restores the built-in set; no argument logs the current thresholds. The tree moves come at the second and third milestones, the end card at the third."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() == 1 && FCString::Atof(*Args[0]) <= 0.f)
			{
				LeaflineUpgrades::SetMilestoneOverride(0.f, 0.f, 0.f);
			}
			else if (Args.Num() == 3)
			{
				const float A = FCString::Atof(*Args[0]), B = FCString::Atof(*Args[1]), C = FCString::Atof(*Args[2]);
				if (!(A > 0.f && B > A && C > B))
				{
					UE_LOG(LogLeafline, Warning, TEXT("Leaf.Milestones: give three rising values above 0, e.g. Leaf.Milestones 25 240 390"));
					return;
				}
				LeaflineUpgrades::SetMilestoneOverride(A, B, C);
			}
			else if (Args.Num() != 0)
			{
				UE_LOG(LogLeafline, Warning, TEXT("Usage: Leaf.Milestones [first second third | 0]"));
				return;
			}
			const ULeaflineGameInstance* GI = World ? World->GetGameInstance<ULeaflineGameInstance>() : nullptr;
			UE_LOG(LogLeafline, Log, TEXT("Leaf.Milestones: one tree with recruits %.0f / %.0f / %.0f (%s); now in use %.0f / %.0f / %.0f, %d reached at %.1f hauled"),
				LeaflineUpgrades::GetMilestoneThreshold(0, true, true), LeaflineUpgrades::GetMilestoneThreshold(1, true, true),
				LeaflineUpgrades::GetMilestoneThreshold(2, true, true), LeaflineUpgrades::HasMilestoneOverride() ? TEXT("override") : TEXT("built in"),
				GI ? GI->GetMilestoneThreshold(0) : 0.f, GI ? GI->GetMilestoneThreshold(1) : 0.f, GI ? GI->GetMilestoneThreshold(2) : 0.f,
				GI ? GI->GetReachedMilestoneCount() : 0, GI ? GI->GetTotalHauled() : 0.f);
		}));
}
