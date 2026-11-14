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

	// In-world cues (AWindCues, Leaf.Cues 0|1): looks only, nothing here changes the wind, exposure or the economy.
	/** Streaks in the view (they replace the streaks above while the cues are on). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cues") int32 CueStreaks = 300;
	/** Specks (pollen, dust, leaf bits) in the air between the camera and the ant. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cues") int32 CueSpecks = 280;
	/** Specks and short streaks streaming past the ant itself (within CueLocalRadius). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cues") int32 CueLocal = 110;
	/** 0: soft ellipses (M_Exposure, vertex colour); 1: M_WindStreak, as before. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cues") int32 CueStreakMaterial = 0;
	/** The gust front: where it is, the air behind it already shows the gust. It reaches the ant as the gust rises (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cues") float CueFrontSpeed = 300.f;
	/** Wind strength the air behind the front shows, times the gust's peak. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cues") float CueFrontLevel = 0.9f;
	/** Half width of the front's soft edge (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cues") float CueFrontBand = 250.f;
	/** During the warning the air everywhere stirs up to this share of the gust air, ahead of the front too (seen when
	 *  the front comes from behind the camera). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cues") float CuePrecursor = 0.45f;
	/** A wall of dust riding on the front during the warning and the rise: this many big faint puffs in view, their alpha
	 *  and radius (cm). It shows against the sky and the bark alike, where thin streaks far away do not. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cues") int32 CueFrontPuffs = 26;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cues") float CueFrontHaze = 0.16f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cues") float CueFrontPuffSize = 170.f;
	/** Colours (unlit vertex colours; the material is bright, so low values still read light): the dust wall, and streaks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cues") FLinearColor CueFrontPuffColor = FLinearColor(0.16f, 0.14f, 0.11f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cues") FLinearColor CueStreakColor = FLinearColor(0.35f, 0.35f, 0.35f);
	/** Extra specks gathered at the front's edge (0 none). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cues") float CueFrontDust = 0.8f;
	/** Nothing nearer the camera than these is drawn (it fades in over the second half): streaks, specks, the ant's air (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cues") float CueStreakNear = 300.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cues") float CueSpeckNear = 120.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cues") float CueLocalNear = 90.f;
	/** Streaks fill the view out to this distance, so the front shows well before it arrives (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cues") float CueFar = 2600.f;
	/** Streak half width (cm) in calm air; CueStreakThick more in full gust air. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cues") float CueStreakWidth = 0.9f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cues") float CueStreakThick = 1.f;
	/** Streak length (cm) in full gust air (half of it in calm air). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cues") float CueStreakLength = 380.f;
	/** Alpha at a streak's tail relative to its head. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cues") float CueStreakTail = 0.35f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cues") float CueStreakAlpha = 0.5f;
	/** Nothing is drawn thinner than this many pixels, so far streaks and specks still show. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cues") float CueMinPixels = 3.f;
	/** Speck radius (cm); leaf bits are CueLeafBitSize and make up CueLeafBitShare of the specks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cues") float CueSpeckSize = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cues") float CueLeafBitSize = 1.8f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cues") float CueLeafBitShare = 0.15f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cues") float CueSpeckAlpha = 0.6f;
	/** Alpha of the few specks that drift in calm air (0: none). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cues") float CueCalmMotes = 0.2f;
	/** Fast specks are stretched along their path by this many seconds of travel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cues") float CueStretch = 0.006f;
	/** Specks fill the view out to this distance, more of them near the camera (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cues") float CueSpeckFar = 1400.f;
	/** Speck colour scale (the materials are unlit). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cues") float CueBrightness = 0.35f;
	/** The ant's own air: specks aimed past the ant start this far upwind of it (cm), spread over a disc across the wind of
	 *  radius CueLocalSpread, more of them near its axis (CueLocalLift above the bark at the ant). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cues") float CueLocalSpread = 70.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cues") float CueLocalLift = 12.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cues") float CueLocalRadius = 200.f;
	/** Calm pocket round a sheltered ant (cm): the air streams round it and motes hang still inside. Shrinks with exposure. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cues") float CueShelterRadius = 70.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cues") float CueMoteAlpha = 0.7f;
	/** Gripping: bark dust scraped off and blown away, a puff at the grip and a trickle while the wind pulls. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cues") int32 CueGripPuff = 14;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cues") float CueGripTrickle = 10.f;
	/** Gripping pose with the cues on: body height and width scale (claws dug in, legs splayed), and how hard the wind
	 *  tugs the body downwind (times the wind the ant feels). Before: 0.78 and 1.14, no tug. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cues") float CueGripSquash = 0.66f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cues") float CueGripSpread = 1.26f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cues") float CueGripTug = 1.f;
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
	float GetPhaseDuration() const { return PhaseDuration; }
	float GetGustPeak() const { return GustPeak; }
	bool AreCuesMuted() const { return bMuteCues; }
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
