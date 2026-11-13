#include "Game/LeaflineSaveGame.h"
#include "Game/Upgrades.h"

ULeaflineSaveGame::ULeaflineSaveGame()
{
	UpgradeLevels.Init(0, LeaflineUpgrades::Count);
}

void ULeaflineSaveGame::Sanitize()
{
	LeafMass = FMath::IsFinite(LeafMass) ? FMath::Max(0.f, LeafMass) : 0.f;
	TotalHauled = FMath::IsFinite(TotalHauled) ? FMath::Max(LeafMass, TotalHauled) : LeafMass;
	BestDayHaul = FMath::IsFinite(BestDayHaul) ? FMath::Max(0.f, BestDayHaul) : 0.f;
	Day = FMath::Max(1, Day);
	UpgradeLevels.SetNum(LeaflineUpgrades::Count);
	for (int32& Level : UpgradeLevels) Level = FMath::Clamp(Level, 0, LeaflineUpgrades::MaxLevel);
	Stage = FMath::Max(0, Stage);
	StageStartDay = FMath::Clamp(StageStartDay, 1, Day);
	TreeLeafCount = FMath::Max(0, TreeLeafCount);
}
