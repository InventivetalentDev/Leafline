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
	/** bWithFillets false ignores the branches' fillet zones (every branch blends with BlendK): the field as it was before
	 *  fillets, which the whole sim uses (ATreeActor::GetField); with them, the surface the skirts lie on. */
	void Build(const FTreeSkeleton& Skeleton, bool bWithFillets = true);
	void Reset();
	bool IsValid() const { return Branches.Num() > 0; }
	bool HasFillets() const { return bAnyFillets || bGroundFillet; }
	bool HasGroundFillet() const { return bGroundFillet; }
	/** The blend radius of the ground with the tree (FTreeParams::FilletGroundK with fillets, else GroundBlend). */
	double GetGroundK() const { return GroundK; }

	/** D(p). OutBranch receives the nearest branch, or INDEX_NONE when the ground is nearest. */
	double Distance(const FVector& P, int32* OutBranch = nullptr) const;

	/** D(p) without touching EvalCount: safe to call from several threads at once. */
	double DistanceMT(const FVector& P) const { return Eval(P, nullptr); }
	/** The tree alone (branches blended, no ground), thread-safe. */
	double TreeDistanceMT(const FVector& P) const { return Eval(P, nullptr, false); }
	/** The ground plane with the nest mound, thread-safe. */
	double GroundDistanceMT(const FVector& P) const { return GroundDistance(P); }

	/** Normalised outward gradient. */
	FVector Gradient(const FVector& P) const;

	/** Unnormalised gradient estimate (tetrahedral differences). OutD receives an estimate of D(P). */
	FVector RawGradient(const FVector& P, double* OutD = nullptr) const;

	/** Moves P onto the level set D = Height. Returns false if it did not converge to within 1 cm. */
	bool Project(FVector& P, FVector& OutNormal, double Height, double* OutError = nullptr) const;

	/** Sphere-traces from From to To with a probe radius. OutT is the hit fraction (0..1) along the segment. */
	bool March(const FVector& From, const FVector& To, double Radius, double& OutT) const;

	/** Distance to one branch's own surface (no blending). SegLo..SegHi limits it to those pieces (inclusive); the branch's
	 *  true ends keep their rounded caps, and points outside the window's end planes get a huge distance. */
	double BranchDistance(int32 Branch, const FVector& P, int32 SegLo = 0, int32 SegHi = MAX_int32) const;
	int32 NumBranchSegments(int32 Branch) const { return Branches.IsValidIndex(Branch) ? Branches[Branch].Num : 0; }

	/** The blend radius the branch joins the field with at P: BlendK, or its fillet radius inside its junction zone. */
	double BlendKAt(int32 Branch, const FVector& P) const;

	/** Polynomial smooth minimum. k = blend radius; k <= 0 is a hard min. */
	static FORCEINLINE double SMin(double A, double B, double K)
	{
		if (K <= 0.0) return FMath::Min(A, B);
		const double H = FMath::Max(K - FMath::Abs(A - B), 0.0) / K;
		return FMath::Min(A, B) - H * H * K * 0.25;
	}

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
		/** Fillet zone (KBig > K): K eases from KBig within RIn of J to K at ROut. */
		float KBig = 0.f;
		FVector J = FVector::ZeroVector;
		float RIn = 0.f;
		float ROut = 0.f;
	};
	struct FGroup
	{
		FBox Bounds = FBox(ForceInit);
		TArray<int32> Branches; // in blend order
		/** Branches of the group with a fillet zone, and the box round those zones. */
		TArray<int32> Fillets;
		FBox FilletBounds = FBox(ForceInit);
	};

	double Eval(const FVector& P, int32* OutBranch, bool bGround = true) const;
	/** The ground plane with the nest mound on it. */
	double GroundDistance(const FVector& P) const;

	TArray<FSeg> Segs;
	TArray<FBranchRange> Branches;
	TArray<FGroup> Groups;
	double GroundK = 60.0;
	/** Nest mound: a half-buried ellipsoid on the ground (radii zero when there is no nest). */
	FVector MoundCenter = FVector::ZeroVector;
	FVector MoundRadii = FVector::ZeroVector;
	/** 3 x the largest blend radius outside fillet zones; inside one, 3 x that zone's fillet radius if larger. */
	double CullMargin = 200.0;
	bool bAnyFillets = false;
	bool bGroundFillet = false;
};
