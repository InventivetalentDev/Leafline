// Console commands for the line: recruits, patches, the harvest, and the unattended tests in docs/briefs/the-line.md.
#include "AntTreeGame.h"
#include "Ant/AntPawn.h"
#include "Ant/AntMovementComponent.h"
#include "Colony/LineSystem.h"
#include "Colony/NestActor.h"
#include "Colony/TrailNetwork.h"
#include "Containers/Ticker.h"
#include "Engine/World.h"
#include "Game/LeaflineGameInstance.h"
#include "HAL/IConsoleManager.h"
#include "Tree/TreeActor.h"
#include "Tree/TreeField.h"
#include "Tree/TreeSkeleton.h"
#include "World/DayDirector.h"
#include "World/WindSystem.h"

namespace
{
	ALineSystem* LinesOf(UWorld* World) { return World ? ALineSystem::Get(World) : nullptr; }

	/** Leaves for test lines: cuttable, in the lowest leafy band (short trips), on the chosen side of the trunk. */
	TArray<int32> PickTestLeaves(UWorld* World, int32 Count, const FString& Side, int32 Seed)
	{
		TArray<int32> Out;
		ATreeActor* Tree = ATreeActor::Get(World);
		const AWindSystem* Wind = AWindSystem::Get(World);
		if (!Tree) return Out;
		const FLeafSet& Leaves = Tree->GetLeaves();
		const FVector Axis = Tree->GetSkeleton().TrunkBase;
		const float Low = Tree->Params.LowestLeafyHeight;
		const FVector Upwind = Wind ? -Wind->GetDirection().GetSafeNormal2D() : FVector::ForwardVector;
		TArray<int32> Candidates;
		for (int32 i = 0; i < Leaves.Num(); ++i)
		{
			const FTreeLeaf& L = Leaves.Get(i);
			if (!L.CanCut() || L.Base.Z > Low + 900.f) continue;
			const FVector Out2D = (L.Base - Axis).GetSafeNormal2D();
			const double Facing = FVector::DotProduct(Out2D, Upwind);
			if (Side == TEXT("exposed") && Facing < 0.5) continue;
			if (Side == TEXT("sheltered") && Facing > -0.5) continue;
			Candidates.Add(i);
		}
		FRandomStream R(Seed);
		// Spread the picks: no two on the same branch.
		TSet<int32> Branches;
		for (int32 Try = 0; Try < 400 && Out.Num() < Count && Candidates.Num() > 0; ++Try)
		{
			const int32 Leaf = Candidates[R.RandRange(0, Candidates.Num() - 1)];
			const int32 Branch = Leaves.Get(Leaf).Branch;
			if (Branches.Contains(Branch)) continue;
			Branches.Add(Branch);
			Out.Add(Leaf);
		}
		return Out;
	}

