#include "Tree/TreeActor.h"
#include "AntTreeGame.h"
#include "Tree/TreeSkeleton.h"
#include "Tree/TreeMeshBuilder.h"
#include "Colony/ColonyTypes.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "EngineUtils.h"
#include "World/GroundRelief.h"

namespace
{
	UProceduralMeshComponent* MakeMeshComponent(AActor* Owner, USceneComponent* Parent, const TCHAR* Name)
	{
		UProceduralMeshComponent* C = NewObject<UProceduralMeshComponent>(Owner, Name, RF_Transient);
		C->SetupAttachment(Parent);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(true);
		C->bUseAsyncCooking = false;
		C->RegisterComponent();
		return C;
	}
}

/**
 * Approach routes over the bark. The route is laid out from the skeleton: along each branch of the branch-graph walk
 * (up the start branch to its junction, along the common ancestor, out the leaf's branch), a guide point every few cm at
 * the ride height above the branch surface, at an angle round the branch (0 = its top). Each guide is projected onto the
 * walking field, gaps and sharp bends between neighbours are subdivided by projecting midpoints, and every step is checked.
 * The angle turns from the start's side onto the top (or a side, if the top is blocked by another twig's base: the
 * retries), runs there, and turns to the leaf's side for the last stretch. A child's guides start at its axis, inside the
 * parent, where they project onto the parent's surface, so a junction crossing is a continuous curve.
 */
namespace LeafRoute
{
	constexpr double Height = 1.5;        // the followers' ride height (FAntMoveTuning::RideHeight) and the trail points'
	constexpr double StepS = 9.0;         // guide spacing along a branch axis (cm)
	constexpr double MaxGap = 12.0;       // longest step between route points (cm)
	constexpr double MinGap = 4.0;        // a guide closer than this to the previous point is dropped
	constexpr double MinNormalDot = 0.0;  // a sharper turn of the surface normal within one step is subdivided
	constexpr double TurnLen = 60.0;      // the route turns from its start's side onto the top over this length (and to the leaf)
	constexpr double BarkTolerance = 10.0;
	constexpr int32 MaxRefineDepth = 5;

	enum EResult : int32 { Ok, BadInput, TooFar, Projection, OffPath, Gap, OffBark, TooLong, NumResults };
	const TCHAR* const ResultNames[NumResults] = { TEXT("ok"), TEXT("bad input"), TEXT("too far along the branches"),
		TEXT("projection failed"), TEXT("left the branches"), TEXT("gap"), TEXT("off the bark"), TEXT("too long") };

	bool OnBark(const FTreeField& Field, const FVector& A, const FVector& B)
	{
		// The follower walks the straight line and projects onto the nearest bark: every sample must stay close to the
		// surface, or it would hop to another branch. A chord across a curved trunk dips a few cm into the wood.
		for (int32 k = 1; k < 6; ++k)
		{
			if (FMath::Abs(Field.Distance(FMath::Lerp(A, B, k / 6.0))) > BarkTolerance) return false;
		}
		return true;
	}

	double RouteLength(const TArray<FTrailPoint>& Pts)
	{
		double L = 0.0;
		for (int32 k = 1; k < Pts.Num(); ++k) L += FVector::Dist(Pts[k - 1].Pos, Pts[k].Pos);
		return L;
	}

	/** Arc length of the point on a branch's centreline nearest P. */
	double NearestS(const FTreeBranch& B, const FVector& P)
	{
		double BestD2 = TNumericLimits<double>::Max(), BestS = 0.0;
		for (int32 i = 0; i + 1 < B.Samples.Num(); ++i)
		{
			const FVector A = B.Samples[i];
			const FVector AB = B.Samples[i + 1] - A;
			const double L2 = AB.SizeSquared();
			const double T = L2 > 1e-6 ? FMath::Clamp(FVector::DotProduct(P - A, AB) / L2, 0.0, 1.0) : 0.0;
			const double D2 = FVector::DistSquared(P, A + AB * T);
			if (D2 < BestD2)
			{
				BestD2 = D2;
				BestS = FMath::Lerp((double)B.SampleDist[i], (double)B.SampleDist[i + 1], T);
			}
		}
		return BestS;
	}

	/** Centre, radius, tangent and an across-frame (Top: world up across the branch; Side = T x Top) at arc length S. */
	struct FAcross
	{
		FVector C = FVector::ZeroVector;
		FVector T = FVector::UpVector;
		FVector Top = FVector::ForwardVector;
		FVector Side = FVector::RightVector;
		double R = 0.0;
	};

	FAcross FrameAt(const FTreeSkeleton& Sk, int32 Branch, double S)
	{
		const FTreeBranch& B = Sk.Branches[Branch];
		const double Len = FMath::Max((double)B.Length(), 1.0);
		FAcross F;
		float Rad = 0.f;
		F.C = TreeSkeleton::SampleAt(B, (float)(S / Len), &Rad);
		F.R = Rad;
		// Central difference over a few cm: SampleAt's own tangent is the chord of one sample span and kinks at samples.
		const FVector Ahead = TreeSkeleton::SampleAt(B, (float)(FMath::Min(S + 6.0, Len) / Len));
		const FVector Behind = TreeSkeleton::SampleAt(B, (float)(FMath::Max(S - 6.0, 0.0) / Len));
		F.T = (Ahead - Behind).GetSafeNormal();
		if (F.T.IsNearlyZero()) F.T = FVector::UpVector;
		FVector Top = FVector::UpVector - F.T * F.T.Z;
		const double TopLen = Top.Size();
		// On a near-vertical branch "up" has no direction across it: blend toward a direction across the parent, so a
		// child's guides at its base never sit on the parent's axis (where the field has no gradient).
		FVector Alt = FVector::ForwardVector;
		if (Sk.Branches.IsValidIndex(B.Parent))
		{
			FVector PT;
			TreeSkeleton::SampleAt(Sk.Branches[B.Parent], B.ParentT, nullptr, &PT);
			const FVector Across = FVector::CrossProduct(PT, FVector::UpVector);
			if (Across.SizeSquared() > 0.01) Alt = Across.GetSafeNormal();
		}
		Alt = (Alt - F.T * FVector::DotProduct(Alt, F.T)).GetSafeNormal();
		if (TopLen < 0.25)
		{
			Top += Alt * ((0.25 - TopLen) * 4.0);
			Top -= F.T * FVector::DotProduct(Top, F.T);
		}
		F.Top = Top.GetSafeNormal();
		if (F.Top.IsNearlyZero()) F.Top = Alt.IsNearlyZero() ? FVector::ForwardVector : Alt;
		F.Side = FVector::CrossProduct(F.T, F.Top);
		return F;
	}

	double AngleIn(const FAcross& F, const FVector& V)
	{
		return FMath::Atan2(FVector::DotProduct(V, F.Side), FVector::DotProduct(V, F.Top));
	}

	FVector Radial(const FAcross& F, double Theta)
	{
		return F.Top * FMath::Cos(Theta) + F.Side * FMath::Sin(Theta);
	}

	double Smooth(double T)
	{
		T = FMath::Clamp(T, 0.0, 1.0);
		return T * T * (3.0 - 2.0 * T);
	}

	double LerpAngle(double A, double B, double T)
	{
		return A + FMath::UnwindRadians(B - A) * T;
	}

	/** One stretch of the walk: along Branch from arc length S0 to S1. */
	struct FLeg
	{
		int32 Branch = INDEX_NONE;
		double S0 = 0.0;
		double S1 = 0.0;
	};
	using FLegs = TArray<FLeg, TInlineAllocator<8>>;

