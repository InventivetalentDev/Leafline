#include "Tree/TreeSkeleton.h"
#include "AntTreeGame.h"
#include "Algo/BinarySearch.h"

namespace
{
	constexpr float GoldenAngle = 2.39996323f;
	constexpr double SkelBigDist = 1.0e9;

	FVector CatmullRom(const FVector& P0, const FVector& P1, const FVector& P2, const FVector& P3, double T)
	{
		const double T2 = T * T, T3 = T2 * T;
		return 0.5 * ((2.0 * P1) + (-P0 + P2) * T + (2.0 * P0 - 5.0 * P1 + 4.0 * P2 - P3) * T2 + (-P0 + 3.0 * P1 - 3.0 * P2 + P3) * T3);
	}

	double SegDist(const FVector& P, const FVector& A, const FVector& B, double& OutT)
	{
		const FVector BA = B - A;
		const double L2 = BA.SizeSquared();
		OutT = L2 > UE_SMALL_NUMBER ? FMath::Clamp(FVector::DotProduct(P - A, BA) / L2, 0.0, 1.0) : 0.0;
		return FVector::Dist(P, A + BA * OutT);
	}

	struct FGrower
	{
		FTreeSkeleton& S;
		const FTreeParams& P;
		FRandomStream R;

		FGrower(FTreeSkeleton& InS, const FTreeParams& InP, int32 Seed) : S(InS), P(InP), R(Seed) {}

		float TrunkRadiusAt(float H) const
		{
			const float Hc = FMath::Max(H, 0.f);
			const float Hn = FMath::Clamp(Hc / P.TrunkHeight, 0.f, 1.f);
			float Rad = P.TrunkTopRadius + (P.TrunkRadius - P.TrunkTopRadius) * FMath::Pow(1.f - Hn, P.TrunkTaperExp);
			Rad *= 1.f + P.FlareMul * FMath::Exp(-Hc / FMath::Max(P.FlareHeight, 1.f));
			return Rad;
		}

		/** Trunk sample at world height H (the trunk is close to vertical, so Z is monotonic along it). */
		FVector TrunkAt(float H, float& OutRadius, FVector& OutTangent, float& OutT) const
		{
			const FTreeBranch& T = S.Branches[0];
			int32 Lo = 0, Hi = T.Samples.Num() - 1;
			while (Hi - Lo > 1)
			{
				const int32 Mid = (Lo + Hi) / 2;
				if (T.Samples[Mid].Z < H) Lo = Mid; else Hi = Mid;
			}
			const double Span = T.Samples[Hi].Z - T.Samples[Lo].Z;
			const double F = Span > UE_SMALL_NUMBER ? FMath::Clamp((H - T.Samples[Lo].Z) / Span, 0.0, 1.0) : 0.0;
			OutRadius = FMath::Lerp(T.SampleRadius[Lo], T.SampleRadius[Hi], (float)F);
			OutTangent = (T.Samples[Hi] - T.Samples[Lo]).GetSafeNormal();
			OutT = FMath::Lerp(T.SampleDist[Lo], T.SampleDist[Hi], (float)F) / FMath::Max(T.Length(), 1.f);
			return FMath::Lerp(T.Samples[Lo], T.Samples[Hi], F);
		}

		/** Smallest gap between a sphere and any grown branch. */
		double Clearance(const FVector& C, float Radius, int32 Ignore, FVector& OutAway) const
		{
			double Best = SkelBigDist;
			for (int32 b = 0; b < S.Branches.Num(); ++b)
			{
				if (b == Ignore) continue;
				const FTreeBranch& Br = S.Branches[b];
				if (Br.Samples.Num() < 2) continue;
				const double LB = FMath::Sqrt(Br.Bounds.ComputeSquaredDistanceToPoint(C)) - Radius;
				if (LB >= Best) continue;
				for (int32 i = 0; i + 1 < Br.Samples.Num(); ++i)
				{
					double T;
					const double D = SegDist(C, Br.Samples[i], Br.Samples[i + 1], T);
					const double Gap = D - FMath::Lerp(Br.SampleRadius[i], Br.SampleRadius[i + 1], (float)T) - Radius;
					if (Gap < Best)
					{
						Best = Gap;
						OutAway = (C - FMath::Lerp(Br.Samples[i], Br.Samples[i + 1], T)).GetSafeNormal();
					}
				}
			}
			return Best;
		}

