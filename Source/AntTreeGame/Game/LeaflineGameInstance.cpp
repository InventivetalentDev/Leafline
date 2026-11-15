#include "Game/LeaflineGameInstance.h"
#include "Game/LeaflineSaveGame.h"
#include "AntTreeGame.h"
#include "Ant/AntPawn.h"
#include "Ant/AntMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UObject/UObjectGlobals.h"

namespace
{
	constexpr int32 SaveUser = 0;
}

void ULeaflineGameInstance::Init()
{
	Super::Init();
	FString RequestedSlot;
	if (FParse::Value(FCommandLine::Get(), TEXT("LeafSlot="), RequestedSlot) && !RequestedSlot.IsEmpty())
		SaveSlot = RequestedSlot;
	LoadColony();
}

bool ULeaflineGameInstance::ReloadFromDisk()
{
	UE_LOG(LogLeafline, Log, TEXT("Reloading the colony from slot '%s'"), *SaveSlot);
	return LoadColony();
}

bool ULeaflineGameInstance::LoadColony()
{
	bool bMigrate = false;
	bool bLoaded = false;
	Colony = nullptr;
	++TreeGeneration;
	if (UGameplayStatics::DoesSaveGameExist(SaveSlot, SaveUser))
	{
		Colony = Cast<ULeaflineSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlot, SaveUser));
		// Version 0: a file from version 1 or 2, which never stored its version (see ULeaflineSaveGame::Version).
		if (!Colony || Colony->Version < 0 || Colony->Version > ULeaflineSaveGame::CurrentVersion)
		{
			UE_LOG(LogLeafline, Warning, TEXT("Leafline save unreadable or unsupported version; starting a fresh colony."));
			Colony = nullptr;
		}
		else
		{
			bMigrate = Colony->Version < ULeaflineSaveGame::CurrentVersion;
			bLoaded = true;
		}
	}
	if (!Colony)
	{
		Colony = NewObject<ULeaflineSaveGame>(this);
		Colony->Version = ULeaflineSaveGame::CurrentVersion;
	}
	Colony->Sanitize();
	if (bMigrate)
	{
		// Days, upgrades and haul carry over. No tree is stored, so the director grows a fresh one for the stage the
		// colony's milestones put it in.
		bLegacySave = true;
		Colony->Version = ULeaflineSaveGame::CurrentVersion;
		Colony->TreeSeed = 0;
		Colony->Bitten.Reset();
		Colony->Yesterday.Reset();
		UE_LOG(LogLeafline, Log, TEXT("Migrated colony save to version %d; levels and progress preserved, a fresh tree on the next day."),
			ULeaflineSaveGame::CurrentVersion);
		SaveNow(); // A write failure is logged; the migrated in-memory colony remains usable.
	}
	UE_LOG(LogLeafline, Log, TEXT("Colony loaded from slot '%s': day %d, mass %.2f, total hauled %.2f, best day %.2f, tree %d (seed %d, from day %d, %d bitten leaves, %d routes of day %d)"),
		*SaveSlot, GetDay(), GetLeafMass(), GetTotalHauled(), GetBestDayHaul(), Colony->Stage + 1, Colony->TreeSeed,
		Colony->StageStartDay, Colony->Bitten.Num(), Colony->Yesterday.Num(), Colony->YesterdayDay);
	RefreshAntUpgrades();
	OnChanged.Broadcast();
	return bLoaded;
}

int32 ULeaflineGameInstance::GetUpgradeLevel(int32 Index) const
{
	return Colony && Colony->UpgradeLevels.IsValidIndex(Index) ? Colony->UpgradeLevels[Index] : 0;
}

int32 ULeaflineGameInstance::GetUpgradeCost(int32 Index) const
{
	return LeaflineUpgrades::Get(Index) ? LeaflineUpgrades::GetNextCost(GetUpgradeLevel(Index)) : 0;
}

bool ULeaflineGameInstance::CanAfford(int32 Index) const
{
	const int32 Cost = GetUpgradeCost(Index);
	return Colony && Cost > 0 && GetSpendableLeaf() >= Cost;
}

bool ULeaflineGameInstance::Buy(int32 Index)
{
	if (!CanAfford(Index)) return false;
	const float PreviousMass = Colony->LeafMass;
	Colony->LeafMass -= GetUpgradeCost(Index);
	++Colony->UpgradeLevels[Index];
	if (!SaveNow())
	{
		Colony->LeafMass = PreviousMass;
		--Colony->UpgradeLevels[Index];
		return false;
	}
	RefreshAntUpgrades();
	UE_LOG(LogLeafline, Log, TEXT("Bought %s level %d; mass %.2f"), LeaflineUpgrades::Get(Index)->Name, GetUpgradeLevel(Index), GetLeafMass());
	OnChanged.Broadcast();
	return true;
}

