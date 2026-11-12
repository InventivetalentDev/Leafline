#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Tree/TreeTypes.h"
#include "Tree/TreeField.h"
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

	UPROPERTY()
	TObjectPtr<UMaterialInterface> FallbackMaterial;

	FTreeSkeleton Skeleton;
	FTreeField Field;
};