		int32 GrowBranch(int32 Level, int32 Parent, float ParentT, const FVector& Start, FVector Dir, float Length,
			float BaseR, float TipR, float Wobble, float UpBias, bool bAvoid, float ParentRadius)
		{
			FTreeBranch B;
			B.Level = Level;
			B.Parent = Parent;
			B.ParentT = ParentT;
			B.Random = R.FRand();

			const int32 N = FMath::Clamp(FMath::RoundToInt(Length / FMath::Max(BaseR * 3.f, Length / 12.f)), 3, 12);
			const float Seg = Length / N;
			B.Nodes.Add({ Start, BaseR });
			FVector Pos = Start;
			for (int32 i = 1; i <= N; ++i)
			{
				const float F = float(i) / N;
				const float Rad = FMath::Lerp(BaseR, TipR, FMath::Pow(F, 0.9f));
				Dir = (Dir + FVector::UpVector * UpBias + R.VRand() * Wobble).GetSafeNormal();
				FVector Cand = Pos + Dir * Seg;
				if (bAvoid)
				{
					bool bOk = false;
					for (int32 Attempt = 0; Attempt < 4; ++Attempt)
					{
						const bool bNearBase = FVector::Dist(Cand, Start) < ParentRadius + Rad * 2.f + P.AvoidMargin;
						FVector Away = FVector::ZeroVector;
						const double Gap = Clearance(Cand, Rad, bNearBase ? Parent : INDEX_NONE, Away);
						const double GroundGap = Cand.Z - Rad - 150.0;
						if (Gap >= P.AvoidMargin && GroundGap >= 0.0)
						{
							bOk = true;
							break;
						}
						if (GroundGap < 0.0) Away += FVector::UpVector;
						Dir = (Dir + Away.GetSafeNormal() * 0.9f).GetSafeNormal();
						Cand = Pos + Dir * Seg;
					}
					if (!bOk) break;
				}
				Pos = Cand;
				B.Nodes.Add({ Pos, Rad });
			}
			if (B.Nodes.Num() < 3) return INDEX_NONE;

			B.BlendK = FMath::Min(P.BlendFrac * BaseR, P.BlendMax); // small: the mesh has no fillets, so a big blend lifts the ant off the visible surface
			B.bLeafy = Level >= 2 && Start.Z >= P.LowestLeafyHeight;
			TreeSkeleton::BuildSamples(B, P);
			return S.Branches.Add(MoveTemp(B));
		}

		/** A direction leaving a parent with tangent T at angle Fork, rotated Azimuth around it; never steeply down. */
		FVector ChildDir(const FVector& T, float Azimuth, float Fork) const
		{
			FVector Side = FVector::CrossProduct(T, FVector::UpVector);
			if (Side.SizeSquared() < 1e-4) Side = FVector::CrossProduct(T, FVector::ForwardVector);
			Side.Normalize();
			FVector Up = FVector::CrossProduct(Side, T).GetSafeNormal();
			if (Up.Z < 0.f) Up = -Up;
			FVector Perp = Side * FMath::Cos(Azimuth) + Up * FMath::Sin(Azimuth);
			if (Perp.Z < -0.35f) Perp.Z = -0.35f - (Perp.Z + 0.35f) * 0.3f;
			Perp = (Perp - T * FVector::DotProduct(Perp, T)).GetSafeNormal();
			return (T * FMath::Cos(Fork) + Perp * FMath::Sin(Fork)).GetSafeNormal();
		}

