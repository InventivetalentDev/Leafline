#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Game/Upgrades.h"
#include "LeaflineGameInstance.generated.h"

class AAntPawn;
class ULeaflineSaveGame;

DECLARE_MULTICAST_DELEGATE(FOnLeaflineColonyChanged);

/** Persistent colony. The director deposits haul, sets the next day, then calls SaveNow at day end. */
UCLASS()
class ANTTREEGAME_API ULeaflineGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;

	/** Broadcast after load and each successful state change, including reset. */
	FOnLeaflineColonyChanged OnChanged;

	int32 GetUpgradeLevel(int32 Index) const;
	int32 GetUpgradeLevel(ELeaflineUpgrade Upgrade) const { return GetUpgradeLevel(static_cast<int32>(Upgrade)); }
	int32 GetUpgradeCost(int32 Index) const;
	bool CanAfford(int32 Index) const;
	bool CanAfford(ELeaflineUpgrade Upgrade) const { return CanAfford(static_cast<int32>(Upgrade)); }
	/** Saves immediately; on write failure, rolls back and returns false. */
	bool Buy(int32 Index);
	bool Buy(ELeaflineUpgrade Upgrade) { return Buy(static_cast<int32>(Upgrade)); }
	/** Adds positive finite mass to both balance and lifetime haul. Call SaveNow at day end. */
	bool AddHaul(float Mass);
	float GetLeafMass() const;
	float GetTotalHauled() const;
	int32 GetDay() const;
	void SetDay(int32 InDay);
	int32 GetBaseSeed() const;

	bool SaveNow();
	/** Overwrites the slot with a fresh colony; does not restart the active day. */
	bool ResetSave();
	float GetUpgradeEffect(ELeaflineUpgrade Upgrade) const;
	float GetGripMultiplier() const { return GetUpgradeEffect(ELeaflineUpgrade::Grip); }
	int32 GetWorkersPerDay(int32 Base) const;
	/** Milestone indices 0..2 correspond to 15/40/80 lifetime haul. Spending cannot undo them. */
	bool HasReachedMilestone(int32 Index) const;
	int32 GetReachedMilestoneCount() const;

	/** Call after configuring a spawned ant. Captures its original tuning once; repeat calls never stack. */
	void ApplyUpgradesToAnt(AAntPawn* Ant);

private:
	struct FAntBaseTuning
	{
		float CutRateMultiplier;
		float SpeedLossPerMass;
		float StaminaDrain;
	};
	void ApplyAntTuning(AAntPawn* Ant, const FAntBaseTuning& Base) const;
	void RefreshAntUpgrades();

	UPROPERTY(Transient) TObjectPtr<ULeaflineSaveGame> Colony;
	/** Weak keys let old worlds/workers be collected normally. */
	TMap<TWeakObjectPtr<AAntPawn>, FAntBaseTuning> AntBases;
};