	/**
	 * A test scout's walk: over the ground from the nest to the trunk, round and up the trunk to the leaf's limb, then
	 * along the top of each branch (limb, bough, twig) to the leaf's base. Guide points are projected onto the walking
	 * field and subdivided to at most 12 cm apart. Rough, but every point is on the bark, which is all a follower needs.
	 */
	bool BuildTestWalk(const ATreeActor* Tree, const FVector& NestLoc, int32 Leaf, TArray<FTrailPoint>& Out)
	{
		const FTreeField& F = Tree->GetField();
		const FTreeSkeleton& S = Tree->GetSkeleton();
		if (Leaf < 0 || Leaf >= Tree->GetLeaves().Num()) return false;
		const FTreeLeaf& TL = Tree->GetLeaves().Get(Leaf);
		auto Project = [&F](FVector P)
		{
			for (int32 k = 0; k < 6; ++k) P -= F.Gradient(P).GetSafeNormal() * F.Distance(P);
			return P;
		};
		TArray<int32> Chain;
		for (int32 b = TL.Branch; S.Branches.IsValidIndex(b); b = S.Branches[b].Parent) Chain.Insert(b, 0);
		if (Chain.Num() < 2 || S.Branches[Chain[0]].Level != 0) return false;
		TArray<FVector> Guides;
		const FVector NestPos = Project(NestLoc + FVector(0, 0, 20));
		Guides.Add(NestPos);
		// Up the trunk, turning round it from the nest's side to the side the next branch leaves from.
		const FTreeBranch& Trunk = S.Branches[Chain[0]];
		const FTreeBranch& First = S.Branches[Chain[1]];
		const float TEnd = First.ParentT;
		float R0;
		const FVector Foot = TreeSkeleton::SampleAt(Trunk, 0.f, &R0, nullptr);
		float TStart = 0.f;
		for (int32 k = 0; k < Trunk.Samples.Num(); ++k)
		{
			if (Trunk.Samples[k].Z > NestPos.Z + 60.0) { TStart = Trunk.SampleDist[k] / FMath::Max(Trunk.Length(), 1.f); break; }
		}
		const FVector AxisAtFoot = TreeSkeleton::SampleAt(Trunk, TStart, nullptr, nullptr);
		FVector DirFrom = (NestPos - AxisAtFoot).GetSafeNormal2D();
		const FVector AxisAtJoin = TreeSkeleton::SampleAt(Trunk, TEnd, nullptr, nullptr);
		const FVector ChildOut = First.Samples.Num() > 3 ? First.Samples[3] : First.Samples.Last();
		FVector DirTo = (ChildOut - AxisAtJoin).GetSafeNormal2D();
		if (DirFrom.IsNearlyZero()) DirFrom = FVector::ForwardVector;
		if (DirTo.IsNearlyZero()) DirTo = DirFrom;
		const float AFrom = FMath::Atan2(DirFrom.Y, DirFrom.X);
		float ATo = FMath::Atan2(DirTo.Y, DirTo.X);
		while (ATo - AFrom > PI) ATo -= 2.f * PI;
		while (ATo - AFrom < -PI) ATo += 2.f * PI;
		const int32 Steps = FMath::Max(8, FMath::CeilToInt((TEnd - TStart) * Trunk.Length() / 40.f));
		for (int32 k = 0; k <= Steps; ++k)
		{
			const float U = float(k) / Steps;
			float R;
			const FVector C = TreeSkeleton::SampleAt(Trunk, FMath::Lerp(TStart, TEnd, U), &R, nullptr);
			const float A = FMath::Lerp(AFrom, ATo, FMath::SmoothStep(0.f, 1.f, FMath::Min(U * 1.3f, 1.f)));
			Guides.Add(Project(C + FVector(FMath::Cos(A), FMath::Sin(A), 0.0) * (R + 60.0)));
		}
		// Along the top of each branch to the next junction, and the last one to the leaf.
		for (int32 c = 1; c < Chain.Num(); ++c)
		{
			const FTreeBranch& B = S.Branches[Chain[c]];
			const float Len = FMath::Max(B.Length(), 1.f);
			float ParentR = 0.f;
			TreeSkeleton::SampleAt(S.Branches[Chain[c - 1]], B.ParentT, &ParentR, nullptr);
			const float S0 = FMath::Min(ParentR + 10.f, Len);
			const float S1 = c + 1 < Chain.Num() ? S.Branches[Chain[c + 1]].ParentT * Len : FMath::Clamp(TL.BranchS, S0, Len);
			const int32 N = FMath::Max(2, FMath::CeilToInt(FMath::Abs(S1 - S0) / 30.f));
			for (int32 k = 0; k <= N; ++k)
			{
				float R;
				FVector Tan;
				const FVector C = TreeSkeleton::SampleAt(B, FMath::Lerp(S0, S1, float(k) / N) / Len, &R, &Tan);
				FVector Top = FVector::UpVector - Tan * FVector::DotProduct(FVector::UpVector, Tan);
				if (Top.SizeSquared() < 1e-4) Top = FVector::ForwardVector - Tan * FVector::DotProduct(FVector::ForwardVector, Tan);
				Guides.Add(Project(C + Top.GetSafeNormal() * (R + 30.0)));
			}
		}
		Guides.Add(Project(TL.Base));
		// Subdivide long steps by projecting midpoints.
		Out.Reset();
		auto Emit = [&](const FVector& P) { Out.Add({ P, F.Gradient(P).GetSafeNormal() }); };
		Emit(Guides[0]);
		for (int32 g = 1; g < Guides.Num(); ++g)
		{
			const FVector A = Out.Last().Pos, B = Guides[g];
			const int32 N = FMath::Max(1, FMath::CeilToInt(FVector::Dist(A, B) / 12.0));
			for (int32 k = 1; k <= N; ++k) Emit(Project(FMath::Lerp(A, B, double(k) / N)));
		}
		return Out.Num() > 2;
	}

