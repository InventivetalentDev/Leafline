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
	bool bMigrate = false;
	if (UGameplayStatics::DoesSaveGameExist(SaveSlot, SaveUser))
	{
		Colony = Cast<ULeaflineSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlot, SaveUser));
		if (!Colony || Colony->Version < 1 || Colony->Version > ULeaflineSaveGame::CurrentVersion)
		{
			UE_LOG(LogLeafline, Warning, TEXT("Leafline save unreadable or unsupported version; starting a fresh colony."));
			Colony = nullptr;
		}
		else bMigrate = Colony->Version == 1;
	}
	if (!Colony) Colony = NewObject<ULeaflineSaveGame>(this);
	Colony->Sanitize();
	if (bMigrate)
	{
		Colony->Version = ULeaflineSaveGame::CurrentVersion;
		UE_LOG(LogLeafline, Log, TEXT("Migrated colony save from version 1 to 2; existing levels and progress preserved."));
		SaveNow(); // A write failure is logged; the migrated in-memory colony remains usable.
	}
	UE_LOG(LogLeafline, Log, TEXT("Colony loaded from slot '%s': day %d, mass %.2f, total hauled %.2f, best day %.2f"),
		*SaveSlot, GetDay(), GetLeafMass(), GetTotalHauled(), GetBestDayHaul());
	OnChanged.Broadcast();
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
	return Colony && Cost > 0 && Colony->LeafMass >= Cost;
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
	const float Threshold = LeaflineUpgrades::GetMilestoneThreshold(Index, AreRecruitsEnabled());
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
