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
struct FTrailPoint;

/** Limits for ATreeActor::BuildPatch (also spelled ATreeActor::FPatchRules). */
struct FTreePatchRules
{
	int32 MaxLeaves = 4;
	int32 MaxPieces = 10;
	/** Farthest a patch leaf's base may be from the cut leaf's base, walking over the bark (cm). */
	float MaxWalk = 700.f;
	/** Fragment size (0..1) the pieces are counted at. */
	float Template = 0.5f;
	float UsePerCut = 0.38f;
};

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

	using FPatchRules = FTreePatchRules;

	/**
	 * The patch around a leaf: CutLeaf first (always, even with no pieces left), then the leaves on the same branch, then
	 * those on the rest of its bough (the bough and its twigs), each group nearest first along the bark. Leaves with no
	 * pieces left at Rules.Template, farther than Rules.MaxWalk, or whose approach from CutLeaf's base cannot be built are
	 * left out, and so is a leaf that would take the pieces (sum of PiecesLeft, CutLeaf included; the scout's own piece is
	 * not counted, the caller adds it) past Rules.MaxPieces. Stops at Rules.MaxLeaves or when the pieces reach MaxPieces.
	 */
	void BuildPatch(int32 CutLeaf, const FPatchRules& Rules, TArray<int32>& OutLeaves) const;

	/**
	 * A walkable route over the bark from From (on branch FromBranch, or the nearest branch if INDEX_NONE) to the base of
	 * a leaf, following the branch graph (along FromBranch to the junction, along the parent, up the leaf's branch).
	 * OutRoute[0] is From on the bark; the last point is at the leaf's Base. Points at most 12 cm apart, each projected onto the
	 * walking field 1.5 cm up (the ants' ride height), normals from the field; every consecutive pair passes StaysOnBark.
	 * Runs over the upper side of branches where it can and turns to the leaf's side near the end. False if no valid
	 * route (a step leaves the bark, the route is longer than MaxWalk, or the leaf is invalid).
	 */
	bool BuildLeafApproach(const FVector& From, int32 FromBranch, int32 Leaf, float MaxWalk, TArray<FTrailPoint>& OutRoute) const;

	/** The straight line A-B stays within 10 cm of bark (sampled at five inner points). */
	bool StaysOnBark(const FVector& A, const FVector& B) const;
	/** Approach outcomes so far on this tree, e.g. "ok 120, left the branches 2" (logs). */
	FString DescribeApproachStats() const;

	/** Debug: logs typical patch sizes (leaves, pieces) at templates 0.3 / 0.6 / 1.0 over Samples cuttable leaves spread
	 *  over the tree, and how many approaches failed and why. Clears the patch reachability cache first. Slow. */
	void LogPatchSurvey(int32 Samples = 40) const;

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
	/** Whether BuildLeafApproach from CutLeaf's base to Leaf works within MaxWalk (cached for the tree's life). */
	bool PatchLeafReachable(int32 CutLeaf, int32 Leaf, float MaxWalk) const;
	void BuildGround();
	void BuildGrove();

	FTreeSkeleton Skeleton;
	FTreeField Field;
	FLeafSet Leaves;
	FLeafSet GroveLeaves;

	/** Child branches of each skeleton branch. */
	TArray<TArray<int32>> BranchChildren;
	/** (CutLeaf << 32 | Leaf) -> approach length (cm) from CutLeaf's base, or -MaxWalk if it failed at that MaxWalk. */
	mutable TMap<uint64, float> PatchWalkCache;
	/** BuildLeafApproach results by outcome (see TreeActor.cpp). */
	mutable TArray<int32> ApproachStats;
};