	/**
	 * Lays test lines at once: each scout's trail is a walk over the bark from the nest to a chosen leaf (the branch-
	 * graph approach, so recruits have a real route), then it cuts and is sent home. Returns lines laid.
	 */
	int32 LayTestLines(UWorld* World, int32 Count, float Size, const FString& Side, int32 Seed)
	{
		ADayDirector* D = ADayDirector::Get(World);
		ATreeActor* Tree = ATreeActor::Get(World);
		if (!D || !Tree || !D->GetNest() || !D->GetTrails()) return 0;
		const TArray<int32> Picks = PickTestLeaves(World, Count, Side, Seed);
		int32 Laid = 0;
		for (int32 Leaf : Picks)
		{
			AAntPawn* Ant = D->DebugTakeNextScout();
			if (!Ant) break;
			TArray<FTrailPoint> Walk;
			if (!BuildTestWalk(Tree, D->GetNest()->GetActorLocation(), Leaf, Walk))
			{
				UE_LOG(LogLeafline, Warning, TEXT("Leaf.TestLines: no test walk to leaf %d"), Leaf);
				continue;
			}
			for (const FTrailPoint& P : Walk) D->GetTrails()->AddPoint(Ant->GetPathIndex(), P.Pos, P.Normal);
			Ant->GetAntMovement()->TeleportTo(Walk.Last().Pos, Walk.Last().Pos - Walk[Walk.Num() - 2].Pos);
			if (!Ant->DebugCutLeaf(Leaf, Size))
			{
				UE_LOG(LogLeafline, Warning, TEXT("Leaf.TestLines: could not cut leaf %d"), Leaf);
				continue;
			}
			D->SendHome(Ant);
			++Laid;
		}
		UE_LOG(LogLeafline, Log, TEXT("Leaf.TestLines: %d of %d lines laid (size %.2f, %s side)"), Laid, Count, Size, *Side);
		return Laid;
	}

