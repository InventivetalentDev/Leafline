#include "Tree/TreeMeshBuilder.h"
#include "AntTreeGame.h"

namespace
{
	constexpr int32 CapRings = 4;
	constexpr float NoOverlap = 10000.f; // UV3.y of a vertex that was not tested against another branch

	/** UV2 = (Hi, Lo): Along split so both halves are exact in half floats; Hi + Lo = Along. */
	FVector2D SplitAlong(double Along)
	{
		const double Hi = FMath::FloorToDouble(Along / 256.0) * 256.0;
		return FVector2D(Hi, Along - Hi);
	}

	/** Where a branch's tube rings sit in the vertex arrays: ring i starts at Start + i * (M + 1). */
	struct FBranchRange
	{
		int32 Start = 0;
		int32 M = 0;
		bool bValid = false;
	};

	void BuildBranch(const FTreeBranch& B, const FTreeParams& P, FTreeMeshData& Out, FBranchRange& Range)
	{
		const int32 NS = B.Samples.Num();
		if (NS < 2) return;

		// Tangents and parallel-transport frames so the rings do not twist.
		TArray<FVector, TInlineAllocator<256>> T, N;
		T.SetNum(NS);
		N.SetNum(NS);
		for (int32 i = 0; i < NS; ++i)
		{
			T[i] = (B.Samples[FMath::Min(i + 1, NS - 1)] - B.Samples[FMath::Max(i - 1, 0)]).GetSafeNormal();
			if (T[i].IsNearlyZero()) T[i] = i > 0 ? T[i - 1] : FVector::UpVector;
		}
		N[0] = FVector::CrossProduct(T[0], FMath::Abs(T[0].Z) < 0.9 ? FVector::UpVector : FVector::ForwardVector).GetSafeNormal();
		for (int32 i = 1; i < NS; ++i)
		{
			const FVector Rotated = FQuat::FindBetweenNormals(T[i - 1], T[i]).RotateVector(N[i - 1]);
			N[i] = (Rotated - T[i] * FVector::DotProduct(Rotated, T[i])).GetSafeNormal();
			if (N[i].IsNearlyZero()) N[i] = N[i - 1];
		}

		float MaxR = 0.f;
		for (float R : B.SampleRadius) MaxR = FMath::Max(MaxR, R);
		const int32 M = FMath::Clamp(FMath::RoundToInt(2.f * PI * MaxR / P.AroundSpacing), P.MinAroundVerts, P.MaxAroundVerts);
		const int32 RingVerts = M + 1; // duplicate seam vertex for continuous UVs
		Range.Start = Out.Vertices.Num();
		Range.M = M;
		Range.bValid = true;

		// Alpha = 1: not buried in another branch (the overlap pass in Build lowers it).
		auto BarkAt = [&](int32 i) { return B.BarkRadius.Num() == NS ? B.BarkRadius[i] : B.SampleRadius[i]; };
		const FLinearColor BaseColor(FMath::Clamp(B.Level, 0, 3) / 3.f, B.Random, 0.f, 1.f);
		const float TreeH = FMath::Max(P.TrunkHeight, 1.f);

		auto AddRing = [&](const FVector& Center, const FVector& Tan, const FVector& Nrm, float Radius, float NormalTilt, float Along, float BarkR)
		{
			const FVector Bin = FVector::CrossProduct(Tan, Nrm);
			for (int32 j = 0; j <= M; ++j)
			{
				const double Theta = 2.0 * PI * j / M;
				const double C = FMath::Cos(Theta), S = FMath::Sin(Theta);
				const FVector Dir = Nrm * C + Bin * S;
				const FVector V = Center + Dir * Radius;
				Out.Vertices.Add(V);
				Out.Normals.Add((Dir * FMath::Cos(NormalTilt) + Tan * FMath::Sin(NormalTilt)).GetSafeNormal());
				Out.UV0.Add(FVector2D(Theta * FMath::Max(Radius, 1.f), Along));
				Out.UV2.Add(SplitAlong(Along));
				Out.UV3.Add(FVector2D(BarkR, NoOverlap));
				Out.UV1.Add(FVector2D(C * Radius, S * Radius));
				FLinearColor Col = BaseColor;
				Col.B = FMath::Clamp((float)V.Z / TreeH, 0.f, 1.f);
				Out.Colors.Add(Col);
				Out.Tangents.Add(FProcMeshTangent(-Nrm * S + Bin * C, false));
			}
		};

		auto Stitch = [&](int32 RingA, int32 RingB)
		{
			for (int32 j = 0; j < M; ++j)
			{
				const int32 A = RingA + j, Bv = RingA + j + 1, C = RingB + j, D = RingB + j + 1;
				// UE front faces: (v1 - v0) x (v2 - v0) points away from the viewer, i.e. into the tube.
				Out.Triangles.Append({ A, C, Bv, Bv, C, D });
			}
		};

		int32 PrevRing = INDEX_NONE;
		for (int32 i = 0; i < NS; ++i)
		{
			const int32 Lo = FMath::Max(i - 1, 0), Hi = FMath::Min(i + 1, NS - 1);
			const float DS = B.SampleDist[Hi] - B.SampleDist[Lo];
			const float DRDS = DS > UE_SMALL_NUMBER ? (B.SampleRadius[Hi] - B.SampleRadius[Lo]) / DS : 0.f;
			const int32 Ring = Out.Vertices.Num();
			AddRing(B.Samples[i], T[i], N[i], B.SampleRadius[i], FMath::Atan(-DRDS), B.SampleDist[i], BarkAt(i));
			if (PrevRing != INDEX_NONE) Stitch(PrevRing, Ring);
			PrevRing = Ring;
		}

		// Rounded tip: a hemisphere of the last radius, like the field's capsule end.
		const FVector TipC = B.Samples.Last();
		const FVector TipT = T.Last();
		const float TipR = B.SampleRadius.Last();
		const float TipAlong = B.SampleDist.Last();
		for (int32 k = 1; k < CapRings; ++k)
		{
			const float Phi = 0.5f * PI * k / CapRings;
			const int32 Ring = Out.Vertices.Num();
			AddRing(TipC + TipT * (TipR * FMath::Sin(Phi)), TipT, N.Last(), TipR * FMath::Cos(Phi), Phi, TipAlong + TipR * Phi, BarkAt(NS - 1));
			Stitch(PrevRing, Ring);
			PrevRing = Ring;
		}
		const int32 Tip = Out.Vertices.Num();
		Out.Vertices.Add(TipC + TipT * TipR);
		Out.Normals.Add(TipT);
		Out.UV0.Add(FVector2D(0.0, TipAlong + TipR * 0.5f * PI));
		Out.UV2.Add(SplitAlong(TipAlong + TipR * 0.5f * PI));
		Out.UV3.Add(FVector2D(BarkAt(NS - 1), NoOverlap));
		Out.UV1.Add(FVector2D::ZeroVector);
		FLinearColor TipCol = BaseColor;
		TipCol.B = FMath::Clamp((float)(TipC.Z / TreeH), 0.f, 1.f);
		Out.Colors.Add(TipCol);
		Out.Tangents.Add(FProcMeshTangent(N.Last(), false));
		for (int32 j = 0; j < M; ++j)
		{
			Out.Triangles.Append({ PrevRing + j, Tip, PrevRing + j + 1 });
		}
		(void)RingVerts;
	}

