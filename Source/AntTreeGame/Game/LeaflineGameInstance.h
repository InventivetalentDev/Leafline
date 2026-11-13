#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Game/Upgrades.h"
#include "Tree/TreeTypes.h"
#include "Colony/ColonyTypes.h"
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
	/** Debug: reads the slot again, as at launch (tests of the save and load path). Returns whether a save was read. */
	bool ReloadFromDisk();
	/** Overwrites the slot with a fresh colony; does not restart the active day. */
	bool ResetSave();
	float GetUpgradeEffect(ELeaflineUpgrade Upgrade) const;
	float GetGripMultiplier() const { return GetUpgradeEffect(ELeaflineUpgrade::Grip); }
	int32 GetWorkersPerDay(int32 Base) const;
	/** Runtime switch, default true; never stored in the colony save. */
	bool AreRecruitsEnabled() const;
	void SetRecruitsEnabled(bool bOn);
	/** Brood's pool, or zero while recruits are disabled. */
	int32 GetRecruitPool() const;
	float GetBestDayHaul() const;
	/** Updates a positive finite record; persisted by SaveNow. */
	bool RecordDayHaul(float DayHaul);
	/** Runtime switch (FDayTuning::bOneTree), default true; never stored in the colony save. Picks the milestone set. */
	bool IsOneTree() const { return bOneTree; }
	void SetOneTree(bool bOn);
	/** Threshold of milestone 0..2 under the current recruit and one-tree flags. */
	float GetMilestoneThreshold(int32 Index) const;

	// The colony's tree (one tree per stage). Changes are written by the next SaveNow (at day end).
	int32 GetStage() const;
	/** 0 when no tree has been grown for the stage yet. */
	int32 GetTreeSeed() const;
	int32 GetStageStartDay() const;
	/** Moves the colony to Stage with a fresh tree from Seed, first day StartDay: drops saved bites and yesterday's trails. */
	void SetTree(int32 Stage, int32 Seed, int32 StartDay);
	/** Day-end snapshot of the tree's bites and the day's routes. */
	void SetTreeState(const TArray<FSavedLeaf>& Bites, int32 LeafCount, const TArray<FSavedTrail>& Routes, int32 RoutesDay);
	/** Forgets the saved bites and routes (one tree off: a new tree every day). */
	void ClearTreeState();
	const TArray<FSavedLeaf>& GetSavedBites() const;
	int32 GetSavedLeafCount() const;
	const TArray<FSavedTrail>& GetYesterdayTrails() const;
	int32 GetYesterdayDay() const;
	/** Changes whenever the colony's tree changes other than by today's bites: a new stage or seed, a reset, a reload. The
	 *  director grows the tree again when it differs from the one it grew. */
	int32 GetTreeGeneration() const { return TreeGeneration; }
	/** The loaded save is from before one tree per stage (version 1 or 2). */
	bool WasLegacySave() const { return bLegacySave; }

	/** Milestone indices 0..2 use the current recruit and one-tree flags. Spending cannot undo them. */
	bool HasReachedMilestone(int32 Index) const;
	int32 GetReachedMilestoneCount() const;
	/** Past the last milestone and the end card not yet shown (survives quitting at the recap). */
	bool IsEndCardDue() const;
	/** Records that the end card was shown, and saves. */
	void MarkEndCardSeen();

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
	/** Loads the slot (or starts a fresh colony), migrating older versions. */
	bool LoadColony();
	void RefreshAntUpgrades();

	UPROPERTY(Transient) TObjectPtr<ULeaflineSaveGame> Colony;
	FString SaveSlot = TEXT("Leafline");
	bool bRecruitsEnabled = true;
	bool bOneTree = true;
	bool bLegacySave = false;
	int32 TreeGeneration = 0;
	/** Weak keys let old worlds/workers be collected normally. */
	TMap<TWeakObjectPtr<AAntPawn>, FAntBaseTuning> AntBases;
};
