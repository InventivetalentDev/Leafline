// Console commands for one tree per colony stage (docs/briefs/one-tree.md).
#include "AntTreeGame.h"
#include "Engine/World.h"
#include "Game/LeaflineGameInstance.h"
#include "HAL/IConsoleManager.h"
#include "Colony/TrailNetwork.h"
#include "EngineUtils.h"
#include "Tree/TreeActor.h"
#include "Tree/TreeMeshBuilder.h"
#include "World/DayDirector.h"
#include "World/GroundRelief.h"

namespace
{
	void SetOneTree(UWorld* World, const TArray<FString>& Args)
	{
		ADayDirector* D = ADayDirector::Get(World);
		if (!D) return;
		D->SetOneTree(Args.Num() > 0 ? FCString::Atoi(*Args[0]) != 0 : !D->IsOneTree());
	}

	void MoveNow(UWorld* World)
	{
		if (ADayDirector* D = ADayDirector::Get(World)) D->DebugMoveNow();
	}

	void Strip(UWorld* World, const TArray<FString>& Args)
	{
		ATreeActor* Tree = ATreeActor::Get(World);
		if (!Tree || Args.Num() < 1) return;
		const float Metres = FCString::Atof(*Args[0]);
		const int32 N = Tree->StripBelow(Metres * 100.f);
		UE_LOG(LogLeafline, Log, TEXT("Leaf.Strip %.1f: %d leaves used up, %d pieces left on the tree"), Metres, N, Tree->CountPiecesLeft());
	}

	void Report(UWorld* World, const TArray<FString>& Args)
	{
		ATreeActor* Tree = ATreeActor::Get(World);
		const ADayDirector* D = ADayDirector::Get(World);
		if (!Tree) return;
		FString Label = Args.Num() > 0 ? FString::Join(Args, TEXT(" ")) : FString();
		if (Label.IsEmpty() && D) Label = FString::Printf(TEXT("day %d tree %d day %d"), D->GetDay(), D->GetStage() + 1, D->GetDayOnTree());
		Tree->LogTreeReport(Label);
	}

	void ReloadColony(UWorld* World)
	{
		ADayDirector* D = ADayDirector::Get(World);
		ULeaflineGameInstance* GI = World ? Cast<ULeaflineGameInstance>(World->GetGameInstance()) : nullptr;
		if (!D || !GI) return;
		GI->ReloadFromDisk();
		D->DebugForceRegrow();
		D->StartDay(GI->GetDay());
	}

	/** Leaf.Stage <field> <value>: change today's stage tuning and grow its tree again, fresh (tuning sweeps). */
	void StageTune(UWorld* World, const TArray<FString>& Args)
	{
		ADayDirector* D = ADayDirector::Get(World);
		ULeaflineGameInstance* GI = World ? Cast<ULeaflineGameInstance>(World->GetGameInstance()) : nullptr;
		if (!D || !GI || Args.Num() < 2 || D->Tuning.Stages.Num() == 0) return;
		FStageTuning& S = D->Tuning.Stages[FMath::Clamp(D->GetStage(), 0, D->Tuning.Stages.Num() - 1)];
		const FString F = Args[0].ToLower();
		const float V = FCString::Atof(*Args[1]);
		if (F == TEXT("sparse")) S.SparseSpacingMul = V;
		else if (F == TEXT("blend")) S.SparseBlendHeight = V * 100.f;
		else if (F == TEXT("drop")) S.SparseTwigDrop = V;
		else if (F == TEXT("leafy")) S.LowestLeafyHeight = V * 100.f;
		else if (F == TEXT("height")) S.TreeHeightMul = V;
		else if (F == TEXT("rich")) S.RichnessMul = V;
		else if (F == TEXT("gust")) S.GustMul = V;
		else
		{
			UE_LOG(LogLeafline, Warning, TEXT("Leaf.Stage: fields are sparse, blend (m), drop, leafy (m), height, rich, gust"));
			return;
		}
		UE_LOG(LogLeafline, Log, TEXT("Leaf.Stage: tree %d: sparse x%.2f over %.0f m, twig drop %.2f, leafy from %.0f m, height x%.2f, rich x%.2f, gust x%.2f"),
			D->GetStage() + 1, S.SparseSpacingMul, S.SparseBlendHeight / 100.f, S.SparseTwigDrop, S.LowestLeafyHeight / 100.f,
			S.TreeHeightMul, S.RichnessMul, S.GustMul);
		// A fresh tree with the new tuning: same stage and seed, saved bites dropped.
		GI->SetTree(D->GetStage(), GI->GetTreeSeed(), GI->GetStageStartDay());
		GI->SetTreeState({}, 0, {}, 0);
		D->DebugForceRegrow();
		D->StartDay(D->GetDay());
	}