bool ULeaflineGameInstance::AddHaul(float Mass)
{
	if (!Colony || !FMath::IsFinite(Mass) || Mass <= 0.f ||
		!FMath::IsFinite(Colony->LeafMass + Mass) || !FMath::IsFinite(Colony->TotalHauled + Mass)) return false;
	Colony->LeafMass += Mass;
	Colony->TotalHauled += Mass;
	UE_LOG(LogLeafline, Log, TEXT("Hauled %.2f; mass %.2f, total %.2f"), Mass, GetLeafMass(), GetTotalHauled());
	OnChanged.Broadcast();
	return true;
}

float ULeaflineGameInstance::GetLeafMass() const { return Colony ? Colony->LeafMass : 0.f; }
int32 ULeaflineGameInstance::GetSpendableLeaf() const { return Colony ? FMath::FloorToInt(Colony->LeafMass) : 0; }
float ULeaflineGameInstance::GetTotalHauled() const { return Colony ? Colony->TotalHauled : 0.f; }
int32 ULeaflineGameInstance::GetDay() const { return Colony ? Colony->Day : 1; }
int32 ULeaflineGameInstance::GetBaseSeed() const { return Colony ? Colony->BaseSeed : 1112; }

void ULeaflineGameInstance::SetDay(int32 InDay)
{
	InDay = FMath::Max(1, InDay);
	if (!Colony || Colony->Day == InDay) return;
	Colony->Day = InDay;
	OnChanged.Broadcast();
}

bool ULeaflineGameInstance::SaveNow()
{
	const bool bSaved = Colony && UGameplayStatics::SaveGameToSlot(Colony, SaveSlot, SaveUser);
	if (!bSaved) UE_LOG(LogLeafline, Error, TEXT("Could not write save slot '%s'."), *SaveSlot);
	return bSaved;
}

bool ULeaflineGameInstance::ResetSave()
{
	ULeaflineSaveGame* Previous = Colony;
	Colony = NewObject<ULeaflineSaveGame>(this);
	Colony->Version = ULeaflineSaveGame::CurrentVersion;
	++TreeGeneration;
	if (!SaveNow())
	{
		Colony = Previous;
		return false;
	}
	RefreshAntUpgrades();
	UE_LOG(LogLeafline, Log, TEXT("Leafline save reset. Start a new day to reset the active tree and roster."));
	OnChanged.Broadcast();
	return true;
}

float ULeaflineGameInstance::GetUpgradeEffect(ELeaflineUpgrade Upgrade) const
{
	return LeaflineUpgrades::GetEffect(static_cast<int32>(Upgrade), GetUpgradeLevel(Upgrade));
}

int32 ULeaflineGameInstance::GetWorkersPerDay(int32 Base) const
{
	return Base;
}

bool ULeaflineGameInstance::AreRecruitsEnabled() const { return bRecruitsEnabled; }

void ULeaflineGameInstance::SetRecruitsEnabled(bool bOn)
{
	if (bRecruitsEnabled == bOn) return;
	bRecruitsEnabled = bOn;
	UE_LOG(LogLeafline, Log, TEXT("Recruits %s; pool %d, milestones %d/%d"), bOn ? TEXT("on") : TEXT("off"),
		GetRecruitPool(), GetReachedMilestoneCount(), LeaflineUpgrades::MilestoneCount);
	OnChanged.Broadcast();
}

int32 ULeaflineGameInstance::GetRecruitPool() const
{
	return bRecruitsEnabled ? static_cast<int32>(GetUpgradeEffect(ELeaflineUpgrade::Brood)) : 0;
}

void ULeaflineGameInstance::SetOneTree(bool bOn)
{
	if (bOneTree == bOn) return;
	bOneTree = bOn;
	UE_LOG(LogLeafline, Log, TEXT("One tree per stage %s; milestones %d/%d"), bOn ? TEXT("on") : TEXT("off"),
		GetReachedMilestoneCount(), LeaflineUpgrades::MilestoneCount);
	OnChanged.Broadcast();
}

float ULeaflineGameInstance::GetMilestoneThreshold(int32 Index) const
{
	return LeaflineUpgrades::GetMilestoneThreshold(Index, AreRecruitsEnabled(), bOneTree);
}

int32 ULeaflineGameInstance::GetStage() const { return Colony ? Colony->Stage : 0; }
int32 ULeaflineGameInstance::GetTreeSeed() const { return Colony ? Colony->TreeSeed : 0; }
int32 ULeaflineGameInstance::GetStageStartDay() const { return Colony ? Colony->StageStartDay : 1; }

