#include "Debug/LeafDebug.h"
#include "AntTreeGame.h"
#include "Ant/AntPawn.h"
#include "Ant/AntMovementComponent.h"
#include "Tree/TreeActor.h"
#include "Tree/TreeMeshBuilder.h"
#include "Game/LeaflineGameMode.h"
#include "Game/LeaflinePlayerController.h"
#include "Ant/AntCamera.h"
#include "Engine/Engine.h"
#include "UObject/UnrealType.h"
#include "World/DayDirector.h"
#include "TimerManager.h"
#include "World/WindSystem.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"

namespace LeafDebug
{
	float AutoMove = 0.f;
	float AutoStrafe = 0.f;
	float AutoTurn = 0.f;
	bool bAutoWander = false;
	bool bDebugField = false;
	float HoldCut = 0.f;
	float HoldGrip = 0.f;
	float HoldFollow = 0.f;
}

namespace
{
	AAntPawn* PlayerAnt(UWorld* World)
	{
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		return PC ? Cast<AAntPawn>(PC->GetPawn()) : nullptr;
	}

	/** After a debug teleport, put the camera behind the ant again so camera-relative input means what it should. */
	void SnapCamera(UWorld* World)
	{
		ALeaflinePlayerController* PC = World ? Cast<ALeaflinePlayerController>(World->GetFirstPlayerController()) : nullptr;
		if (PC && PC->GetAntCamera()) PC->GetAntCamera()->SetTarget(PC->GetAnt(), false);
	}

