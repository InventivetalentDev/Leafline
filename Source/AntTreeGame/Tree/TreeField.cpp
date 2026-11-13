#include "Tree/TreeField.h"
#include "AntTreeGame.h"

namespace
{
	constexpr double BigDist = 1.0e12;

	/** Lower bound of the distance from P to any surface inside Box (very negative when P is inside the box). */
	FORCEINLINE double BoxLowerBound(const FBox& Box, const FVector& P)
	{
		const double Sq = Box.ComputeSquaredDistanceToPoint(P);
		return Sq > 0.0 ? FMath::Sqrt(Sq) : -BigDist;
	}
}

void FTreeField::Reset()
{
	MoundRadii = FVector::ZeroVector;
	Segs.Reset();
	Branches.Reset();
	Groups.Reset();
	bAnyFillets = false;
	bGroundFillet = false;
}

void FTreeField::Build(const FTreeSkeleton& S, bool bWithFillets)
{
	Reset();
	GroundK = S.Params.GroundBlend;
	if (S.NestSpots.Num() > 0 && S.Params.NestMoundRadius > 0.f && S.Params.NestMoundHeight > 0.f)
	{
		MoundCenter = FVector(S.NestSpots[0].X, S.NestSpots[0].Y, GroundZ);
		MoundRadii = FVector(S.Params.NestMoundRadius, S.Params.NestMoundRadius, S.Params.NestMoundHeight);
	}

	double MaxK = GroundK;
	for (const FTreeBranch& B : S.Branches)
	{
		FBranchRange Range;
		Range.First = Segs.Num();
		// Ring planes exactly as TreeMeshBuilder orients its rings (central-difference tangents), so the field's pieces
		// meet where the mesh's do: no gaps on the outer side of bends and no overlaps on the inner side.
		const int32 NS = B.Samples.Num();
		TArray<FVector, TInlineAllocator<256>> Tan;
		Tan.SetNum(NS);
		for (int32 i = 0; i < NS; ++i)
		{
			Tan[i] = (B.Samples[FMath::Min(i + 1, NS - 1)] - B.Samples[FMath::Max(i - 1, 0)]).GetSafeNormal();
			if (Tan[i].IsNearlyZero()) Tan[i] = i > 0 ? Tan[i - 1] : FVector::UpVector;
		}
		for (int32 i = 0; i + 1 < NS; ++i)
		{
			FSeg Seg;
			Seg.A = B.Samples[i];
			Seg.BA = B.Samples[i + 1] - B.Samples[i];
			Seg.NA = Tan[i];
			Seg.NB = Tan[i + 1];
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
		if (bWithFillets && B.FilletK > B.BlendK && B.FilletOuter > B.FilletInner)
		{
			Range.KBig = B.FilletK;
			Range.J = B.FilletCenter;
			Range.RIn = B.FilletInner;
			Range.ROut = B.FilletOuter;
			bAnyFillets = true;
		}
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
		if (Branches[b].KBig > 0.f)
		{
			Groups[G].Fillets.Add(b);
			Groups[G].FilletBounds += FBox(Branches[b].J - FVector(Branches[b].ROut), Branches[b].J + FVector(Branches[b].ROut));
		}
	}
	CullMargin = 3.0 * MaxK;
	// With fillets the ground blends in wider where the trunk and roots meet it. The cull margin only has to cover that
	// radius once (a branch farther than ground + k cannot change the blend with the ground), so it stays as it was.
	if (bWithFillets && S.Params.bFillets && S.Params.FilletGroundK > GroundK)
	{
		GroundK = S.Params.FilletGroundK;
		bGroundFillet = true;
		CullMargin = FMath::Max(CullMargin, GroundK + 10.0);
	}

	int32 NumFillets = 0;
	for (const FGroup& G : Groups) NumFillets += G.Fillets.Num();
	UE_LOG(LogLeafline, Log, TEXT("TreeField: %d branches, %d segments, %d groups, cull margin %.0f cm, %d fillet zones, ground blend %.0f cm"),
		Branches.Num(), Segs.Num(), Groups.Num(), CullMargin, NumFillets, GroundK);
}

double FTreeField::BlendKAt(int32 Branch, const FVector& P) const
{
	const FBranchRange& R = Branches[Branch];
	if (R.KBig <= 0.f) return R.K;
	const double D2 = FVector::DistSquared(P, R.J);
	if (D2 >= (double)R.ROut * R.ROut) return R.K;
	if (D2 <= (double)R.RIn * R.RIn) return R.KBig;
	// Smoothstep from the inner to the outer radius: the blend radius, and so the field, stays smooth.
	const double W = 1.0 - FMath::SmoothStep((double)R.RIn, (double)R.ROut, FMath::Sqrt(D2));
	return R.K + (R.KBig - R.K) * W;
}

double FTreeField::BranchDistance(int32 Branch, const FVector& P, int32 SegLo, int32 SegHi) const
{
	const FBranchRange& R = Branches[Branch];
	double Best = BigDist;
	const int32 Lo = FMath::Max(SegLo, 0), Hi = FMath::Min(SegHi, R.Num - 1);
	const FSeg* S = Segs.GetData() + R.First + Lo;
	for (int32 i = Lo; i <= Hi; ++i, ++S)
	{
		const FVector PA = P - S->A;
		const double TRaw = FVector::DotProduct(PA, S->BA) * S->InvLenSq;
		const double AxisDist = (PA - S->BA * FMath::Clamp(TRaw, 0.0, 1.0)).Size();
		// The piece reaches a little past the axis segment where a ring plane leans over a bend, but every point of it
		// stays within about 1/0.85 of its larger radius of the segment: a cheap lower bound to skip far pieces.
		const double MaxR = FMath::Max(S->RA, S->RB);
		if (AxisDist * 0.85 - MaxR >= Best) continue;

		// Which side of the two ring planes. Outside them the neighbour owns the point, except at the branch's true
		// ends, which are rounded caps of the end radius like the mesh's tip (the start is buried in the parent).
		// A hair of overlap at the planes: mesh vertices lie exactly on them, and rounding must not drop a point
		// between two neighbours (it would read as a hole of metres).
		constexpr double PlaneSlack = 1e-3;
		const double SA = FVector::DotProduct(PA, S->NA);
		if (SA < -PlaneSlack)
		{
			if (i == 0) Best = FMath::Min(Best, PA.Size() - S->RA);
			continue;
		}
		const FVector PB = PA - S->BA;
		const double SB = FVector::DotProduct(PB, S->NB);
		if (SB > PlaneSlack)
		{
			if (i == R.Num - 1) Best = FMath::Min(Best, PB.Size() - S->RB);
			continue;
		}

		// Between the planes: blend centre, radius and axis from ring A to ring B by where P sits between the planes.
		// On a ring plane this is exactly that ring (radius R about its centre, perpendicular to its tangent), and two
		// neighbours agree on their shared plane, so the field is continuous and passes through every mesh ring.
		const double Den = SA - SB;
		const double T = Den > 1e-9 ? FMath::Clamp(SA / Den, 0.0, 1.0) : 0.0;
		const FVector C = S->A + S->BA * T;
		const double Rad = S->RA + (S->RB - S->RA) * T;
		const FVector Ax = (S->NA + (S->NB - S->NA) * T).GetSafeNormal();
		const FVector Q = P - C;
		const double Along = FVector::DotProduct(Q, Ax);
		const double Rho = FMath::Sqrt(FMath::Max(Q.SizeSquared() - Along * Along, 0.0));
		Best = FMath::Min(Best, Rho - Rad);
	}
	return Best;
}

double FTreeField::GroundDistance(const FVector& P) const
{
	const double Plane = P.Z - GroundZ;
	if (MoundRadii.Z <= 0.0) return Plane;
	// Ellipsoid bound (k0 (k0 - 1) / k1): exact on the surface, which is all the walker needs.
	const FVector Q = P - MoundCenter;
	if (FVector::DistSquared2D(Q, FVector::ZeroVector) > FMath::Square(MoundRadii.X + 50.0)) return Plane;
	const double K0 = (Q / MoundRadii).Size();
	const double K1 = (Q / (MoundRadii * MoundRadii)).Size();
	const double Mound = K1 > 1e-9 ? K0 * (K0 - 1.0) / K1 : -MoundRadii.Z;
	return SMin(Plane, Mound, 4.0);
}

double FTreeField::Distance(const FVector& P, int32* OutBranch) const
{
	++EvalCount;
	return Eval(P, OutBranch);
}

double FTreeField::Eval(const FVector& P, int32* OutBranch, bool bGround) const
{
	const double Ground = bGround ? GroundDistance(P) : BigDist;
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

	// Branches farther than U + 3k cannot change the smooth union. Inside a fillet zone k is that zone's fillet radius.
	double Margin = CullMargin;
	if (bAnyFillets)
	{
		for (const FGroup& G : Groups)
		{
			if (G.Fillets.IsEmpty() || !G.FilletBounds.IsInsideOrOn(P)) continue;
			for (int32 b : G.Fillets)
			{
				const FBranchRange& R = Branches[b];
				if (FVector::DistSquared(P, R.J) < (double)R.ROut * R.ROut) Margin = FMath::Max(Margin, 3.0 * R.KBig);
			}
		}
	}
	const double Threshold = U + Margin;
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
			Res = bAny ? SMin(Res, D, Branches[b].KBig > 0.f ? BlendKAt(b, P) : Branches[b].K) : D;
			bAny = true;
		}
	}
	if (!bGround) return bAny ? Res : BigDist;
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
	for (int32 i = 0; i < 96; ++i)
	{
		const double D = Distance(From + Dir * T) - Radius;
		if (D < 0.5)
		{
			OutT = T / L;
			return true;
		}
		// The distance overestimates clearance on steep tapers (the root flare), so step conservatively.
		T += D * 0.6;
		if (T >= L) return false;
	}
	// Out of steps while still grazing a surface: report a hit where we stopped rather than a clear path.
	OutT = FMath::Clamp(T / L, 0.0, 1.0);
	return true;
}
