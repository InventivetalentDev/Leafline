// Junction skirts (FTreeParams::bFillets). Included by TreeMeshBuilder.cpp after its helpers (FBranchRange, SplitAlong,
// NoOverlap); see TreeMeshBuilder::Build in the header for what a skirt is.
//
// Per junction (child C leaving parent P), in one plane per column of the child's ring:
//  - Ring: the child's first ring whose vertices are all clear of the fillet blend (their distance to P is above the
//    fillet radius there). Its vertices are the skirt's first row, so the skirt meets the child's tube exactly.
//  - Origin O: a point on the child's axis inside both tubes. The union of two convex tubes containing O is star-shaped
//    about O (the fillet only adds material in the corner), so a ray from O crosses the walking surface once.
//  - Column j: rays from O in the plane of the child's axis and ring vertex j, swept from the ring vertex down the child,
//    over the fillet, onto the parent. The seam is where the surface is equally far from both tubes; the end is where
//    it is 1.1 fillet radii from the child (the blend is over). Rows are spaced half by arc length, half by angle (the
//    angle half crowds them into the curve).
//  - The rays use the two tubes only (pieces near the junction, blended with the fillet radius); each inner point is
//    then moved onto the whole walking field, which differs only where a third branch or the ground is near.
#include "HAL/IConsoleManager.h"
#include <atomic>

namespace
{
	TAutoConsoleVariable<int32> CVarSkirts(TEXT("leaf.Skirts"), 1,
		TEXT("Junction skirts on the bark mesh when fillets are on (1) or not (0, the walking field keeps its fillets: for comparing). Takes effect when the tree next grows (Leaf.Fillets 1)."));
	TAutoConsoleVariable<int32> CVarGroundSkirt(TEXT("leaf.GroundSkirt"), 1,
		TEXT("The trunk and roots' skirt at the ground when fillets are on (1) or not (0, the field keeps its ground fillet: for comparing). Takes effect when the tree next grows."));
	TAutoConsoleVariable<int32> CVarGroundFill(TEXT("leaf.GroundFill"), 1,
		TEXT("Ground skirt: fill runs of failed columns from their neighbours (1) or leave the gap (0, the strip ends in an open edge there: the old lip). Takes effect when the tree next grows."));
	TAutoConsoleVariable<int32> CVarGroundClose(TEXT("leaf.GroundClose"), 1,
		TEXT("Ground skirt: lay the columns at open ends of the strip onto the wall and the ground underneath (1) or leave them on the fillet (0: the strip ends in a raised edge). Takes effect when the tree next grows."));
	TAutoConsoleVariable<int32> CVarSkirtSeam(TEXT("leaf.SkirtSeam"), 0,
		TEXT("Where a skirt switches from the child's bark to the parent's: 0 where the surface is equally far from both tubes (the old crease), 1 at its outer edge on the parent (the child's bark over the whole fillet). Takes effect when the tree next grows."));

	constexpr double SkirtLift = 0.3;   // cm: inner rows sit this far outside the walking surface (against z-fighting)
	constexpr double SkirtDPhi = 0.0523598776; // 3 degrees between the rays of a column's first sweep
	constexpr double SkirtPhiMax = 4.36332313; // 250 degrees: a column that has not reached the parent by then fails
	constexpr int32 SkirtMaxRows = 24;  // per half
	constexpr double SkirtHidden = 3.0;  // cm: a skirt point this deep inside the whole field lies inside a branch crossing the skirt
	constexpr double SkirtMaxMove = 8.0; // cm: moving a point further than this onto the whole field means a third branch is in the way
	                                     // (smaller moves follow the small blend of a branch crossing the skirt, at most k / 4)

	struct FSkirtStats
	{
		int32 Junctions = 0, Built = 0, Vertices = 0, Triangles = 0;
		int32 FailWindow = 0, FailRing = 0, FailOrigin = 0, FailRay = 0, FailEnd = 0, FailCrowded = 0;
		/** Inner vertices where the blend is still active but the fillet radius has started to ease off (zone too small). */
		int32 FadeConflicts = 0;
		/** |field| at inner vertices after the move onto the field (before the lift). */
		double FitSum = 0.0, FitWorst = 0.0;
		int32 FitCount = 0;
		/** Field at the centres of the skirt's quads, minus the lift: negative where the skirt's flat quads cut under the
		 *  curved walking surface (chord error). */
		double ChordSum = 0.0, ChordWorst = 0.0;
		int32 ChordCount = 0;
		int32 ChordWorstBranch = INDEX_NONE;
		/** Quads whose centre lies inside another branch crossing the skirt (hidden; not in the chord numbers). */
		int32 HiddenQuads = 0;
		/** Largest move from the two-tube surface onto the whole field. */
		double MoveWorst = 0.0;
		FVector MoveWorstAt = FVector::ZeroVector;
		int32 MoveWorstBranch = INDEX_NONE;
		TArray<int32> Failed;

		void Add(const FSkirtStats& O)
		{
			Junctions += O.Junctions; Built += O.Built; Vertices += O.Vertices; Triangles += O.Triangles;
			FailWindow += O.FailWindow; FailRing += O.FailRing; FailOrigin += O.FailOrigin; FailRay += O.FailRay; FailEnd += O.FailEnd;
			FailCrowded += O.FailCrowded;
			FadeConflicts += O.FadeConflicts;
			FitSum += O.FitSum; FitCount += O.FitCount; FitWorst = FMath::Max(FitWorst, O.FitWorst);
			ChordSum += O.ChordSum; ChordCount += O.ChordCount; HiddenQuads += O.HiddenQuads;
			if (O.ChordWorst < ChordWorst) { ChordWorst = O.ChordWorst; ChordWorstBranch = O.ChordWorstBranch; }
			if (O.MoveWorst > MoveWorst) { MoveWorst = O.MoveWorst; MoveWorstAt = O.MoveWorstAt; MoveWorstBranch = O.MoveWorstBranch; }
			Failed.Append(O.Failed);
		}
	};

	/** The two tubes of one junction, blended as the field blends them. */
	struct FSkirtPair
	{
		const FTreeField& Field;
		int32 C, P;
		int32 PLo, PHi, CLo, CHi;
		FVector O = FVector::ZeroVector;
		double F0 = 0.0;

		double Dp(const FVector& X) const { return Field.BranchDistance(P, X, PLo, PHi); }
		double Dc(const FVector& X) const { return Field.BranchDistance(C, X, CLo, CHi); }
		double F(const FVector& X) const { return FTreeField::SMin(Dp(X), Dc(X), Field.BlendKAt(C, X)); }
		FVector Normal(const FVector& X) const
		{
			constexpr double E = 0.25;
			static const FVector K1(1, -1, -1), K2(-1, -1, 1), K3(-1, 1, -1), K4(1, 1, 1);
			const FVector G = K1 * F(X + K1 * E) + K2 * F(X + K2 * E) + K3 * F(X + K3 * E) + K4 * F(X + K4 * E);
			return G.GetSafeNormal();
		}
	};

	FVector FieldNormalMT(const FTreeField& Field, const FVector& X)
	{
		constexpr double E = 0.5;
		static const FVector K1(1, -1, -1), K2(-1, -1, 1), K3(-1, 1, -1), K4(1, 1, 1);
		const FVector G = K1 * Field.DistanceMT(X + K1 * E) + K2 * Field.DistanceMT(X + K2 * E)
			+ K3 * Field.DistanceMT(X + K3 * E) + K4 * Field.DistanceMT(X + K4 * E);
		return G.GetSafeNormal();
	}

	/** Distance along Dir from Pair.O to the two-tube surface (Pair.F crosses zero), starting from a guess. */
	template <typename TPair>
	bool SolveSkirtRay(const TPair& Pair, const FVector& Dir, double TGuess, double TMax, double& OutT)
	{
		double T0 = 0.0, F0 = Pair.F0; // inside
		double T1 = FMath::Clamp(TGuess, 0.5, TMax);
		double F1 = Pair.F(Pair.O + Dir * T1);
		int32 Guard = 0;
		if (F1 < 0.0)
		{
			// Inside at the guess: step out by the distance (the field is about 1-Lipschitz) until outside.
			T0 = T1;
			F0 = F1;
			for (;;)
			{
				if (++Guard > 100) return false;
				T1 = T0 + FMath::Max(-F0 * 0.8, 0.5);
				if (T1 > TMax) return false;
				F1 = Pair.F(Pair.O + Dir * T1);
				if (F1 >= 0.0) break;
				T0 = T1;
				F0 = F1;
			}
		}
		else
		{
			// Outside at the guess: step in until inside (O itself is inside).
			for (;;)
			{
				const double Tm = T1 - FMath::Max(F1 * 1.2, 0.5);
				if (++Guard > 60 || Tm <= 0.0)
				{
					T0 = 0.0;
					F0 = Pair.F0;
					break;
				}
				const double Fm = Pair.F(Pair.O + Dir * Tm);
				if (Fm < 0.0)
				{
					T0 = Tm;
					F0 = Fm;
					break;
				}
				T1 = Tm;
				F1 = Fm;
			}
		}
		// Illinois false position on [T0, T1] (F0 < 0 <= F1).
		int32 Side = 0;
		for (int32 It = 0; It < 50; ++It)
		{
			const double T = (T0 * F1 - T1 * F0) / (F1 - F0);
			const double Fv = Pair.F(Pair.O + Dir * T);
			if (FMath::Abs(Fv) < 0.002 || T1 - T0 < 0.002)
			{
				OutT = T;
				return true;
			}
			if (Fv < 0.0)
			{
				T0 = T;
				F0 = Fv;
				if (Side == -1) F1 *= 0.5;
				Side = -1;
			}
			else
			{
				T1 = T;
				F1 = Fv;
				if (Side == 1) F0 *= 0.5;
				Side = 1;
			}
		}
		OutT = 0.5 * (T0 + T1);
		return true;
	}

