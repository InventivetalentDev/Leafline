#include "Game/Upgrades.h"

namespace
{
	const FLeaflineUpgradeInfo Table[] =
	{
		{ TEXT("Mandibles"), TEXT("Cut faster."),                          { 1.f, 1.25f, 1.5f, 1.8f } },
		{ TEXT("Strength"),  TEXT("Reduce carry slowdown."),               { 1.f, 0.75f, 0.55f, 0.4f } },
		{ TEXT("Grip"),      TEXT("Withstand stronger gusts."),            { 1.f, 1.25f, 1.5f, 1.8f } },
		{ TEXT("Stamina"),   TEXT("Reduce sprint stamina drain."),         { 1.f, 0.8f, 0.65f, 0.5f } },
		{ TEXT("Brood"),     TEXT("One more worker per day per level."),   { 0.f, 1.f, 2.f, 3.f } }
	};
	const int32 Costs[] = { 6, 14, 24 };
	const float Milestones[] = { 15.f, 40.f, 80.f };
	static_assert(UE_ARRAY_COUNT(Table) == LeaflineUpgrades::Count);
	static_assert(UE_ARRAY_COUNT(Costs) == LeaflineUpgrades::MaxLevel);
	static_assert(UE_ARRAY_COUNT(Milestones) == LeaflineUpgrades::MilestoneCount);
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

float LeaflineUpgrades::GetMilestoneThreshold(int32 Index)
{
	return Index >= 0 && Index < MilestoneCount ? Milestones[Index] : 0.f;
}
