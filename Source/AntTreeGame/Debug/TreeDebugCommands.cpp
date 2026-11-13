// Console commands for one tree per colony stage (docs/briefs/one-tree.md).
#include "AntTreeGame.h"
#include "Engine/World.h"
#include "Game/LeaflineGameInstance.h"
#include "HAL/IConsoleManager.h"
#include "Colony/TrailNetwork.h"
#include "EngineUtils.h"
#include "Tree/TreeActor.h"
#include "World/DayDirector.h"

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
