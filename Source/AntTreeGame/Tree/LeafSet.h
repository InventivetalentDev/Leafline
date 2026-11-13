#pragma once

#include "CoreMinimal.h"
#include "Tree/TreeTypes.h"

class FTreeField;
struct FTreeMeshData;

/** One leaf: a blade on a short stalk, attached to a twig. All sizes in cm. */
struct FTreeLeaf
{
	FVector Base = FVector::ZeroVector;   // where the stalk meets the twig surface
	FVector Origin = FVector::ZeroVector; // where the blade starts
	FVector Dir = FVector::ForwardVector; // along the midrib, base to tip
	FVector Side = FVector::RightVector;  // across the blade
	FVector Normal = FVector::UpVector;   // blade face
	float Length = 200.f;
	float Width = 90.f;
	int32 Branch = INDEX_NONE;
	int32 Chunk = 0;
	float Remaining = 1.f;
	uint8 Bites = 0;
	FVector2f BiteCenter[3]; // leaf-local (along, across) in cm
	float BiteRadius[3] = { 0.f, 0.f, 0.f };
	/** Value multiplier of pieces cut from this leaf: 1 at the lowest leafy branches, about 3 at high outer tips. Set by
	 *  FLeafSet::Build from the tree's shape alone; decor leaves keep 1. */
	float Richness = 1.f;
	/** Arc length along Branch (cm) where the stalk joins it. */
	float BranchS = 0.f;

	bool CanCut() const { return Bites < 3 && Remaining > 0.16f; }
	/** Half width of the untouched blade at a distance along the midrib. */
	float HalfWidthAt(float Along) const;
	/** World point at leaf-local (along, across). */
	FVector Point(float Along, float Across) const { return Origin + Dir * Along + Side * Across; }
};

/** All leaves of a tree. Pure data plus mesh generation; the tree actor owns the mesh component. */
class FLeafSet
{
public:
	void Build(const FTreeSkeleton& Skeleton, const FTreeField& Field);
	void Reset();

	int32 Num() const { return Leaves.Num(); }
	int32 NumChunks() const { return ChunkLeaves.Num(); }
	const FTreeLeaf& Get(int32 Index) const { return Leaves[Index]; }

	/** Nearest cuttable leaf whose blade start is within Reach of P, or INDEX_NONE. */
	int32 FindNearest(const FVector& P, float Reach) const;
	/** Nearest leaf that can no longer be cut (!CanCut), measured as FindNearest does, or INDEX_NONE. */
	int32 FindNearestSpent(const FVector& P, float Reach) const;

	/** The tree's FTreeParams::RichnessMul at the last Build (1 for a set that was never built, like the grove's). */
	float GetRichnessMul() const { return RichnessMul; }

	/** Where the next bite of this diameter would go (leaf-local along/across, cm) and its radius. */
	void PlanBite(int32 Index, float DiameterCm, FVector2f& OutCenter, float& OutRadius) const;

	/** Bites a fragment of size Size01 (0..1) and diameter DiameterCm out of a leaf. Returns false if it cannot be cut. */
	bool ApplyBite(int32 Index, float Size01, float DiameterCm, float UsePerCut);

	/** Largest fragment size (0..1) the leaf still allows. */
	float SizeCap(int32 Index, float UsePerCut) const;

	/** How many more pieces of size Size01 the leaf gives under the bite limits (CanCut, SizeCap); 0 if none. */
	int32 PiecesLeft(int32 Index, float Size01, float UsePerCut) const;

	/** Leaves growing on a skeleton branch, in order along it; null if none. */
	const TArray<int32>* LeavesOnBranch(int32 Branch) const { return BranchLeaves.Find(Branch); }

	/** A leaf for scenery only (distant grove crowns): no grid entry, so it can never be found or cut. */
	void AddDecorLeaf(const FTreeLeaf& Leaf);

	/**
	 * Blades for one chunk. A leaf with little area left (Remaining <= 0.16, the CanCut threshold) is drawn as its bare
	 * stalk only. Vertex colour: R 1 for a leaf that can still be cut, 0 for a spent one (M_Leaf turns it brown); G a
	 * per-leaf random; B height on the tree; A the shape-based richness (Richness / RichnessMul - 1) / 2.
	 */
	void BuildChunkMesh(int32 Chunk, FTreeMeshData& Out, int32 Segments = 0) const;

	TArray<FTreeLeaf> Leaves;

private:
	FIntVector Cell(const FVector& P) const;
	int32 FindNearestWhere(const FVector& P, float Reach, bool bCuttable) const;

	TArray<TArray<int32>> ChunkLeaves;
	TMap<FIntVector, TArray<int32>> Grid;
	TMap<int32, TArray<int32>> BranchLeaves;
	float CellSize = 300.f;
	float RichnessMul = 1.f;
};
