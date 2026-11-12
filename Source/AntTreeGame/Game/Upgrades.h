#pragma once

#include "CoreMinimal.h"

/** Stable save/console indices; append new entries rather than reordering. */
enum class ELeaflineUpgrade : uint8
{
	Mandibles,
	Strength,
	Grip,
	Stamina,
	Brood,
	Count
};

struct ANTTREEGAME_API FLeaflineUpgradeInfo
{
	const TCHAR* Name;
	const TCHAR* Description;
	/** Absolute effect at levels 0..3, not compounded per purchase. Brood is additive. */
	float Effects[4];
};

namespace LeaflineUpgrades
{
	inline constexpr int32 Count = static_cast<int32>(ELeaflineUpgrade::Count);
	inline constexpr int32 MaxLevel = 3;
	inline constexpr int32 MilestoneCount = 3;

	ANTTREEGAME_API const FLeaflineUpgradeInfo* Get(int32 Index);
	/** Cost of the next level; zero means maxed or invalid. */
	ANTTREEGAME_API int32 GetNextCost(int32 CurrentLevel);
	/** Returns zero for an invalid index; valid levels are clamped to 0..3. */
	ANTTREEGAME_API float GetEffect(int32 Index, int32 Level);
	/** Milestone indices are 0..2; zero means invalid. */
	ANTTREEGAME_API float GetMilestoneThreshold(int32 Index);
}