		void GrowTrunk()
		{
			FTreeBranch T;
			T.Level = 0;
			T.Random = R.FRand();
			const float LeanAz = R.FRandRange(0.f, 2.f * PI);
			const FVector LeanDir(FMath::Cos(LeanAz), FMath::Sin(LeanAz), 0.f);
			const float Lean = FMath::DegreesToRadians(P.TrunkLeanDeg * R.FRandRange(0.5f, 1.f));
			const float Ph1 = R.FRandRange(0.f, 2.f * PI), Ph2 = R.FRandRange(0.f, 2.f * PI);
			const float WobbleAmp = P.TrunkWobble * P.TrunkRadius * 4.f;
			const float H = P.TrunkHeight;
			const int32 N = 30;
			for (int32 i = 0; i <= N; ++i)
			{
				const float Z = -P.TrunkSinkDepth + (H + P.TrunkSinkDepth) * FMath::Pow(float(i) / N, 1.6f);
				const float Hp = FMath::Max(Z, 0.f);
				FVector Lat = LeanDir * FMath::Tan(Lean) * Hp * (0.6f + 0.4f * Hp / H);
				const float W = WobbleAmp * FMath::Sqrt(Hp / H);
				Lat += FVector(FMath::Sin(Hp * 2.f * PI / (H * 0.45f) + Ph1), FMath::Sin(Hp * 2.f * PI / (H * 0.3f) + Ph2), 0.f) * W;
				T.Nodes.Add({ FVector(Lat.X, Lat.Y, Z), TrunkRadiusAt(Z) });
			}
			T.BlendK = 0.f;
			TreeSkeleton::BuildSamples(T, P);
			S.Branches.Add(MoveTemp(T));
		}

		void GrowRoots()
		{
			const float Base0 = TrunkRadiusAt(0.f);
			float Rad, TParent;
			FVector Tan;
			const FVector Start = TrunkAt(500.f, Rad, Tan, TParent);
			const float AzOffset = R.FRandRange(0.f, 2.f * PI);
			TArray<float> Azimuths;
			for (int32 k = 0; k < P.NumRoots; ++k)
			{
				const float Az = AzOffset + 2.f * PI * k / FMath::Max(P.NumRoots, 1) + R.FRandRange(-0.3f, 0.3f);
				const FVector Horiz(FMath::Cos(Az), FMath::Sin(Az), 0.f);
				const float Dip = FMath::DegreesToRadians(P.RootDipDeg * R.FRandRange(0.6f, 1.2f));
				const FVector Dir = Horiz * FMath::Cos(Dip) - FVector::UpVector * FMath::Sin(Dip);
				const float Len = P.RootLength * R.FRandRange(0.8f, 1.2f);
				if (GrowBranch(-1, 0, TParent, Start, Dir, Len, Base0 * P.RootRadiusMul, 40.f, 0.08f, -0.03f, false, Base0) != INDEX_NONE)
				{
					Azimuths.Add(Az);
				}
			}
			// Nest spots sit on the ground halfway between neighbouring roots.
			Azimuths.Sort([](float A, float B) { return FMath::Fmod(A, 2.f * PI) < FMath::Fmod(B, 2.f * PI); });
			const FVector Axis0 = TrunkAt(0.f, Rad, Tan, TParent);
			S.TrunkBase = FVector(Axis0.X, Axis0.Y, 0.f);
			for (int32 k = 0; k < Azimuths.Num(); ++k)
			{
				const float A = FMath::Fmod(Azimuths[k], 2.f * PI);
				float B = FMath::Fmod(Azimuths[(k + 1) % Azimuths.Num()], 2.f * PI);
				if (B <= A) B += 2.f * PI;
				const float Mid = 0.5f * (A + B);
				const FVector Horiz(FMath::Cos(Mid), FMath::Sin(Mid), 0.f);
				S.NestSpots.Add(S.TrunkBase + Horiz * (Base0 + 450.f));
			}
		}