	FAutoConsoleCommandWithWorldAndArgs CmdAutoMove(
		TEXT("Leaf.AutoMove"),
		TEXT("Leaf.AutoMove <-1..1>: constant forward input for unattended movement tests (0 stops)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld*)
		{
			LeafDebug::AutoMove = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 0.f;
			UE_LOG(LogLeafline, Log, TEXT("Leaf.AutoMove %.2f"), LeafDebug::AutoMove);
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdAutoStrafe(
		TEXT("Leaf.AutoStrafe"),
		TEXT("Leaf.AutoStrafe <-1..1>: constant sideways input (positive = screen right) for unattended movement tests (0 stops)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld*)
		{
			LeafDebug::AutoStrafe = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 0.f;
			UE_LOG(LogLeafline, Log, TEXT("Leaf.AutoStrafe %.2f"), LeafDebug::AutoStrafe);
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
			if (Ant->GetAntMovement()->PlaceOnBranch(Branch, T))
			{
				SnapCamera(World);
			}
			else
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

	FAutoConsoleCommandWithWorldAndArgs CmdGoLeaf(
		TEXT("Leaf.GoLeaf"),
		TEXT("Leaf.GoLeaf: put the controlled ant on a twig beside a random cuttable leaf."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			AAntPawn* Ant = PlayerAnt(World);
			ATreeActor* Tree = ATreeActor::Get(World);
			if (!Ant || !Tree || Tree->GetLeaves().Num() == 0) return;
			const FLeafSet& Leaves = Tree->GetLeaves();
			for (int32 Try = 0; Try < 50; ++Try)
			{
				const int32 i = FMath::RandRange(0, Leaves.Num() - 1);
				const FTreeLeaf& L = Leaves.Get(i);
				if (!L.CanCut()) continue;
				Ant->GetAntMovement()->TeleportTo(L.Base - L.Dir * 15.0, L.Dir);
				SnapCamera(World);
				UE_LOG(LogLeafline, Log, TEXT("Leaf.GoLeaf: beside leaf %d on branch %d at z %.0f"), i, L.Branch, L.Base.Z);
				return;
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdCut(
		TEXT("Leaf.Cut"),
		TEXT("Leaf.Cut [0-1]: cut the leaf in reach at once with that fragment size."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			AAntPawn* Ant = PlayerAnt(World);
			if (!Ant) return;
			const float Size = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 0.6f;
			if (!Ant->DebugCutNow(Size)) UE_LOG(LogLeafline, Warning, TEXT("Leaf.Cut: no cuttable leaf in reach (or already carrying)"));
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdSendHome(
		TEXT("Leaf.SendHome"),
		TEXT("Leaf.SendHome: send the controlled ant home, as if Q were pressed."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			ADayDirector* D = ADayDirector::Get(World);
			if (D) D->SendHome(PlayerAnt(World));
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdHold(
		TEXT("Leaf.Hold"),
		TEXT("Leaf.Hold <cut|grip|follow> <seconds>: hold that button for a while, as if pressed (tests hold-to-cut, grip and riding)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld*)
		{
			if (Args.Num() < 1) return;
			const float Seconds = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 2.f;
			if (Args[0].Equals(TEXT("cut"), ESearchCase::IgnoreCase)) LeafDebug::HoldCut = Seconds;
			else if (Args[0].Equals(TEXT("grip"), ESearchCase::IgnoreCase)) LeafDebug::HoldGrip = Seconds;
			else if (Args[0].Equals(TEXT("follow"), ESearchCase::IgnoreCase)) LeafDebug::HoldFollow = Seconds;
			UE_LOG(LogLeafline, Log, TEXT("Leaf.Hold %s %.1f s"), *Args[0], Seconds);
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdQuitAfter(
		TEXT("Leaf.QuitAfter"),
		TEXT("Leaf.QuitAfter <seconds>: exit the game after a delay, for unattended standalone runs (ignored in the editor)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (GIsEditor || !World)
			{
				UE_LOG(LogLeafline, Warning, TEXT("Leaf.QuitAfter is ignored in the editor"));
				return;
			}
			const float Seconds = Args.Num() > 0 ? FMath::Max(1.f, FCString::Atof(*Args[0])) : 30.f;
			FTimerHandle Handle;
			World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([]()
			{
				UE_LOG(LogLeafline, Log, TEXT("Leaf.QuitAfter: exiting"));
				FPlatformMisc::RequestExit(false);
			}), Seconds, false);
			UE_LOG(LogLeafline, Log, TEXT("Leaf.QuitAfter: exiting in %.0f s"), Seconds);
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdGust(
		TEXT("Leaf.Gust"),
		TEXT("Leaf.Gust [0-1]: start a gust now (warning, rise, hold, fall) with that peak strength."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (AWindSystem* Wind = AWindSystem::Get(World))
			{
				const float Peak = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 1.f;
				Wind->TriggerGust(Peak);
				UE_LOG(LogLeafline, Log, TEXT("Leaf.Gust %.2f"), Peak);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdWindFace(
		TEXT("Leaf.FaceWind"),
		TEXT("Leaf.FaceWind exposed|sheltered: put the ant on the trunk's windward or lee side at the same height (wind tests)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			AAntPawn* Ant = PlayerAnt(World);
			ATreeActor* Tree = ATreeActor::Get(World);
			AWindSystem* Wind = AWindSystem::Get(World);
			if (!Ant || !Tree || !Wind) return;
			const bool bExposed = Args.Num() == 0 || !Args[0].Equals(TEXT("sheltered"), ESearchCase::IgnoreCase);
			const FVector Axis = Tree->GetSkeleton().TrunkBase + FVector(0.0, 0.0, 1500.0);
			const FVector Side = bExposed ? -Wind->GetDirection() : Wind->GetDirection();
			Ant->GetAntMovement()->TeleportTo(Axis + Side * 1100.0, FVector::UpVector); // just outside the trunk, so the trunk (not the ground) is nearest
			SnapCamera(World);
			UE_LOG(LogLeafline, Log, TEXT("Leaf.FaceWind: %s side, exposure %.2f"), bExposed ? TEXT("exposed") : TEXT("sheltered"),
				Wind->GetExposure(Ant->GetAntMovement()->GetSurfacePosition(), Ant->GetAntMovement()->GetSurfaceNormal()));
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

	/** Signed distance from P to the visible bark: the union of the mesh's cone frusta (lateral faces only). */
	double MeshDistance(const TArray<FTreeBranch>& Branches, const FVector& P)
	{
		double Best = 1e12;
		for (const FTreeBranch& B : Branches)
		{
			if (!B.Bounds.ExpandBy(50.0).IsInsideOrOn(P)) continue;
			for (int32 i = 0; i + 1 < B.Samples.Num(); ++i)
			{
				const FVector A = B.Samples[i];
				const FVector AB = B.Samples[i + 1] - A;
				const double L = AB.Size();
				if (L < 1e-3) continue;
				const FVector Dir = AB / L;
				const FVector AP = P - A;
				const double X = FVector::DotProduct(AP, Dir);
				const double Rho = (AP - Dir * X).Size();
				// Distance in the (axial, radial) plane to the side line from (0, RA) to (L, RB).
				const FVector2D S0(0.0, B.SampleRadius[i]), S1(L, B.SampleRadius[i + 1]), Q(X, Rho);
				const FVector2D E = S1 - S0;
				const double T = FMath::Clamp(FVector2D::DotProduct(Q - S0, E) / E.SizeSquared(), 0.0, 1.0);
				double D = (Q - (S0 + E * T)).Size();
				if (X >= 0.0 && X <= L && Rho < S0.Y + E.Y * (X / L)) D = -D;
				Best = FMath::Min(Best, D);
			}
		}
		return Best;
	}

	/** Logs, for a point, the field distance and each nearby branch's distance to the visible bark. */
	void GapDetail(ATreeActor* Tree, const FVector& WorstP)
	{
		const FTreeField& Field = Tree->GetField();
		const TArray<FTreeBranch>& Branches = Tree->GetSkeleton().Branches;
		int32 NearB = INDEX_NONE;
		const double FD = Field.Distance(WorstP, &NearB);
		UE_LOG(LogLeafline, Log, TEXT("Leaf.GapAt %s: field %.2f, nearest branch %d"), *WorstP.ToCompactString(), FD, NearB);
		for (int32 b = 0; b < Branches.Num(); ++b)
		{
			const TArray<FTreeBranch> One = { Branches[b] };
			const double MD = MeshDistance(One, WorstP);
			if (MD > 400.0) continue;
			int32 BestI = 0;
			double BestS = 1e12;
			for (int32 i = 0; i < Branches[b].Samples.Num(); ++i)
			{
				const double DS = FVector::Dist(Branches[b].Samples[i], WorstP);
				if (DS < BestS) { BestS = DS; BestI = i; }
			}
			const FTreeBranch& B = Branches[b];
			UE_LOG(LogLeafline, Log, TEXT("  branch %d level %d: bark %.1f, nearest sample %d/%d at %s r %.1f dist %.1f"),
				b, B.Level, MD, BestI, B.Samples.Num(), *B.Samples[BestI].ToCompactString(), B.SampleRadius[BestI], BestS);
		}
		const FTreeBranch& Trunk = Branches[0];
		FString Line;
		for (int32 i = 0; i < Trunk.Samples.Num() && Trunk.Samples[i].Z < 2000.0; ++i)
		{
			Line += FString::Printf(TEXT(" [%d z%.0f r%.0f]"), i, Trunk.Samples[i].Z, Trunk.SampleRadius[i]);
		}
		UE_LOG(LogLeafline, Log, TEXT("  trunk samples:%s"), *Line);
	}

	FAutoConsoleCommandWithWorldAndArgs CmdGapAt(
		TEXT("Leaf.GapAt"),
		TEXT("Leaf.GapAt <x> <y> <z>: log the walking field and each nearby branch's bark distance at a point."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ATreeActor* Tree = ATreeActor::Get(World);
			if (!Tree || Args.Num() < 3) return;
			GapDetail(Tree, FVector(FCString::Atod(*Args[0]), FCString::Atod(*Args[1]), FCString::Atod(*Args[2])));
		}));

	/** Field distance at every bark mesh vertex: positive means the walking surface floats above the visible bark there. */
	void MeshGapReport(ATreeActor* Tree)
	{
		const FTreeField& Field = Tree->GetField();
		FTreeMeshData Data;
		TreeMeshBuilder::Build(Tree->GetSkeleton(), Data);
		int32 Count = 0, Over1 = 0, Over3 = 0;
		double Sum = 0.0, Worst = -1e12;
		FVector WorstP = FVector::ZeroVector;
		for (const FVector& V : Data.Vertices)
		{
			if (V.Z < Field.GroundZ + 20.0) continue; // the ground blend lifts the field there
			const double D = Field.Distance(V);
			if (D < -0.5) continue; // inside another branch: hidden
			++Count;
			Sum += D;
			if (D > 1.0) ++Over1;
			if (D > 3.0) ++Over3;
			if (D > Worst)
			{
				Worst = D;
				WorstP = V;
			}
		}
		UE_LOG(LogLeafline, Log, TEXT("Leaf.MeshGap: %d visible bark vertices, mean field %.3f cm, worst %.2f cm at %s, %d over 1 cm, %d over 3 cm"),
			Count, Count ? Sum / Count : 0.0, Worst, *WorstP.ToCompactString(), Over1, Over3);
	}

	FAutoConsoleCommandWithWorldAndArgs CmdMeshGap(
		TEXT("Leaf.MeshGap"),
		TEXT("Leaf.MeshGap: evaluate the walking field at every visible bark mesh vertex and log how far it floats above them."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (ATreeActor* Tree = ATreeActor::Get(World)) MeshGapReport(Tree);
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdSurfaceGap(
		TEXT("Leaf.SurfaceGap"),
		TEXT("Leaf.SurfaceGap [maxZ]: project points around the trunk and roots onto the walking field and log how far they float above the visible bark (only points below maxZ cm, default 1500)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ATreeActor* Tree = ATreeActor::Get(World);
			if (!Tree) return;
			const FTreeField& Field = Tree->GetField();
			const TArray<FTreeBranch>& Branches = Tree->GetSkeleton().Branches;
			const double MaxZ = Args.Num() > 0 ? FCString::Atod(*Args[0]) : 1500.0;
			int32 Count = 0, Over5 = 0, Failed = 0;
			double Sum = 0.0, Worst = -1e12;
			FVector WorstP = FVector::ZeroVector;
			for (int32 b = 0; b < Branches.Num(); ++b)
			{
				const FTreeBranch& B = Branches[b];
				if (B.Level > 0) continue;
				for (int32 i = 0; i < B.Samples.Num(); ++i)
				{
					if (B.Samples[i].Z > MaxZ) continue;
					const FVector Tan = (B.Samples[FMath::Min(i + 1, B.Samples.Num() - 1)] - B.Samples[FMath::Max(i - 1, 0)]).GetSafeNormal();
					const FVector N0 = FVector::CrossProduct(Tan, FMath::Abs(Tan.Z) < 0.9 ? FVector::UpVector : FVector::ForwardVector).GetSafeNormal();
					const FVector N1 = FVector::CrossProduct(Tan, N0);
					for (int32 a = 0; a < 16; ++a)
					{
						const double Th = 2.0 * PI * a / 16;
						FVector P = B.Samples[i] + (N0 * FMath::Cos(Th) + N1 * FMath::Sin(Th)) * (B.SampleRadius[i] + 30.0);
						if (P.Z < Field.GroundZ + 5.0) continue;
						FVector Nrm;
						if (!Field.Project(P, Nrm, 0.0))
						{
							++Failed;
							continue;
						}
						if (P.Z < Field.GroundZ + 5.0) continue; // landed on the ground plane
						const double Gap = MeshDistance(Branches, P);
						++Count;
						Sum += Gap;
						if (Gap > 5.0) ++Over5;
						if (Gap > Worst)
						{
							Worst = Gap;
							WorstP = P;
						}
					}
				}
			}
			UE_LOG(LogLeafline, Log, TEXT("Leaf.SurfaceGap: %d points below z %.0f, mean gap %.2f cm, worst %.1f cm at %s, %d over 5 cm, %d failed projections"),
				Count, MaxZ, Count ? Sum / Count : 0.0, Worst, *WorstP.ToCompactString(), Over5, Failed);
			GapDetail(Tree, WorstP);
		}));
}
