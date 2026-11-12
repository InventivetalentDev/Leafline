#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"
#include "Tree/TreeTypes.h"

struct FTreeMeshData
{
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UV0;
	TArray<FLinearColor> Colors;
	TArray<FProcMeshTangent> Tangents;

	void Reset()
	{
		Vertices.Reset();
		Triangles.Reset();
		Normals.Reset();
		UV0.Reset();
		Colors.Reset();
		Tangents.Reset();
	}
};

/**
 * Smooth tubes around each branch centreline, with rounded tips matching the field's capsule ends.
 * UV0 = (arc length around, distance along) in cm. Colour = (level/3, per-branch random, height on tree, 0).
 */
namespace TreeMeshBuilder
{
	void Build(const FTreeSkeleton& Skeleton, FTreeMeshData& Out);
}