		void GrowLimbs(TArray<int32>& OutLimbs)
		{
			const float H = P.TrunkHeight;
			const float H0 = P.LimbStartHeight, H1 = P.LimbEndFrac * H;
			const float Spacing = (H1 - H0) / FMath::Max(P.NumLimbs - 1, 1);
			const float AzOffset = R.FRandRange(0.f, 2.f * PI);
			const int32 StubIndex = P.bBrokenLimb && P.NumLimbs > 2 ? R.RandRange(1, FMath::Min(4, P.NumLimbs - 1)) : INDEX_NONE;
			for (int32 i = 0; i < P.NumLimbs; ++i)
			{
				const float Hf = float(i) / FMath::Max(P.NumLimbs - 1, 1);
				const float Hgt = FMath::Lerp(H0, H1, FMath::Pow(Hf, 0.9f)) + R.FRandRange(-0.3f, 0.3f) * Spacing;
				float TrunkR, TParent;
				FVector Tan;
				const FVector Base = TrunkAt(Hgt, TrunkR, Tan, TParent);
				const float Az = AzOffset + i * GoldenAngle + R.FRandRange(-0.25f, 0.25f);
				FVector Horiz(FMath::Cos(Az), FMath::Sin(Az), 0.f);
				Horiz = (Horiz - Tan * FVector::DotProduct(Horiz, Tan)).GetSafeNormal();
				const float Pitch = FMath::DegreesToRadians(FMath::Lerp(P.LimbPitchMinDeg, P.LimbPitchMaxDeg, Hf) + R.FRandRange(-6.f, 6.f));
				const FVector Dir = (Horiz * FMath::Cos(Pitch) + Tan * FMath::Sin(Pitch)).GetSafeNormal();
				const float Shape = FMath::Sin(PI * (0.25f + 0.75f * Hf));
				const float Len = FMath::Lerp(P.LimbLengthMin, P.LimbLengthMax, FMath::Max(Shape, 0.f)) * R.FRandRange(0.85f, 1.15f);
				const float BaseR = FMath::Clamp(TrunkR * P.LimbRadiusRatio, 60.f, 320.f);
				if (i == StubIndex)
				{
					// The broken limb: short, still thick at its end, nothing growing on it.
					const int32 Idx = GrowBranch(1, 0, TParent, Base, Dir, FMath::Clamp(Len * 0.18f, 500.f, 1200.f), BaseR, BaseR * 0.7f, 0.05f, 0.f, true, TrunkR);
					if (Idx != INDEX_NONE)
					{
						S.Branches[Idx].bStub = true;
						OutLimbs.Add(Idx);
					}
					continue;
				}
				const int32 Idx = GrowBranch(1, 0, TParent, Base, Dir, Len, BaseR, P.LimbTipRadius, 0.1f, P.Gravitropism, true, TrunkR);
				if (Idx != INDEX_NONE) OutLimbs.Add(Idx);
			}
		}

