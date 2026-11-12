#include "Tree/TreeField.h"
#include "AntTreeGame.h"

namespace
{
	constexpr double BigDist = 1.0e12;

	/** Polynomial smooth minimum. k = blend radius; k <= 0 is a hard min. */
	FORCEINLINE double SMin(double A, double B, double K)
	{
		if (K <= 0.0) return FMath::Min(A, B);
		const double H = FMath::Max(K - FMath::Abs(A - B), 0.0) / K;
		return FMath::Min(A, B) - H * H * K * 0.25;
	}

	/** Lower bound of the distance from P to any surface inside Box (very negative when P is inside the box). */
	FORCEINLINE double BoxLowerBound(const FBox& Box, const FVector& P)
	{
		const double Sq = Box.ComputeSquaredDistanceToPoint(P);
		return Sq > 0.0 ? FMath::Sqrt(Sq) : -BigDist;
	}
}

void FTreeField::Reset()
{
	Segs.Reset();
	Branches.Reset();
	Groups.Reset();
}

void FTreeField::Build(const FTreeSkeleton& S)
{
	Reset();
	GroundK = S.Params.GroundBlend;

	double MaxK = GroundK;
	for (const FTreeBranch& B : S.Branches)
	{
		FBranchRange Range;
		Range.First = Segs.Num();
		for (int32 i = 0; i + 1 < B.Samples.Num(); ++i)
		{
			FSeg Seg;
			Seg.A = B.Samples[i];
			Seg.BA = B.Samples[i + 1] - B.Samples[i];
			const double L2 = Seg.BA.SizeSquared();
			Seg.InvLenSq = L2 > UE_SMALL_NUMBER ? 1.0 / L2 : 0.0;
			Seg.RA = B.SampleRadius[i];
			Seg.RB = B.SampleRadius[i + 1];
			Segs.Add(Seg);
		}
		Range.Num = Segs.Num() - Range.First;
		Range.Bounds = B.Bounds;
		Range.K = B.BlendK;
		MaxK = FMath::Max(MaxK, (double)B.BlendK);
		Branches.Add(Range);
	}

	// Groups: the trunk, each root, and each limb with all its descendants. Blend order is trunk, roots, limbs.
	TMap<int32, int32> RootToGroup;
	Groups.AddDefaulted();
	for (int32 b = 0; b < S.Branches.Num(); ++b)
	{
		int32 A = b;
		while (S.Branches[A].Level > 1 && S.Branches[A].Parent != INDEX_NONE) A = S.Branches[A].Parent;
		int32 G = 0;
		if (S.Branches[A].Level != 0)
		{
			if (int32* Found = RootToGroup.Find(A)) G = *Found;
			else G = RootToGroup.Add(A, Groups.AddDefaulted());
		}
		Groups[G].Branches.Add(b);
		Groups[G].Bounds += Branches[b].Bounds;
	}
	CullMargin = 3.0 * MaxK;

	UE_LOG(LogLeafline, Log, TEXT("TreeField: %d branches, %d segments, %d groups, cull margin %.0f cm"),
		Branches.Num(), Segs.Num(), Groups.Num(), CullMargin);
}

double FTreeField::BranchDistance(int32 Branch, const FVector& P) const
{
	const FBranchRange& R = Branches[Branch];
	double Best = BigDist;
	const FSeg* S = Segs.GetData() + R.First;
	for (int32 i = 0; i < R.Num; ++i, ++S)
	{
		const FVector PA = P - S->A;
		const double T = FMath::Clamp(FVector::DotProduct(PA, S->BA) * S->InvLenSq, 0.0, 1.0);
		const double D = (PA - S->BA * T).Size() - (S->RA + (S->RB - S->RA) * T);
		Best = FMath::Min(Best, D);
	}
	return Best;
}

