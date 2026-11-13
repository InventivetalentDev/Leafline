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
	/** Optional (empty for leaves). */
	TArray<FVector2D> UV1;
	/** Bark only: distance along the branch (the value in UV0.y) as (Hi, Lo), Hi = floor(Along / 256) * 256, Lo = Along - Hi. */
	TArray<FVector2D> UV2;
	TArray<FLinearColor> Colors;
	TArray<FProcMeshTangent> Tangents;

	void Reset()
	{
		Vertices.Reset();
		Triangles.Reset();
		Normals.Reset();
		UV0.Reset();
		UV1.Reset();
		UV2.Reset();
		Colors.Reset();
		Tangents.Reset();
	}
};

/**
 * Smooth tubes around each branch centreline, with rounded tips matching the field's capsule ends.
 * UV0 = (arc length around, distance along) in cm. UV1 = (R cos theta, R sin theta) in cm: with UV0.y it places the
 * bark on a seamless cylinder for 3D noise (no seam around, physical scale everywhere, tips close to a point).
 * UV2 = (Hi, Lo) split of UV0.y: Hi = floor(along / 256) * 256, Lo = along - Hi. Both are exact in half floats and Hi + Lo
 * interpolates linearly, giving the along distance at sub-millimetre precision.
 * Colour = (level/3, per-branch random, height on tree, buried). Alpha is a signed distance to the other branches the
 * vertex overlaps (its parent for a child, its children's bases for a parent): A = clamp(0.5 + d / 400, 0, 1), d in cm,
 * negative inside the other branch; 1 where nothing overlaps. The material treats A < 0.4625 (more than 15 cm inside)
 * as buried; the wide range and the margin keep the linear interpolation across large triangles from cutting real
 * surface at a junction. Colours reach the GPU linearly, so this is the exact 8-bit value.
 */
namespace TreeMeshBuilder
{
	void Build(const FTreeSkeleton& Skeleton, FTreeMeshData& Out);
}