		void GrowChildren(const TArray<int32>& Parents, int32 Level, TArray<int32>& OutChildren)
		{
			for (int32 ParentIdx : Parents)
			{
				if (S.Branches[ParentIdx].bStub) continue;
				const float ParentLen = S.Branches[ParentIdx].Length();
				const int32 Count = Level == 2
					? FMath::Max(3, FMath::RoundToInt(P.BoughsPerLimb * FMath::Clamp(ParentLen / P.LimbLengthMax, 0.4f, 1.2f)))
					: FMath::Max(3, P.TwigsPerBough);
				const float T0 = Level == 2 ? 0.15f : 0.2f;
				const float T1 = Level == 2 ? 0.92f : 0.98f;
				const float AzOffset = R.FRandRange(0.f, 2.f * PI);
				for (int32 j = 0; j < Count; ++j)
				{
					// Re-read the parent each time: S.Branches may reallocate as children are added.
					const float T = FMath::Clamp(FMath::Lerp(T0, T1, (j + 0.5f) / Count) + R.FRandRange(-0.04f, 0.04f), 0.f, 1.f);
					float ParentR;
					FVector Tan;
					const FVector Base = TreeSkeleton::SampleAt(S.Branches[ParentIdx], T, &ParentR, &Tan);
					const float Az = AzOffset + j * GoldenAngle + R.FRandRange(-0.3f, 0.3f);
					if (Level == 2)
					{
						const FVector Dir = ChildDir(Tan, Az, FMath::DegreesToRadians(R.FRandRange(35.f, 60.f)));
						const float Len = FMath::Max(600.f, P.BoughLengthFrac * ParentLen * (1.f - 0.5f * T) * R.FRandRange(0.8f, 1.2f));
						const float BaseR = FMath::Clamp(ParentR * P.BoughRadiusRatio, 15.f, 120.f);
						const int32 Idx = GrowBranch(2, ParentIdx, T, Base, Dir, Len, BaseR, P.BoughTipRadius, 0.16f, P.Gravitropism * 0.7f, true, ParentR);
						if (Idx != INDEX_NONE) OutChildren.Add(Idx);
					}
					else
					{
						const FVector Dir = ChildDir(Tan, Az, FMath::DegreesToRadians(R.FRandRange(40.f, 70.f)));
						const float Len = R.FRandRange(P.TwigLengthMin, P.TwigLengthMax);
						const float BaseR = FMath::Clamp(ParentR * 0.5f, P.TwigRadiusMin, P.TwigRadiusMax);
						const int32 Idx = GrowBranch(3, ParentIdx, T, Base, Dir, Len, BaseR, P.TwigTipRadius, 0.22f, P.Gravitropism * 0.5f, true, ParentR);
						if (Idx != INDEX_NONE) OutChildren.Add(Idx);
					}
				}
			}
		}
	};
}

void TreeSkeleton::BuildSamples(FTreeBranch& B, const FTreeParams& P)
{
	B.Samples.Reset();
	B.SampleRadius.Reset();
	B.SampleDist.Reset();
	B.Bounds = FBox(ForceInit);
	const int32 NN = B.Nodes.Num();
	if (NN < 2) return;

	for (int32 i = 0; i + 1 < NN; ++i)
	{
		const FVector P1 = B.Nodes[i].Pos, P2 = B.Nodes[i + 1].Pos;
		const FVector P0 = i > 0 ? B.Nodes[i - 1].Pos : 2.0 * P1 - P2;
		const FVector P3 = i + 2 < NN ? B.Nodes[i + 2].Pos : 2.0 * P2 - P1;
		const float R1 = B.Nodes[i].Radius, R2 = B.Nodes[i + 1].Radius;
		const double SpanLen = FVector::Dist(P1, P2);
		const float Spacing = FMath::Clamp(0.5f * (R1 + R2) * P.SampleSpacingFrac, P.SampleSpacingMin, P.SampleSpacingMax);
		const int32 N = FMath::Max(1, FMath::CeilToInt(SpanLen / Spacing));
		for (int32 s = 0; s < N; ++s)
		{
			const double T = double(s) / N;
			B.Samples.Add(CatmullRom(P0, P1, P2, P3, T));
			B.SampleRadius.Add(FMath::Lerp(R1, R2, (float)T));
		}
	}
	B.Samples.Add(B.Nodes.Last().Pos);
	B.SampleRadius.Add(B.Nodes.Last().Radius);

	float Dist = 0.f;
	for (int32 i = 0; i < B.Samples.Num(); ++i)
	{
		if (i > 0) Dist += (float)FVector::Dist(B.Samples[i], B.Samples[i - 1]);
		B.SampleDist.Add(Dist);
		B.Bounds += FBox(B.Samples[i] - FVector(B.SampleRadius[i]), B.Samples[i] + FVector(B.SampleRadius[i]));
	}
}

