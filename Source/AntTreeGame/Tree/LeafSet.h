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

	bool CanCut() const { return Bites < 3 && Remaining > 0.12f; }
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

	/** Bites a fragment of size Size01 (0..1) and diameter DiameterCm out of a leaf. Returns false if it cannot be cut. */
	bool ApplyBite(int32 Index, float Size01, float DiameterCm, float UsePerCut);

	/** Largest fragment size (0..1) the leaf still allows. */
	float SizeCap(int32 Index, float UsePerCut) const;

	/** Two-sided blades for one chunk. */
	void BuildChunkMesh(int32 Chunk, FTreeMeshData& Out) const;

	TArray<FTreeLeaf> Leaves;

private:
	FIntVector Cell(const FVector& P) const;

	TArray<TArray<int32>> ChunkLeaves;
	TMap<FIntVector, TArray<int32>> Grid;
	float CellSize = 300.f;
};