	/** Signed distance from P to the surface of the tube around segment i of B (negative inside). */
	FORCEINLINE float SegSignedDist(const FVector& P, const FTreeBranch& B, int32 i, float* OutRadius = nullptr)
	{
		const FVector& A = B.Samples[i];
		const FVector AB = B.Samples[i + 1] - A;
		const double L2 = AB.SizeSquared();
		const double T = L2 > UE_DOUBLE_SMALL_NUMBER ? FMath::Clamp(FVector::DotProduct(P - A, AB) / L2, 0.0, 1.0) : 0.0;
		const float R = FMath::Lerp(B.SampleRadius[i], B.SampleRadius[i + 1], (float)T);
		if (OutRadius) *OutRadius = R;
		return (float)FVector::Dist(P, A + AB * T) - R;
	}

	/**
	 * UV3.y is the buried distance plus this fraction of the radius of the branch the vertex is buried in; the bark
	 * hides UV3.y below -BuriedDepth (15 cm, MPC_View). Across the 200 to 300 cm triangles of big branches the linearly
	 * interpolated distance dips far below the true one (the flare is bell-shaped and branches bend, so the solids are
	 * not convex): on the owner's tree a root lost visible bark up to about 200 cm "inside" the trunk (1100 cm radius).
	 * Scaling with the radius keeps a depth of about 235 cm by the trunk and about 33 cm inside a 90 cm limb, where the
	 * buried trunk bark made the black wedge.
	 */
	constexpr float BuriedDepthPerRadius = 0.2f;