FVector TreeSkeleton::SampleAt(const FTreeBranch& B, float T, float* OutRadius, FVector* OutTangent)
{
	const int32 N = B.Samples.Num();
	if (N == 0) return FVector::ZeroVector;
	if (N == 1)
	{
		if (OutRadius) *OutRadius = B.SampleRadius[0];
		if (OutTangent) *OutTangent = FVector::UpVector;
		return B.Samples[0];
	}
	const float Target = FMath::Clamp(T, 0.f, 1.f) * B.Length();
	int32 Hi = FMath::Clamp(Algo::LowerBound(B.SampleDist, Target), 1, N - 1);
	const int32 Lo = Hi - 1;
	const float Span = B.SampleDist[Hi] - B.SampleDist[Lo];
	const float F = Span > UE_SMALL_NUMBER ? FMath::Clamp((Target - B.SampleDist[Lo]) / Span, 0.f, 1.f) : 0.f;
	if (OutRadius) *OutRadius = FMath::Lerp(B.SampleRadius[Lo], B.SampleRadius[Hi], F);
	if (OutTangent) *OutTangent = (B.Samples[Hi] - B.Samples[Lo]).GetSafeNormal();
	return FMath::Lerp(B.Samples[Lo], B.Samples[Hi], (double)F);
}

namespace
{
	/** Signed distance from P to the tube around segment i of the polyline (negative inside); same model as the mesh. */
	double TubeSegDist(const FVector& P, const TArray<FVector>& S, const TArray<float>& R, int32 i)
	{
		const FVector& A = S[i];
		const FVector AB = S[i + 1] - A;
		const double L2 = AB.SizeSquared();
		const double T = L2 > UE_DOUBLE_SMALL_NUMBER ? FMath::Clamp(FVector::DotProduct(P - A, AB) / L2, 0.0, 1.0) : 0.0;
		return FVector::Dist(P, A + AB * T) - FMath::Lerp(R[i], R[i + 1], (float)T);
	}

	int32 FirstDistAtLeast(const TArray<float>& Dist, float S)
	{
		int32 Lo = 0, Hi = Dist.Num();
		while (Lo < Hi)
		{
			const int32 Mid = (Lo + Hi) / 2;
			if (Dist[Mid] < S) Lo = Mid + 1; else Hi = Mid;
		}
		return Lo;
	}