	struct FSkirtVert
	{
		FVector P = FVector::ZeroVector;
		FVector N = FVector::UpVector;
		FVector2D UV0 = FVector2D::ZeroVector, UV1 = FVector2D::ZeroVector, UV3 = FVector2D::ZeroVector;
		double Along = 0.0;
		FLinearColor Col = FLinearColor::White;
		FVector Tan = FVector::ForwardVector;
	};

	/**
	 * The parent tube's mesh at X: finds the quad X lies over (by the ring planes, then the angle round the interpolated
	 * frame) and interpolates its vertex data exactly as the tube's two triangles do. OutV.P is the point on the facets.
	 */
	void ParentSurfaceAt(const FTreeBranch& Pb, const FBranchRange& RP, const FTreeMeshData& Mesh, int32 Lo, int32 Hi,
		const FVector& X, FSkirtVert& OutV)
	{
		int32 Seg = Lo;
		double BestViol = TNumericLimits<double>::Max(), SA = 0.0, SB = 0.0;
		for (int32 i = Lo; i <= Hi; ++i)
		{
			const double A = FVector::DotProduct(X - Pb.Samples[i], RP.T[i]);
			const double B = FVector::DotProduct(X - Pb.Samples[i + 1], RP.T[i + 1]);
			const double Viol = FMath::Max(-A, 0.0) + FMath::Max(B, 0.0);
			if (Viol < BestViol)
			{
				BestViol = Viol;
				Seg = i;
				SA = A;
				SB = B;
				if (Viol <= 0.0) break;
			}
		}
		const double U = SA - SB > 1e-9 ? FMath::Clamp(SA / (SA - SB), 0.0, 1.0) : 0.0;
		const FVector Tu = FMath::Lerp(RP.T[Seg], RP.T[Seg + 1], U).GetSafeNormal();
		FVector Nu = FMath::Lerp(RP.N[Seg], RP.N[Seg + 1], U);
		Nu = (Nu - Tu * FVector::DotProduct(Nu, Tu)).GetSafeNormal();
		const FVector Bu = FVector::CrossProduct(Tu, Nu);
		const FVector Q = X - FMath::Lerp(Pb.Samples[Seg], Pb.Samples[Seg + 1], U);
		double Th = FMath::Atan2(FVector::DotProduct(Q, Bu), FVector::DotProduct(Q, Nu));
		if (Th < 0.0) Th += 2.0 * PI;
		const int32 M = RP.M, RV = M + 1;
		const double Fj = Th * M / (2.0 * PI);
		const int32 J = FMath::Clamp(FMath::FloorToInt32(Fj), 0, M - 1);
		const double A = FMath::Clamp(Fj - J, 0.0, 1.0);
		const int32 IA = RP.Start + Seg * RV + J, IB = IA + 1, IC = IA + RV, ID = IC + 1;
		double WA, WB, WC, WD;
		if (A + U <= 1.0)
		{
			WA = 1.0 - A - U; WB = A; WC = U; WD = 0.0;
		}
		else
		{
			WA = 0.0; WB = 1.0 - U; WC = 1.0 - A; WD = A + U - 1.0;
		}
		auto Mix3 = [&](const TArray<FVector>& Arr) { return Arr[IA] * WA + Arr[IB] * WB + Arr[IC] * WC + Arr[ID] * WD; };
		auto Mix2 = [&](const TArray<FVector2D>& Arr) { return Arr[IA] * WA + Arr[IB] * WB + Arr[IC] * WC + Arr[ID] * WD; };
		OutV.P = Mix3(Mesh.Vertices);
		OutV.N = Mix3(Mesh.Normals).GetSafeNormal();
		OutV.UV0 = Mix2(Mesh.UV0);
		OutV.UV1 = Mix2(Mesh.UV1);
		const FVector2D UV2 = Mix2(Mesh.UV2);
		OutV.Along = UV2.X + UV2.Y;
		OutV.UV3 = FVector2D(Mix2(Mesh.UV3).X, NoOverlap);
		OutV.Col = Mesh.Colors[IA];
		OutV.Col.A = 1.f;
		OutV.Tan = (Mesh.Tangents[IA].TangentX * WA + Mesh.Tangents[IB].TangentX * WB + Mesh.Tangents[IC].TangentX * WC
			+ Mesh.Tangents[ID].TangentX * WD).GetSafeNormal();
	}

	/** One sample of a column's sweep. */
	struct FSkirtRay
	{
		double Phi = 0.0;
		double T = 0.0;
		FVector Hit = FVector::ZeroVector;
	};

	struct FSkirtColumn
	{
		FVector E1, E2;
		TArray<FSkirtRay> Child;  // ring vertex .. seam
		TArray<FSkirtRay> Parent; // seam .. end
	};

	/** Rows along one half of a column: half by arc length, half by angle. Re-solves each ray. */
	template <typename TPair, typename TCol>
	bool SkirtRows(const TPair& Pair, const TCol& Col, const TArray<FSkirtRay>& Dense, int32 N, double TMax,
		TArray<FVector>& OutHits)
	{
		const int32 K = Dense.Num();
		TArray<double, TInlineAllocator<128>> G;
		G.SetNum(K);
		double Arc = 0.0;
		TArray<double, TInlineAllocator<128>> ArcAt;
		ArcAt.SetNum(K);
		ArcAt[0] = 0.0;
		for (int32 k = 1; k < K; ++k)
		{
			Arc += FVector::Dist(Dense[k].Hit, Dense[k - 1].Hit);
			ArcAt[k] = Arc;
		}
		const double DPhi = FMath::Max(Dense.Last().Phi - Dense[0].Phi, 1e-9);
		for (int32 k = 0; k < K; ++k)
		{
			G[k] = 0.5 * (Arc > 1e-6 ? ArcAt[k] / Arc : 0.0) + 0.5 * (Dense[k].Phi - Dense[0].Phi) / DPhi;
		}
		OutHits.SetNum(N + 1);
		OutHits[0] = Dense[0].Hit;
		OutHits[N] = Dense.Last().Hit;
		int32 k = 0;
		for (int32 r = 1; r < N; ++r)
		{
			const double Target = double(r) / N;
			while (k + 2 < K && G[k + 1] < Target) ++k;
			const double Span = FMath::Max(G[k + 1] - G[k], 1e-9);
			const double F = FMath::Clamp((Target - G[k]) / Span, 0.0, 1.0);
			const double Phi = FMath::Lerp(Dense[k].Phi, Dense[k + 1].Phi, F);
			const double TGuess = FMath::Lerp(Dense[k].T, Dense[k + 1].T, F);
			const FVector Dir = Col.E1 * FMath::Cos(Phi) + Col.E2 * FMath::Sin(Phi);
			double T;
			if (!SolveSkirtRay(Pair, Dir, TGuess, TMax, T)) return false;
			OutHits[r] = Pair.O + Dir * T;
		}
		return true;
	}