	/** First index whose SampleDist is >= S (NS if none). */
	int32 FirstSampleAtLeast(const FTreeBranch& B, float S)
	{
		int32 Lo = 0, Hi = B.SampleDist.Num();
		while (Lo < Hi)
		{
			const int32 Mid = (Lo + Hi) / 2;
			if (B.SampleDist[Mid] < S) Lo = Mid + 1; else Hi = Mid;
		}
		return Lo;
	}

	/** Segment index range [OutLo, OutHi] (inclusive) covering the arc length interval [S0, S1]. False if empty. */
	bool SegmentWindow(const FTreeBranch& B, float S0, float S1, int32& OutLo, int32& OutHi)
	{
		const int32 NSeg = B.Samples.Num() - 1;
		if (NSeg < 1) return false;
		OutLo = FMath::Clamp(FirstSampleAtLeast(B, S0) - 1, 0, NSeg - 1);
		OutHi = FMath::Clamp(FirstSampleAtLeast(B, S1), 0, NSeg - 1);
		return OutHi >= OutLo;
	}

	// A = 0.5 + d / BuriedRange. The range must be wide: alpha is interpolated linearly across triangles of up to about
	// 60 cm, and a clamped end value moves the cut outward on both surfaces of a junction, which opened holes there (owner,
	// 13 Nov, with a range of 50 cm and a margin of 25). 400 cm keeps a step of 1.6 cm per 8-bit level.
	constexpr float BuriedMargin = 400.f; // rings and segments within this far outside the other tube are tested (UV3.y is unclamped)
	constexpr float AlphaClamp = 200.f;   // the vertex alpha (no longer read by the material) still clamps distances here
	constexpr float BuriedRange = 400.f;  // cm per full alpha range

	/**
	 * Lowers the alpha of one ring's vertices by their signed distance to the tubes of Other's segments SegLo..SegHi.
	 * Returns false when no segment comes within the margin of the ring (nothing to do).
	 */
	bool BuryRing(FTreeMeshData& Out, int32 FirstVert, int32 M, const FVector& Center, float RingR,
		const FTreeBranch& Other, int32 SegLo, int32 SegHi)
	{
		TArray<int32, TInlineAllocator<32>> Near;
		for (int32 s = SegLo; s <= SegHi; ++s)
		{
			if (SegSignedDist(Center, Other, s) - RingR < BuriedMargin) Near.Add(s);
		}
		if (Near.IsEmpty()) return false;
		for (int32 j = 0; j <= M; ++j)
		{
			const int32 V = FirstVert + j;
			const FVector& Pt = Out.Vertices[V];
			float D = TNumericLimits<float>::Max(), RAt = 0.f;
			for (int32 s : Near)
			{
				float Rs;
				const float Ds = SegSignedDist(Pt, Other, s, &Rs);
				if (Ds < D)
				{
					D = Ds;
					RAt = Rs;
				}
			}
			const float A = FMath::Clamp(0.5f + FMath::Min(D, AlphaClamp) / BuriedRange, 0.f, 1.f);
			float& Alpha = Out.Colors[V].A;
			Alpha = FMath::Min(Alpha, A);
			Out.UV3[V].Y = FMath::Min((double)Out.UV3[V].Y, (double)(D + BuriedDepthPerRadius * RAt));
		}
		return true;
	}
}