void ULeaflineGameInstance::SetTree(int32 InStage, int32 InSeed, int32 StartDay)
{
	if (!Colony) return;
	if (Colony->Stage != FMath::Max(0, InStage) || Colony->TreeSeed != InSeed) ++TreeGeneration;
	Colony->Stage = FMath::Max(0, InStage);
	Colony->TreeSeed = InSeed;
	Colony->StageStartDay = FMath::Max(1, StartDay);
	Colony->Bitten.Reset();
	Colony->TreeLeafCount = 0;
	Colony->Yesterday.Reset();
	Colony->YesterdayDay = 0;
	OnChanged.Broadcast();
}

void ULeaflineGameInstance::SetTreeState(const TArray<FSavedLeaf>& Bites, int32 LeafCount, const TArray<FSavedTrail>& Routes, int32 RoutesDay)
{
	if (!Colony) return;
	Colony->Bitten = Bites;
	Colony->TreeLeafCount = LeafCount;
	Colony->Yesterday = Routes;
	Colony->YesterdayDay = RoutesDay;
}

void ULeaflineGameInstance::ClearTreeState()
{
	if (!Colony) return;
	if (Colony->TreeSeed != 0) ++TreeGeneration;
	Colony->TreeSeed = 0;
	Colony->Bitten.Reset();
	Colony->TreeLeafCount = 0;
	Colony->Yesterday.Reset();
	Colony->YesterdayDay = 0;
}

const TArray<FSavedLeaf>& ULeaflineGameInstance::GetSavedBites() const
{
	static const TArray<FSavedLeaf> None;
	return Colony ? Colony->Bitten : None;
}
int32 ULeaflineGameInstance::GetSavedLeafCount() const { return Colony ? Colony->TreeLeafCount : 0; }
const TArray<FSavedTrail>& ULeaflineGameInstance::GetYesterdayTrails() const
{
	static const TArray<FSavedTrail> None;
	return Colony ? Colony->Yesterday : None;
}
int32 ULeaflineGameInstance::GetYesterdayDay() const { return Colony ? Colony->YesterdayDay : 0; }

float ULeaflineGameInstance::GetBestDayHaul() const { return Colony ? Colony->BestDayHaul : 0.f; }

bool ULeaflineGameInstance::RecordDayHaul(float DayHaul)
{
	if (!Colony || !FMath::IsFinite(DayHaul) || DayHaul <= Colony->BestDayHaul) return false;
	Colony->BestDayHaul = DayHaul;
	UE_LOG(LogLeafline, Log, TEXT("New best day: %.2f"), DayHaul);
	OnChanged.Broadcast();
	return true;
}

bool ULeaflineGameInstance::HasReachedMilestone(int32 Index) const
{
	const float Threshold = GetMilestoneThreshold(Index);
	return Threshold > 0.f && GetTotalHauled() >= Threshold;
}

int32 ULeaflineGameInstance::GetReachedMilestoneCount() const
{
	int32 Count = 0;
	for (int32 Index = 0; Index < LeaflineUpgrades::MilestoneCount; ++Index)
		if (HasReachedMilestone(Index)) ++Count;
	return Count;
}

bool ULeaflineGameInstance::IsEndCardDue() const
{
	return Colony && !Colony->bEndCardSeen && GetReachedMilestoneCount() >= LeaflineUpgrades::MilestoneCount;
}

void ULeaflineGameInstance::MarkEndCardSeen()
{
	if (!Colony || Colony->bEndCardSeen) return;
	Colony->bEndCardSeen = true;
	SaveNow();
}

void ULeaflineGameInstance::ApplyUpgradesToAnt(AAntPawn* Ant)
{
	if (!IsValid(Ant) || !IsValid(Ant->GetAntMovement())) return;
	UAntMovementComponent* Movement = Ant->GetAntMovement();
	const FAntBaseTuning Base = { Ant->CutRateMultiplier, Movement->Tuning.SpeedLossPerMass, Movement->Tuning.StaminaDrain };
	AntBases.FindOrAdd(TWeakObjectPtr<AAntPawn>(Ant), Base);
	RefreshAntUpgrades();
}

void ULeaflineGameInstance::ApplyAntTuning(AAntPawn* Ant, const FAntBaseTuning& Base) const
{
	Ant->CutRateMultiplier = Base.CutRateMultiplier * GetUpgradeEffect(ELeaflineUpgrade::Mandibles);
	UAntMovementComponent* Movement = Ant->GetAntMovement();
	if (IsValid(Movement))
	{
		Movement->Tuning.SpeedLossPerMass = Base.SpeedLossPerMass * GetUpgradeEffect(ELeaflineUpgrade::Strength);
		Movement->Tuning.StaminaDrain = Base.StaminaDrain * GetUpgradeEffect(ELeaflineUpgrade::Stamina);
	}
}

void ULeaflineGameInstance::RefreshAntUpgrades()
{
	for (auto It = AntBases.CreateIterator(); It; ++It)
	{
		if (It.Key().IsValid()) ApplyAntTuning(It.Key().Get(), It.Value());
		else It.RemoveCurrent();
	}
}
