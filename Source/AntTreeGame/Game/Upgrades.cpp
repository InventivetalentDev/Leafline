#include "Game/Upgrades.h"

namespace
{
	const FLeaflineUpgradeInfo Table[] =
	{
		{ TEXT("Mandibles"), TEXT("Cut faster (recruits too)."),         { 1.f, 1.25f, 1.5f, 1.8f, 2.15f, 2.55f, 3.f } },
		{ TEXT("Strength"),  TEXT("Big pieces slow you down less."),     { 1.f, 0.75f, 0.55f, 0.4f, 0.3f, 0.22f, 0.16f } },
		{ TEXT("Grip"),      TEXT("Hold on in stronger gusts (recruits too)."),  { 1.f, 1.25f, 1.5f, 1.8f, 2.15f, 2.55f, 3.f } },
		{ TEXT("Stamina"),   TEXT("Sprint for longer."),                 { 1.f, 0.8f, 0.65f, 0.5f, 0.4f, 0.32f, 0.25f } },
		{ TEXT("Brood"),     TEXT("More recruits to fetch pieces along your trails."),  { 0.f, 2.f, 3.f, 4.f, 6.f, 8.f, 10.f } }
	};
	// Recommended by Tools/econ_sim.py; source defaults and estimates are in docs/economy.md.
	const int32 Costs[] = { 14, 32, 68, 144, 304, 640 };
	const float MilestonesWithRecruits[] = { 20.f, 95.f, 240.f };
	const float MilestonesByHand[] = { 5.f, 30.f, 80.f };
	// One tree per stage (FDayTuning::bOneTree): placeholders until Tools/econ_sim.py retunes them.
	const float MilestonesOneTreeWithRecruits[] = { 25.f, 155.f, 270.f };
	const float MilestonesOneTreeByHand[] = { 5.f, 60.f, 95.f };
	static_assert(UE_ARRAY_COUNT(Table) == LeaflineUpgrades::Count);
	static_assert(UE_ARRAY_COUNT(Costs) == LeaflineUpgrades::MaxLevel);
	static_assert(UE_ARRAY_COUNT(MilestonesWithRecruits) == LeaflineUpgrades::MilestoneCount);
	static_assert(UE_ARRAY_COUNT(MilestonesByHand) == LeaflineUpgrades::MilestoneCount);
	static_assert(UE_ARRAY_COUNT(MilestonesOneTreeWithRecruits) == LeaflineUpgrades::MilestoneCount);
	static_assert(UE_ARRAY_COUNT(MilestonesOneTreeByHand) == LeaflineUpgrades::MilestoneCount);
	/** Leaf.Milestones (session only): the one-tree-with-recruits set while the third value is above zero. */
	float MilestoneOverride[] = { 0.f, 0.f, 0.f };
	static_assert(UE_ARRAY_COUNT(MilestoneOverride) == LeaflineUpgrades::MilestoneCount);
}

const FLeaflineUpgradeInfo* LeaflineUpgrades::Get(int32 Index)
{
	return Index >= 0 && Index < Count ? &Table[Index] : nullptr;
}

int32 LeaflineUpgrades::GetNextCost(int32 CurrentLevel)
{
	return CurrentLevel >= 0 && CurrentLevel < MaxLevel ? Costs[CurrentLevel] : 0;
}

float LeaflineUpgrades::GetEffect(int32 Index, int32 Level)
{
	const FLeaflineUpgradeInfo* Info = Get(Index);
	return Info ? Info->Effects[FMath::Clamp(Level, 0, MaxLevel)] : 0.f;
}

float LeaflineUpgrades::GetMilestoneThreshold(int32 Index, bool bRecruits, bool bOneTree)
{
	if (Index < 0 || Index >= MilestoneCount) return 0.f;
	if (bOneTree && bRecruits && HasMilestoneOverride()) return MilestoneOverride[Index];
	if (bOneTree) return bRecruits ? MilestonesOneTreeWithRecruits[Index] : MilestonesOneTreeByHand[Index];
	return bRecruits ? MilestonesWithRecruits[Index] : MilestonesByHand[Index];
}

void LeaflineUpgrades::SetMilestoneOverride(float First, float Second, float Third)
{
	MilestoneOverride[0] = FMath::Max(First, 0.f);
	MilestoneOverride[1] = FMath::Max(Second, 0.f);
	MilestoneOverride[2] = FMath::Max(Third, 0.f);
}

bool LeaflineUpgrades::HasMilestoneOverride()
{
	return MilestoneOverride[2] > 0.f;
}