	/**
	 * After all branches are grown (draws no random numbers): fills BarkRadius, and when bBranchCollars is on swells every
	 * branch with a parent where it leaves that parent. s_exit is the arc length of the child's first sample outside the
	 * parent's tube; the swelling ramps in over [s_exit / 2, s_exit] (so the base ring buried in the parent does not grow
	 * and cannot poke out) and decays beyond it.
	 */
	void FinishRadii(FTreeSkeleton& S, const FTreeParams& P)
	{
		const int32 NB = S.Branches.Num();
		// The radii as grown: the parents' tubes are measured with these, not with collared radii.
		TArray<TArray<float>> Orig;
		Orig.SetNum(NB);
		for (int32 b = 0; b < NB; ++b)
		{
			FTreeBranch& B = S.Branches[b];
			Orig[b] = B.SampleRadius;
			B.BarkRadius = B.SampleRadius;
			if (B.Level == 0)
			{
				// Undo the flare applied in TrunkRadiusAt, from each sample's height.
				for (int32 i = 0; i < B.BarkRadius.Num(); ++i)
				{
					const float Hc = FMath::Max((float)B.Samples[i].Z, 0.f);
					B.BarkRadius[i] /= 1.f + P.FlareMul * FMath::Exp(-Hc / FMath::Max(P.FlareHeight, 1.f));
				}
			}
		}
		if (!P.bBranchCollars) return;

		for (int32 c = 0; c < NB; ++c)
		{
			FTreeBranch& B = S.Branches[c];
			if (B.Parent == INDEX_NONE || !S.Branches.IsValidIndex(B.Parent) || B.Parent == c || B.Samples.Num() < 2) continue;
			const FTreeBranch& Par = S.Branches[B.Parent];
			const TArray<float>& ParR = Orig[B.Parent];
			const int32 NSp = Par.Samples.Num();
			if (NSp < 2) continue;
			const float R0 = Orig[c][0];
			// Twigs carry leaves from 15% of their length: a collar there could move or drop a leaf, and one-tree saves store
			// bites by leaf index. Twig collars would hardly show anyway.
			const float Amount = P.CollarAmount * (B.Level == 3 ? 0.f : B.Level == 2 ? 0.8f : 1.f);
			if (Amount <= 0.f || R0 <= 0.f) continue;

			// Parent segments within a window round the junction.
			const float Sp = FMath::Clamp(B.ParentT, 0.f, 1.f) * Par.Length();
			const float Rp = ParR[FMath::Clamp(FirstDistAtLeast(Par.SampleDist, Sp), 0, NSp - 1)];
			const float Win = 3.f * (Rp + R0) + 200.f;
			const int32 SegLo = FMath::Clamp(FirstDistAtLeast(Par.SampleDist, Sp - Win) - 1, 0, NSp - 2);
			const int32 SegHi = FMath::Clamp(FirstDistAtLeast(Par.SampleDist, Sp + Win), 0, NSp - 2);

			float SExit = B.Length();
			for (int32 i = 0; i < B.Samples.Num(); ++i)
			{
				double D = TNumericLimits<double>::Max();
				for (int32 sg = SegLo; sg <= SegHi; ++sg) D = FMath::Min(D, TubeSegDist(B.Samples[i], Par.Samples, ParR, sg));
				if (D >= 0.0)
				{
					SExit = B.SampleDist[i];
					break;
				}
			}

			const float Decay = FMath::Max(P.CollarLength * R0, 1.f);
			const float Ramp0 = 0.5f * SExit;
			B.Bounds = FBox(ForceInit);
			for (int32 i = 0; i < B.Samples.Num(); ++i)
			{
				const float Sd = B.SampleDist[i];
				const float Ramp = SExit - Ramp0 > UE_SMALL_NUMBER ? FMath::SmoothStep(Ramp0, SExit, Sd) : (Sd >= SExit ? 1.f : 0.f);
				const float Extra = Amount * R0 * Ramp * FMath::Exp(-FMath::Max(0.f, Sd - SExit) / Decay);
				B.SampleRadius[i] = Orig[c][i] + Extra;
				B.Bounds += FBox(B.Samples[i] - FVector(B.SampleRadius[i]), B.Samples[i] + FVector(B.SampleRadius[i]));
			}
		}
	}
}

void TreeSkeleton::Grow(FTreeSkeleton& Out, const FTreeParams& Params, int32 Seed)
{
	const double StartTime = FPlatformTime::Seconds();
	Out = FTreeSkeleton();
	Out.Params = Params;
	Out.Seed = Seed;

	FGrower G(Out, Params, Seed);
	G.GrowTrunk();
	G.GrowRoots();
	TArray<int32> Limbs, Boughs, Twigs;
	G.GrowLimbs(Limbs);
	G.GrowChildren(Limbs, 2, Boughs);
	G.GrowChildren(Boughs, 3, Twigs);
	FinishRadii(Out, Params);

	int32 Samples = 0;
	for (const FTreeBranch& B : Out.Branches) Samples += B.Samples.Num();
	UE_LOG(LogLeafline, Log, TEXT("TreeSkeleton: seed %d, %d branches (%d limbs, %d boughs, %d twigs, %d roots), %d samples, %d nest spots, %.1f ms"),
		Seed, Out.Branches.Num(), Limbs.Num(), Boughs.Num(), Twigs.Num(), Params.NumRoots, Samples, Out.NestSpots.Num(),
		(FPlatformTime::Seconds() - StartTime) * 1000.0);
}
