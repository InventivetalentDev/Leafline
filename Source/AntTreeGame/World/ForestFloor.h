#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ForestFloor.generated.h"

class ATreeActor;
class UProceduralMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/** One struct for the floor's numbers (cm). */
USTRUCT(BlueprintType)
struct FForestFloorTuning
{
	GENERATED_BODY()

	/** Nothing at all closer than this to the trunk base, measured from the tip of the farthest root plus the margin
	 *  (the nest and the trails sit inside). The larger of the two is used. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clearing") float ClearRadius = 2800.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clearing") float RootMargin = 700.f;
	/** Extra clear distance around every nest spot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clearing") float NestClear = 900.f;
	/** Past the clearing, leaves only lie flat (a few mm above the ground) for this much more. Curled and tilted leaves,
	 *  grass and twigs start beyond it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Clearing") float FlatRing = 1800.f;
	/** Outer edge of the scattered litter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Litter") float LitterRadius = 14000.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Litter") int32 RingLeaves = 2200;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Litter") int32 OuterLeaves = 2600;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Litter") float LeafLengthMin = 45.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Litter") float LeafLengthMax = 130.f;
	/** Fraction of the leaves that are small fragments (8 to 24 cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Litter") float FragmentFraction = 0.35f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grass") int32 Tufts = 650;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grass") int32 FarTufts = 450;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grass") float FarRadius = 30000.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grass") float BladeHeightMin = 70.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grass") float BladeHeightMax = 150.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Twigs") int32 Twigs = 90;
	/** Flat leaves' height above the ground (cm): below the trail ribbons (1.2 cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Litter") float FlatLift = 0.2f;
};

/**
 * The forest floor: a leaf-litter clearing kept open by the colony, with sparse litter, grass tufts and twigs round it.
 * Visual only (no collision, the walking field is not touched), generated once per tree seed in its own procedural
 * meshes (not through LeafSet, so saves and the leaf count are unaffected). Spawned by the game mode; it builds when
 * the tree has grown and rebuilds when the tree's seed changes. Leaf.Floor 0|1 hides or shows it.
 */
UCLASS()
class ANTTREEGAME_API AForestFloor : public AActor
{
	GENERATED_BODY()

public:
	AForestFloor();

	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Floor")
	FForestFloorTuning Tuning;

	void SetFloorEnabled(bool bOn);
	bool IsFloorEnabled() const { return bEnabled; }

private:
	void Build(const ATreeActor& Tree);
	void Clear();

	bool bEnabled = true;
	int32 BuiltSeed = INT32_MIN;

	UPROPERTY(Transient) TArray<TObjectPtr<UProceduralMeshComponent>> Sectors;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> Mats;
	UPROPERTY() TObjectPtr<UMaterialInterface> BasicMaterial;
};