	FAutoConsoleCommandWithWorldAndArgs CmdRecruits(
		TEXT("Leaf.Recruits"),
		TEXT("Leaf.Recruits 0|1: the line's off switch (recruits, harvest, Brood row, milestone set). The pool follows from the next day."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ADayDirector* D = ADayDirector::Get(World);
			if (!D) return;
			D->SetRecruitsOn(Args.Num() > 0 ? FCString::Atoi(*Args[0]) != 0 : !D->AreRecruitsOn());
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdStartDay(
		TEXT("Leaf.StartDay"),
		TEXT("Leaf.StartDay <day> [seed]: start that day now (seed from the day unless given), e.g. after Leaf.ResetSave for repeatable tests."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ADayDirector* D = ADayDirector::Get(World);
			if (!D || Args.Num() < 1) return;
			D->StartDay(FCString::Atoi(*Args[0]), Args.Num() > 1 ? FCString::Atoi(*Args[1]) : -1);
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdPool(
		TEXT("Leaf.Pool"),
		TEXT("Leaf.Pool <n>: force the recruit pool from the next day (-1: back to the Brood upgrade)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ALineSystem* L = LinesOf(World);
			if (!L) return;
			const int32 N = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : -1;
			L->SetPoolOverride(N);
			UE_LOG(LogLeafline, Log, TEXT("Leaf.Pool %d (from the next day)"), N);
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdHarvest(
		TEXT("Leaf.Harvest"),
		TEXT("Leaf.Harvest: end the shifts now (waiting scouts stay home, a scout with a piece is sent home) and go to the harvest."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (ADayDirector* D = ADayDirector::Get(World)) D->ForceHarvest();
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdSkip(
		TEXT("Leaf.SkipHarvest"),
		TEXT("Leaf.SkipHarvest: skip the running harvest, as the confirm button does."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (ADayDirector* D = ADayDirector::Get(World)) D->SkipHarvest();
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdPatch(
		TEXT("Leaf.Patch"),
		TEXT("Leaf.Patch [leaf] [size]: log the patch around the leaf in reach (or the given leaf): leaves, pieces, richness."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ALineSystem* L = LinesOf(World);
			ATreeActor* Tree = ATreeActor::Get(World);
			APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
			AAntPawn* Ant = PC ? Cast<AAntPawn>(PC->GetPawn()) : nullptr;
			if (!L || !Tree) return;
			int32 Leaf = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : INDEX_NONE;
			if (Leaf == INDEX_NONE && Ant)
			{
				Leaf = Ant->GetLeafInReach();
				if (Leaf == INDEX_NONE) Leaf = Tree->FindLeafNear(Ant->GetActorTransform().TransformPosition(Ant->MandibleOffset), Ant->Cut.Reach * 2.f);
			}
			const float Size = Args.Num() > 1 ? FCString::Atof(*Args[1]) : L->Tuning.PreviewTemplate;
			FPatchPreview P;
			if (Leaf == INDEX_NONE || !L->GetPatchPreview(Leaf, Size, P))
			{
				UE_LOG(LogLeafline, Warning, TEXT("Leaf.Patch: no leaf in reach (give a leaf index)"));
				return;
			}
			FString List;
			for (int32 i : P.Leaves)
			{
				const FTreeLeaf& TL = Tree->GetLeaves().Get(i);
				List += FString::Printf(TEXT(" %d(br %d, %d pc, x%.2f)"), i, TL.Branch, Tree->GetLeaves().PiecesLeft(i, P.Template, Ant ? Ant->Cut.LeafUsePerCut : 0.38f), TL.Richness);
			}
			UE_LOG(LogLeafline, Log, TEXT("Leaf.Patch leaf %d at size %.2f: %d leaves, %d pieces, rich x%.2f:%s"),
				Leaf, P.Template, P.Leaves.Num(), P.Pieces, P.Richness, *List);
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdPatchSurvey(
		TEXT("Leaf.PatchSurvey"),
		TEXT("Leaf.PatchSurvey [samples]: patch sizes over random leaves at templates 0.3 / 0.6 / 1.0, and approach failures by reason."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ATreeActor* Tree = ATreeActor::Get(World)) Tree->LogPatchSurvey(Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 40);
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdLineInfo(
		TEXT("Leaf.LineInfo"),
		TEXT("Leaf.LineInfo: log every line (pieces home, lost, patch left, recruits on it) and the pool."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			ALineSystem* L = LinesOf(World);
			if (!L) return;
			for (const FColonyLine& Line : L->GetLines())
			{
				UE_LOG(LogLeafline, Log, TEXT("Line %d (worker %d): %d home (%.1f), %d lost, patch %d of %d left (%d reserved), %d recruits, template %.2f, rich x%.2f"),
					Line.Number, Line.Worker, Line.PiecesHome, Line.MassHome, Line.PiecesLost, Line.PatchRemaining, Line.PatchTotal,
					Line.Reserved, Line.Assigned, Line.Template, Line.Richness);
			}
			UE_LOG(LogLeafline, Log, TEXT("Pool %d: %d out, %d idle, %d trips left, %d lost to wind, %d fallback cuts, speed x%.0f"),
				L->GetPoolSize(), L->NumOut(), L->NumIdle(), L->TripsLeft(), L->GetRecruitLosses(), L->GetFallbackCount(), L->GetSpeedFactor());
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdTestLines(
		TEXT("Leaf.TestLines"),
		TEXT("Leaf.TestLines <count> [size] [exposed|sheltered|any] [seed]: scouts lay lines at once to leaves in the lowest leafy band (tests)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const int32 Count = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 3;
			const float Size = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 0.5f;
			const FString Side = Args.Num() > 2 ? Args[2].ToLower() : FString(TEXT("any"));
			const int32 Seed = Args.Num() > 3 ? FCString::Atoi(*Args[3]) : 7;
			LayTestLines(World, Count, Size, Side, Seed);
		}));

	// Unattended days: scripted scouts lay random lines, the harvest is skipped, menus are confirmed. Every day must end.
	struct FAutoPlay
	{
		TWeakObjectPtr<UWorld> World;
		int32 DaysLeft = 0;
		int32 DaysDone = 0;
		int32 LinesPerDay = 3;
		bool bSkip = true;
		bool bGusts = true;
		bool bBuy = false;
		bool bQuit = false;
		int32 LaidOnDay = -1;
		double DayStart = 0.0;
		double LastGust = 0.0;
		double HarvestSince = -1.0;
		double RunStart = 0.0;
		FTSTicker::FDelegateHandle Handle;
	};
	FAutoPlay Auto;

	bool AutoTick(float)
	{
		UWorld* World = Auto.World.Get();
		ADayDirector* D = World ? ADayDirector::Get(World) : nullptr;
		if (!D || Auto.DaysLeft <= 0)
		{
			Auto.Handle.Reset();
			return false;
		}
		const double Now = FPlatformTime::Seconds();
		switch (D->GetPhase())
		{
		case EDayPhase::Title:
			D->MenuConfirm();
			break;
		case EDayPhase::Shift:
			if (Auto.LaidOnDay != D->GetDay())
			{
				Auto.LaidOnDay = D->GetDay();
				Auto.DayStart = Now;
				Auto.HarvestSince = -1.0;
				FRandomStream R(D->GetSeed() + 31 * D->GetDay());
				const float Size = R.FRandRange(0.3f, 0.95f);
				const TCHAR* Sides[] = { TEXT("any"), TEXT("exposed"), TEXT("sheltered") };
				LayTestLines(World, Auto.LinesPerDay, Size, Sides[R.RandRange(0, 2)], R.RandRange(1, 100000));
				D->ForceHarvest();
			}
			break;
		case EDayPhase::Harvest:
			if (Auto.HarvestSince < 0.0) Auto.HarvestSince = Now;
			if (Auto.bSkip && Now - Auto.HarvestSince > 2.0) D->SkipHarvest();
			if (Auto.bGusts && Now - Auto.LastGust > 25.0 && !Auto.bSkip)
			{
				Auto.LastGust = Now;
				if (AWindSystem* W = AWindSystem::Get(World)) W->TriggerGust(1.f);
			}
			break;
		case EDayPhase::Recap:
			UE_LOG(LogLeafline, Log, TEXT("AutoPlay: day %d ended normally after %.0f s"), D->GetDay(), Now - Auto.DayStart);
			++Auto.DaysDone;
			if (--Auto.DaysLeft <= 0)
			{
				UE_LOG(LogLeafline, Log, TEXT("AutoPlay: done, %d days ended, %.0f s in all"), Auto.DaysDone, Now - Auto.RunStart);
				Auto.Handle.Reset();
				if (Auto.bQuit && !GIsEditor) FPlatformMisc::RequestExit(false);
				return false;
			}
			D->MenuConfirm();
			break;
		case EDayPhase::EndCard:
			UE_LOG(LogLeafline, Log, TEXT("AutoPlay: end card after %d days, %.0f s"), Auto.DaysDone, Now - Auto.RunStart);
			D->MenuConfirm();
			break;
		case EDayPhase::Nest:
			if (Auto.bBuy)
			{
				// Buy whatever is affordable, cheapest first, then start the next day.
				ULeaflineGameInstance* GI = Cast<ULeaflineGameInstance>(World->GetGameInstance());
				for (bool bBought = true; GI && bBought;)
				{
					bBought = false;
					int32 Best = INDEX_NONE;
					for (int32 u = 0; u < LeaflineUpgrades::Count; ++u)
					{
						if (!D->IsNestRowVisible(u) || !GI->CanAfford(u)) continue;
						if (Best == INDEX_NONE || GI->GetUpgradeCost(u) < GI->GetUpgradeCost(Best)) Best = u;
					}
					if (Best != INDEX_NONE && GI->Buy(Best))
					{
						bBought = true;
						UE_LOG(LogLeafline, Log, TEXT("AutoPlay: bought %s level %d"), LeaflineUpgrades::Get(Best)->Name, GI->GetUpgradeLevel(Best));
					}
				}
			}
			D->SetNestSelection(LeaflineUpgrades::Count);
			D->MenuConfirm();
			break;
		default:
			break;
		}
		if (D->GetPhase() != EDayPhase::Nest && D->GetPhase() != EDayPhase::Recap && Auto.LaidOnDay == D->GetDay() && Now - Auto.DayStart > 900.0)
		{
			UE_LOG(LogLeafline, Error, TEXT("AutoPlay: day %d has not ended after %.0f s (phase %s)"), D->GetDay(), Now - Auto.DayStart,
				*UEnum::GetValueAsString(D->GetPhase()));
			Auto.DayStart = Now; // report again in 15 minutes rather than every tick
		}
		return true;
	}

	FAutoConsoleCommandWithWorldAndArgs CmdAutoPlay(
		TEXT("Leaf.AutoPlay"),
		TEXT("Leaf.AutoPlay <days> [lines] [watch|skip] [buy] [quit]: unattended days with scripted scouts and random lines; logs whether every day ends; quit exits a standalone game when done. 0 stops."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			Auto.World = World;
			Auto.DaysLeft = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 20;
			Auto.LinesPerDay = Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 3;
			Auto.bSkip = !(Args.Num() > 2 && Args[2].ToLower() == TEXT("watch"));
			Auto.bBuy = false;
			Auto.bQuit = false;
			for (int32 k = 2; k < Args.Num(); ++k)
			{
				Auto.bBuy |= Args[k].ToLower() == TEXT("buy");
				Auto.bQuit |= Args[k].ToLower() == TEXT("quit");
			}
			Auto.DaysDone = 0;
			Auto.LaidOnDay = -1;
			Auto.RunStart = FPlatformTime::Seconds();
			if (!Auto.Handle.IsValid() && Auto.DaysLeft > 0)
			{
				Auto.Handle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&AutoTick), 0.25f);
			}
			UE_LOG(LogLeafline, Log, TEXT("Leaf.AutoPlay: %d days, %d lines a day, harvest %s%s"), Auto.DaysLeft, Auto.LinesPerDay,
				Auto.bSkip ? TEXT("skipped") : TEXT("watched"), Auto.bBuy ? TEXT(", buying upgrades") : TEXT(""));
		}));
}