	void BuildSkirt(const FTreeSkeleton& S, const FTreeField& Field, const TArray<FBranchRange>& Ranges, const FTreeMeshData& Mesh,
		int32 c, FTreeMeshData& Out, FSkirtStats& St)
	{
		St.Junctions = 1;
		const FTreeBranch& Cb = S.Branches[c];
		const int32 p = Cb.Parent;
		const FTreeBranch& Pb = S.Branches[p];
		const FBranchRange& RC = Ranges[c];
		const FBranchRange& RP = Ranges[p];
		const int32 NSc = Cb.Samples.Num(), NSp = Pb.Samples.Num();
		const double K = Cb.FilletK;
		const FVector Jc = Cb.FilletCenter;
		const double Zone = Cb.FilletOuter + K;
		auto Fail = [&](int32& Counter) { ++Counter; St.Failed.Add(c); };

		// The pieces of both tubes near the junction.
		auto Window = [&](const FTreeBranch& B, int32 Last, int32& OutLo, int32& OutHi)
		{
			OutLo = MAX_int32;
			OutHi = -1;
			for (int32 i = 0; i + 1 < B.Samples.Num() && i <= Last; ++i)
			{
				const FVector A = B.Samples[i], Bv = B.Samples[i + 1];
				const double Reach = 0.5 * FVector::Dist(A, Bv) + FMath::Max(B.SampleRadius[i], B.SampleRadius[i + 1]);
				if (FVector::Dist(0.5 * (A + Bv), Jc) - Reach < Zone)
				{
					OutLo = FMath::Min(OutLo, i);
					OutHi = FMath::Max(OutHi, i);
				}
			}
		};
		FSkirtPair Pair{ Field, c, p, 0, 0, 0, 0 };
		Window(Pb, NSp - 2, Pair.PLo, Pair.PHi);
		int32 CLoUnused, CHi;
		Window(Cb, NSc - 2, CLoUnused, CHi);
		if (Pair.PHi < 0 || CHi < 0)
		{
			Fail(St.FailWindow);
			return;
		}
		Pair.CLo = 0;

		// The first ring clear of the blend.
		const int32 M = RC.M, RV = M + 1;
		int32 IOut = INDEX_NONE;
		for (int32 i = 1; i <= NSc - 2; ++i)
		{
			if (Pair.Dp(Cb.Samples[i]) <= 0.0) continue; // the axis is still inside the parent
			bool bClear = true;
			for (int32 j = 0; j < M && bClear; ++j)
			{
				const FVector& V = Mesh.Vertices[RC.Start + i * RV + j];
				bClear = Pair.Dp(V) >= 1.05 * Field.BlendKAt(c, V) + 0.5;
			}
			if (bClear)
			{
				IOut = i;
				break;
			}
		}
		if (IOut == INDEX_NONE)
		{
			Fail(St.FailRing);
			return;
		}
		Pair.CHi = FMath::Clamp(FMath::Max(CHi, IOut + 1), 0, NSc - 2);

		// The origin: on the child's axis, inside the parent by m.
		float Rpar = Pb.SampleRadius[0];
		{
			double Best = TNumericLimits<double>::Max();
			for (int32 i = Pair.PLo; i <= Pair.PHi + 1 && i < NSp; ++i)
			{
				const double D = FVector::DistSquared(Pb.Samples[i], Jc);
				if (D < Best)
				{
					Best = D;
					Rpar = Pb.SampleRadius[i];
				}
			}
		}
		const double Rc = Cb.SampleRadius[FMath::Min(IOut, NSc - 1)];
		const double Depth = FMath::Min(0.6 * Rc + 0.5 * K, 0.8 * Rpar);
		bool bOrigin = false;
		double DPrev = Pair.Dp(Cb.Samples[0]);
		if (DPrev >= -Depth)
		{
			Pair.O = Cb.Samples[0];
			bOrigin = DPrev < -1.0;
		}
		else
		{
			for (int32 i = 1; i <= IOut; ++i)
			{
				const double D = Pair.Dp(Cb.Samples[i]);
				if (D >= -Depth)
				{
					Pair.O = FMath::Lerp(Cb.Samples[i - 1], Cb.Samples[i], (-Depth - DPrev) / (D - DPrev));
					bOrigin = true;
					break;
				}
				DPrev = D;
			}
		}
		Pair.F0 = bOrigin ? Pair.F(Pair.O) : 0.0;
		if (!bOrigin || Pair.F0 > -1.0)
		{
			Fail(St.FailOrigin);
			return;
		}

		// Sweep each column.
		const FVector Tout = RC.T[IOut];
		const FVector Sring = Cb.Samples[IOut];
		const double TMax = 4.0 * Zone + 2.0 * FVector::Dist(Pair.O, Sring) + 1000.0;
		TArray<FSkirtColumn> Cols;
		Cols.SetNum(M);
		double MaxArcC = 0.0, MaxArcP = 0.0;
		for (int32 j = 0; j < M; ++j)
		{
			FSkirtColumn& Col = Cols[j];
			const FVector V = Mesh.Vertices[RC.Start + IOut * RV + j];
			const FVector D = (V - Sring).GetSafeNormal();
			Col.E1 = (V - Pair.O).GetSafeNormal();
			const FVector Npl = FVector::CrossProduct(Tout, D).GetSafeNormal();
			Col.E2 = FVector::CrossProduct(Npl, Col.E1).GetSafeNormal();
			auto Ray = [&](double Phi, double TGuess, FSkirtRay& R)
			{
				const FVector Dir = Col.E1 * FMath::Cos(Phi) + Col.E2 * FMath::Sin(Phi);
				if (!SolveSkirtRay(Pair, Dir, TGuess, TMax, R.T)) return false;
				R.Phi = Phi;
				R.Hit = Pair.O + Dir * R.T;
				return true;
			};
			auto Stop = [&](const FVector& X) { return 1.1 * Field.BlendKAt(c, X) + 1.0; };
			// Bisection on the sweep angle for the zero of Fn between rays A (Fn < 0) and B (Fn >= 0).
			auto Refine = [&](FSkirtRay A, FSkirtRay B, TFunctionRef<double(const FVector&)> Fn, FSkirtRay& OutR)
			{
				for (int32 It = 0; It < 9; ++It)
				{
					FSkirtRay Mid;
					if (!Ray(0.5 * (A.Phi + B.Phi), 0.5 * (A.T + B.T), Mid)) return false;
					if (Fn(Mid.Hit) < 0.0) A = Mid; else B = Mid;
				}
				OutR = B;
				return true;
			};

			FSkirtRay R0;
			R0.Phi = 0.0;
			R0.T = FVector::Dist(V, Pair.O);
			R0.Hit = V;
			Col.Child.Add(R0);
			FSkirtRay Prev = R0;
			bool bSeam = false, bEnd = false;
			// Small fillets (twigs) sweep at 6 degrees: the crossings are bisected anyway, and there are many of them.
			const double DPhi = K < 10.0 ? 2.0 * SkirtDPhi : SkirtDPhi;
			for (double Phi = DPhi; Phi <= SkirtPhiMax; Phi += DPhi)
			{
				FSkirtRay R;
				if (!Ray(Phi, Prev.T, R) || R.T > 3.0 * Prev.T + 50.0 || R.T < Prev.T / 3.0 - 50.0)
				{
					Fail(St.FailRay);
					return;
				}
				FSkirtRay From = Prev;
				if (!bSeam && Pair.Dc(R.Hit) >= Pair.Dp(R.Hit))
				{
					FSkirtRay Seam;
					if (!Refine(Prev, R, [&](const FVector& X) { return Pair.Dc(X) - Pair.Dp(X); }, Seam))
					{
						Fail(St.FailRay);
						return;
					}
					Col.Child.Add(Seam);
					Col.Parent.Add(Seam);
					bSeam = true;
					From = Seam;
				}
				if (bSeam && Pair.Dc(R.Hit) >= Stop(R.Hit))
				{
					FSkirtRay End;
					if (Pair.Dc(From.Hit) >= Stop(From.Hit)) End = From; // the seam itself is already clear (cannot happen with Dc = Dp)
					else if (!Refine(From, R, [&](const FVector& X) { return Pair.Dc(X) - Stop(X); }, End))
					{
						Fail(St.FailRay);
						return;
					}
					if (End.Phi > Col.Parent.Last().Phi) Col.Parent.Add(End);
					bEnd = true;
					break;
				}
				(bSeam ? Col.Parent : Col.Child).Add(R);
				Prev = R;
			}
			if (!bEnd || Col.Child.Num() < 2 || Col.Parent.Num() < 2)
			{
				Fail(St.FailEnd);
				return;
			}
			double ArcC = 0.0, ArcP = 0.0;
			for (int32 k = 1; k < Col.Child.Num(); ++k) ArcC += FVector::Dist(Col.Child[k].Hit, Col.Child[k - 1].Hit);
			for (int32 k = 1; k < Col.Parent.Num(); ++k) ArcP += FVector::Dist(Col.Parent[k].Hit, Col.Parent[k - 1].Hit);
			MaxArcC = FMath::Max(MaxArcC, ArcC);
			MaxArcP = FMath::Max(MaxArcP, ArcP);
		}

		// Rows: the same count in every column, so the halves are regular grids.
		const double RowStep = FMath::Clamp(K / 4.0, 6.0, 20.0);
		const int32 Nc = FMath::Clamp(FMath::CeilToInt32(MaxArcC / RowStep), 2, SkirtMaxRows);
		const int32 Np = FMath::Clamp(FMath::CeilToInt32(MaxArcP / RowStep), 2, SkirtMaxRows);
		const float TreeH = FMath::Max(S.Params.TrunkHeight, 1.f);
		// Near the ground the ground's own skirt lies over this one: leave it on the two tubes there (inside the field).
		const double GroundSafe = Field.GroundZ + 1.5 * Field.GetGroundK() + 10.0;

		// Grids: child half (Nc + 1) x (M + 1), then parent half (Np + 1) x (M + 1); column M repeats column 0.
		TArray<FSkirtVert> GridC, GridP;
		GridC.SetNum((Nc + 1) * RV);
		GridP.SetNum((Np + 1) * RV);
		TArray<FVector> HitsC, HitsP;
		// Moves an inner point from the two-tube surface onto the whole field; returns its normal. A big move means a third
		// branch lies in the skirt's way: the skirt is given up (and its fillet removed by the caller).
		double LocalMove = 0.0;
		auto OntoField = [&](FVector& X) -> FVector
		{
			FVector Nrm = Pair.Normal(X);
			if (X.Z < GroundSafe) return Nrm; // near or under the ground the field is the ground's: leave it on the tubes
			double Moved = 0.0;
			for (int32 It = 0; It < 3; ++It)
			{
				const double D = Field.DistanceMT(X);
				if (FMath::Abs(D) < 0.01) break;
				// Deep inside the whole field: inside a branch folded in after the child (one of its boughs) that crosses
				// the skirt. Hidden there; leave it on the two tubes rather than drag it onto that branch.
				if (It == 0 && D < -SkirtHidden) return Nrm;
				X -= Nrm * D;
				Moved += FMath::Abs(D);
			}
			if (Moved > 0.5) Nrm = FieldNormalMT(Field, X);
			LocalMove = FMath::Max(LocalMove, Moved);
			if (Moved > St.MoveWorst)
			{
				St.MoveWorst = Moved;
				St.MoveWorstAt = X;
				St.MoveWorstBranch = c;
			}
			const double Fit = FMath::Abs(Field.DistanceMT(X));
			St.FitSum += Fit;
			St.FitWorst = FMath::Max(St.FitWorst, Fit);
			++St.FitCount;
			const double KHere = Field.BlendKAt(c, X);
			if (KHere < K * 0.999 && FMath::Abs(Pair.Dp(X) - Pair.Dc(X)) < KHere) ++St.FadeConflicts;
			return Nrm;
		};

		for (int32 j = 0; j < M; ++j)
		{
			const FSkirtColumn& Col = Cols[j];
			if (!SkirtRows(Pair, Col, Col.Child, Nc, TMax, HitsC) || !SkirtRows(Pair, Col, Col.Parent, Np, TMax, HitsP))
			{
				Fail(St.FailRay);
				return;
			}
			// Child half: the ring's bark data continued down the column.
			const int32 Vi = RC.Start + IOut * RV + j;
			double Arc = 0.0;
			FVector PrevHit = HitsC[0];
			for (int32 r = 0; r <= Nc; ++r)
			{
				FSkirtVert& V = GridC[r * RV + j];
				FVector X = HitsC[r];
				if (r == 0)
				{
					V.P = Mesh.Vertices[Vi];
					V.N = Mesh.Normals[Vi];
				}
				else
				{
					V.N = OntoField(X);
					V.P = X + V.N * SkirtLift;
				}
				Arc += FVector::Dist(X, PrevHit);
				PrevHit = X;
				V.UV0 = FVector2D(Mesh.UV0[Vi].X, Mesh.UV0[Vi].Y - Arc);
				V.Along = Mesh.UV0[Vi].Y - Arc;
				V.UV1 = Mesh.UV1[Vi];
				V.UV3 = FVector2D(Mesh.UV3[Vi].X, NoOverlap);
				V.Col = Mesh.Colors[Vi];
				V.Col.A = 1.f;
				V.Tan = Mesh.Tangents[Vi].TangentX;
				if (r == Nc) HitsP[0] = X; // the seam: both halves start from the same moved point
			}
			// Parent half: the parent tube's data where the point lies over it (seam 0), or the child's continued (seam 1).
			const bool bChildAll = CVarSkirtSeam.GetValueOnAnyThread() == 1;
			for (int32 r = 0; r <= Np; ++r)
			{
				FSkirtVert& V = GridP[r * RV + j];
				FVector X = HitsP[r];
				if (bChildAll)
				{
					const FSkirtVert& SeamC = GridC[Nc * RV + j];
					if (r == 0)
					{
						V = SeamC;
						PrevHit = X;
						continue;
					}
					FSkirtVert Facet;
					if (r == Np)
					{
						ParentSurfaceAt(Pb, RP, Mesh, Pair.PLo, Pair.PHi, X, Facet);
						V.P = Facet.P;
						V.N = Facet.N;
					}
					else
					{
						V.N = OntoField(X);
						V.P = X + V.N * SkirtLift;
					}
					Arc += FVector::Dist(X, PrevHit);
					PrevHit = X;
					V.UV0 = FVector2D(SeamC.UV0.X, Mesh.UV0[Vi].Y - Arc);
					V.Along = Mesh.UV0[Vi].Y - Arc;
					V.UV1 = SeamC.UV1;
					V.UV3 = SeamC.UV3;
					V.Col = SeamC.Col;
					V.Tan = SeamC.Tan;
					continue;
				}
				if (r == 0)
				{
					const FSkirtVert& SeamC = GridC[Nc * RV + j];
					ParentSurfaceAt(Pb, RP, Mesh, Pair.PLo, Pair.PHi, X, V);
					V.P = SeamC.P;
					V.N = SeamC.N;
				}
				else if (r == Np)
				{
					ParentSurfaceAt(Pb, RP, Mesh, Pair.PLo, Pair.PHi, X, V); // on the parent's facets, its own normal
				}
				else
				{
					const FVector Nrm = OntoField(X);
					ParentSurfaceAt(Pb, RP, Mesh, Pair.PLo, Pair.PHi, X, V);
					V.N = Nrm;
					V.P = X + Nrm * SkirtLift;
				}
			}
		}
		if (LocalMove > SkirtMaxMove)
		{
			Fail(St.FailCrowded);
			return;
		}
		// Column M repeats column 0 (the child's UVs take the ring's duplicate seam vertex, as its tube does).
		for (int32 r = 0; r <= Nc; ++r)
		{
			FSkirtVert V = GridC[r * RV];
			const int32 ViM = RC.Start + IOut * RV + M;
			V.UV0.X = Mesh.UV0[ViM].X;
			V.UV1 = Mesh.UV1[ViM];
			GridC[r * RV + M] = V;
		}
		for (int32 r = 0; r <= Np; ++r)
		{
			FSkirtVert V = GridP[r * RV];
			if (CVarSkirtSeam.GetValueOnAnyThread() == 1)
			{
				V.UV0.X = GridC[M].UV0.X; // the child's data: its duplicate seam column
				V.UV1 = GridC[M].UV1;
			}
			GridP[r * RV + M] = V;
		}

		// Emit both grids.
		auto Emit = [&](const TArray<FSkirtVert>& Grid, int32 Rows)
		{
			const int32 Base = Out.Vertices.Num();
			for (const FSkirtVert& V : Grid)
			{
				Out.Vertices.Add(V.P);
				Out.Normals.Add(V.N);
				Out.UV0.Add(V.UV0);
				Out.UV1.Add(V.UV1);
				Out.UV2.Add(SplitAlong(V.Along));
				Out.UV3.Add(V.UV3);
				FLinearColor Col = V.Col;
				Col.B = FMath::Clamp((float)V.P.Z / TreeH, 0.f, 1.f);
				Out.Colors.Add(Col);
				Out.Tangents.Add(FProcMeshTangent(V.Tan, false));
			}
			for (int32 r = 0; r < Rows; ++r)
			{
				for (int32 j = 0; j < M; ++j)
				{
					const int32 A = Base + r * RV + j, B = A + 1, C = A + RV, D = C + 1;
					for (const FIntVector& Tri : { FIntVector(A, C, B), FIntVector(B, C, D) })
					{
						const FVector& P0 = Out.Vertices[Tri.X];
						const FVector& P1 = Out.Vertices[Tri.Y];
						const FVector& P2 = Out.Vertices[Tri.Z];
						const FVector Cr = FVector::CrossProduct(P1 - P0, P2 - P0);
						if (Cr.SizeSquared() < 1e-8) continue;
						// UE front faces: (v1 - v0) x (v2 - v0) points into the surface.
						const FVector Avg = Out.Normals[Tri.X] + Out.Normals[Tri.Y] + Out.Normals[Tri.Z];
						if (FVector::DotProduct(Cr, Avg) > 0.0) Out.Triangles.Append({ Tri.X, Tri.Z, Tri.Y });
						else Out.Triangles.Append({ Tri.X, Tri.Y, Tri.Z });
					}
					// Chord error at the quad's centre.
					const FVector Mid = 0.25 * (Out.Vertices[A] + Out.Vertices[B] + Out.Vertices[C] + Out.Vertices[D]);
					if (Mid.Z > GroundSafe)
					{
						const double E = Field.DistanceMT(Mid) - SkirtLift;
						if (E < -2.0 * SkirtMaxMove)
						{
							++St.HiddenQuads;
							continue;
						}
						St.ChordSum += E;
						if (E < St.ChordWorst)
						{
							St.ChordWorst = E;
							St.ChordWorstBranch = c;
						}
						++St.ChordCount;
					}
				}
			}
		};
		Emit(GridC, Nc);
		Emit(GridP, Np);
		St.Built = 1;
		St.Vertices = Out.Vertices.Num();
		St.Triangles = Out.Triangles.Num() / 3;
	}

