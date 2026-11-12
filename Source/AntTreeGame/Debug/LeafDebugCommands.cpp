#include "Debug/LeafDebug.h"
#include "AntTreeGame.h"
#include "Ant/AntPawn.h"
#include "Ant/AntMovementComponent.h"
#include "Tree/TreeActor.h"
#include "Game/LeaflineGameMode.h"
#include "Game/LeaflinePlayerController.h"
#include "Ant/AntCamera.h"
#include "Engine/Engine.h"
#include "UObject/UnrealType.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"

namespace LeafDebug
{
	float AutoMove = 0.f;
	float AutoTurn = 0.f;
	bool bAutoWander = false;
	bool bDebugField = false;
}

namespace
{
	AAntPawn* PlayerAnt(UWorld* World)
	{
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		return PC ? Cast<AAntPawn>(PC->GetPawn()) : nullptr;
	}

	FAutoConsoleCommandWithWorldAndArgs CmdAutoMove(
		TEXT("Leaf.AutoMove"),
		TEXT("Leaf.AutoMove <-1..1>: constant forward input for unattended movement tests (0 stops)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld*)
		{
			LeafDebug::AutoMove = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 0.f;
			UE_LOG(LogLeafline, Log, TEXT("Leaf.AutoMove %.2f"), LeafDebug::AutoMove);
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdAutoTurn(
		TEXT("Leaf.AutoTurn"),
		TEXT("Leaf.AutoTurn <deg/s> [wander]: constant camera yaw rate; with 'wander' the direction flips at random."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld*)
		{
			LeafDebug::AutoTurn = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 0.f;
			LeafDebug::bAutoWander = Args.Num() > 1 && Args[1].Equals(TEXT("wander"), ESearchCase::IgnoreCase);
			UE_LOG(LogLeafline, Log, TEXT("Leaf.AutoTurn %.1f deg/s%s"), LeafDebug::AutoTurn, LeafDebug::bAutoWander ? TEXT(" (wander)") : TEXT(""));
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdDebugField(
		TEXT("Leaf.DebugField"),
		TEXT("Leaf.DebugField 0|1: draw surface normals and log projection error."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld*)
		{
			LeafDebug::bDebugField = Args.Num() > 0 ? FCString::Atoi(*Args[0]) != 0 : !LeafDebug::bDebugField;
			UE_LOG(LogLeafline, Log, TEXT("Leaf.DebugField %d"), LeafDebug::bDebugField ? 1 : 0);
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdTeleport(
		TEXT("Leaf.Teleport"),
		TEXT("Leaf.Teleport <branch> <t>: put the ant on a branch at t (0..1). Leaf.Teleport L<level> [t]: a random branch of that level (0 trunk, 1 limb, 2 bough, 3 twig, -1 root)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			AAntPawn* Ant = PlayerAnt(World);
			ATreeActor* Tree = ATreeActor::Get(World);
			if (!Ant || !Tree || Args.Num() < 1)
			{
				UE_LOG(LogLeafline, Warning, TEXT("Leaf.Teleport: needs a running game and a branch argument"));
				return;
			}
			const TArray<FTreeBranch>& Branches = Tree->GetSkeleton().Branches;
			int32 Branch = INDEX_NONE;
			float T = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 0.5f;
			if (Args[0].StartsWith(TEXT("L"), ESearchCase::IgnoreCase))
			{
				const int32 Level = FCString::Atoi(*Args[0].Mid(1));
				TArray<int32> Candidates;
				for (int32 i = 0; i < Branches.Num(); ++i)
				{
					if (Branches[i].Level == Level) Candidates.Add(i);
				}
				if (Candidates.Num() > 0) Branch = Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
				if (Args.Num() < 2) T = FMath::FRandRange(0.2f, 0.9f);
			}
			else
			{
				Branch = FCString::Atoi(*Args[0]);
			}
			if (!Ant->GetAntMovement()->PlaceOnBranch(Branch, T))
			{
				UE_LOG(LogLeafline, Warning, TEXT("Leaf.Teleport: no branch %s (tree has %d)"), *Args[0], Branches.Num());
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdNewDay(
		TEXT("Leaf.NewDay"),
		TEXT("Leaf.NewDay [seed]: regrow the tree (random seed if none) and return the ant to the nest."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ALeaflineGameMode* GM = World ? World->GetAuthGameMode<ALeaflineGameMode>() : nullptr;
			if (!GM)
			{
				UE_LOG(LogLeafline, Warning, TEXT("Leaf.NewDay: no Leafline game mode running"));
				return;
			}
			const int32 Seed = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : FMath::RandRange(1, 1000000);
			GM->NewDay(Seed);
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdCarry(
		TEXT("Leaf.Carry"),
		TEXT("Leaf.Carry <0-1>: hand the controlled ant a fragment of that size (0 drops it)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			AAntPawn* Ant = PlayerAnt(World);
			if (!Ant) return;
			const float Size = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 0.5f;
			if (Size <= 0.f) Ant->ClearFragment();
			else Ant->GiveFragment(Size);
			UE_LOG(LogLeafline, Log, TEXT("Leaf.Carry %.2f (mass %.1f)"), Size, Ant->GetCarriedMass());
		}));

	void TuneMessage(const FString& Text)
	{
		UE_LOG(LogLeafline, Log, TEXT("%s"), *Text);
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Yellow, Text);
	}

	FAutoConsoleCommandWithWorldAndArgs CmdTune(
		TEXT("Leaf.Tune"),
		TEXT("Leaf.Tune <cam|move|input> [Field] [Value]: list, read or set a tuning value while playing, e.g. Leaf.Tune cam SurfaceFollow 0."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ALeaflinePlayerController* PC = World ? Cast<ALeaflinePlayerController>(World->GetFirstPlayerController()) : nullptr;
			if (!PC || Args.Num() < 1)
			{
				TuneMessage(TEXT("Leaf.Tune <cam|move|input> [Field] [Value] (needs a running game)"));
				return;
			}
			const FString Which = Args[0].ToLower();
			const UStruct* Type = nullptr;
			void* Data = nullptr;
			auto UseTuning = [&](UObject* Owner)
			{
				if (!Owner) return;
				if (FStructProperty* SP = FindFProperty<FStructProperty>(Owner->GetClass(), TEXT("Tuning")))
				{
					Type = SP->Struct;
					Data = SP->ContainerPtrToValuePtr<void>(Owner);
				}
			};
			if (Which == TEXT("cam")) UseTuning(PC->GetAntCamera());
			else if (Which == TEXT("move")) UseTuning(PC->GetAnt() ? PC->GetAnt()->GetAntMovement() : nullptr);
			else if (Which == TEXT("input"))
			{
				Type = ALeaflinePlayerController::StaticClass();
				Data = PC;
			}
			if (!Type || !Data)
			{
				TuneMessage(FString::Printf(TEXT("Leaf.Tune: nothing to tune for '%s' (use cam, move or input)"), *Args[0]));
				return;
			}
			auto Print = [&](const FProperty* P)
			{
				FString Value;
				P->ExportTextItem_Direct(Value, P->ContainerPtrToValuePtr<void>(Data), nullptr, nullptr, PPF_None);
				TuneMessage(FString::Printf(TEXT("%s.%s = %s"), *Which, *P->GetName(), *Value));
			};
			if (Args.Num() == 1)
			{
				for (TFieldIterator<FProperty> It(Type, EFieldIteratorFlags::ExcludeSuper); It; ++It)
				{
					if (It->HasAnyPropertyFlags(CPF_Edit)) Print(*It);
				}
				return;
			}
			FProperty* P = FindFProperty<FProperty>(Type, FName(*Args[1]));
			if (!P || !P->HasAnyPropertyFlags(CPF_Edit))
			{
				TuneMessage(FString::Printf(TEXT("Leaf.Tune: %s has no field %s"), *Which, *Args[1]));
				return;
			}
			if (Args.Num() >= 3)
			{
				P->ImportText_Direct(*Args[2], P->ContainerPtrToValuePtr<void>(Data), nullptr, PPF_None);
			}
			Print(P);
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdFieldBench(
		TEXT("Leaf.FieldBench"),
		TEXT("Leaf.FieldBench [n]: time n distance-field evaluations near the tree surface and log the cost."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ATreeActor* Tree = ATreeActor::Get(World);
			if (!Tree) return;
			const FTreeField& Field = Tree->GetField();
			const TArray<FTreeBranch>& Branches = Tree->GetSkeleton().Branches;
			const int32 N = Args.Num() > 0 ? FMath::Max(1, FCString::Atoi(*Args[0])) : 20000;
			FRandomStream R(7);
			TArray<FVector> Points;
			Points.Reserve(N);
			for (int32 i = 0; i < N; ++i)
			{
				const FTreeBranch& B = Branches[R.RandRange(0, Branches.Num() - 1)];
				const int32 S = R.RandRange(0, B.Samples.Num() - 1);
				Points.Add(B.Samples[S] + R.VRand() * (B.SampleRadius[S] + R.FRandRange(0.f, 100.f)));
			}
			double Sum = 0.0;
			const double Start = FPlatformTime::Seconds();
			for (const FVector& P : Points) Sum += Field.Distance(P);
			const double Elapsed = FPlatformTime::Seconds() - Start;
			UE_LOG(LogLeafline, Log, TEXT("Leaf.FieldBench: %d evaluations, %.2f us each (checksum %.1f)"), N, Elapsed * 1e6 / N, Sum);
		}));
}