	/**
	 * The branch-graph walk from (BranchA, SA) to (BranchB, SB) through their common ancestor. OutLen is its centreline
	 * length; OutBark discounts the stretch of each child branch buried in its parent (a closer estimate of the walk over
	 * the bark). False if the branches are not connected.
	 */
	bool Walk(const FTreeSkeleton& Sk, int32 BranchA, double SA, int32 BranchB, double SB, FLegs* OutLegs, double& OutLen, double& OutBark)
	{
		OutLen = OutBark = 0.0;
		if (OutLegs) OutLegs->Reset();
		TArray<int32, TInlineAllocator<8>> ChainA, ChainB;
		for (int32 b = BranchA; Sk.Branches.IsValidIndex(b) && ChainA.Num() < 16; b = Sk.Branches[b].Parent) ChainA.Add(b);
		for (int32 b = BranchB; Sk.Branches.IsValidIndex(b) && ChainB.Num() < 16; b = Sk.Branches[b].Parent) ChainB.Add(b);
		int32 IA = INDEX_NONE, IB = INDEX_NONE;
		for (int32 i = 0; i < ChainA.Num() && IA == INDEX_NONE; ++i)
		{
			const int32 j = ChainB.Find(ChainA[i]);
			if (j != INDEX_NONE)
			{
				IA = i;
				IB = j;
			}
		}
		if (IA == INDEX_NONE) return false;

		auto JunctionS = [&Sk](int32 Child)
		{
			const FTreeBranch& C = Sk.Branches[Child];
			return (double)C.ParentT * Sk.Branches[C.Parent].Length();
		};
		auto ParentRadius = [&Sk](int32 Child)
		{
			const FTreeBranch& C = Sk.Branches[Child];
			float R = 0.f;
			TreeSkeleton::SampleAt(Sk.Branches[C.Parent], C.ParentT, &R);
			return (double)R;
		};
		auto AddLeg = [&](int32 Branch, double S0, double S1, bool bChild, int32 ChildIndex)
		{
			const double D = FMath::Abs(S1 - S0);
			OutLen += D;
			OutBark += bChild ? FMath::Max(D - ParentRadius(ChildIndex), 0.0) : D;
			if (OutLegs) OutLegs->Add({ Branch, S0, S1 });
		};

		// Down the start branch's ancestry to the common ancestor (toward each branch's base)...
		double S = SA;
		for (int32 i = 0; i < IA; ++i)
		{
			const int32 b = ChainA[i];
			AddLeg(b, S, 0.0, true, b);
			S = JunctionS(b);
		}
		// ...along it...
		const int32 Lca = ChainA[IA];
		const double SEnd = IB == 0 ? SB : JunctionS(ChainB[IB - 1]);
		AddLeg(Lca, S, SEnd, false, INDEX_NONE);
		// ...and out to the leaf's branch.
		for (int32 j = IB - 1; j >= 0; --j)
		{
			const int32 b = ChainB[j];
			AddLeg(b, 0.0, j == 0 ? SB : JunctionS(ChainB[j - 1]), true, b);
		}
		return true;
	}

	/** Projects guides onto the field and joins them into a route with checked steps. */
	struct FTracer
	{
		const FTreeField& Field;
		TArray<int32, TInlineAllocator<32>> Allowed; // branches the route may touch
		TArray<FTrailPoint> Pts;

		explicit FTracer(const FTreeField& InField) : Field(InField) {}

		EResult Place(FVector& P, FVector& N) const
		{
			double Err = 0.0;
			if (!Field.Project(P, N, Height, &Err))
			{
				// A symmetric spot can have no gradient: nudge off it and try again (as UAntMovementComponent::SnapToSurface).
				P += FVector(0.7, 0.3, 1.1);
				if (!Field.Project(P, N, Height, &Err)) return Projection;
			}
			int32 Nearest = INDEX_NONE;
			Field.Distance(P, &Nearest);
			return Nearest != INDEX_NONE && Allowed.Contains(Nearest) ? Ok : OffPath;
		}

		/** Appends the points after A up to B, subdividing until each step is short and gently curved. */
		EResult Refine(const FTrailPoint& A, const FTrailPoint& B, int32 Depth)
		{
			if (FVector::Dist(A.Pos, B.Pos) <= MaxGap && FVector::DotProduct(A.Normal, B.Normal) >= MinNormalDot)
			{
				if (!OnBark(Field, A.Pos, B.Pos)) return OffBark;
				Pts.Add(B);
				return Ok;
			}
			if (Depth >= MaxRefineDepth) return Gap;
			FTrailPoint M;
			M.Pos = (A.Pos + B.Pos) * 0.5;
			const EResult R = Place(M.Pos, M.Normal);
			if (R != Ok) return R;
			const EResult R1 = Refine(A, M, Depth + 1);
			return R1 != Ok ? R1 : Refine(M, B, Depth + 1);
		}

		/** Projects a guide and joins it to the route. The last guide is always kept: the route must end at the leaf. */
		EResult Add(const FVector& Guide, bool bLast)
		{
			FTrailPoint Q;
			Q.Pos = Guide;
			const EResult R = Place(Q.Pos, Q.Normal);
			if (R != Ok) return R;
			// Drop points the route would double back over: where a child rises from its parent's top, its first guides
			// project a little behind the junction. Only near-reversals (over about 155 degrees); fork turns are gentler.
			while (Pts.Num() >= 2)
			{
				const FVector In = Pts.Last().Pos - Pts[Pts.Num() - 2].Pos;
				const FVector Out = Q.Pos - Pts.Last().Pos;
				if (FVector::DotProduct(In, Out) >= -0.9 * In.Size() * Out.Size()) break;
				Pts.RemoveAt(Pts.Num() - 1);
			}
			if (FVector::Dist(Pts.Last().Pos, Q.Pos) < MinGap)
			{
				if (!bLast) return Ok;
				if (Pts.Num() > 1) Pts.RemoveAt(Pts.Num() - 1);
			}
			const FTrailPoint Prev = Pts.Last(); // a copy: Refine appends to Pts
			return Refine(Prev, Q, 0);
		}
	};

	EResult BuildApproach(const FTreeSkeleton& Sk, const FTreeField& Field, const FLeafSet& Leaves,
		const TArray<TArray<int32>>& Children, const FVector& From, int32 FromBranch, int32 Leaf, double MaxWalk,
		TArray<FTrailPoint>& OutRoute)
	{
		OutRoute.Reset();
		if (!Field.IsValid() || Leaf < 0 || Leaf >= Leaves.Num()) return BadInput;
		const FTreeLeaf& L = Leaves.Get(Leaf);
		auto Usable = [&Sk](int32 b) { return Sk.Branches.IsValidIndex(b) && Sk.Branches[b].Samples.Num() >= 2; };
		if (!Usable(L.Branch)) return BadInput;

		FTracer Tr(Field);
		// Start on the bark under From.
		FTrailPoint Start;
		Start.Pos = From;
		{
			double Err = 0.0;
			if (!Field.Project(Start.Pos, Start.Normal, Height, &Err))
			{
				Start.Pos += FVector(0.7, 0.3, 1.1);
				if (!Field.Project(Start.Pos, Start.Normal, Height, &Err)) return Projection;
			}
		}
		int32 NearStart = INDEX_NONE;
		Field.Distance(Start.Pos, &NearStart);
		int32 B0 = Usable(FromBranch) ? FromBranch : NearStart;
		if (!Usable(B0)) return BadInput; // on the ground
		double S0 = NearestS(Sk.Branches[B0], Start.Pos);
		FAcross F0 = FrameAt(Sk, B0, S0);
		if (B0 != NearStart && Usable(NearStart))
		{
			// The given branch is not under From (more than a hand's width off its surface): walk from the nearest one.
			const FVector V = Start.Pos - F0.C;
			const double Off = (V - F0.T * FVector::DotProduct(V, F0.T)).Size() - F0.R;
			if (FMath::Abs(Off) > 25.0)
			{
				B0 = NearStart;
				S0 = NearestS(Sk.Branches[B0], Start.Pos);
				F0 = FrameAt(Sk, B0, S0);
			}
		}
		const double S1 = FMath::Clamp((double)L.BranchS, 0.0, (double)Sk.Branches[L.Branch].Length());
		const FAcross F1 = FrameAt(Sk, L.Branch, S1);

		FLegs Legs;
		double Len = 0.0, Bark = 0.0;
		if (!Walk(Sk, B0, S0, L.Branch, S1, &Legs, Len, Bark)) return BadInput;
		if (Bark > MaxWalk * 1.15 + 30.0) return TooFar;

		// The route may touch the walk's branches, their parents (junctions) and their children (it may brush past the base
		// of a twig it does not take), but nothing else: a point on any other branch means it hopped across the air.
		auto Allow = [&](int32 b)
		{
			if (!Sk.Branches.IsValidIndex(b)) return;
			Tr.Allowed.AddUnique(b);
			if (Sk.Branches.IsValidIndex(Sk.Branches[b].Parent)) Tr.Allowed.AddUnique(Sk.Branches[b].Parent);
			if (Children.IsValidIndex(b)) for (int32 c : Children[b]) Tr.Allowed.AddUnique(c);
		};
		for (const FLeg& G : Legs) Allow(G.Branch);
		if (Usable(NearStart)) Tr.Allowed.AddUnique(NearStart);

		const double ThStart = AngleIn(F0, Start.Pos - F0.C);
		const double ThEnd = AngleIn(F1, L.Base - F1.C);
		const bool bCruise = Len >= 2.0 * TurnLen + 20.0;
		// Long routes: over the top first, then further round on either side (another twig's base can block the top).
		// Short ones turn straight from the start's side to the leaf's, or the other way round the branch.
		TArray<double, TInlineAllocator<6>> Options;
		const double Delta = FMath::UnwindRadians(ThEnd - ThStart);
		if (bCruise) Options.Append({ 0.0, 1.05, -1.05, 2.1, -2.1, PI });
		else
		{
			Options.Add(Delta);
			if (FMath::Abs(Delta) > 0.5) Options.Add(Delta - FMath::Sign(Delta) * 2.0 * PI);
		}

		EResult First = Ok;
		for (int32 o = 0; o < Options.Num(); ++o)
		{
			const double Opt = Options[o];
			auto Theta = [&](double U)
			{
				if (!bCruise) return ThStart + Opt * Smooth(Len > 1.0 ? U / Len : 1.0);
				if (U < TurnLen) return LerpAngle(ThStart, Opt, Smooth(U / TurnLen));
				if (U > Len - TurnLen) return LerpAngle(Opt, ThEnd, Smooth((U - (Len - TurnLen)) / TurnLen));
				return Opt;
			};
			Tr.Pts.Reset();
			Tr.Pts.Add(Start);
			EResult R = Ok;
			double U = 0.0;
			for (int32 g = 0; g < Legs.Num() && R == Ok; ++g)
			{
				const FLeg& Leg = Legs[g];
				const double D = FMath::Abs(Leg.S1 - Leg.S0);
				const int32 N = FMath::Max(1, FMath::CeilToInt32(D / StepS));
				for (int32 k = 0; k <= N && R == Ok; ++k)
				{
					const double A = double(k) / N;
					const FAcross F = FrameAt(Sk, Leg.Branch, FMath::Lerp(Leg.S0, Leg.S1, A));
					const bool bLast = g == Legs.Num() - 1 && k == N;
					// At U = Len the angle is ThEnd in the leaf's frame, so the last guide sits on the leaf's Base.
					R = Tr.Add(F.C + Radial(F, Theta(bLast ? Len : U + D * A)) * (F.R + Height), bLast);
				}
				U += D;
			}
			if (R == Ok && FVector::Dist(Tr.Pts.Last().Pos, L.Base) > 25.0) R = Gap;
			if (R == Ok && RouteLength(Tr.Pts) > MaxWalk) R = TooLong;
			if (R == Ok)
			{
				OutRoute = MoveTemp(Tr.Pts);
				return Ok;
			}
			if (o == 0) First = R;
		}
		return First;
	}
}