double FTreeField::Distance(const FVector& P, int32* OutBranch) const
{
	++EvalCount;
	const double Ground = P.Z - GroundZ;
	if (Groups.Num() == 0)
	{
		if (OutBranch) *OutBranch = INDEX_NONE;
		return Ground;
	}

	TArray<double, TInlineAllocator<128>> GroupLB;
	GroupLB.SetNumUninitialized(Groups.Num());
	int32 NearG = INDEX_NONE;
	double NearGLB = BigDist;
	for (int32 g = 0; g < Groups.Num(); ++g)
	{
		GroupLB[g] = BoxLowerBound(Groups[g].Bounds, P);
		if (GroupLB[g] < NearGLB)
		{
			NearGLB = GroupLB[g];
			NearG = g;
		}
	}

	// Seed an upper bound U from the most promising branch of the nearest group.
	double U = Ground;
	int32 SeedBranch = INDEX_NONE;
	double SeedD = BigDist;
	if (NearG != INDEX_NONE)
	{
		double BestLB = BigDist;
		for (int32 b : Groups[NearG].Branches)
		{
			const double LB = BoxLowerBound(Branches[b].Bounds, P);
			if (LB < BestLB)
			{
				BestLB = LB;
				SeedBranch = b;
			}
		}
		if (SeedBranch != INDEX_NONE)
		{
			SeedD = BranchDistance(SeedBranch, P);
			U = FMath::Min(U, SeedD);
		}
	}

	// Branches farther than U + 3k cannot change the smooth union.
	const double Threshold = U + CullMargin;
	double Res = 0.0;
	bool bAny = false;
	int32 Nearest = INDEX_NONE;
	double NearestD = BigDist;
	for (int32 g = 0; g < Groups.Num(); ++g)
	{
		if (GroupLB[g] >= Threshold) continue;
		for (int32 b : Groups[g].Branches)
		{
			if (b != SeedBranch && BoxLowerBound(Branches[b].Bounds, P) >= Threshold) continue;
			const double D = b == SeedBranch ? SeedD : BranchDistance(b, P);
			if (D < NearestD)
			{
				NearestD = D;
				Nearest = b;
			}
			Res = bAny ? SMin(Res, D, Branches[b].K) : D;
			bAny = true;
		}
	}
	Res = bAny ? SMin(Res, Ground, GroundK) : Ground;
	if (OutBranch) *OutBranch = NearestD <= Ground ? Nearest : INDEX_NONE;
	return Res;
}

FVector FTreeField::RawGradient(const FVector& P, double* OutD) const
{
	constexpr double E = 0.5;
	static const FVector K1(1, -1, -1), K2(-1, -1, 1), K3(-1, 1, -1), K4(1, 1, 1);
	const double V1 = Distance(P + K1 * E);
	const double V2 = Distance(P + K2 * E);
	const double V3 = Distance(P + K3 * E);
	const double V4 = Distance(P + K4 * E);
	if (OutD) *OutD = 0.25 * (V1 + V2 + V3 + V4);
	return (K1 * V1 + K2 * V2 + K3 * V3 + K4 * V4) / (4.0 * E);
}

FVector FTreeField::Gradient(const FVector& P) const
{
	const FVector G = RawGradient(P);
	return G.SizeSquared() > 1e-12 ? G.GetUnsafeNormal() : FVector::UpVector;
}

bool FTreeField::Project(FVector& P, FVector& OutNormal, double Height, double* OutError) const
{
	FVector G = FVector::UpVector;
	for (int32 i = 0; i < 8; ++i)
	{
		double D;
		G = RawGradient(P, &D);
		const double Err = D - Height;
		const double GL2 = G.SizeSquared();
		if (GL2 < 1e-10 || FMath::Abs(Err) < 0.02) break;
		// Newton step along the gradient, divided by |grad|^2 because the smooth union is not an exact SDF.
		FVector Step = G * (Err / GL2);
		const double MaxStep = FMath::Max(FMath::Abs(Err) * 2.0, 1.0);
		const double StepLen = Step.Size();
		if (StepLen > MaxStep) Step *= MaxStep / StepLen;
		P -= Step;
	}
	const double Final = Distance(P);
	const double Err = FMath::Abs(Final - Height);
	G = RawGradient(P);
	OutNormal = G.SizeSquared() > 1e-12 ? G.GetUnsafeNormal() : FVector::UpVector;
	if (OutError) *OutError = Err;
	return Err < 1.0;
}

bool FTreeField::March(const FVector& From, const FVector& To, double Radius, double& OutT) const
{
	const FVector Delta = To - From;
	const double L = Delta.Size();
	OutT = 1.0;
	if (L < 1e-3) return false;
	const FVector Dir = Delta / L;
	double T = 0.0;
	for (int32 i = 0; i < 64; ++i)
	{
		const double D = Distance(From + Dir * T) - Radius;
		if (D < 0.5)
		{
			OutT = T / L;
			return true;
		}
		T += D * 0.9;
		if (T >= L) return false;
	}
	return false;
}