	void BuildSkirts(const FTreeSkeleton& S, const FTreeField& Field, const TArray<FBranchRange>& Ranges, FTreeMeshData& Out,
		TArray<int32>* OutNoSkirt)
	{
		if (CVarSkirts.GetValueOnAnyThread() == 0)
		{
			UE_LOG(LogLeafline, Log, TEXT("TreeSkirts: off (leaf.Skirts 0)"));
			return;
		}
		const double Start = FPlatformTime::Seconds();
		TArray<int32> Junctions;
		for (int32 c = 0; c < S.Branches.Num(); ++c)
		{
			const FTreeBranch& B = S.Branches[c];
			if (B.FilletK > 0.f && S.Branches.IsValidIndex(B.Parent) && B.Parent != c && Ranges[c].bValid && Ranges[B.Parent].bValid)
			{
				Junctions.Add(c);
			}
		}
		TArray<FTreeMeshData> Parts;
		TArray<FSkirtStats> PartStats;
		Parts.SetNum(Junctions.Num());
		PartStats.SetNum(Junctions.Num());
		ParallelFor(Junctions.Num(), [&](int32 k) { BuildSkirt(S, Field, Ranges, Out, Junctions[k], Parts[k], PartStats[k]); });

		FSkirtStats St;
		int32 Levels[4] = { 0, 0, 0, 0 }; // built: roots, limbs, boughs, twigs
		for (int32 k = 0; k < Parts.Num(); ++k)
		{
			St.Add(PartStats[k]);
			const FTreeMeshData& Pt = Parts[k];
			if (Pt.Vertices.IsEmpty()) continue;
			const int32 Lv = S.Branches[Junctions[k]].Level;
			++Levels[Lv < 0 ? 0 : FMath::Clamp(Lv, 1, 3)];
			const int32 Base = Out.Vertices.Num();
			Out.Vertices.Append(Pt.Vertices);
			Out.Normals.Append(Pt.Normals);
			Out.UV0.Append(Pt.UV0);
			Out.UV1.Append(Pt.UV1);
			Out.UV2.Append(Pt.UV2);
			Out.UV3.Append(Pt.UV3);
			Out.Colors.Append(Pt.Colors);
			Out.Tangents.Append(Pt.Tangents);
			Out.Triangles.Reserve(Out.Triangles.Num() + Pt.Triangles.Num());
			for (int32 Idx : Pt.Triangles) Out.Triangles.Add(Base + Idx);
		}
		if (OutNoSkirt) *OutNoSkirt = St.Failed;
		FString FailedList;
		for (int32 k = 0; k < FMath::Min(St.Failed.Num(), 12); ++k) FailedList += FString::Printf(TEXT(" %d"), St.Failed[k]);
		UE_LOG(LogLeafline, Log, TEXT("TreeSkirts: %d of %d junctions (%d roots, %d limbs, %d boughs, %d twigs), %d vertices, %d triangles, %.1f ms; failed: window %d, ring %d, origin %d, ray %d, end %d, crowded %d%s%s"),
			St.Built, St.Junctions, Levels[0], Levels[1], Levels[2], Levels[3], St.Vertices, St.Triangles, (FPlatformTime::Seconds() - Start) * 1000.0,
			St.FailWindow, St.FailRing, St.FailOrigin, St.FailRay, St.FailEnd, St.FailCrowded, FailedList.IsEmpty() ? TEXT("") : TEXT(" (branches"),
			FailedList.IsEmpty() ? TEXT("") : *(FailedList + TEXT(")")));
		UE_LOG(LogLeafline, Log, TEXT("TreeSkirts fit: field at inner vertices mean %.3f worst %.3f cm; at quad centres (minus the %.1f cm lift) mean %.3f worst %.2f cm (branch %d), %d quads hidden inside a crossing branch; largest move onto the whole field %.1f cm at %s (branch %d); %d inner vertices where the fillet eases off inside the blend"),
			St.FitCount ? St.FitSum / St.FitCount : 0.0, St.FitWorst, SkirtLift, St.ChordCount ? St.ChordSum / St.ChordCount : 0.0, St.ChordWorst,
			St.ChordWorstBranch, St.HiddenQuads, St.MoveWorst, *St.MoveWorstAt.ToCompactString(), St.MoveWorstBranch, St.FadeConflicts);
	}

