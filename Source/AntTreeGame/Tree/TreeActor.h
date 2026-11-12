#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Tree/TreeTypes.h"
#include "Tree/TreeField.h"
#include "Tree/LeafSet.h"
#include "TreeActor.generated.h"

class UProceduralMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/** Owns the tree skeleton, its distance field and its procedural mesh. Spawned at runtime by the game mode. */
UCLASS()
class ANTTREEGAME_API ATreeActor : public AActor
{
	GENERATED_BODY()

public:
	ATreeActor();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tree")
	FTreeParams Params;

	/** Material for the wood. If empty, the engine's basic shape material tinted with BarkColor is used. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tree")
	TObjectPtr<UMaterialInterface> BarkMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tree")
	FLinearColor BarkColor = FLinearColor(0.20f, 0.13f, 0.08f);

	/** Material for leaves. If empty, the basic shape material tinted with LeafColor is used (blades are two-sided geometry). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tree")
	TObjectPtr<UMaterialInterface> LeafMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tree")
	FLinearColor LeafColor = FLinearColor(0.10f, 0.30f, 0.05f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tree")
	FLinearColor GroundColor = FLinearColor(0.09f, 0.075f, 0.05f);

	/** Procedural soil material. If empty, the basic shape material tinted with GroundColor is used. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tree")
	TObjectPtr<UMaterialInterface> GroundMaterial;

	const FLeafSet& GetLeaves() const { return Leaves; }
	int32 FindLeafNear(const FVector& P, float Reach) const { return Leaves.FindNearest(P, Reach); }
	float LeafSizeCap(int32 Leaf, float UsePerCut) const { return Leaves.SizeCap(Leaf, UsePerCut); }
	/** Bites a fragment out of a leaf and rebuilds that leaf's chunk. */
	bool BiteLeaf(int32 Leaf, float Size01, float DiameterCm, float UsePerCut);

	/** Regrows skeleton, field and mesh from a seed. */
	UFUNCTION(BlueprintCallable, Category = "Tree")
	void Grow(int32 Seed);

	const FTreeSkeleton& GetSkeleton() const { return Skeleton; }
	const FTreeField& GetField() const { return Field; }
	int32 GetSeed() const { return Skeleton.Seed; }

	/** A point on the ground beside the trunk, between two roots, and the direction facing away from the trunk. */
	FTransform GetNestTransform(int32 Index = 0) const;

	/** The tree in this world, or null. */
	static ATreeActor* Get(const UObject* WorldContext);

private:
	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> WoodMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BarkMID;

	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> LeafMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> LeafMaterialInUse;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> GroundMID;

	/** Distant trunks (no shadows, not walkable). */
	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> GroveMesh;

	/** Their crowns: big leaf clumps in the leaf material, scenery only. */
	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> GroveLeafMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> FallbackMaterial;

	void BuildLeafChunk(int32 Chunk);
	void BuildGround();
	void BuildGrove();

	FTreeSkeleton Skeleton;
	FTreeField Field;
	FLeafSet Leaves;
	FLeafSet GroveLeaves;
};
