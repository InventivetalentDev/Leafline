#pragma once

#include "CoreMinimal.h"
#include "Tree/TreeTypes.h"

/** Grows a tree skeleton deterministically from a seed. Pure data, no engine objects. */
namespace TreeSkeleton
{
	void Grow(FTreeSkeleton& Out, const FTreeParams& Params, int32 Seed);

	/** Point on a branch at T (0..1 by arc length). */
	FVector SampleAt(const FTreeBranch& Branch, float T, float* OutRadius = nullptr, FVector* OutTangent = nullptr);

	/** Fills Samples, SampleRadius, SampleDist and Bounds from Nodes (Catmull-Rom). */
	void BuildSamples(FTreeBranch& Branch, const FTreeParams& Params);
}
