#pragma once

#include "CoreMinimal.h"
#include "Tree/TreeTypes.h"

/**
 * Signed distance to the tree surface (branches smooth-unioned with the ground plane).
 * Everything that touches the tree asks this instead of using collision.
 */
class FTreeField
{
public:
	void Build(const FTreeSkeleton& Skeleton);
	void Reset();
	bool IsValid() const { return Branches.Num() > 0; }

	/** D(p). OutBranch receives the nearest branch, or INDEX_NONE when the ground is nearest. */
	double Distance(const FVector& P, int32* OutBranch = nullptr) const;

	/** Normalised outward gradient. */
	FVector Gradient(const FVector& P) const;

	/** Unnormalised gradient estimate (tetrahedral differences). OutD receives an estimate of D(P). */
	FVector RawGradient(const FVector& P, double* OutD = nullptr) const;

	/** Moves P onto the level set D = Height. Returns false if it did not converge to within 1 cm. */
	bool Project(FVector& P, FVector& OutNormal, double Height, double* OutError = nullptr) const;

	/** Sphere-traces from From to To with a probe radius. OutT is the hit fraction (0..1) along the segment. */
	bool March(const FVector& From, const FVector& To, double Radius, double& OutT) const;

	double GroundZ = 0.0;

	/** Number of Distance() calls since the counter was last reset (for profiling). */
	mutable int64 EvalCount = 0;

private:
	/** A piece of branch between two mesh rings: it runs from the ring plane at A (normal NA) to the one at B (NB). */
	struct FSeg
	{
		FVector A;
		FVector BA;
		FVector NA;
		FVector NB;
		double InvLenSq;
		float RA;
		float RB;
	};
	struct FBranchRange
	{
		int32 First = 0;
		int32 Num = 0;
		FBox Bounds = FBox(ForceInit);
		float K = 0.f;
	};
	struct FGroup
	{
		FBox Bounds = FBox(ForceInit);
		TArray<int32> Branches; // in blend order
	};

	double BranchDistance(int32 Branch, const FVector& P) const;
	/** The ground plane with the nest mound on it. */
	double GroundDistance(const FVector& P) const;

	TArray<FSeg> Segs;
	TArray<FBranchRange> Branches;
	TArray<FGroup> Groups;
	double GroundK = 60.0;
	/** Nest mound: a half-buried ellipsoid on the ground (radii zero when there is no nest). */
	FVector MoundCenter = FVector::ZeroVector;
	FVector MoundRadii = FVector::ZeroVector;
	double CullMargin = 200.0;
};