ATreeActor::ATreeActor()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMat(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (BasicMat.Succeeded()) FallbackMaterial = BasicMat.Object;
	// Procedural materials built over MCP (maths only, no textures).
	// M_TreeBark: world-space bark with the camera cutaway. M_Bark is the older UV-based version.
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BarkMat(TEXT("/Game/Leafline/Materials/M_TreeBark.M_TreeBark"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> OldBarkMat(TEXT("/Game/Leafline/Materials/M_Bark.M_Bark"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> LeafMat(TEXT("/Game/Leafline/Materials/M_Leaf.M_Leaf"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> GroundMat(TEXT("/Game/Leafline/Materials/M_Ground.M_Ground"));
	if (BarkMat.Succeeded()) BarkMaterial = BarkMat.Object;
	else if (OldBarkMat.Succeeded()) BarkMaterial = OldBarkMat.Object;
	if (LeafMat.Succeeded()) LeafMaterial = LeafMat.Object;
	if (GroundMat.Succeeded()) GroundMaterial = GroundMat.Object;
}

void ATreeActor::Grow(int32 Seed)
{
	const double StartTime = FPlatformTime::Seconds();
	GrowShape(Seed);
	BuildLeafMeshes();
	UE_LOG(LogLeafline, Log, TEXT("Tree grown: seed %d in %.0f ms"), Seed, (FPlatformTime::Seconds() - StartTime) * 1000.0);
}

namespace
{
	bool GNewBark = true;
	int32 GCollarsOverride = -1; // -1: FTreeParams::bBranchCollars decides; 0/1: Leaf.Collars
	int32 GFilletsOverride = -1; // -1: FTreeParams::bFillets decides; 0/1: Leaf.Fillets
	int32 GFilletWalkOverride = -1; // -1: FTreeParams::bFilletWalk decides; 0/1: Leaf.FilletWalk
	float GFilletFrac = -1.f, GFilletMax = -1.f, GFilletBoughMul = -1.f, GFilletGroundK = -1.f, GFilletTwigMin = -1.f; // < 0: FTreeParams decides
}

void ATreeActor::SetFilletsOverride(bool bOn, float Frac, float Max, float BoughMul, float GroundK, float TwigMinRadius)
{
	if (TwigMinRadius >= 0.f) GFilletTwigMin = TwigMinRadius;
	if (GroundK >= 0.f) GFilletGroundK = GroundK;
	GFilletsOverride = bOn ? 1 : 0;
	if (Frac >= 0.f) GFilletFrac = Frac;
	if (Max >= 0.f) GFilletMax = Max;
	if (BoughMul >= 0.f) GFilletBoughMul = BoughMul;
}

void ATreeActor::SetFilletWalkOverride(int32 Walk)
{
	GFilletWalkOverride = Walk < 0 ? -1 : (Walk != 0 ? 1 : 0);
}

FTreeParams ATreeActor::WithFilletOverrides(const FTreeParams& P)
{
	FTreeParams Out = P;
	if (GFilletsOverride >= 0) Out.bFillets = GFilletsOverride != 0;
	if (GFilletWalkOverride >= 0) Out.bFilletWalk = GFilletWalkOverride != 0;
	if (GFilletFrac >= 0.f) Out.FilletFrac = GFilletFrac;
	if (GFilletMax >= 0.f) Out.FilletMax = GFilletMax;
	if (GFilletBoughMul >= 0.f) Out.FilletBoughMul = GFilletBoughMul;
	if (GFilletGroundK >= 0.f) Out.FilletGroundK = GFilletGroundK;
	if (GFilletTwigMin >= 0.f) Out.FilletTwigMinRadius = GFilletTwigMin;
	return Out;
}

void ATreeActor::SetBranchCollarsOverride(bool bOn)
{
	GCollarsOverride = bOn ? 1 : 0;
}

bool ATreeActor::AreBranchCollarsOn(const FTreeParams& P)
{
	return GCollarsOverride >= 0 ? GCollarsOverride != 0 : P.bBranchCollars;
}

bool ATreeActor::IsNewBark()
{
	return GNewBark;
}

UMaterialInterface* ATreeActor::PickBark()
{
	if (GNewBark && !NewBarkMaterial)
	{
		// Loaded by path (the folder is always cooked) so a missing asset only falls back to M_TreeBark.
		NewBarkMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Leafline/Materials/M_TreeBark2.M_TreeBark2"), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!NewBarkMaterial) UE_LOG(LogLeafline, Warning, TEXT("M_TreeBark2 not found; the bark uses M_TreeBark"));
	}
	return GNewBark && NewBarkMaterial ? NewBarkMaterial.Get() : BarkMaterial.Get();
}

namespace
{
	bool GGroundLook2 = true;
}

bool ATreeActor::IsGroundLook2()
{
	return GGroundLook2;
}

UMaterialInterface* ATreeActor::PickGround()
{
	if (GGroundLook2 && !Ground2MID)
	{
		// Loaded by path (the folder is always cooked) so a missing asset only falls back to M_Ground.
		if (UMaterialInterface* M2 = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Leafline/Materials/M_Ground2.M_Ground2"), nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			Ground2MID = UMaterialInstanceDynamic::Create(M2, this);
			// Half the material's default undulation (0.3): from 60 to 120 m up it read as a dimpled mud flat (floor pass).
			Ground2MID->SetScalarParameterValue(TEXT("Undulate"), 0.15f);
		}
		else
		{
			UE_LOG(LogLeafline, Warning, TEXT("M_Ground2 not found; the ground uses M_Ground"));
			GGroundLook2 = false;
		}
	}
	return GGroundLook2 && Ground2MID ? static_cast<UMaterialInterface*>(Ground2MID.Get()) : GroundMaterial.Get();
}

void ATreeActor::SetGroundLook(bool bNew)
{
	GGroundLook2 = bNew;
	UMaterialInterface* Mat = PickGround();
	if (Mat && WoodMesh && WoodMesh->GetNumSections() > 1) WoodMesh->SetMaterial(1, Mat);
	UE_LOG(LogLeafline, Log, TEXT("Ground material: %s"), Mat ? *Mat->GetName() : TEXT("none"));
}

bool ATreeActor::SetGroundParam(FName Name, float Value)
{
	if (!Ground2MID) return false;
	Ground2MID->SetScalarParameterValue(Name, Value);
	return true;
}

void ATreeActor::RebuildGroundAndGrove()
{
	if (!WoodMesh) return;
	BuildGround();
	BuildGrove();
	BuildGroveCrowns();
}

void ATreeActor::SetNewBark(bool bOn)
{
	GNewBark = bOn;
	UMaterialInterface* Mat = PickBark();
	if (!Mat) return;
	if (WoodMesh) WoodMesh->SetMaterial(0, Mat);
	if (GroveMesh) GroveMesh->SetMaterial(0, Mat);
	UE_LOG(LogLeafline, Log, TEXT("Bark material: %s"), *Mat->GetName());
}

bool ATreeActor::GrowWithBites(int32 Seed, const TArray<FSavedLeaf>& Bites, int32 LeafCount)
{
	const double StartTime = FPlatformTime::Seconds();
	GrowShape(Seed);
	const bool bApplied = ApplySavedBites(Bites, LeafCount);
	BuildLeafMeshes(); // once, with the bites already in
	UE_LOG(LogLeafline, Log, TEXT("Tree grown: seed %d in %.0f ms"), Seed, (FPlatformTime::Seconds() - StartTime) * 1000.0);
	return bApplied;
}

void ATreeActor::GrowShape(int32 Seed)
{
	Params.bBranchCollars = AreBranchCollarsOn(Params);
	Params = WithFilletOverrides(Params);
	++ShapeVersion;
	TreeSkeleton::Grow(Skeleton, Params, Seed);
	const double FieldStart = FPlatformTime::Seconds();
	// With bFilletWalk the sim walks on the fillets (the walking field has them and the skirts lie on it); otherwise the
	// fillets only shape the skirts over the junctions in the mesh and the sim walks on the field without them. Leaves are
	// placed against the field without fillets either way, so leaf count and order do not depend on them (one-tree saves
	// store bites by leaf index).
	const bool bWalkFillets = Params.bFillets && Params.bFilletWalk;
	Field.Build(Skeleton, bWalkFillets);
	SkirtField.Reset();
	if (Params.bFillets && !bWalkFillets) SkirtField.Build(Skeleton, true);
	PlainField.Reset();
	if (Field.HasFillets()) PlainField.Build(Skeleton, false);
	const double FieldMs = (FPlatformTime::Seconds() - FieldStart) * 1000.0;
	BranchChildren.Reset();
	BranchChildren.SetNum(Skeleton.Branches.Num());
	for (int32 b = 0; b < Skeleton.Branches.Num(); ++b)
	{
		const int32 Parent = Skeleton.Branches[b].Parent;
		if (BranchChildren.IsValidIndex(Parent)) BranchChildren[Parent].Add(b);
	}

	FTreeMeshData Data, GroundSkirt;
	TArray<int32> NoSkirt;
	TreeMeshBuilder::Build(Skeleton, Data, &GetVisualField(), &NoSkirt, &GroundSkirt);
	if (NoSkirt.Num() > 0)
	{
		// A junction whose skirt could not be built shows its crease: drop its fillet, and lay the other skirts again on the
		// field without it (they differ only near it).
		for (int32 b : NoSkirt) Skeleton.Branches[b].FilletK = 0.f;
		if (bWalkFillets) Field.Build(Skeleton, true);
		else SkirtField.Build(Skeleton, true);
		UE_LOG(LogLeafline, Log, TEXT("Tree shape: %d fillets without a skirt dropped; building the mesh again"), NoSkirt.Num());
		TreeMeshBuilder::Build(Skeleton, Data, &GetVisualField(), nullptr, &GroundSkirt);
	}

	if (!WoodMesh) WoodMesh = MakeMeshComponent(this, RootComponent, TEXT("WoodMesh"));
	WoodMesh->ClearAllMeshSections();
	WoodMesh->CreateMeshSection_LinearColor(0, Data.Vertices, Data.Triangles, Data.Normals, Data.UV0, Data.UV1, Data.UV2, Data.UV3,
		Data.Colors, Data.Tangents, false, false);

	UMaterialInterface* Mat = PickBark();
	if (!Mat && FallbackMaterial)
	{
		if (!BarkMID) BarkMID = UMaterialInstanceDynamic::Create(FallbackMaterial, this);
		BarkMID->SetVectorParameterValue(TEXT("Color"), BarkColor);
		Mat = BarkMID;
	}
	if (Mat) WoodMesh->SetMaterial(0, Mat);
	BuildGround(&GroundSkirt);
	BuildGrove();

	// Leaves: one mesh section per limb, so a cut rebuilds only a small chunk.
	Leaves.Build(Skeleton, PlainField.IsValid() ? PlainField : Field);
	PatchWalkCache.Reset();
	UE_LOG(LogLeafline, Log, TEXT("Tree shape: fillets %s in the mesh, %s the walking field (frac %.2f, bough x%.2f, max %.0f cm, ground %.0f cm, twigs from %.1f cm), fields built in %.1f ms"),
		GetVisualField().HasFillets() ? TEXT("on") : TEXT("off"), Field.HasFillets() ? TEXT("in") : TEXT("not in"),
		Params.FilletFrac, Params.FilletBoughMul, Params.FilletMax, Params.FilletGroundK, Params.FilletTwigMinRadius, FieldMs);
}

bool ATreeActor::LiftOntoSkirts(FVector& P, FVector& N, double Height) const
{
	const FTreeField& Skirts = GetVisualField();
	if (!Skirts.HasFillets()) return false;
	// Outside every skirt the two fields agree, so the walking height is already the skirt height there (and everywhere
	// when the ants walk on the fillets, except for points laid on the field without them).
	if (Skirts.DistanceMT(P) >= Height - 0.05) return false;
	FVector Q = P, QN;
	if (!Skirts.Project(Q, QN, Height)) return false;
	P = Q;
	N = QN;
	return true;
}

void ATreeActor::BuildLeafMeshes()
{
	if (!LeafMesh) LeafMesh = MakeMeshComponent(this, RootComponent, TEXT("LeafMesh"));
	LeafMesh->ClearAllMeshSections();
	LeafMaterialInUse = LeafMaterial;
	if (!LeafMaterialInUse && FallbackMaterial)
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(FallbackMaterial, this);
		MID->SetVectorParameterValue(TEXT("Color"), LeafColor);
		LeafMaterialInUse = MID;
	}
	for (int32 c = 0; c < Leaves.NumChunks(); ++c) BuildLeafChunk(c, false);
	BuildGroveCrowns();
}

void ATreeActor::BuildGroveCrowns()
{
	// Grove crowns (leaves made in BuildGrove), drawn with the same leaf material.
	if (!GroveLeafMesh)
	{
		GroveLeafMesh = MakeMeshComponent(this, RootComponent, TEXT("GroveLeafMesh"));
		GroveLeafMesh->SetCastShadow(false);
	}
	GroveLeafMesh->ClearAllMeshSections();
	if (GroveLeaves.Num() > 0)
	{
		FTreeMeshData Crowns;
		GroveLeaves.BuildChunkMesh(0, Crowns, 3); // far away: three segments per blade are plenty
		GroveLeafMesh->CreateMeshSection_LinearColor(0, Crowns.Vertices, Crowns.Triangles, Crowns.Normals, Crowns.UV0, {}, {}, {},
			Crowns.Colors, Crowns.Tangents, false, false);
		if (LeafMaterialInUse) GroveLeafMesh->SetMaterial(0, LeafMaterialInUse);
		UE_LOG(LogLeafline, Log, TEXT("Grove crowns: %d leaves, %d triangles"), GroveLeaves.Num(), Crowns.Triangles.Num() / 3);
	}
}

void ATreeActor::BuildLeafChunk(int32 Chunk, bool bHitchCheck)
{
	if (!LeafMesh) return;
	const double StartTime = FPlatformTime::Seconds();
	FTreeMeshData Data;
	Leaves.BuildChunkMesh(Chunk, Data);
	LeafMesh->CreateMeshSection_LinearColor(Chunk, Data.Vertices, Data.Triangles, Data.Normals, Data.UV0, {}, {}, {},
		Data.Colors, Data.Tangents, false, false);
	if (LeafMaterialInUse) LeafMesh->SetMaterial(Chunk, LeafMaterialInUse);
	const double Ms = (FPlatformTime::Seconds() - StartTime) * 1000.0;
	if (bHitchCheck && Ms > 4.0)
	{
		UE_LOG(LogLeafline, Warning, TEXT("Leaf chunk %d rebuilt in %.1f ms (over 4 ms: a hitch), %d vertices"), Chunk, Ms,
			Data.Vertices.Num());
	}
}

bool ATreeActor::BiteLeaf(int32 Leaf, float Size01, float DiameterCm, float UsePerCut)
{
	if (!Leaves.ApplyBite(Leaf, Size01, DiameterCm, UsePerCut)) return false;
	BuildLeafChunk(Leaves.Get(Leaf).Chunk);
	UE_LOG(LogLeafline, Log, TEXT("Leaf %d bitten: size %.2f, %d bites, %.0f%% left"), Leaf, Size01, Leaves.Get(Leaf).Bites,
		Leaves.Get(Leaf).Remaining * 100.f);
	return true;
}

// --- One tree per stage: saved bites, stripping, the tree report ---
void ATreeActor::ExportBites(TArray<FSavedLeaf>& Out) const
{
	Out.Reset();
	for (int32 i = 0; i < Leaves.Num(); ++i)
	{
		const FTreeLeaf& L = Leaves.Get(i);
		if (L.Bites == 0 && L.Remaining >= 1.f) continue;
		FSavedLeaf& S = Out.AddDefaulted_GetRef();
		S.Leaf = i;
		S.Bites = FMath::Min<int32>(L.Bites, 3);
		S.Remaining = L.Remaining;
		S.BiteData.Reserve(S.Bites * 3);
		for (int32 b = 0; b < S.Bites; ++b)
		{
			S.BiteData.Add(L.BiteCenter[b].X);
			S.BiteData.Add(L.BiteCenter[b].Y);
			S.BiteData.Add(L.BiteRadius[b]);
		}
	}
}

bool ATreeActor::ApplySavedBites(const TArray<FSavedLeaf>& Saved, int32 LeafCount)
{
	if (Saved.Num() == 0) return true;
	if (LeafCount != Leaves.Num())
	{
		UE_LOG(LogLeafline, Warning, TEXT("Tree: saved bites dropped (%d leaves): saved on a tree of %d leaves, seed %d grew %d"),
			Saved.Num(), LeafCount, GetSeed(), Leaves.Num());
		return false;
	}
	// Check everything first: a save is applied whole or not at all.
	TBitArray<> Seen(false, Leaves.Num());
	for (int32 e = 0; e < Saved.Num(); ++e)
	{
		const FSavedLeaf& S = Saved[e];
		const TCHAR* Problem = nullptr;
		if (S.Leaf < 0 || S.Leaf >= Leaves.Num()) Problem = TEXT("leaf index out of range");
		else if (Seen[S.Leaf]) Problem = TEXT("leaf listed twice");
		else if (S.Bites < 0 || S.Bites > 3) Problem = TEXT("bite count out of range");
		else if (S.BiteData.Num() != S.Bites * 3) Problem = TEXT("bite data does not match the bite count");
		else if (!FMath::IsFinite(S.Remaining) || S.Remaining < 0.f || S.Remaining > 1.f) Problem = TEXT("remaining share out of range");
		for (int32 k = 0; !Problem && k < S.BiteData.Num(); ++k)
		{
			if (!FMath::IsFinite(S.BiteData[k])) Problem = TEXT("bite data not finite");
			else if (k % 3 == 2 && S.BiteData[k] < 0.f) Problem = TEXT("negative bite radius");
		}
		if (Problem)
		{
			UE_LOG(LogLeafline, Warning, TEXT("Tree: saved bites dropped (%d leaves): entry %d (leaf %d): %s"), Saved.Num(), e,
				S.Leaf, Problem);
			return false;
		}
		Seen[S.Leaf] = true;
	}

	int32 NumBites = 0;
	for (const FSavedLeaf& S : Saved)
	{
		FTreeLeaf& L = Leaves.Leaves[S.Leaf];
		L.Bites = (uint8)S.Bites;
		L.Remaining = S.Remaining;
		for (int32 b = 0; b < 3; ++b)
		{
			const bool bHas = b < S.Bites;
			L.BiteCenter[b] = bHas ? FVector2f(S.BiteData[3 * b], S.BiteData[3 * b + 1]) : FVector2f(0.f, 0.f);
			L.BiteRadius[b] = bHas ? S.BiteData[3 * b + 2] : 0.f;
		}
		NumBites += S.Bites;
	}
	UE_LOG(LogLeafline, Log, TEXT("Tree: applied %d saved bites (%d leaves)"), NumBites, Saved.Num());
	return true;
}

int32 ATreeActor::StripBelow(float Z)
{
	TBitArray<> Dirty(false, Leaves.NumChunks());
	int32 Changed = 0;
	for (FTreeLeaf& L : Leaves.Leaves)
	{
		if (L.Base.Z >= Z || L.Remaining <= 0.f) continue;
		L.Remaining = 0.f;
		++Changed;
		if (Dirty.IsValidIndex(L.Chunk)) Dirty[L.Chunk] = true;
	}
	const double StartTime = FPlatformTime::Seconds();
	int32 Rebuilt = 0;
	for (int32 c = 0; c < Dirty.Num(); ++c)
	{
		if (!Dirty[c]) continue;
		BuildLeafChunk(c);
		++Rebuilt;
	}
	UE_LOG(LogLeafline, Log, TEXT("Tree: stripped %d leaves below %.1f m, %d chunks rebuilt in %.1f ms"), Changed, Z / 100.f,
		Rebuilt, (FPlatformTime::Seconds() - StartTime) * 1000.0);
	return Changed;
}

int32 ATreeActor::FindSpentLeafNear(const FVector& P, float Reach) const
{
	return Leaves.FindNearestSpent(P, Reach);
}

int32 ATreeActor::CountPiecesLeft(float Template, float UsePerCut) const
{
	int32 Pieces = 0;
	for (int32 i = 0; i < Leaves.Num(); ++i) Pieces += Leaves.PiecesLeft(i, Template, UsePerCut);
	return Pieces;
}

void ATreeActor::LogTreeReport(const FString& Label) const
{
	const int32 N = Leaves.Num();
	if (N == 0)
	{
		UE_LOG(LogLeafline, Log, TEXT("TreeReport %s: no leaves (seed %d)"), *Label, GetSeed());
		return;
	}
	const FTreePatchRules Rules{}; // pieces are counted at the patch template (0.5) and use per cut (0.38)
	struct FCount
	{
		int32 Live = 0;
		int32 All = 0;
		int32 Pieces = 0;
		void Add(bool bLive, int32 InPieces)
		{
			Live += bLive ? 1 : 0;
			++All;
			Pieces += InPieces;
		}
		FString Describe() const { return FString::Printf(TEXT("%d/%d live, %d pieces"), Live, All, Pieces); }
	};

	// Bands of 10 m on multiples of 10 m, from the band holding the lowest leaf (spent leaves count too, so the bands stay put).
	constexpr double Band = 1000.0;
	double ZMin = TNumericLimits<double>::Max(), ZMax = -TNumericLimits<double>::Max();
	for (const FTreeLeaf& L : Leaves.Leaves)
	{
		ZMin = FMath::Min(ZMin, L.Base.Z);
		ZMax = FMath::Max(ZMax, L.Base.Z);
	}
	const double Z0 = FMath::FloorToDouble(ZMin / Band) * Band;
	const int32 NumBands = FMath::FloorToInt32((ZMax - Z0) / Band) + 1;
	TArray<FCount> Bands;
	Bands.SetNum(NumBands);

	// A leaf's bough (level 2: a patch is a bough and its twigs) and limb (level 1): up from its branch to that level.
	auto AncestorAt = [this](int32 Branch, int32 Level)
	{
		int32 A = Branch;
		for (int32 Guard = 0; Guard < 16 && Skeleton.Branches.IsValidIndex(A) && Skeleton.Branches[A].Level > Level; ++Guard)
		{
			A = Skeleton.Branches[A].Parent;
		}
		return Skeleton.Branches.IsValidIndex(A) && Skeleton.Branches[A].Level == Level ? A : INDEX_NONE;
	};
	TMap<int32, FCount> Boughs, Limbs;
	FCount Total;
	double LiveZ = 0.0;
	for (int32 i = 0; i < N; ++i)
	{
		const FTreeLeaf& L = Leaves.Get(i);
		const bool bLive = L.CanCut();
		const int32 Pieces = Leaves.PiecesLeft(i, Rules.Template, Rules.UsePerCut);
		Bands[FMath::Clamp(FMath::FloorToInt32((L.Base.Z - Z0) / Band), 0, NumBands - 1)].Add(bLive, Pieces);
		Total.Add(bLive, Pieces);
		if (bLive) LiveZ += L.Base.Z;
		const int32 Bough = AncestorAt(L.Branch, 2);
		if (Bough != INDEX_NONE) Boughs.FindOrAdd(Bough).Add(bLive, Pieces);
		const int32 Limb = AncestorAt(L.Branch, 1);
		if (Limb != INDEX_NONE) Limbs.FindOrAdd(Limb).Add(bLive, Pieces);
	}

	UE_LOG(LogLeafline, Log, TEXT("TreeReport %s: seed %d, %d leaves; per 10 m band: live/all leaves, pieces left at template %.1f"),
		*Label, GetSeed(), N, Rules.Template);
	// One line per band; on a tall tree the bands above the lowest 16 share lines, to keep the report near 30 lines.
	constexpr int32 MaxBandLines = 24, ExactBands = 16;
	auto BandText = [&](int32 k)
	{
		return FString::Printf(TEXT("%.0f-%.0f m: %s"), (Z0 + k * Band) / 100.0, (Z0 + (k + 1) * Band) / 100.0, *Bands[k].Describe());
	};
	const int32 Single = NumBands <= MaxBandLines ? NumBands : ExactBands;
	for (int32 k = 0; k < Single; ++k) UE_LOG(LogLeafline, Log, TEXT("TreeReport %s: band %s"), *Label, *BandText(k));
	if (Single < NumBands)
	{
		const int32 PerLine = FMath::DivideAndRoundUp(NumBands - Single, MaxBandLines - Single);
		for (int32 k = Single; k < NumBands; k += PerLine)
		{
			FString Line;
			for (int32 j = k; j < FMath::Min(k + PerLine, NumBands); ++j)
			{
				if (j > k) Line += TEXT("; ");
				Line += BandText(j);
			}
			UE_LOG(LogLeafline, Log, TEXT("TreeReport %s: bands %s"), *Label, *Line);
		}
	}

	// The lowest leafy boughs (leaves on the bough or its twigs) by the height of the bough's start, then the lowest leafy
	// limbs by the height where they leave the trunk.
	struct FRow
	{
		double Z;
		int32 Branch;
		FCount Count;
	};
	auto LowestRows = [this](const TMap<int32, FCount>& Counts, bool bAtParent)
	{
		TArray<FRow> Rows;
		for (const TPair<int32, FCount>& It : Counts)
		{
			const FTreeBranch& B = Skeleton.Branches[It.Key];
			FVector At = B.Samples.Num() > 0 ? B.Samples[0] : FVector::ZeroVector;
			if (bAtParent && Skeleton.Branches.IsValidIndex(B.Parent) && Skeleton.Branches[B.Parent].Samples.Num() >= 2)
			{
				At = TreeSkeleton::SampleAt(Skeleton.Branches[B.Parent], B.ParentT);
			}
			Rows.Add({ At.Z, It.Key, It.Value });
		}
		Rows.Sort([](const FRow& A, const FRow& B) { return A.Z < B.Z; });
		return Rows;
	};
	const TArray<FRow> BoughRows = LowestRows(Boughs, false);
	for (int32 r = 0; r < FMath::Min(6, BoughRows.Num()); ++r)
	{
		const FRow& Row = BoughRows[r];
		UE_LOG(LogLeafline, Log, TEXT("TreeReport %s: low bough %d (branch %d, on limb %d) starts at %.1f m: %s"), *Label, r + 1,
			Row.Branch, AncestorAt(Row.Branch, 1), Row.Z / 100.0, *Row.Count.Describe());
	}
	const TArray<FRow> LimbRows = LowestRows(Limbs, true);
	for (int32 r = 0; r < FMath::Min(2, LimbRows.Num()); ++r)
	{
		const FRow& Row = LimbRows[r];
		UE_LOG(LogLeafline, Log, TEXT("TreeReport %s: low limb %d (branch %d) at %.1f m: %s"), *Label, r + 1, Row.Branch,
			Row.Z / 100.0, *Row.Count.Describe());
	}
	UE_LOG(LogLeafline, Log, TEXT("TreeReport %s: whole tree %s; mean height of live leaves %.1f m"), *Label,
		*Total.Describe(), Total.Live > 0 ? LiveZ / Total.Live / 100.0 : 0.0);
}

bool ATreeActor::StaysOnBark(const FVector& A, const FVector& B) const
{
	return LeafRoute::OnBark(Field, A, B);
}

bool ATreeActor::BuildLeafApproach(const FVector& From, int32 FromBranch, int32 Leaf, float MaxWalk, TArray<FTrailPoint>& OutRoute) const
{
	const LeafRoute::EResult R = LeafRoute::BuildApproach(Skeleton, Field, Leaves, BranchChildren, From, FromBranch, Leaf, MaxWalk, OutRoute);
	if (ApproachStats.Num() < LeafRoute::NumResults) ApproachStats.SetNumZeroed(LeafRoute::NumResults);
	++ApproachStats[R];
	if (R != LeafRoute::Ok)
	{
		UE_LOG(LogLeafline, Verbose, TEXT("Approach to leaf %d from branch %d failed: %s"), Leaf, FromBranch, LeafRoute::ResultNames[R]);
	}
	return R == LeafRoute::Ok;
}

bool ATreeActor::PatchLeafReachable(int32 CutLeaf, int32 Leaf, float MaxWalk) const
{
	// Reachability depends only on the tree, so it is kept for the tree's life: the HUD asks again as the cut grows.
	const uint64 Key = (uint64(uint32(CutLeaf)) << 32) | uint64(uint32(Leaf));
	if (const float* Known = PatchWalkCache.Find(Key))
	{
		if (*Known >= 0.f) return *Known <= MaxWalk;
		if (MaxWalk <= -*Known) return false;
	}
	const FTreeLeaf& Cut = Leaves.Get(CutLeaf);
	TArray<FTrailPoint> Route;
	const bool bOk = BuildLeafApproach(Cut.Base, Cut.Branch, Leaf, MaxWalk, Route);
	PatchWalkCache.Add(Key, bOk ? (float)LeafRoute::RouteLength(Route) : -FMath::Max(MaxWalk, 1.f));
	return bOk;
}

void ATreeActor::BuildPatch(int32 CutLeaf, const FPatchRules& Rules, TArray<int32>& OutLeaves) const
{
	OutLeaves.Reset();
	if (CutLeaf < 0 || CutLeaf >= Leaves.Num()) return;
	const FTreeLeaf& Cut = Leaves.Get(CutLeaf);
	if (!Skeleton.Branches.IsValidIndex(Cut.Branch)) return;
	OutLeaves.Add(CutLeaf);
	int32 Pieces = Leaves.PiecesLeft(CutLeaf, Rules.Template, Rules.UsePerCut);
	const int32 MaxLeaves = FMath::Max(Rules.MaxLeaves, 1);
	if (OutLeaves.Num() >= MaxLeaves || Pieces >= Rules.MaxPieces) return;

	// Leaves grow on boughs (level 2) and twigs (level 3): the patch is the cut leaf's bough and that bough's twigs.
	const FTreeBranch& CutBranch = Skeleton.Branches[Cut.Branch];
	int32 Bough = Cut.Branch;
	if (CutBranch.Level >= 3 && Skeleton.Branches.IsValidIndex(CutBranch.Parent)) Bough = CutBranch.Parent;
	TArray<int32, TInlineAllocator<16>> Group;
	Group.Add(Bough);
	if (BranchChildren.IsValidIndex(Bough))
	{
		for (int32 c : BranchChildren[Bough])
		{
			if (Skeleton.Branches[c].Level > Skeleton.Branches[Bough].Level) Group.Add(c);
		}
	}

	// Candidates by the cheap estimate (along the branch graph), the cut leaf's own branch first; approaches are built
	// lazily in that order, since each costs a few thousand field evaluations.
	struct FCand
	{
		int32 Leaf;
		bool bOtherBranch;
		double Walk;
	};
	TArray<FCand, TInlineAllocator<64>> Cands;
	for (int32 b : Group)
	{
		const TArray<int32>* List = Leaves.LeavesOnBranch(b);
		if (!List) continue;
		for (int32 i : *List)
		{
			if (i == CutLeaf || Leaves.PiecesLeft(i, Rules.Template, Rules.UsePerCut) <= 0) continue;
			double Len = 0.0, Bark = 0.0;
			if (!LeafRoute::Walk(Skeleton, Cut.Branch, Cut.BranchS, b, Leaves.Get(i).BranchS, nullptr, Len, Bark)) continue;
			if (Bark > Rules.MaxWalk) continue;
			Cands.Add({ i, b != Cut.Branch, Bark });
		}
	}
	Cands.Sort([](const FCand& A, const FCand& B) { return A.bOtherBranch != B.bOtherBranch ? !A.bOtherBranch : A.Walk < B.Walk; });

	for (const FCand& C : Cands)
	{
		if (OutLeaves.Num() >= MaxLeaves || Pieces >= Rules.MaxPieces) break;
		const int32 P = Leaves.PiecesLeft(C.Leaf, Rules.Template, Rules.UsePerCut);
		if (Pieces + P > Rules.MaxPieces) continue; // a smaller leaf further on may still fit
		if (!PatchLeafReachable(CutLeaf, C.Leaf, Rules.MaxWalk)) continue;
		OutLeaves.Add(C.Leaf);
		Pieces += P;
	}
}

void ATreeActor::LogPatchSurvey(int32 Samples) const
{
	if (Leaves.Num() == 0) return;
	const double StartTime = FPlatformTime::Seconds();
	PatchWalkCache.Reset();
	if (ApproachStats.Num() < LeafRoute::NumResults) ApproachStats.SetNumZeroed(LeafRoute::NumResults);
	const TArray<int32> Before = ApproachStats;

	const float Templates[3] = { 0.3f, 0.6f, 1.0f };
	TArray<int32> LeafCount[3], PieceCount[3];
	FPatchRules Rules;
	TArray<int32> Patch;
	const int32 Want = FMath::Clamp(Samples, 1, Leaves.Num());
	const int32 Stride = FMath::Max(1, Leaves.Num() / Want);
	int32 Taken = 0;
	for (int32 i = Stride / 2; i < Leaves.Num() && Taken < Want; i += Stride)
	{
		if (!Leaves.Get(i).CanCut()) continue;
		++Taken;
		for (int32 t = 0; t < 3; ++t)
		{
			Rules.Template = Templates[t];
			BuildPatch(i, Rules, Patch);
			int32 P = 0;
			for (int32 l : Patch) P += Leaves.PiecesLeft(l, Rules.Template, Rules.UsePerCut);
			LeafCount[t].Add(Patch.Num());
			PieceCount[t].Add(P);
		}
	}
	auto Summary = [](TArray<int32>& V)
	{
		if (V.Num() == 0) return FString(TEXT("none"));
		V.Sort();
		return FString::Printf(TEXT("median %d (%d..%d)"), V[V.Num() / 2], V[0], V.Last());
	};
	for (int32 t = 0; t < 3; ++t)
	{
		UE_LOG(LogLeafline, Log, TEXT("Patch survey, template %.1f: %d patches, leaves %s, pieces %s (scout's leaf included, before its cut)"),
			Templates[t], LeafCount[t].Num(), *Summary(LeafCount[t]), *Summary(PieceCount[t]));
	}
	int32 Built = 0;
	FString Failures;
	for (int32 r = 0; r < LeafRoute::NumResults; ++r)
	{
		const int32 N = ApproachStats[r] - Before[r];
		Built += N;
		if (r != LeafRoute::Ok && N > 0) Failures += FString::Printf(TEXT(", %s %d"), LeafRoute::ResultNames[r], N);
	}
	const int32 Failed = Built - (ApproachStats[LeafRoute::Ok] - Before[LeafRoute::Ok]);
	UE_LOG(LogLeafline, Log, TEXT("Patch survey: %d candidate approaches built, %d failed%s; %.0f ms (seed %d, max walk %.0f cm)"),
		Built, Failed, *Failures, (FPlatformTime::Seconds() - StartTime) * 1000.0, GetSeed(), Rules.MaxWalk);
}

FString ATreeActor::DescribeApproachStats() const
{
	FString Out;
	for (int32 r = 0; r < LeafRoute::NumResults && r < ApproachStats.Num(); ++r)
	{
		if (ApproachStats[r] > 0) Out += FString::Printf(TEXT("%s%s %d"), Out.IsEmpty() ? TEXT("") : TEXT(", "), LeafRoute::ResultNames[r], ApproachStats[r]);
	}
	return Out.IsEmpty() ? FString(TEXT("none built")) : Out;
}

void ATreeActor::BuildGrove()
{
	// Plain trunks with a few bare limbs in a loose ring far out, drawn with the bark material and left to the haze.
	// Built like the real tree (control nodes, samples, the same tube builder) but coarser, and never walked on.
	FTreeSkeleton Grove;
	Grove.Params = Params;
	Grove.Params.AroundSpacing = 120.f;
	Grove.Params.MaxAroundVerts = 40;
	FRandomStream R(Skeleton.Seed * 7919 + 17);
	const FVector C(Skeleton.TrunkBase.X, Skeleton.TrunkBase.Y, Field.GroundZ);
	const int32 Count = FMath::Max(0, Params.GroveCount);
	struct FClump { FVector Center; double Radius; };
	TArray<FClump> Clumps;
	for (int32 k = 0; k < Count; ++k)
	{
		const float Az = 2.f * PI * k / FMath::Max(Count, 1) + R.FRandRange(-0.18f, 0.18f);
		const float Dist = R.FRandRange(Params.GroveMinDistance, Params.GroveMaxDistance);
		FVector Base = C + FVector(FMath::Cos(Az), FMath::Sin(Az), 0.f) * Dist;
		const float R0 = R.FRandRange(500.f, 1100.f);
		// On the visible ground's relief, sunk by the drop across the flared foot so its downhill side does not float.
		Base.Z += LeafRelief::Height(Base.X, Base.Y) - LeafRelief::Slope(Base.X, Base.Y) * R0 * 1.6f * 1.2f;
		const float H = R.FRandRange(22000.f, 38000.f);
		const FVector Lean = FVector(R.FRandRange(-1.f, 1.f), R.FRandRange(-1.f, 1.f), 0.f).GetSafeNormal() * R.FRandRange(0.01f, 0.04f);
		auto RadiusAt = [&](float Z)
		{
			const float Zc = FMath::Max(Z, 0.f);
			return (40.f + (R0 - 40.f) * FMath::Pow(FMath::Max(1.f - Zc / H, 0.f), 0.85f)) * (1.f + 0.6f * FMath::Exp(-Zc / 600.f));
		};
		FTreeBranch T;
		T.Level = 0;
		T.Random = R.FRand();
		constexpr int32 N = 16;
		for (int32 i = 0; i <= N; ++i)
		{
			const float Z = -300.f + (H + 300.f) * FMath::Pow(float(i) / N, 1.4f);
			T.Nodes.Add({ Base + Lean * FMath::Max(Z, 0.f) + FVector(0.f, 0.f, Z), RadiusAt(Z) });
		}
		TreeSkeleton::BuildSamples(T, Grove.Params);
		Grove.Branches.Add(MoveTemp(T));

		// Crown: a clump over the top and others spread down the upper part of the trunk, like the main tree, whose
		// leaves start low (at 40 m of 300); from the canopy the grove then shows leaves at eye level, not bare poles.
		if (Params.GroveCrownClumps > 0)
		{
			Clumps.Add({ Base + Lean * H + FVector(0.f, 0.f, H * 0.95f), H * R.FRandRange(0.11f, 0.14f) });
			for (int32 k2 = 1; k2 < Params.GroveCrownClumps; ++k2)
			{
				const float Z2 = H * FMath::Lerp(0.38f, 0.92f, (k2 - 0.5f + R.FRandRange(-0.4f, 0.4f)) / (Params.GroveCrownClumps - 1));
				const float A2 = R.FRandRange(0.f, 2.f * PI);
				const float Out = H * R.FRandRange(0.05f, 0.14f);
				Clumps.Add({ Base + Lean * Z2 + FVector(FMath::Cos(A2) * Out, FMath::Sin(A2) * Out, Z2), H * R.FRandRange(0.06f, 0.10f) });
			}
		}

		const int32 Limbs = R.RandRange(3, 5);
		for (int32 l = 0; l < Limbs; ++l)
		{
			const float Z0 = H * R.FRandRange(0.35f, 0.85f);
			const float A = R.FRandRange(0.f, 2.f * PI);
			FVector Dir = (FVector(FMath::Cos(A), FMath::Sin(A), 0.f) + FVector(0.f, 0.f, R.FRandRange(0.3f, 0.9f))).GetSafeNormal();
			const float Len = R.FRandRange(3000.f, 9000.f);
			const float LR = RadiusAt(Z0) * 0.35f;
			FTreeBranch B;
			B.Level = 1;
			B.Random = R.FRand();
			FVector P = Base + Lean * Z0 + FVector(0.f, 0.f, Z0);
			constexpr int32 M = 6;
			for (int32 i = 0; i <= M; ++i)
			{
				B.Nodes.Add({ P, FMath::Lerp(LR, 30.f, float(i) / M) });
				Dir = (Dir + FVector(0.f, 0.f, 0.06f)).GetSafeNormal();
				P += Dir * (Len / M);
			}
			TreeSkeleton::BuildSamples(B, Grove.Params);
			Grove.Branches.Add(MoveTemp(B));
			if (Params.GroveCrownClumps > 0) Clumps.Add({ P, H * R.FRandRange(0.05f, 0.08f) });
		}
	}

	// Leaves on the surface of each clump, facing out and drooping a little; far enough away that they are scenery.
	GroveLeaves.Reset();
	for (const FClump& Cl : Clumps)
	{
		for (int32 i = 0; i < Params.GroveLeavesPerClump; ++i)
		{
			FVector Out = R.VRand();
			Out.Z = Out.Z * 0.7f + 0.2f; // flatter underneath, fuller on top
			Out.Normalize();
			FTreeLeaf L;
			L.Length = Cl.Radius * R.FRandRange(0.2f, 0.3f); // a few metres: big enough to read at a distance, not giant
			L.Width = L.Length * Params.LeafWidthRatio;
			const FVector Tan = FVector::CrossProduct(Out, R.VRand()).GetSafeNormal();
			L.Dir = (Tan + Out * 0.35f + FVector(0.f, 0.f, -0.3f)).GetSafeNormal();
			L.Side = FVector::CrossProduct(L.Dir, Out).GetSafeNormal();
			if (L.Side.IsNearlyZero()) continue;
			L.Normal = FVector::CrossProduct(L.Side, L.Dir).GetSafeNormal(); // points outward
			L.Base = Cl.Center + Out * Cl.Radius * R.FRandRange(0.7f, 1.f) - L.Dir * L.Length * 0.3f;
			L.Origin = L.Base;
			L.Chunk = 0;
			GroveLeaves.AddDecorLeaf(L);
		}
	}

	FTreeMeshData Data;
	TreeMeshBuilder::Build(Grove, Data);
	if (!GroveMesh)
	{
		GroveMesh = MakeMeshComponent(this, RootComponent, TEXT("GroveMesh"));
		GroveMesh->SetCastShadow(false);
	}
	GroveMesh->ClearAllMeshSections();
	if (Data.Vertices.Num() == 0) return;
	GroveMesh->CreateMeshSection_LinearColor(0, Data.Vertices, Data.Triangles, Data.Normals, Data.UV0, Data.UV1, Data.UV2, Data.UV3,
		Data.Colors, Data.Tangents, false, false);
	if (UMaterialInterface* Mat = WoodMesh ? WoodMesh->GetMaterial(0) : nullptr) GroveMesh->SetMaterial(0, Mat);
	UE_LOG(LogLeafline, Log, TEXT("Grove: %d distant trunks, %d triangles"), Count, Data.Triangles.Num() / 3);
}

void ATreeActor::BuildGround(const FTreeMeshData* Skirt)
{
	// A polar disk at the field's ground height, rings spaced wider with distance. Wood mesh section 1. Beyond the ants'
	// reach the visible ground rises and falls (LeafRelief, Leaf.Relief); the rings there are 4% apart, not 12%, so the
	// swells have enough of them, and normals and tangents come from the height function, not the plane.
	const FVector C(Skeleton.TrunkBase.X, Skeleton.TrunkBase.Y, Field.GroundZ);
	LeafRelief::SetFrame(FVector2D(C.X, C.Y), Skeleton.Seed);
	const int32 Around = 160;
	TArray<double> Radii;
	for (double R = 200.0; R < Params.GroundRadius; R *= (R < 8000.0 ? 1.12 : 1.04)) Radii.Add(R);
	TArray<FVector> V, N;
	TArray<FVector2D> UV;
	TArray<FLinearColor> Col;
	TArray<FProcMeshTangent> Tan;
	TArray<int32> Tri;
	auto AddVert = [&](const FVector& Flat)
	{
		const float H = LeafRelief::Height(Flat.X, Flat.Y);
		const FVector Up = H != 0.f ? LeafRelief::Normal(Flat.X, Flat.Y) : FVector::UpVector;
		V.Add(Flat + FVector(0.0, 0.0, H));
		N.Add(Up);
		UV.Add(FVector2D(Flat.X, Flat.Y));
		Col.Add(FLinearColor::Black);
		Tan.Add(FProcMeshTangent((FVector::ForwardVector - Up * Up.X).GetSafeNormal(), false));
	};
	AddVert(C);
	for (double R : Radii)
	{
		for (int32 j = 0; j < Around; ++j)
		{
			const double A = 2.0 * PI * j / Around;
			AddVert(C + FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, 0.0));
		}
	}
	// UE front faces: (b - a) x (c - a) points away from the viewer, so for an upward face it points down.
	auto AddTri = [&](int32 A, int32 B, int32 Cc)
	{
		if (FVector::CrossProduct(V[B] - V[A], V[Cc] - V[A]).Z > 0.0) Tri.Append({ A, Cc, B });
		else Tri.Append({ A, B, Cc });
	};
	for (int32 j = 0; j < Around; ++j) AddTri(0, 1 + j, 1 + (j + 1) % Around);
	for (int32 k = 0; k + 1 < Radii.Num(); ++k)
	{
		const int32 R0 = 1 + k * Around, R1 = 1 + (k + 1) * Around;
		for (int32 j = 0; j < Around; ++j)
		{
			const int32 J1 = (j + 1) % Around;
			AddTri(R0 + j, R0 + J1, R1 + j);
			AddTri(R0 + J1, R1 + J1, R1 + j);
		}
	}
	if (Skirt && Skirt->Vertices.Num() > 0)
	{
		// The ground half of the trunk and roots' ground skirt: the ground's own vertex data, already on its surface.
		const int32 Base = V.Num();
		V.Append(Skirt->Vertices);
		N.Append(Skirt->Normals);
		UV.Append(Skirt->UV0);
		Col.Append(Skirt->Colors);
		Tan.Append(Skirt->Tangents);
		for (int32 Idx : Skirt->Triangles) Tri.Add(Base + Idx);
	}
	WoodMesh->CreateMeshSection_LinearColor(1, V, Tri, N, UV, {}, {}, {}, Col, Tan, false, false);
	if (UMaterialInterface* Ground = PickGround())
	{
		WoodMesh->SetMaterial(1, Ground);
	}
	else if (FallbackMaterial)
	{
		if (!GroundMID) GroundMID = UMaterialInstanceDynamic::Create(FallbackMaterial, this);
		GroundMID->SetVectorParameterValue(TEXT("Color"), GroundColor);
		WoodMesh->SetMaterial(1, GroundMID);
	}
}

FTransform ATreeActor::GetNestTransform(int32 Index) const
{
	if (Skeleton.NestSpots.Num() == 0)
	{
		return FTransform(FRotator::ZeroRotator, FVector(1500.0, 0.0, 0.0));
	}
	const FVector Spot = Skeleton.NestSpots[Index % Skeleton.NestSpots.Num()];
	// Face the trunk: the first thing a worker does is climb.
	FVector ToTrunk = Skeleton.TrunkBase - Spot;
	ToTrunk.Z = 0.0;
	return FTransform(ToTrunk.GetSafeNormal().Rotation(), Spot);
}

ATreeActor* ATreeActor::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World) return nullptr;
	for (TActorIterator<ATreeActor> It(const_cast<UWorld*>(World)); It; ++It)
	{
		return *It;
	}
	return nullptr;
}