void TreeMeshBuilder::Build(const FTreeSkeleton& S, FTreeMeshData& Out)
{
	const double StartTime = FPlatformTime::Seconds();
	Out.Reset();
	TArray<FBranchRange> Ranges;
	Ranges.SetNum(S.Branches.Num());
	for (int32 b = 0; b < S.Branches.Num(); ++b)
	{
		BuildBranch(S.Branches[b], S.Params, Out, Ranges[b]);
	}

	// Overlap pass: alpha = clamp(0.5 + d / 400, 0, 1), d = signed distance (cm) to the surface of the parent (for a
	// child's vertices) or of a child's base (for a parent's vertices, only when the child's base ring is sealed inside
	// the parent); negative inside the other branch.
	const double OverlapStart = FPlatformTime::Seconds();
	int32 Unsealed = 0;
	for (int32 c = 0; c < S.Branches.Num(); ++c)
	{
		const FTreeBranch& Child = S.Branches[c];
		const int32 p = Child.Parent;
		if (!S.Branches.IsValidIndex(p) || p == c || !Ranges[c].bValid || !Ranges[p].bValid) continue;
		const FTreeBranch& Par = S.Branches[p];
		const int32 NSc = Child.Samples.Num(), NSp = Par.Samples.Num();
		const float Sp = FMath::Clamp(Child.ParentT, 0.f, 1.f) * Par.Length();
		const float Rc0 = Child.SampleRadius[0];
		const int32 NearP = FMath::Clamp(FirstSampleAtLeast(Par, Sp), 0, NSp - 1);
		const float Rp = Par.SampleRadius[NearP];

		// The child's rings against the parent, from the base until a ring is clear of the parent.
		int32 SegLo, SegHi;
		const float WinP = 2.f * (Rp + Rc0) + 60.f + BuriedMargin;
		const bool bParWindow = SegmentWindow(Par, Sp - WinP, Sp + WinP, SegLo, SegHi);
		// The child's tube starts with an open ring. Hiding the parent's bark inside the child is only safe when that
		// ring lies wholly inside the parent: otherwise the open end shows from outside, and with the parent's bark gone
		// behind it the player looks straight through the tree (owner, 13 Nov: holes where big limbs and roots meet the
		// trunk, whose base rings are wider than the trunk there).
		bool bBaseSealed = false;
		if (bParWindow)
		{
			float MaxD = -TNumericLimits<float>::Max();
			for (int32 j = 0; j <= Ranges[c].M; ++j)
			{
				const FVector& Pt = Out.Vertices[Ranges[c].Start + j];
				float D = TNumericLimits<float>::Max();
				for (int32 sg = SegLo; sg <= SegHi; ++sg) D = FMath::Min(D, SegSignedDist(Pt, Par, sg));
				MaxD = FMath::Max(MaxD, D);
			}
			bBaseSealed = MaxD < -15.f;
			Unsealed += bBaseSealed ? 0 : 1;
		}
		if (bParWindow)
		{
			const int32 RV = Ranges[c].M + 1;
			for (int32 i = 0; i < NSc; ++i)
			{
				const int32 First = Ranges[c].Start + i * RV;
				if (!BuryRing(Out, First, Ranges[c].M, Child.Samples[i], Child.SampleRadius[i], Par, SegLo, SegHi)) break;
			}
		}

		// The parent's rings near the junction against the child's base.
		const float WinC = 3.f * Rc0 + 60.f + BuriedMargin;
		int32 CLo, CHi;
		if (bBaseSealed && SegmentWindow(Child, 0.f, 2.f * (Rp + Rc0) + 60.f + BuriedMargin, CLo, CHi)
			&& SegmentWindow(Par, Sp - WinC, Sp + WinC, SegLo, SegHi))
		{
			const int32 RV = Ranges[p].M + 1;
			for (int32 i = SegLo; i <= FMath::Min(SegHi + 1, NSp - 1); ++i)
			{
				BuryRing(Out, Ranges[p].Start + i * RV, Ranges[p].M, Par.Samples[i], Par.SampleRadius[i], Child, CLo, CHi);
			}
		}
	}
	const double OverlapMs = (FPlatformTime::Seconds() - OverlapStart) * 1000.0;
	int32 Buried = 0;
	for (const FVector2D& Uv : Out.UV3)
	{
		if (Uv.Y < -15.0) ++Buried;
	}

	UE_LOG(LogLeafline, Log, TEXT("TreeMeshBuilder: %d vertices, %d triangles, %.1f ms (overlap pass %.1f ms, %d buried vertices, %d child bases not sealed in their parent)"),
		Out.Vertices.Num(), Out.Triangles.Num() / 3, (FPlatformTime::Seconds() - StartTime) * 1000.0, OverlapMs, Buried, Unsealed);
}