	// ---- Ground skirt: where the trunk and the roots meet the ground --------------------------------------------------
	//
	// Columns stand along the tree's footprint at ground level (the outline of the tree alone, by marching squares), each
	// a sweep of rays in the vertical plane across the outline from a point inside the tree below the ground: from the
	// wall above the ground blend, over the fillet, to the ground beyond it. The rays use the whole field (the tree and the
	// ground blended with the fillet radius), so the skirt lies exactly on the walking surface. The tree half carries the
	// bark of the nearest tube (trunk or root) interpolated as that tube does, in the bark section; the ground half goes
	// to the ground's own section with its vertex data (UV0 = world XY, black, as BuildGround makes it), switching where
	// the surface is equally far from the tree and the ground. Columns near the nest are left out.

	constexpr double GroundLift = 0.5;     // above the junction skirts' 0.3 where both reach (root-trunk corners)
	constexpr double GroundSpacing = 30.0; // cm between columns along the outline
	constexpr double GroundCell = 25.0;    // marching-squares cell
	constexpr int32 GroundFillMax = 12;   // longest run of failed columns filled from its neighbours (columns)
	constexpr double GroundFillReach = 30.0; // cm: a filled point may move this far from its neighbours' blend onto the field

	struct FGroundPair
	{
		const FTreeField& Field;
		FVector O = FVector::ZeroVector;
		double F0 = 0.0;
		double F(const FVector& X) const { return Field.DistanceMT(X); }
	};

	struct FGroundColumn
	{
		FVector C = FVector::ZeroVector; // on the outline, at ground height
		FVector N2 = FVector::ForwardVector; // outward, horizontal
		FVector O = FVector::ZeroVector;
		double F0 = 0.0;
		FVector E1, E2;
		bool bValid = false;
		bool bTopOnWall = true;
		bool bCollapse = false; // an open end of the strip: laid onto the wall and the ground underneath (no raised edge)
		int32 Why = 0; // why it failed: 1 normal, 2 origin, 3 top ray, 4 sweep ray, 5 seam, 6 end refine, 7 no end, 8 rows, 9 top off the tree, 10 end off the ground, 11 wandered
		TArray<FSkirtRay> Tree;   // top .. seam
		TArray<FSkirtRay> Ground; // seam .. end
		TArray<FVector> RowsT, RowsG;
	};

