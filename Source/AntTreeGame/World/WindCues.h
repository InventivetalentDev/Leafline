#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WindCues.generated.h"

class AWindSystem;
class AAntPawn;
class UProceduralMeshComponent;
class UMaterialInterface;
struct FWindTuning;

namespace WindCues
{
	/** Leaf.Cues: in-world wind cues on (default) or off (exactly the old streaks). */
	bool IsEnabled();
	void SetEnabled(bool bOn);
	/** Leaf.CuesStat: log the cues' game-thread cost over the next Seconds. */
	void StartStat(float Seconds);
	/** Leaf.CuesLog: log the front and the ant's air every half second. */
	bool IsLogging();
	void SetLogging(bool bOn);
	/** The gripping ant's body pose with the cues on (AAntPawn::UpdateCutVisuals): a press as the claws dig in, flatter
	 *  and wider than before, tugged downwind by the wind it feels. GripTime counts from the grip; LocalWind is the wind
	 *  direction in the ant's frame. */
	void GripPose(const FWindTuning& T, float GripTime, float WindFeel, const FVector& LocalWind, float Time, FVector& Loc, FRotator& Rot, FVector& Scale);
}

/**
 * In-world wind cues, looks only (docs/briefs/in-world-cues.md). Spawned by AWindSystem; reads its phase, strength and
 * exposure, changes nothing. Tuning lives in FWindTuning (Cue*, Leaf.Tune wind).
 *
 * - Gust front: during the warning a front moves downwind and reaches the controlled ant as the gust rises. Air upwind of
 *   it already shows the gust (thicker, longer, faster streaks, specks streaming), so the gust is seen coming.
 * - The ant's air: specks and short streaks aimed past the ant. Where the ant is sheltered they bend round a calm
 *   pocket (radius shrinking with exposure) and a few motes hang still inside; on exposed bark they rush straight past.
 * - Grip: bark dust scraped off at the grip and blown downwind while the wind pulls.
 *
 * Particles live on the CPU in fixed pools and are drawn as camera-facing quads in two procedural mesh sections:
 * streaks (M_Exposure soft ellipses, or M_WindStreak) and specks (M_Exposure: vertex colour, soft round dot from UV).
 */
UCLASS()
class ANTTREEGAME_API AWindCues : public AActor
{
	GENERATED_BODY()

public:
	AWindCues();

	virtual void Tick(float DeltaTime) override;

	void SetWind(AWindSystem* InWind) { Wind = InWind; }

private:
	enum class EKind : uint8 { Streak, Speck, LeafBit, Mote, Scrape };

	struct FParticle
	{
		FVector Pos = FVector::ZeroVector;
		FVector Dir = FVector::ForwardVector;
		float Life = 0.f;
		float MaxLife = -1.f; // < 0: free slot
		float Size = 1.f;
		float Length = 100.f;
		float Seed = 0.f;     // visibility threshold and variety
		float Phase = 0.f;    // wobble and tumble
		float Speed = 0.f;    // motes and scrapes keep their own speed
		FLinearColor Color = FLinearColor::White;
		EKind Kind = EKind::Speck;
		bool bSlid = false;   // the ant's air: slid along the bark after striking it
	};

	struct FQuads
	{
		TArray<FVector> V;
		TArray<FLinearColor> C;
		int32 Used = 0;
		int32 Capacity = 0;
	};

	/** The front and the ant's spot, computed once per frame. */
	struct FAir
	{
		FVector W = FVector::ForwardVector; // wind direction (toward)
		float Strength = 0.f;               // the wind's own strength
		bool bFront = false;
		bool bFrontEdge = false;
		float SFront = 0.f;                 // front position along W (world)
		float GustAir = 0.f;                // strength shown behind the front
		float Band = 300.f;
		bool bAnt = false;
		FVector AntPos = FVector::ZeroVector;
		FVector AntN = FVector::UpVector;
		FVector Across = FVector::RightVector;
		FVector Flow = FVector::ForwardVector;  // the ant's air: the wind without the part leaving the bark (lee faces: along it)
		float AntAir = 0.f;                 // visible gust amount at the ant (0..1)
		FVector PocketCentre = FVector::ZeroVector;
		float FrontHaze = 0.f;              // the dust wall's strength now (0..1)
		float Pocket = 0.f;                 // calm pocket radius (cm)
		float Shelter = 0.f;                // 0 exposed .. 1 sheltered
		FVector CamPos = FVector::ZeroVector;
		FVector CamFwd = FVector::ForwardVector, CamRight = FVector::RightVector, CamUp = FVector::UpVector;
		float TanH = 1.f, TanV = 0.5625f;   // half view extents per unit depth
		float PixelAngle = 1.f / 640.f;     // cm per pixel per cm of distance
	};

	void HideAll();
	bool ComputeAir(FAir& Air, float Dt);
	float AirAt(const FAir& Air, const FVector& P) const;
	float FrontEdgeAt(const FAir& Air, const FVector& P) const;
	FVector PocketMap(const FAir& Air, const FVector& P, float Seed, const FVector& Axis) const;
	/** A random point in the view between Near and Far (depth drawn as u^DepthPow, so > 1 favours near). */
	FVector ViewPoint(const FAir& Air, float Near, float Far, float DepthPow);
	static bool InView(const FAir& Air, const FVector& P, float Near, float Far);
	void StepStreaks(const FAir& Air, float Dt);
	void StepSpecks(const FAir& Air, float Dt);
	void StepLocal(const FAir& Air, float Dt);
	void StepScrapes(const FAir& Air, float Dt);
	void StepPuffs(const FAir& Air, float Dt);
	/** BurstAngle >= 0: a grain of the grip's burst, thrown out along the bark at that angle; < 0: blown downwind. */
	void SpawnScrape(const FAir& Air, float Strength01, float BurstAngle);
	void AddQuad(FQuads& Q, const FAir& Air, const FVector& A, const FVector& B, float HalfWidth, const FLinearColor& Col, float TailAlpha, float FadeNear);
	void Flush(int32 Section, FQuads& Q, UMaterialInterface* Material, bool bCentredUV);

	UPROPERTY(VisibleAnywhere, Category = "Wind")
	TObjectPtr<UProceduralMeshComponent> Mesh;

	UPROPERTY() TObjectPtr<UMaterialInterface> SoftMaterial;   // M_Exposure
	UPROPERTY() TObjectPtr<UMaterialInterface> StreakMaterial; // M_WindStreak

	TWeakObjectPtr<AWindSystem> Wind;
	FRandomStream Rand;
	TArray<FParticle> Streaks;
	TArray<FParticle> Specks;
	TArray<FParticle> Local;
	TArray<FParticle> Scrapes;
	TArray<FParticle> Puffs;
	FQuads StreakQuads;
	FQuads SpeckQuads;
	int32 StreakSectionMaterial = -1;
	float Clock = 0.f;
	float ExposureTimer = 0.f;
	float Exposure = 1.f;
	float PocketNow = 0.f;
	FVector ParkPos = FVector::ZeroVector;
	bool bWasGripping = false;
	float TrickleDebt = 0.f;
	bool bHidden = true;
};
