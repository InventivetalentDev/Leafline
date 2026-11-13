#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WindSystem.generated.h"

class ATreeActor;
class UProceduralMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class ULeaflineAudioLoop;
class UMaterialParameterCollection;

UENUM(BlueprintType)
enum class EWindPhase : uint8
{
	Calm,
	Warning,
	Rising,
	Holding,
	Falling
};

USTRUCT(BlueprintType)
struct FWindTuning
{
	GENERATED_BODY()

	// Gust cycle (seconds). Calm shortens and the hold lengthens with each day.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cycle") float CalmMin = 9.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cycle") float CalmMax = 15.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cycle") float CalmShrinkPerDay = 1.2f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cycle") float CalmFloor = 4.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cycle") float Warning = 3.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cycle") float Rising = 1.2f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cycle") float HoldMin = 2.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cycle") float HoldMax = 4.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cycle") float HoldGrowPerDay = 0.4f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cycle") float Falling = 2.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cycle") float CalmStrength = 0.12f;

	// Force model: Force = Strength * PeakForce * Exposure * (BodyArea + FragmentArea); blown off when Force > Hold.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Force") float PeakForceDay1 = 0.22f;
	/** Fractional rise of the peak force per day. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Force") float PeakForcePerDay = 0.15f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Force") float BodyArea = 1.f;
	/** Fragment diameter (cm) that counts as one area unit; area grows with the square of the diameter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Force") float FragmentAreaDiameter = 40.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Force") float BaseGrip = 1.f;
	/** Hold multiplier while the player grips. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Force") float GripBonus = 2.5f;
	/** Exposure multiplier where the tree blocks the wind upwind. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Force") float ShelterFactor = 0.25f;
	/** How far upwind the tree is looked for (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Force") float ShelterDistance = 400.f;
	/** Risk ratio (force / hold at full gust, full exposure) from which the cut arc turns amber. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Force") float RiskyRatio = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look") float DirectionWanderDeg = 8.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look") int32 StreakCount = 60;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look") float StreakSpeed = 2200.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look") float StreakRadius = 900.f;
};

/**
 * One prevailing wind per day with a gust cycle. Exposure comes from which way a surface faces and whether the tree
 * shelters it upwind. Draws wind streaks while a gust builds, so the player has time to react.
 */
UCLASS()
class ANTTREEGAME_API AWindSystem : public AActor
{
	GENERATED_BODY()

public:
	AWindSystem();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind")
	FWindTuning Tuning;

	/** Optional unlit translucent material for streaks (soft reference, so a missing asset never blocks a build). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind")
	TSoftObjectPtr<UMaterialInterface> StreakMaterial;

	/** ForceScale >= 0 (one tree per stage): peak force = PeakForceDay1 x ForceScale, and the gust cycle ramps with RampDay
	 *  (days on this tree) instead of the day number. Without them the force rises PeakForcePerDay per day, as before. */
	/** Yaw >= 0 sets the day's direction (degrees) instead of a random one; the random draw still happens, so the gust
	 *  timing is the same either way. */
	void StartDay(int32 InDay, int32 Seed, float InForceScale = -1.f, int32 InRampDay = -1, float InYaw = -1.f);
	/** Starts a gust now (Leaf.Gust). */
	void TriggerGust(float Peak = 1.f);
	/** Advances the gust clock (phases, strength, direction). The line system calls it in its fixed sim steps; the
	 *  actor's own tick then only updates sound, materials and streaks. */
	void Advance(float Dt);
	void SetExternalClock(bool bExternal) { bExternalClock = bExternal; }
	/** No warning sound on phase changes (a skipped harvest runs many gusts a second). */
	void SetMuteCues(bool bMute) { bMuteCues = bMute; }

	EWindPhase GetPhase() const { return Phase; }
	float GetStrength() const { return Strength; }
	/** The direction the wind blows toward (horizontal). */
	FVector GetDirection() const { return Direction; }
	/** Blow-off checks run while the gust rises and holds. */
	bool IsGustActive() const { return Phase == EWindPhase::Rising || Phase == EWindPhase::Holding; }
	float GetPhaseTimeLeft() const { return PhaseTimeLeft; }
	float GetPeakForce() const;

	float GetExposure(const FVector& P, const FVector& Normal) const;
	float FragmentArea(float DiameterCm) const;
	float ComputeForce(const FVector& P, const FVector& Normal, float FragmentDiameterCm, float* OutExposure = nullptr) const;
	float GetHold(float GripMultiplier, bool bGripping) const;
	/** Force over hold for a fragment at full gust and full exposure, without gripping (what an unattended ant faces). */
	float PredictRiskRatio(float FragmentDiameterCm, float GripMultiplier) const;

	static AWindSystem* Get(const UObject* WorldContext);

private:
	void EnterPhase(EWindPhase NewPhase);
	void UpdateStreaks(float Dt);

	struct FStreak
	{
		FVector Pos = FVector::ZeroVector;
		float Length = 200.f;
		float Life = 0.f;
		float MaxLife = 1.f;
	};

	UPROPERTY(VisibleAnywhere, Category = "Wind")
	TObjectPtr<UProceduralMeshComponent> StreakMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> StreakMaterialInUse;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> FallbackMaterial;

	/** Wind strength and direction for materials (leaf sway). */
	UPROPERTY()
	TObjectPtr<UMaterialParameterCollection> WindCollection;

	UPROPERTY(Transient) TObjectPtr<ULeaflineAudioLoop> CalmLoop;
	UPROPERTY(Transient) TObjectPtr<ULeaflineAudioLoop> GustLoop;
	UPROPERTY(Transient) TObjectPtr<ULeaflineAudioLoop> RustleLoop;
	UPROPERTY(Transient) TObjectPtr<ULeaflineAudioLoop> PadLoop;

	TWeakObjectPtr<ATreeActor> Tree;
	FRandomStream Rand;
	FRandomStream StreakRand;
	TArray<FStreak> Streaks;
	EWindPhase Phase = EWindPhase::Calm;
	float PhaseTimeLeft = 10.f;
	float PhaseDuration = 10.f;
	float Strength = 0.12f;
	float GustPeak = 1.f;
	FVector Direction = FVector::ForwardVector;
	float BaseYaw = 0.f;
	float Clock = 0.f;
	int32 Day = 1;
	/** Day count the gust cycle ramps with (the day, or days on this tree). */
	int32 RampDay = 1;
	/** Negative: the old per-day force rise. */
	float ForceScale = -1.f;
	bool bExternalClock = false;
	bool bMuteCues = false;
	float SpareFloat = 0.f;
	int32 SpareInt = 0;
};