	/** Leaf.Smoke [0|1]: the trails' smoke look or the plain ribbon (toggle with no argument). */
	void Smoke(UWorld* World, const TArray<FString>& Args)
	{
		if (!World) return;
		for (TActorIterator<ATrailNetwork> It(World); It; ++It)
		{
			It->SetSmoke(Args.Num() > 0 ? FCString::Atoi(*Args[0]) != 0 : !It->IsSmoke());
		}
	}

	FAutoConsoleCommandWithWorldAndArgs CmdSmoke(
		TEXT("Leaf.Smoke"),
		TEXT("Leaf.Smoke [0|1]: scent trails as a thin haze with an upright ribbon (1, default) or the plain flat ribbon (0); no argument toggles."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World) { Smoke(World, Args); }));

	/** Leaf.Bark [0|1]: the look-pass bark (M_TreeBark2) or the older M_TreeBark (toggle with no argument). */
	void Bark(UWorld* World, const TArray<FString>& Args)
	{
		ATreeActor* Tree = ATreeActor::Get(World);
		const bool bOn = Args.Num() > 0 ? FCString::Atoi(*Args[0]) != 0 : !ATreeActor::IsNewBark();
		if (Tree) Tree->SetNewBark(bOn);
	}

	FAutoConsoleCommandWithWorldAndArgs CmdBark(
		TEXT("Leaf.Bark"),
		TEXT("Leaf.Bark [0|1]: the look-pass bark M_TreeBark2 (1, default) or the older M_TreeBark (0); no argument toggles."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World) { Bark(World, Args); }));

	/** Leaf.Ground [0|1] [Name Value ...]: the floor-pass ground M_Ground2 or the older M_Ground, plus scalar parameters on
	 *  M_Ground2's instance (e.g. Leaf.Ground 1 FarStart 2500). */
	void GroundLook(UWorld* World, const TArray<FString>& Args)
	{
		ATreeActor* Tree = ATreeActor::Get(World);
		if (!Tree) return;
		const bool bOn = Args.Num() > 0 ? FCString::Atoi(*Args[0]) != 0 : !ATreeActor::IsGroundLook2();
		Tree->SetGroundLook(bOn);
		for (int32 i = 1; i + 1 < Args.Num(); i += 2)
		{
			const float V = FCString::Atof(*Args[i + 1]);
			const bool bSet = Tree->SetGroundParam(FName(*Args[i]), V);
			UE_LOG(LogLeafline, Log, TEXT("Leaf.Ground: %s = %.3f%s"), *Args[i], V, bSet ? TEXT("") : TEXT(" (no M_Ground2 instance, not set)"));
		}
	}

	FAutoConsoleCommandWithWorldAndArgs CmdGround(
		TEXT("Leaf.Ground"),
		TEXT("Leaf.Ground [0|1] [Name Value ...]: the floor-pass ground M_Ground2 (1, default; far look beyond about 20 m) or the older M_Ground (0); no argument toggles. Name Value pairs set scalar parameters on M_Ground2's instance."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World) { GroundLook(World, Args); }));

	/** Leaf.Relief [0|1] [start m] [near] [far] [rim m] [ground km]: the visible ground's relief beyond the ants' reach on or
	 *  off, optionally where it starts, factors on the heights of its near (80, 240 m) and far (900 m, 3 km) bands (one factor
	 *  sets both), a rim rising toward 5 km and how far the ground reaches while it is on; rebuilds the ground, the grove and (by the relief's stamp) the forest floor. */
	void Relief(UWorld* World, const TArray<FString>& Args)
	{
		const bool bOn = Args.Num() > 0 ? FCString::Atoi(*Args[0]) != 0 : !LeafRelief::IsOn();
		const float Start = Args.Num() > 1 ? FCString::Atof(*Args[1]) * 100.f : LeafRelief::GetStartRadius();
		const float Near = Args.Num() > 2 ? FCString::Atof(*Args[2]) : LeafRelief::GetNearScale();
		const float Far = Args.Num() > 3 ? FCString::Atof(*Args[3]) : (Args.Num() > 2 ? Near : LeafRelief::GetFarScale());
		const float Rim = Args.Num() > 4 ? FCString::Atof(*Args[4]) * 100.f : LeafRelief::GetRimHeight();
		const float Outer = Args.Num() > 5 ? FCString::Atof(*Args[5]) * 100000.f : LeafRelief::GetOuterRadius();
		LeafRelief::SetOn(bOn);
		LeafRelief::SetShape(Start, Near, Far, Rim, Outer);
		ATreeActor* Tree = ATreeActor::Get(World);
		if (Tree) Tree->RebuildGroundAndGrove();
		const FVector Base = Tree ? Tree->GetSkeleton().TrunkBase : FVector::ZeroVector;
		UE_LOG(LogLeafline, Log, TEXT("Leaf.Relief: %s, from %.0f m (asked %.0f m) to full at %.0f m round the trunk axis (%.0f %.0f), near bands x%.2f, far bands x%.2f, rim %.0f m, ground to %.1f km"),
			bOn ? TEXT("on") : TEXT("off"), LeafRelief::GetEffectiveStart() / 100.f, LeafRelief::GetStartRadius() / 100.f, (LeafRelief::GetEffectiveStart() + LeafRelief::RampLength) / 100.f,
			Base.X, Base.Y, LeafRelief::GetNearScale(), LeafRelief::GetFarScale(), LeafRelief::GetRimHeight() / 100.f,
			(LeafRelief::IsOn() && LeafRelief::GetOuterRadius() > 0.f ? LeafRelief::GetOuterRadius() : (Tree ? Tree->Params.GroundRadius : 0.f)) / 100000.f);
	}

	FAutoConsoleCommandWithWorldAndArgs CmdRelief(
		TEXT("Leaf.Relief"),
		TEXT("Leaf.Relief [0|1] [start m] [near] [far]: the visible ground rises and falls beyond the ants' reach (1) or stays a plane (0); no argument toggles. Optionally where it starts (default 90 m from the trunk axis) and factors on the heights of its near bands (80 and 240 m wide) and far bands (900 m and 3 km); one factor sets both; a rim height (m at 5 km) and how far the ground reaches (km; 0: the usual 5 km). The walking field is a plane either way."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World) { Relief(World, Args); }));

	/** Leaf.Collars [0|1]: branch collars on or off (toggle with no argument), then today's tree grows again, fresh. */
	void Collars(UWorld* World, const TArray<FString>& Args)
	{
		ADayDirector* D = ADayDirector::Get(World);
		ATreeActor* Tree = ATreeActor::Get(World);
		ULeaflineGameInstance* GI = World ? Cast<ULeaflineGameInstance>(World->GetGameInstance()) : nullptr;
		if (!D || !Tree || !GI) return;
		const bool bOn = Args.Num() > 0 ? FCString::Atoi(*Args[0]) != 0 : !ATreeActor::AreBranchCollarsOn(Tree->Params);
		ATreeActor::SetBranchCollarsOverride(bOn);
		Tree->Params.bBranchCollars = bOn;
		UE_LOG(LogLeafline, Log, TEXT("Leaf.Collars: branch collars %s; growing tree %d again, fresh"), bOn ? TEXT("on") : TEXT("off"), D->GetStage() + 1);
		// Same as Leaf.Stage: the same stage and seed, saved bites dropped.
		GI->SetTree(D->GetStage(), GI->GetTreeSeed(), GI->GetStageStartDay());
		GI->SetTreeState({}, 0, {}, 0);
		D->DebugForceRegrow();
		D->StartDay(D->GetDay());
	}

	/** Leaf.RootBark [0|1|taper]: the roots' bark ridges held long (1, FTreeParams::RootBarkTaper) or following the root's own
	 *  taper as before (0); a value with a point sets the taper (so 1.0 is the old look). Bark only, so today's tree grows again with
	 *  the bites saved at the last day end (StartDay's GrowWithBites); the day restarts from dawn. */
	void RootBark(UWorld* World, const TArray<FString>& Args)
	{
		ADayDirector* D = ADayDirector::Get(World);
		ATreeActor* Tree = ATreeActor::Get(World);
		if (!D || !Tree) return;
		const bool bOld = ATreeActor::GetRootBarkTaper(Tree->Params) >= 1.f;
		float Override = bOld ? -1.f : 1.f; // no argument toggles
		if (Args.Num() > 0)
		{
			const float V = FCString::Atof(*Args[0]);
			Override = Args[0].Contains(TEXT(".")) ? V : (V != 0.f ? -1.f : 1.f);
		}
		ATreeActor::SetRootBarkTaperOverride(Override);
		UE_LOG(LogLeafline, Log, TEXT("Leaf.RootBark: roots' bark taper %.2f (%s); growing today's tree again"),
			ATreeActor::GetRootBarkTaper(Tree->Params), Override < 0.f ? TEXT("params") : TEXT("override"));
		// No vertex moves, so no SetTree / SetTreeState as in Leaf.Collars: StartDay regrows the same tree with the bites saved at
		// the last day end (GrowWithBites); the day itself restarts from dawn, as any StartDay does.
		D->DebugForceRegrow();
		D->StartDay(D->GetDay());
	}

	/** Leaf.Fillets [0|1] [frac] [max] [boughmul] [groundk]: junction fillets on or off (toggle with no argument), optionally their
	 *  size, then today's tree grows again, fresh. */
	void Fillets(UWorld* World, const TArray<FString>& Args)
	{
		ADayDirector* D = ADayDirector::Get(World);
		ATreeActor* Tree = ATreeActor::Get(World);
		ULeaflineGameInstance* GI = World ? Cast<ULeaflineGameInstance>(World->GetGameInstance()) : nullptr;
		if (!D || !Tree || !GI) return;
		const bool bOn = Args.Num() > 0 ? FCString::Atoi(*Args[0]) != 0 : !Tree->GetVisualField().HasFillets();
		auto Arg = [&](int32 i) { return Args.Num() > i ? FCString::Atof(*Args[i]) : -1.f; };
		ATreeActor::SetFilletsOverride(bOn, Arg(1), Arg(2), Arg(3), Arg(4), Arg(5));
		const FTreeParams P = ATreeActor::WithFilletOverrides(Tree->Params);
		UE_LOG(LogLeafline, Log, TEXT("Leaf.Fillets: fillets %s (frac %.2f, bough x%.2f, max %.0f cm, ground %.0f cm, twigs from %.1f cm); growing tree %d again, fresh"),
			bOn ? TEXT("on") : TEXT("off"), P.FilletFrac, P.FilletBoughMul, P.FilletMax, P.FilletGroundK, P.FilletTwigMinRadius, D->GetStage() + 1);
		GI->SetTree(D->GetStage(), GI->GetTreeSeed(), GI->GetStageStartDay());
		GI->SetTreeState({}, 0, {}, 0);
		D->DebugForceRegrow();
		D->StartDay(D->GetDay());
	}

	/** Leaf.FilletWalk [0|1]: the sim walks on the fillets (1) or they are looks only (0); toggles without an argument. Today's
	 *  tree grows again, fresh. */
	void FilletWalk(UWorld* World, const TArray<FString>& Args)
	{
		ADayDirector* D = ADayDirector::Get(World);
		ATreeActor* Tree = ATreeActor::Get(World);
		ULeaflineGameInstance* GI = World ? Cast<ULeaflineGameInstance>(World->GetGameInstance()) : nullptr;
		if (!D || !Tree || !GI) return;
		const bool bWalk = Args.Num() > 0 ? FCString::Atoi(*Args[0]) != 0 : !Tree->GetField().HasFillets();
		ATreeActor::SetFilletWalkOverride(bWalk ? 1 : 0);
		UE_LOG(LogLeafline, Log, TEXT("Leaf.FilletWalk: the ants walk %s the fillets; growing tree %d again, fresh"),
			bWalk ? TEXT("on") : TEXT("under (looks only)"), D->GetStage() + 1);
		GI->SetTree(D->GetStage(), GI->GetTreeSeed(), GI->GetStageStartDay());
		GI->SetTreeState({}, 0, {}, 0);
		D->DebugForceRegrow();
		D->StartDay(D->GetDay());
	}

	FAutoConsoleCommandWithWorldAndArgs CmdFilletWalk(
		TEXT("Leaf.FilletWalk"),
		TEXT("Leaf.FilletWalk [0|1]: with fillets on, the sim walks on them (1, default) or they are looks only and the ants are drawn lifted onto the skirts (0); no argument toggles. Grows today's tree again, fresh."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World) { FilletWalk(World, Args); }));

	FAutoConsoleCommandWithWorldAndArgs CmdFillets(
		TEXT("Leaf.Fillets"),
		TEXT("Leaf.Fillets [0|1] [frac] [max] [boughmul] [groundk] [twigmin]: skirts that round the junctions of limbs, boughs, twigs and roots (and of the trunk and roots with the ground) in the bark mesh (1, default) or not (0); the ants walk on them unless Leaf.FilletWalk 0; no argument toggles; optional fillet radius as a fraction of the child's radius, its cap in cm, the bough multiplier, the ground blend in cm (0 keeps the plain one), the smallest twig base radius with a fillet in cm (1000: no twig fillets). Grows today's tree again, fresh."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World) { Fillets(World, Args); }));

	/**
	 * Leaf.FilletCheck: rebuilds the bark mesh with skirts (logs their fit), then compares the walking field with the field
	 * without fillets at every tube vertex inside a fillet zone. A vertex lifted more than 2 cm more by the fillets is
	 * covered when it lies on the junction's own child or parent (the skirt is over it); otherwise the fillet has webbed
	 * an unrelated branch. Also counts leaves the skip test would have dropped against the field with fillets (leaves are
	 * placed against the one without).
	 */
	void FilletCheck(UWorld* World, const TArray<FString>& Args)
	{
		ATreeActor* Tree = ATreeActor::Get(World);
		if (!Tree) return;
		const FTreeField& Field = Tree->GetVisualField();
		const FTreeSkeleton& S = Tree->GetSkeleton();
		if (Args.Num() > 0)
		{
			// One junction: its fillet, and a Leaf.Goto that stands on top of the child just outside the fillet zone (so
			// the ant stands in the same place with fillets on and off) looking back at the junction.
			const int32 b = FCString::Atoi(*Args[0]);
			if (!S.Branches.IsValidIndex(b) || S.Branches[b].Samples.Num() < 3) return;
			const FTreeBranch& B = S.Branches[b];
			const float Extra = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 0.f; // cm beyond the zone
			const float Pitch = Args.Num() > 2 ? FCString::Atof(*Args[2]) : -20.f;
			// Arc length of the zone's centre along the child, then FilletOuter (or 3 blend radii) beyond it.
			int32 c0 = 0;
			double BestD = TNumericLimits<double>::Max();
			for (int32 i = 0; i < B.Samples.Num(); ++i)
			{
				const double D = FVector::DistSquared(B.Samples[i], B.FilletCenter);
				if (D < BestD) { BestD = D; c0 = i; }
			}
			const float Beyond = (B.FilletK > 0.f ? B.FilletOuter : B.SampleRadius[0] + 3.f * B.BlendK) + Extra;
			const float Target = B.SampleDist[c0] + Beyond;
			int32 i = 1;
			while (i + 1 < B.Samples.Num() && B.SampleDist[i] < Target) ++i;
			const FVector T = (B.Samples[i] - B.Samples[i - 1]).GetSafeNormal();
			FVector Up = FVector::UpVector - T * T.Z;
			if (Up.SizeSquared() < 1e-4) Up = FVector::ForwardVector;
			Up.Normalize();
			FVector P = B.Samples[i] + Up * (B.SampleRadius[i] + 30.f);
			FVector Look = -T;
			if (Args.Num() > 3 && FCString::Atoi(*Args[3]) != 0 && S.Branches.IsValidIndex(B.Parent))
			{
				// From the parent instead: on top of it, past the junction on the side the child leans to, looking back at
				// the crotch (the acute side, where the fillet is longest).
				const FTreeBranch& Par = S.Branches[B.Parent];
				int32 q = 0;
				double BestQ = TNumericLimits<double>::Max();
				for (int32 k = 0; k < Par.Samples.Num(); ++k)
				{
					const double D = FVector::DistSquared(Par.Samples[k], B.FilletCenter);
					if (D < BestQ) { BestQ = D; q = k; }
				}
				const FVector Tp = (Par.Samples[FMath::Min(q + 1, Par.Samples.Num() - 1)] - Par.Samples[FMath::Max(q - 1, 0)]).GetSafeNormal();
				const float Sign = FVector::DotProduct(T, Tp) >= 0.f ? 1.f : -1.f;
				const float Want = Par.SampleDist[q] + Sign * Beyond;
				int32 k = q;
				while (Sign > 0.f ? (k + 1 < Par.Samples.Num() && Par.SampleDist[k] < Want) : (k > 0 && Par.SampleDist[k] > Want)) k += Sign > 0.f ? 1 : -1;
				const FVector Tk = (Par.Samples[FMath::Min(k + 1, Par.Samples.Num() - 1)] - Par.Samples[FMath::Max(k - 1, 0)]).GetSafeNormal();
				FVector UpP = FVector::UpVector - Tk * Tk.Z;
				if (UpP.SizeSquared() < 1e-4) UpP = (B.FilletCenter - Par.Samples[k]).GetSafeNormal();
				UpP.Normalize();
				P = Par.Samples[k] + UpP * (Par.SampleRadius[k] + 30.f);
				Look = (B.FilletCenter - P).GetSafeNormal2D();
			}
			const float Heading = FMath::RadiansToDegrees(FMath::Atan2(Look.Y, Look.X));
			UE_LOG(LogLeafline, Log, TEXT("Leaf.FilletCheck %d: level %d, parent %d, radius %.0f, blend %.1f, fillet %.1f, centre %s, zone %.0f..%.0f cm; view: Leaf.Goto %.0f %.0f %.0f %.0f %.0f"),
				b, B.Level, B.Parent, B.SampleRadius[0], B.BlendK, B.FilletK, *B.FilletCenter.ToCompactString(), B.FilletInner, B.FilletOuter,
				P.X, P.Y, P.Z, Heading, Pitch);
			return;
		}
		if (!Field.HasFillets())
		{
			UE_LOG(LogLeafline, Log, TEXT("Leaf.FilletCheck: fillets are off"));
			return;
		}
		const double Start = FPlatformTime::Seconds();
		FTreeMeshData Skirted;
		TreeMeshBuilder::Build(S, Skirted, &Field);
		FTreeField Legacy;
		Legacy.Build(S, false);
		FTreeMeshData Tubes;
		TreeMeshBuilder::Build(S, Tubes);

		TArray<int32> Zones;
		for (int32 b = 0; b < S.Branches.Num(); ++b)
		{
			if (S.Branches[b].FilletK > 0.f) Zones.Add(b);
		}
		int32 InZone = 0, Lifted = 0, Webbed = 0;
		double WorstWeb = 0.0, WorstLift = 0.0;
		FVector WorstWebAt = FVector::ZeroVector;
		int32 WorstWebZone = INDEX_NONE;
		for (const FVector& V : Tubes.Vertices)
		{
			if (V.Z < Field.GroundZ + 1.2 * Field.GetGroundK() + 20.0) continue; // under the ground, or in the ground's own fillet (its skirt)
			// Expected here: the field without fillets, or the two tubes of a zone this vertex lies in blended with its
			// fillet (what that zone's skirt is laid on). Lower than both means a third branch lifts it: webbing.
			bool bAny = false;
			int32 FirstZone = INDEX_NONE;
			const double Old = Legacy.Distance(V);
			double Expected = Old;
			for (int32 c : Zones)
			{
				const FTreeBranch& B = S.Branches[c];
				if (FVector::DistSquared(V, B.FilletCenter) >= FMath::Square((double)B.FilletOuter)) continue;
				bAny = true;
				if (FirstZone == INDEX_NONE) FirstZone = c;
				Expected = FMath::Min(Expected, FTreeField::SMin(Field.BranchDistance(B.Parent, V), Field.BranchDistance(c, V), Field.BlendKAt(c, V)));
			}
			if (!bAny) continue;
			++InZone;
			const double Now = Field.Distance(V);
			const double Lift = Old - Now;
			if (Lift > 2.0)
			{
				++Lifted;
				WorstLift = FMath::Max(WorstLift, Lift);
			}
			if (Expected - Now <= 2.0) continue;
			const double Web = Expected - Now;
			++Webbed;
			if (Web > WorstWeb)
			{
				WorstWeb = Web;
				WorstWebAt = V;
				WorstWebZone = FirstZone;
			}
		}

		const FLeafSet& Leaves = Tree->GetLeaves();
		int32 Touching = 0;
		for (const FTreeLeaf& L : Leaves.Leaves)
		{
			if (Field.Distance(L.Point(L.Length * 0.5f, 0.f)) < 8.0 || Field.Distance(L.Point(L.Length, 0.f)) < 8.0 ||
				Field.Distance(L.Point(L.Length * 0.25f, 0.f)) < 5.0)
			{
				++Touching;
			}
		}
		UE_LOG(LogLeafline, Log, TEXT("Leaf.FilletCheck: %d fillet zones; %d tube vertices in them, %d lifted over 2 cm by the fillets (worst %.1f cm), %d lifted over 2 cm more than their own junction's two tubes explain (webbing; worst %.1f cm at %s, zone of branch %d); %d of %d leaves would fail the skip test against the field with fillets; %.0f ms"),
			Zones.Num(), InZone, Lifted, WorstLift, Webbed, WorstWeb, *WorstWebAt.ToCompactString(), WorstWebZone, Touching, Leaves.Num(),
			(FPlatformTime::Seconds() - Start) * 1000.0);
	}

	FAutoConsoleCommandWithWorldAndArgs CmdFilletCheck(
		TEXT("Leaf.FilletCheck"),
		TEXT("Leaf.FilletCheck [branch [extra cm] [pitch] [fromparent]]: log the junction skirts' fit to the walking field, tube vertices the fillets lift without a skirt over them (webbing), and leaves touching fillets; with a branch, its fillet and a Leaf.Goto that looks back at its junction from just outside the fillet zone (plus extra cm), on the child or (fromparent 1) on the parent."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World) { FilletCheck(World, Args); }));

	FAutoConsoleCommandWithWorldAndArgs CmdCollars(
		TEXT("Leaf.Collars"),
		TEXT("Leaf.Collars [0|1]: branches swell where they leave their parent (1, default) or not (0); no argument toggles. Grows today's tree again, fresh."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World) { Collars(World, Args); }));

	FAutoConsoleCommandWithWorldAndArgs CmdRootBark(
		TEXT("Leaf.RootBark"),
		TEXT("Leaf.RootBark [0|1|taper]: roots' bark ridges held long (1, default) or following the root's own taper as before (0); a value with a point (0.3) sets the taper; no argument toggles. Grows today's tree again with its saved bites; the day restarts (bark only)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World) { RootBark(World, Args); }));

	FAutoConsoleCommandWithWorldAndArgs CmdStage(
		TEXT("Leaf.Stage"),
		TEXT("Leaf.Stage <sparse|blend|drop|leafy|height|rich|gust> <value>: change today's stage tuning and grow its tree again, fresh (metres for blend and leafy)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World) { StageTune(World, Args); }));

	FAutoConsoleCommandWithWorldAndArgs CmdOneTree(
		TEXT("Leaf.OneTree"),
		TEXT("Leaf.OneTree 0|1: one tree per colony stage (kept tree, saved bites, moves at milestones). Takes effect from the next day."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World) { SetOneTree(World, Args); }));

	FAutoConsoleCommandWithWorldAndArgs CmdMove(
		TEXT("Leaf.Move"),
		TEXT("Leaf.Move: move the colony to the next tree now (today restarts on it, with the move card)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World) { MoveNow(World); }));

	FAutoConsoleCommandWithWorldAndArgs CmdStrip(
		TEXT("Leaf.Strip"),
		TEXT("Leaf.Strip <metres>: use up every leaf below that height (testing the climb)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World) { Strip(World, Args); }));

	FAutoConsoleCommandWithWorldAndArgs CmdReport(
		TEXT("Leaf.TreeReport"),
		TEXT("Leaf.TreeReport [label]: pieces left per 10 m height band and on the lowest leafy limbs."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World) { Report(World, Args); }));

	FAutoConsoleCommandWithWorldAndArgs CmdReload(
		TEXT("Leaf.ReloadColony"),
		TEXT("Leaf.ReloadColony: read the save again and restart the day from it, growing the tree as at launch (no haul or bites since the last day end)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World) { ReloadColony(World); }));
}