	void BuildGroundSkirt(const FTreeSkeleton& S, const FTreeField& Field, const TArray<FBranchRange>& Ranges, FTreeMeshData& Out,
		FTreeMeshData& OutGround)
	{
		const double Start = FPlatformTime::Seconds();
		OutGround.Reset();
		const int32 BarkStart = Out.Vertices.Num();
		const double Kg = Field.GetGroundK();
		const double Gz = Field.GroundZ;
		TArray<int32> Meet;
		FBox2D Box(ForceInit);
		for (int32 b = 0; b < S.Branches.Num(); ++b)
		{
			const FTreeBranch& B = S.Branches[b];
			if ((B.Level != 0 && B.Level != -1) || !Ranges[b].bValid) continue;
			Meet.Add(b);
			for (int32 i = 0; i < B.Samples.Num(); ++i)
			{
				if (B.Samples[i].Z - B.SampleRadius[i] > Gz + 2.0 * Kg) continue;
				const double R = B.SampleRadius[i] + 2.0 * Kg + 100.0;
				Box += FBox2D(FVector2D(B.Samples[i].X - R, B.Samples[i].Y - R), FVector2D(B.Samples[i].X + R, B.Samples[i].Y + R));
			}
		}
		if (Meet.IsEmpty() || !Box.bIsValid) return;

		// The tree alone on a grid at ground height.
		const int32 NX = FMath::CeilToInt32((Box.Max.X - Box.Min.X) / GroundCell) + 1;
		const int32 NY = FMath::CeilToInt32((Box.Max.Y - Box.Min.Y) / GroundCell) + 1;
		TArray<double> Val;
		Val.SetNumUninitialized(NX * NY);
		auto GridPos = [&](int32 i, int32 j) { return FVector2D(Box.Min.X + i * GroundCell, Box.Min.Y + j * GroundCell); };
		ParallelFor(NY, [&](int32 j)
		{
			for (int32 i = 0; i < NX; ++i)
			{
				const FVector2D P = GridPos(i, j);
				Val[j * NX + i] = Field.TreeDistanceMT(FVector(P.X, P.Y, Gz));
			}
		});

		// Marching squares: crossing points on grid edges (id = 2 * corner + 0 for +x, 1 for +y), segments between them.
		TMap<int32, FVector2D> EdgePt;
		TArray<TPair<int32, int32>> Segs;
		auto Edge = [&](int32 i, int32 j, int32 Dir) -> int32
		{
			const int32 Id = (j * NX + i) * 2 + Dir;
			if (!EdgePt.Contains(Id))
			{
				const int32 i2 = i + (Dir == 0 ? 1 : 0), j2 = j + (Dir == 1 ? 1 : 0);
				const double A = Val[j * NX + i], B = Val[j2 * NX + i2];
				const double T = FMath::Clamp(A / (A - B), 0.0, 1.0);
				EdgePt.Add(Id, FMath::Lerp(GridPos(i, j), GridPos(i2, j2), T));
			}
			return Id;
		};
		for (int32 j = 0; j + 1 < NY; ++j)
		{
			for (int32 i = 0; i + 1 < NX; ++i)
			{
				const bool In0 = Val[j * NX + i] <= 0.0, In1 = Val[j * NX + i + 1] <= 0.0;
				const bool In2 = Val[(j + 1) * NX + i + 1] <= 0.0, In3 = Val[(j + 1) * NX + i] <= 0.0;
				const int32 Code = (In0 ? 1 : 0) | (In1 ? 2 : 0) | (In2 ? 4 : 0) | (In3 ? 8 : 0);
				if (Code == 0 || Code == 15) continue;
				// Edges: 0 bottom (c0-c1), 1 right (c1-c2), 2 top (c3-c2), 3 left (c0-c3).
				auto E = [&](int32 k) { return k == 0 ? Edge(i, j, 0) : k == 1 ? Edge(i + 1, j, 1) : k == 2 ? Edge(i, j + 1, 0) : Edge(i, j, 1); };
				const bool bCross[4] = { In0 != In1, In1 != In2, In3 != In2, In0 != In3 };
				TArray<int32, TInlineAllocator<4>> Cr;
				for (int32 k = 0; k < 4; ++k) if (bCross[k]) Cr.Add(k);
				if (Cr.Num() == 2) Segs.Add({ E(Cr[0]), E(Cr[1]) });
				else if (Cr.Num() == 4)
				{
					const double Mid = 0.25 * (Val[j * NX + i] + Val[j * NX + i + 1] + Val[(j + 1) * NX + i + 1] + Val[(j + 1) * NX + i]);
					const bool bCutC1C3 = (In0 == (Mid <= 0.0)); // the centre joins c0 and c2: cut off c1 and c3
					if (bCutC1C3) { Segs.Add({ E(0), E(1) }); Segs.Add({ E(2), E(3) }); }
					else { Segs.Add({ E(3), E(0) }); Segs.Add({ E(1), E(2) }); }
				}
			}
		}
		// Link segments into loops.
		TMap<int32, TArray<int32, TInlineAllocator<2>>> At;
		for (int32 k = 0; k < Segs.Num(); ++k)
		{
			At.FindOrAdd(Segs[k].Key).Add(k);
			At.FindOrAdd(Segs[k].Value).Add(k);
		}
		TArray<bool> Used;
		Used.SetNumZeroed(Segs.Num());
		TArray<TArray<FVector2D>> Loops;
		for (int32 k0 = 0; k0 < Segs.Num(); ++k0)
		{
			if (Used[k0]) continue;
			TArray<FVector2D> Pts;
			int32 Seg = k0, From = Segs[k0].Key;
			Pts.Add(EdgePt[From]);
			bool bClosed = false;
			while (Seg != INDEX_NONE && !Used[Seg])
			{
				Used[Seg] = true;
				const int32 To = Segs[Seg].Key == From ? Segs[Seg].Value : Segs[Seg].Key;
				if (To == Segs[k0].Key && Seg != k0) { bClosed = true; break; }
				Pts.Add(EdgePt[To]);
				int32 Next = INDEX_NONE;
				for (int32 c : At[To]) if (c != Seg && !Used[c]) Next = c;
				if (Next == INDEX_NONE)
				{
					for (int32 c : At[To]) if (c == k0 && c != Seg) bClosed = true;
				}
				From = To;
				Seg = Next;
			}
			if (bClosed && Pts.Num() > 8) Loops.Add(MoveTemp(Pts));
		}

		// Columns along each loop.
		const FVector Nest = S.NestSpots.Num() > 0 ? S.NestSpots[0] : FVector(1e9);
		const double NestKeep = S.Params.NestMoundRadius + 130.0 + 1.2 * Kg;
		TArray<FGroundColumn> Cols;
		TArray<int32> LoopStart;
		int32 NearNest = 0;
		for (const TArray<FVector2D>& L : Loops)
		{
			double Len = 0.0;
			for (int32 k = 0; k < L.Num(); ++k) Len += FVector2D::Distance(L[k], L[(k + 1) % L.Num()]);
			if (Len < 300.0) continue;
			LoopStart.Add(Cols.Num());
			const int32 N = FMath::Max(8, FMath::RoundToInt32(Len / GroundSpacing));
			double Walked = 0.0;
			int32 k = 0;
			double SegStart = 0.0;
			for (int32 c = 0; c < N; ++c)
			{
				const double Target = Len * c / N;
				while (true)
				{
					const double SL = FVector2D::Distance(L[k], L[(k + 1) % L.Num()]);
					if (SegStart + SL >= Target || k + 1 >= L.Num()) break;
					SegStart += SL;
					++k;
				}
				const FVector2D A = L[k], B = L[(k + 1) % L.Num()];
				const double SL = FMath::Max(FVector2D::Distance(A, B), 1e-6);
				const FVector2D P = FMath::Lerp(A, B, FMath::Clamp((Target - SegStart) / SL, 0.0, 1.0));
				FGroundColumn Col;
				Col.C = FVector(P.X, P.Y, Gz);
				if (FVector::Dist2D(Col.C, Nest) < NestKeep) { ++NearNest; Col.bValid = false; Cols.Add(Col); continue; }
				Col.bValid = true;
				Cols.Add(MoveTemp(Col));
			}
			(void)Walked;
		}
		LoopStart.Add(Cols.Num());

		// Sweep each column (in parallel).
		const double RowStep = FMath::Clamp(Kg / 4.0, 8.0, 20.0);
		const double ZTop = Gz + 1.1 * Kg;
		std::atomic<int32> Failed{ 0 };
		ParallelFor(Cols.Num(), [&](int32 ci)
		{
			FGroundColumn& Col = Cols[ci];
			if (!Col.bValid) return;
			constexpr double E = 2.0;
			const double Gx = Field.TreeDistanceMT(Col.C + FVector(E, 0, 0)) - Field.TreeDistanceMT(Col.C - FVector(E, 0, 0));
			const double Gy = Field.TreeDistanceMT(Col.C + FVector(0, E, 0)) - Field.TreeDistanceMT(Col.C - FVector(0, E, 0));
			Col.N2 = FVector(Gx, Gy, 0.0).GetSafeNormal();
			if (Col.N2.IsNearlyZero()) { Col.bValid = false; Col.Why = 1; ++Failed; return; }
			FGroundPair Pair{ Field };
			// Into the corner: against the tree's surface normal and the ground's (a root lying on the ground narrows below
			// it, so straight down from its side would leave it).
			const FVector N3(Gx, Gy, Field.TreeDistanceMT(Col.C + FVector(0, 0, E)) - Field.TreeDistanceMT(Col.C - FVector(0, 0, E)));
			FVector In = -(N3.GetSafeNormal() + FVector::UpVector).GetSafeNormal();
			if (In.IsNearlyZero() || In.Z > -0.05) In = (-Col.N2 - FVector::UpVector).GetSafeNormal();
			bool bIn = false;
			for (double M = Kg; M >= Kg / 16.0 && !bIn; M *= 0.5)
			{
				Pair.O = Col.C + In * (1.4 * M);
				Pair.F0 = Field.DistanceMT(Pair.O);
				bIn = Pair.F0 < -1.0 && Field.TreeDistanceMT(Pair.O) < -1.0;
			}
			if (!bIn) { Col.bValid = false; Col.Why = 2; ++Failed; return; }
			Col.O = Pair.O;
			Col.F0 = Pair.F0;
			Col.E1 = ((Col.C + FVector::UpVector * (1.4 * Kg)) - Pair.O).GetSafeNormal();
			Col.E2 = (Col.N2 - Col.E1 * FVector::DotProduct(Col.N2, Col.E1)).GetSafeNormal();
			const double TMax = 20.0 * Kg + 2000.0;
			auto Ray = [&](double Phi, double TGuess, FSkirtRay& R)
			{
				const FVector Dir = Col.E1 * FMath::Cos(Phi) + Col.E2 * FMath::Sin(Phi);
				if (!SolveSkirtRay(Pair, Dir, TGuess, TMax, R.T)) return false;
				R.Phi = Phi;
				R.Hit = Pair.O + Dir * R.T;
				return true;
			};
			auto Refine = [&](FSkirtRay A, FSkirtRay B, TFunctionRef<double(const FVector&)> Fn, FSkirtRay& OutR)
			{
				for (int32 It = 0; It < 8; ++It)
				{
					FSkirtRay Mid;
					if (!Ray(0.5 * (A.Phi + B.Phi), 0.5 * (A.T + B.T), Mid)) return false;
					if (Fn(Mid.Hit) < 0.0) A = Mid; else B = Mid;
				}
				OutR = B;
				return true;
			};
			const double DPhi = FMath::DegreesToRadians(5.0);
			// The top: aim higher until the hit is above the ground blend (or as high as the tree reaches here).
			FSkirtRay Top;
			if (!Ray(0.0, 1.5 * Kg, Top)) { Col.bValid = false; Col.Why = 3; ++Failed; return; }
			for (int32 Up = 0; Up < 12 && Top.Hit.Z < ZTop; ++Up)
			{
				FSkirtRay R;
				if (!Ray(Top.Phi - DPhi, Top.T, R) || R.Hit.Z <= Top.Hit.Z) break;
				Top = R;
			}
			Col.bTopOnWall = Top.Hit.Z >= ZTop - 1.0;
			Col.Tree.Add(Top);
			FSkirtRay Prev = Top;
			bool bSeam = false, bEnd = false;
			auto Seamf = [&](const FVector& X) { return Field.TreeDistanceMT(X) - Field.GroundDistanceMT(X); };
			auto Endf = [&](const FVector& X) { return Field.TreeDistanceMT(X) - (1.1 * Kg + 1.0); };
			for (double Phi = Top.Phi + DPhi; Phi <= Top.Phi + FMath::DegreesToRadians(200.0); Phi += DPhi)
			{
				FSkirtRay R;
				if (!Ray(Phi, Prev.T, R) || R.T > 4.0 * Prev.T + 100.0) { Col.bValid = false; Col.Why = 4; ++Failed; return; }
				FSkirtRay From = Prev;
				if (!bSeam && Seamf(R.Hit) >= 0.0)
				{
					FSkirtRay Seam;
					if (Seamf(Prev.Hit) >= 0.0) Seam = Prev; // the top is already nearer the ground (low root): no tree half
					else if (!Refine(Prev, R, Seamf, Seam)) { Col.bValid = false; Col.Why = 5; ++Failed; return; }
					if (Seam.Phi > Col.Tree.Last().Phi) Col.Tree.Add(Seam);
					Col.Ground.Add(Col.Tree.Last());
					bSeam = true;
					From = Col.Tree.Last();
				}
				if (bSeam && Endf(R.Hit) >= 0.0)
				{
					FSkirtRay EndR;
					if (!Refine(From, R, Endf, EndR)) { Col.bValid = false; Col.Why = 6; ++Failed; return; }
					if (EndR.Phi > Col.Ground.Last().Phi) Col.Ground.Add(EndR);
					bEnd = true;
					break;
				}
				(bSeam ? Col.Ground : Col.Tree).Add(R);
				Prev = R;
			}
			if (!bEnd || Col.Tree.Num() < 2 || Col.Ground.Num() < 2) { Col.bValid = false; Col.Why = 7; ++Failed; return; }
			// The column must start on the tree, end on the ground and stay round its outline point (over a root that humps
			// out of the ground the rays can wander onto another part of it).
			const FVector& TopHit = Col.Tree[0].Hit;
			const FVector& EndHit = Col.Ground.Last().Hit;
			// (Where a root is lower than the blend, its crest lies on the fillet, not on the tree: no top check there.)
			const int32 Bad = (Col.bTopOnWall && FMath::Abs(Field.TreeDistanceMT(TopHit)) > 2.0) ? 9
				: FMath::Abs(Field.GroundDistanceMT(EndHit)) > 2.0 ? 10
				: (FVector::Dist2D(TopHit, Col.C) > 3.0 * Kg + 50.0 || FVector::Dist2D(EndHit, Col.C) > 3.0 * Kg + 50.0) ? 11 : 0;
			if (Bad != 0)
			{
				Col.bValid = false;
				Col.Why = Bad;
				++Failed;
				return;
			}
		});

		// Per loop: common row counts, rows, vertex data, two grids.
		const float TreeH = FMath::Max(S.Params.TrunkHeight, 1.f);
		int32 Built = 0, Unstitched = 0, Folded = 0, GroundFilled = 0, GroundClosed = 0;
		double GroundFillMove = 0.0;
		for (int32 l = 0; l + 1 < LoopStart.Num(); ++l)
		{
			const int32 C0 = LoopStart[l], C1 = LoopStart[l + 1];
			double ArcT = 0.0, ArcG = 0.0;
			for (int32 c = C0; c < C1; ++c)
			{
				const FGroundColumn& Col = Cols[c];
				if (!Col.bValid) continue;
				double A = 0.0, B = 0.0;
				for (int32 k = 1; k < Col.Tree.Num(); ++k) A += FVector::Dist(Col.Tree[k].Hit, Col.Tree[k - 1].Hit);
				for (int32 k = 1; k < Col.Ground.Num(); ++k) B += FVector::Dist(Col.Ground[k].Hit, Col.Ground[k - 1].Hit);
				ArcT = FMath::Max(ArcT, A);
				ArcG = FMath::Max(ArcG, B);
			}
			const int32 Nt = FMath::Clamp(FMath::CeilToInt32(ArcT / RowStep), 2, SkirtMaxRows);
			const int32 Ng = FMath::Clamp(FMath::CeilToInt32(ArcG / RowStep), 2, SkirtMaxRows);
			const int32 NC = C1 - C0;
			TArray<FSkirtVert> GT, GG;
			GT.SetNum((Nt + 1) * NC);
			GG.SetNum((Ng + 1) * NC);
			ParallelFor(NC, [&](int32 k)
			{
				FGroundColumn& Col = Cols[C0 + k];
				if (!Col.bValid) return;
				FGroundPair Pair{ Field, Col.O, Col.F0 };
				const double TMax = 20.0 * Kg + 2000.0;
				if (!SkirtRows(Pair, Col, Col.Tree, Nt, TMax, Col.RowsT) || !SkirtRows(Pair, Col, Col.Ground, Ng, TMax, Col.RowsG))
				{
					Col.bValid = false;
					Col.Why = 8;
					return;
				}
				Col.RowsG[0] = Col.RowsT[Nt];
			});
			// Runs of failed columns between two good ones (round the corner where a root meets the trunk at the ground,
			// where the sweep wanders onto the root): rows interpolated from the two neighbours and moved onto the field, so
			// the strip does not end there in an open edge (the lip). Not across a sharp corner of the outline.
			if (CVarGroundFill.GetValueOnAnyThread() != 0)
			{
				for (int32 a = 0; a < NC; ++a)
				{
					if (!Cols[C0 + a].bValid || Cols[C0 + (a + 1) % NC].bValid) continue;
					int32 Run = 1;
					while (Run <= NC && !Cols[C0 + (a + Run) % NC].bValid) ++Run;
					if (Run > NC || Run - 1 > GroundFillMax) continue;
					const FGroundColumn& Ca = Cols[C0 + a];
					const FGroundColumn& Cb = Cols[C0 + (a + Run) % NC];
					if (FVector::DotProduct(Ca.N2, Cb.N2) < 0.5) continue;
					for (int32 m = 1; m < Run; ++m)
					{
						FGroundColumn& Col = Cols[C0 + (a + m) % NC];
						const double T = double(m) / Run;
						TArray<FVector> RowsT, RowsG;
						bool bOk = true;
						auto Lift = [&](const FVector& A, const FVector& B, TArray<FVector>& Dst)
						{
							const FVector L = FMath::Lerp(A, B, T);
							FVector X = L, N;
							double Err = 0.0;
							// Far from the blend of its neighbours, the field has something else there (the root's crest): leave the gap.
							if (!Field.Project(X, N, 0.0, &Err) || FVector::Dist(X, L) > GroundFillReach) bOk = false;
							Dst.Add(X);
						};
						for (int32 r = 0; r <= Nt; ++r) Lift(Ca.RowsT[r], Cb.RowsT[r], RowsT);
						for (int32 r = 0; r <= Ng; ++r) Lift(Ca.RowsG[r], Cb.RowsG[r], RowsG);
						if (!bOk) continue;
						for (int32 r = 0; r <= Nt; ++r) GroundFillMove = FMath::Max(GroundFillMove, FVector::Dist(RowsT[r], FMath::Lerp(Ca.RowsT[r], Cb.RowsT[r], T)));
						for (int32 r = 0; r <= Ng; ++r) GroundFillMove = FMath::Max(GroundFillMove, FVector::Dist(RowsG[r], FMath::Lerp(Ca.RowsG[r], Cb.RowsG[r], T)));
						RowsG[0] = RowsT[Nt];
						Col.RowsT = MoveTemp(RowsT);
						Col.RowsG = MoveTemp(RowsG);
						Col.N2 = (Ca.N2 * (1.0 - T) + Cb.N2 * T).GetSafeNormal();
						Col.bTopOnWall = Ca.bTopOnWall && Cb.bTopOnWall;
						Col.bValid = true;
						Col.Why = 0;
						++GroundFilled;
					}
				}
			}
			// Which neighbours join (not across a sharp corner of the outline, a root meeting the trunk at the ground, where
			// their planes cross, nor where their seams or ends lie far apart). A column at an open end of the strip is laid
			// onto the wall and the ground underneath, so the strip closes onto the mesh below instead of ending in a raised
			// edge (the lip; and short strips between open ends, the slats, lie flat).
			TArray<uint8> JoinNext;
			JoinNext.Init(0, NC);
			for (int32 k = 0; k < NC; ++k)
			{
				const FGroundColumn& Ca = Cols[C0 + k];
				const FGroundColumn& Cb = Cols[C0 + (k + 1) % NC];
				JoinNext[k] = Ca.bValid && Cb.bValid && FVector::DotProduct(Ca.N2, Cb.N2) >= 0.8
					&& FVector::Dist(Ca.RowsT.Last(), Cb.RowsT.Last()) <= 3.0 * GroundSpacing && FVector::Dist(Ca.RowsG.Last(), Cb.RowsG.Last()) <= 4.0 * GroundSpacing;
			}
			int32 Collapsed = 0;
			for (int32 k = 0; k < NC; ++k)
			{
				FGroundColumn& Col = Cols[C0 + k];
				Col.bCollapse = Col.bValid && CVarGroundClose.GetValueOnAnyThread() != 0 && (!JoinNext[k] || !JoinNext[(k + NC - 1) % NC]);
				Collapsed += Col.bCollapse ? 1 : 0;
			}
			GroundClosed += Collapsed;
			ParallelFor(NC, [&](int32 k)
			{
				FGroundColumn& Col = Cols[C0 + k];
				if (!Col.bValid) return;
				if (Col.bCollapse)
				{
					// Onto the tree alone (a few steps down its gradient), or at ground height onto its foot.
					auto TreeGrad = [&](const FVector& X)
					{
						constexpr double E = 0.5;
						return FVector(Field.TreeDistanceMT(X + FVector(E, 0, 0)) - Field.TreeDistanceMT(X - FVector(E, 0, 0)),
							Field.TreeDistanceMT(X + FVector(0, E, 0)) - Field.TreeDistanceMT(X - FVector(0, E, 0)),
							Field.TreeDistanceMT(X + FVector(0, 0, E)) - Field.TreeDistanceMT(X - FVector(0, 0, E)));
					};
					auto OntoTree = [&](FVector X, bool bFoot)
					{
						if (bFoot) X.Z = Gz;
						for (int32 It = 0; It < 8; ++It)
						{
							const double D = Field.TreeDistanceMT(X);
							if (FMath::Abs(D) < 0.05) break;
							FVector G = TreeGrad(X);
							if (bFoot) G.Z = 0.0;
							if (G.SizeSquared() < 1e-12) break;
							X -= G.GetSafeNormal() * D;
						}
						return X;
					};
					for (int32 r = 0; r < Nt; ++r) Col.RowsT[r] = OntoTree(Col.RowsT[r], false);
					Col.RowsT[Nt] = OntoTree(Col.RowsT[Nt], true);
					Col.RowsG[0] = Col.RowsT[Nt];
					for (int32 r = 1; r <= Ng; ++r) Col.RowsG[r].Z = Gz;
				}
				// Tree half: bark of the nearest tube there.
				for (int32 r = 0; r <= Nt; ++r)
				{
					const FVector X = Col.RowsT[r];
					int32 Best = Meet[0];
					double BestD = TNumericLimits<double>::Max();
					for (int32 b : Meet)
					{
						const double D = Field.BranchDistance(b, X);
						if (D < BestD) { BestD = D; Best = b; }
					}
					FSkirtVert& V = GT[r * NC + k];
					const FTreeBranch& Bb = S.Branches[Best];
					ParentSurfaceAt(Bb, Ranges[Best], Out, 0, Bb.Samples.Num() - 2, X, V);
					const FVector Facet = V.P;
					const FVector Nrm = FieldNormalMT(Field, X);
					if (r == 0 && Col.bTopOnWall && FVector::Dist(Facet, X) < 3.0)
					{
						V.P = Facet; // on the tube's facets: nothing steps where the skirt meets it (keeps its normal)
					}
					else
					{
						V.N = Nrm;
						V.P = X + Nrm * GroundLift;
					}
				}
				// Ground half: the ground's vertex data.
				for (int32 r = 0; r <= Ng; ++r)
				{
					FVector X = Col.RowsG[r];
					FSkirtVert& V = GG[r * NC + k];
					if (r == 0)
					{
						V.P = GT[Nt * NC + k].P;
						V.N = GT[Nt * NC + k].N;
					}
					else if (r == Ng)
					{
						X.Z = Gz;
						V.P = X;
						V.N = FVector::UpVector;
					}
					else
					{
						V.N = FieldNormalMT(Field, X);
						V.P = X + V.N * GroundLift;
					}
					V.UV0 = FVector2D(V.P.X, V.P.Y);
					V.Col = FLinearColor::Black;
					V.Tan = (FVector::ForwardVector - V.N * V.N.X).GetSafeNormal();
				}
			});
			// Emit: tree half into the bark section, ground half into the ground's. Quads between neighbouring valid
			// columns, the loop closed.
			auto EmitGrid = [&](const TArray<FSkirtVert>& Grid, int32 Rows, FTreeMeshData& Dst, bool bBark)
			{
				const int32 Base = Dst.Vertices.Num();
				for (const FSkirtVert& V : Grid)
				{
					Dst.Vertices.Add(V.P);
					Dst.Normals.Add(V.N);
					Dst.UV0.Add(V.UV0);
					Dst.Colors.Add(bBark ? FLinearColor(V.Col.R, V.Col.G, FMath::Clamp((float)V.P.Z / TreeH, 0.f, 1.f), 1.f) : V.Col);
					Dst.Tangents.Add(FProcMeshTangent(V.Tan, false));
					if (bBark)
					{
						Dst.UV1.Add(V.UV1);
						Dst.UV2.Add(SplitAlong(V.Along));
						Dst.UV3.Add(V.UV3);
					}
				}
				for (int32 k = 0; k < NC; ++k)
				{
					const int32 k2 = (k + 1) % NC;
					if (!Cols[C0 + k].bValid || !Cols[C0 + k2].bValid) continue;
					if (!JoinNext[k])
					{
						++Unstitched;
						continue;
					}
					for (int32 r = 0; r < Rows; ++r)
					{
						const int32 A = Base + r * NC + k, B = Base + r * NC + k2, C = A + NC, D = B + NC;
						for (const FIntVector& Tri : { FIntVector(A, C, B), FIntVector(B, C, D) })
						{
							const FVector Cr = FVector::CrossProduct(Dst.Vertices[Tri.Y] - Dst.Vertices[Tri.X], Dst.Vertices[Tri.Z] - Dst.Vertices[Tri.X]);
							if (Cr.SizeSquared() < 1e-8) continue;
							const FVector Avg = Dst.Normals[Tri.X] + Dst.Normals[Tri.Y] + Dst.Normals[Tri.Z];
							// Folded (its face nearly edge-on to its vertex normals): leave it out.
							if (FMath::Abs(FVector::DotProduct(Cr.GetSafeNormal(), Avg.GetSafeNormal())) < 0.2) { ++Folded; continue; }
							if (FVector::DotProduct(Cr, Avg) > 0.0) Dst.Triangles.Append({ Tri.X, Tri.Z, Tri.Y });
							else Dst.Triangles.Append({ Tri.X, Tri.Y, Tri.Z });
						}
					}
				}
			};
			EmitGrid(GT, Nt, Out, true);
			EmitGrid(GG, Ng, OutGround, false);
			++Built;
		}
		int32 Valid = 0;
		double MinNest = TNumericLimits<double>::Max();
		FString FailedAt;
		for (const FGroundColumn& Col : Cols)
		{
			if (!Col.bValid)
			{
				if (Col.Why != 0 && FailedAt.Len() < 400) FailedAt += FString::Printf(TEXT(" [%d at %.0f %.0f]"), Col.Why, Col.C.X, Col.C.Y);
				continue;
			}
			++Valid;
			for (const FVector& X : Col.RowsG) MinNest = FMath::Min(MinNest, FVector::Dist2D(X, Nest));
		}
		UE_LOG(LogLeafline, Log, TEXT("TreeSkirts ground: blend %.0f cm, %d outline loops, %d of %d columns built (%d left out near the nest, %d failed), %d bark and %d ground vertices, nearest to the nest %.0f cm, %.1f ms; %d neighbour pairs not joined (sharp corner or far apart), %d folded triangles left out; %d failed columns filled from their neighbours (largest move onto the field %.1f cm), %d columns at open ends laid onto the wall and ground; still failed:%s"),
			Kg, Built, Valid, Cols.Num(), NearNest, Failed.load(), Out.Vertices.Num() - BarkStart, OutGround.Vertices.Num(), MinNest, (FPlatformTime::Seconds() - Start) * 1000.0, Unstitched / 2, Folded,
			GroundFilled, GroundFillMove, GroundClosed, *FailedAt);
	}
}
