#include "Debug/LeafDebug.h"
#include "Containers/Ticker.h"
#include "Engine/PostProcessVolume.h"
#include "EngineUtils.h"
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
#include "Colony/LineSystem.h"
#include "World/BirdSystem.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"

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
	float HoldCycle = 0.f;
	int32 ForcePadIcons = -1;
	bool bInputViz = false;
	TArray<FString> InputVizLines;
	FColor InputVizColor = FColor::White;
	TArray<FInputVizLabel> InputVizLabels;
}

namespace
{
	AAntPawn* PlayerAnt(UWorld* World)
	{
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		return PC ? Cast<AAntPawn>(PC->GetPawn()) : nullptr;
	}

	/** Log the listed properties of an object that exist (dotted paths reach into structs), by reflection. */
	FString DescribeProps(const UObject* Obj, std::initializer_list<const TCHAR*> Paths)
	{
		FString Out;
		for (const TCHAR* Path : Paths)
		{
			TArray<FString> Parts;
			FString(Path).ParseIntoArray(Parts, TEXT("."));
			const UStruct* Type = Obj->GetClass();
			const void* Container = Obj;
			const FProperty* Prop = nullptr;
			for (int32 k = 0; k < Parts.Num() && Type; ++k)
			{
				Prop = FindFProperty<FProperty>(Type, *Parts[k]);
				if (!Prop) break;
				const void* Value = Prop->ContainerPtrToValuePtr<void>(Container);
				if (k + 1 < Parts.Num())
				{
					const FStructProperty* SP = CastField<FStructProperty>(Prop);
					Type = SP ? SP->Struct : nullptr;
					Container = Value;
					if (!SP) Prop = nullptr;
				}
			}
			if (!Prop) continue;
			FString Text;
			Prop->ExportTextItem_Direct(Text, Prop->ContainerPtrToValuePtr<void>(Container), nullptr, nullptr, PPF_None);
			Out += FString::Printf(TEXT(" %s=%s"), Path, *Text);
		}
		return Out;
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
		TEXT("Leaf.SendHome [delay]: send the controlled ant home, as if Q were pressed; after delay seconds if given (unattended return tests)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const float Delay = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 0.f;
			if (Delay <= 0.f || !World)
			{
				ADayDirector* D = ADayDirector::Get(World);
				if (D) D->SendHome(PlayerAnt(World));
				return;
			}
			FTimerHandle Handle;
			TWeakObjectPtr<UWorld> WeakWorld(World);
			World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([WeakWorld]()
			{
				UWorld* W = WeakWorld.Get();
				ADayDirector* D = W ? ADayDirector::Get(W) : nullptr;
				UE_LOG(LogLeafline, Log, TEXT("Leaf.SendHome: delay over, sending home"));
				if (D) D->SendHome(PlayerAnt(W));
			}), Delay, false);
			UE_LOG(LogLeafline, Log, TEXT("Leaf.SendHome in %.0f s"), Delay);
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdHold(
		TEXT("Leaf.Hold"),
		TEXT("Leaf.Hold <cut|grip|follow|cycle> <seconds>: hold that button for a while, as if pressed (tests hold-to-cut, grip, riding and the Tab tap / hold)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld*)
		{
			if (Args.Num() < 1) return;
			const float Seconds = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 2.f;
			if (Args[0].Equals(TEXT("cut"), ESearchCase::IgnoreCase)) LeafDebug::HoldCut = Seconds;
			else if (Args[0].Equals(TEXT("grip"), ESearchCase::IgnoreCase)) LeafDebug::HoldGrip = Seconds;
			else if (Args[0].Equals(TEXT("follow"), ESearchCase::IgnoreCase)) LeafDebug::HoldFollow = Seconds;
			else if (Args[0].Equals(TEXT("cycle"), ESearchCase::IgnoreCase)) LeafDebug::HoldCycle = Seconds;
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
			// The core ticker, not a world timer: it keeps counting while the game is paused.
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float)
			{
				UE_LOG(LogLeafline, Log, TEXT("Leaf.QuitAfter: exiting"));
				FPlatformMisc::RequestExit(false);
				return false;
			}), Seconds);
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
		TEXT("Leaf.Tune <cam|move|input|look|day|line|wind|bird> [Field] [Value]: list, read or set a tuning value while playing, e.g. Leaf.Tune cam SurfaceFollow 0. In colours ';' may stand for ','."),
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
			ADayDirector* LookOwner = nullptr;
			if (Which == TEXT("cam")) UseTuning(PC->GetAntCamera());
			else if (Which == TEXT("day")) UseTuning(ADayDirector::Get(World));
			else if (Which == TEXT("line")) UseTuning(ALineSystem::Get(World));
			else if (Which == TEXT("wind")) UseTuning(AWindSystem::Get(World));
			else if (Which == TEXT("bird")) UseTuning(ABirdSystem::Get(World));
			else if (Which == TEXT("look"))
			{
				LookOwner = ADayDirector::Get(World);
				if (LookOwner)
				{
					Type = FLookTuning::StaticStruct();
					Data = &LookOwner->Look;
				}
			}
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
				// ';' stands for ',' so colours can be given in -ExecCmds, which splits on commas: (R=1;G=0.8;B=0.6;A=1)
				P->ImportText_Direct(*Args[2].Replace(TEXT(";"), TEXT(",")), P->ContainerPtrToValuePtr<void>(Data), nullptr, PPF_None);
				if (LookOwner) LookOwner->ApplyLook();
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

	FAutoConsoleCommandWithWorldAndArgs CmdAim(
		TEXT("Leaf.Aim"),
		TEXT("Leaf.Aim <heading deg|up|trunk> <pitch deg>: aim the camera toward a world heading (0 = +X), up the trunk or toward it, at a pitch, for repeatable screenshots."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ALeaflinePlayerController* PC = World ? Cast<ALeaflinePlayerController>(World->GetFirstPlayerController()) : nullptr;
			if (!PC || !PC->GetAntCamera() || Args.Num() < 2) return;
			if (Args[0].Equals(TEXT("up"), ESearchCase::IgnoreCase)) PC->GetAntCamera()->DebugAimDirection(FVector::UpVector, FCString::Atof(*Args[1]));
			else if (Args[0].Equals(TEXT("trunk"), ESearchCase::IgnoreCase) && PC->GetAnt() && ATreeActor::Get(World))
			{
				FVector ToTrunk = ATreeActor::Get(World)->GetSkeleton().TrunkBase - PC->GetAnt()->GetActorLocation();
				ToTrunk.Z = 0.0;
				PC->GetAntCamera()->DebugAimDirection(ToTrunk.GetSafeNormal(), FCString::Atof(*Args[1]));
			}
			else PC->GetAntCamera()->DebugAim(FCString::Atof(*Args[0]), FCString::Atof(*Args[1]));
			UE_LOG(LogLeafline, Log, TEXT("Leaf.Aim heading %s pitch %s"), *Args[0], *Args[1]);
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdGoto(
		TEXT("Leaf.Goto"),
		TEXT("Leaf.Goto <x> <y> <z> [heading] [pitch]: put the ant on the surface nearest that point and aim the camera; the HUD's faint bottom-left line gives the exact command for the view on screen."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ALeaflinePlayerController* PC = World ? Cast<ALeaflinePlayerController>(World->GetFirstPlayerController()) : nullptr;
			AAntPawn* Ant = PC ? PC->GetAnt() : nullptr;
			if (!Ant || !Ant->GetAntMovement() || Args.Num() < 3) return;
			const FVector P(FCString::Atod(*Args[0]), FCString::Atod(*Args[1]), FCString::Atod(*Args[2]));
			const float Heading = Args.Num() > 3 ? FCString::Atof(*Args[3]) : 0.f;
			Ant->GetAntMovement()->TeleportTo(P, FRotator(0.f, Heading, 0.f).Vector());
			SnapCamera(World);
			if (Args.Num() > 4)
			{
				// The camera snaps behind the ant on its next tick; aim after that.
				const FString Aim = FString::Printf(TEXT("Leaf.Aim %s %s"), *Args[3], *Args[4]);
				TWeakObjectPtr<UWorld> Weak(World);
				FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak, Aim](float)
				{
					UWorld* W = Weak.Get();
					if (APlayerController* C = W ? W->GetFirstPlayerController() : nullptr) C->ConsoleCommand(Aim);
					return false;
				}), 0.3f);
			}
			UE_LOG(LogLeafline, Log, TEXT("Leaf.Goto %s"), *FString::Join(Args, TEXT(" ")));
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdDelay(
		TEXT("Leaf.Delay"),
		TEXT("Leaf.Delay <seconds> <command...>: run a console command later, also while paused (unattended screenshot runs)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (Args.Num() < 2 || !World) return;
			const float Seconds = FMath::Max(0.f, FCString::Atof(*Args[0]));
			FString Cmd;
			for (int32 k = 1; k < Args.Num(); ++k) Cmd += (k > 1 ? TEXT(" ") : TEXT("")) + Args[k];
			TWeakObjectPtr<UWorld> Weak(World);
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak, Cmd](float)
			{
				UWorld* W = Weak.Get();
				APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
				UE_LOG(LogLeafline, Log, TEXT("Leaf.Delay: %s"), *Cmd);
				if (PC) PC->ConsoleCommand(Cmd);
				return false;
			}), Seconds);
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdLightInfo(
		TEXT("Leaf.LightInfo"),
		TEXT("Leaf.LightInfo: log the lights, sky, fog and post-process volumes in the level with their main settings."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (!World) return;
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				const AActor* A = *It;
				for (const UActorComponent* C : A->GetComponents())
				{
					if (!C) continue;
					const FString Cls = C->GetClass()->GetName();
					FString Props;
					if (Cls.Contains(TEXT("DirectionalLight")))
						Props = DescribeProps(C, { TEXT("Intensity"), TEXT("LightColor"), TEXT("bUseTemperature"), TEXT("Temperature"), TEXT("LightSourceAngle"),
							TEXT("bEnableLightShaftBloom"), TEXT("BloomScale"), TEXT("CastShadows"), TEXT("DynamicShadowDistanceMovableLight"), TEXT("bAtmosphereSunLight"),
							TEXT("bCastVolumetricShadow"), TEXT("VolumetricScatteringIntensity"), TEXT("Mobility") });
					else if (Cls.Contains(TEXT("SkyLight")))
						Props = DescribeProps(C, { TEXT("Intensity"), TEXT("LightColor"), TEXT("bRealTimeCapture"), TEXT("SourceType"), TEXT("LowerHemisphereColor"), TEXT("bLowerHemisphereIsBlack"), TEXT("Mobility") });
					else if (Cls.Contains(TEXT("ExponentialHeightFog")))
						Props = DescribeProps(C, { TEXT("FogDensity"), TEXT("FogHeightFalloff"), TEXT("FogInscatteringLuminance"), TEXT("FogInscatteringColor"), TEXT("StartDistance"),
							TEXT("FogMaxOpacity"), TEXT("bEnableVolumetricFog"), TEXT("VolumetricFogScatteringDistribution"), TEXT("VolumetricFogExtinctionScale"), TEXT("VolumetricFogDistance"),
							TEXT("SecondFogData.FogDensity"), TEXT("DirectionalInscatteringExponent"), TEXT("DirectionalInscatteringLuminance") });
					else if (Cls.Contains(TEXT("SkyAtmosphere")))
						Props = DescribeProps(C, { TEXT("BottomRadius"), TEXT("AtmosphereHeight"), TEXT("RayleighScatteringScale"), TEXT("MieScatteringScale"), TEXT("MultiScatteringFactor"), TEXT("TransformMode") });
					else if (Cls.Contains(TEXT("VolumetricCloud")))
						Props = TEXT(" (present)");
					else if (Cls.Contains(TEXT("PostProcess")) || Cls.Contains(TEXT("Camera")))
						Props = DescribeProps(C, { TEXT("Settings.AutoExposureMethod"), TEXT("Settings.AutoExposureBias"), TEXT("Settings.AutoExposureMinBrightness"),
							TEXT("Settings.AutoExposureMaxBrightness"), TEXT("Settings.BloomIntensity"), TEXT("PostProcessSettings.AutoExposureBias"), TEXT("PostProcessSettings.DepthOfFieldFstop") });
					else
						continue;
					UE_LOG(LogLeafline, Log, TEXT("LightInfo: %s / %s at %s rot=%s%s"), *A->GetName(), *Cls, *A->GetActorLocation().ToCompactString(), *A->GetActorRotation().ToCompactString(), *Props);
				}
				if (const APostProcessVolume* V = Cast<APostProcessVolume>(A))
				{
					UE_LOG(LogLeafline, Log, TEXT("LightInfo: %s unbound=%d%s"), *A->GetName(), V->bUnbound ? 1 : 0,
						*DescribeProps(V, { TEXT("Settings.bOverride_AutoExposureMethod"), TEXT("Settings.AutoExposureMethod"), TEXT("Settings.bOverride_AutoExposureBias"), TEXT("Settings.AutoExposureBias"),
							TEXT("Settings.bOverride_AutoExposureMinBrightness"), TEXT("Settings.AutoExposureMinBrightness"), TEXT("Settings.AutoExposureMaxBrightness"),
							TEXT("Settings.BloomIntensity"), TEXT("Settings.DynamicGlobalIlluminationMethod"), TEXT("Settings.ReflectionMethod") }));
				}
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdPauseMenu(
		TEXT("Leaf.PauseMenu"),
		TEXT("Leaf.PauseMenu <page 0|1|2|3> [row]: pause and show the pause menu (0), the options (1) with a row selected, the credits (2; the row is the scroll in design pixels, -1 the end) or the controls (3) (screenshots)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ALeaflinePlayerController* PC = World ? Cast<ALeaflinePlayerController>(World->GetFirstPlayerController()) : nullptr;
			if (!PC) return;
			PC->DebugShowPauseMenu(Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 0, Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 0);
		}));

	/**
	 * Leaf.Orbit: hold "screen right" for a while and log how far the ant gets round the branch it is on (playtest:
	 * "still occasionally get stuck at the tangent point"). With a radius, first put the ant on the limb or bough point
	 * whose radius is nearest to it.
	 */
	struct FOrbitState
	{
		FTSTicker::FDelegateHandle Handle;
		TWeakObjectPtr<UWorld> World;
		double Start = 0.0, NextLog = 0.0, Duration = 0.0, StrafeAt = 0.0;
		int32 Branch = INDEX_NONE, Samples = 0, Stalls = 0;
		float Radius = 0.f, LastAngle = 0.f, Total = 0.f, SinceLog = 0.f;
		FVector Ref = FVector::ZeroVector;
	};
	FOrbitState Orbit;

	/** Angle (deg) of P round branch B at its nearest sample, against a reference fixed while on that branch. */
	bool OrbitAngle(const FTreeBranch& B, const FVector& P, FVector& Ref, float& OutAngle, float& OutRadius)
	{
		if (B.Samples.Num() < 2) return false;
		int32 Best = 0;
		double BestD = TNumericLimits<double>::Max();
		for (int32 i = 0; i < B.Samples.Num(); ++i)
		{
			const double D = FVector::DistSquared(B.Samples[i], P);
			if (D < BestD) { BestD = D; Best = i; }
		}
		const int32 Lo = FMath::Max(Best - 1, 0), Hi = FMath::Min(Best + 1, B.Samples.Num() - 1);
		const FVector Axis = (B.Samples[Hi] - B.Samples[Lo]).GetSafeNormal();
		const FVector R = FVector::VectorPlaneProject(P - B.Samples[Best], Axis).GetSafeNormal();
		if (Ref.IsNearlyZero()) Ref = R;
		const FVector RefHere = FVector::VectorPlaneProject(Ref, Axis).GetSafeNormal();
		OutAngle = FMath::RadiansToDegrees(FMath::Atan2(FVector::DotProduct(R, FVector::CrossProduct(Axis, RefHere)), FVector::DotProduct(R, RefHere)));
		OutRadius = B.SampleRadius[Best];
		return true;
	}

	bool OrbitTick(float)
	{
		UWorld* World = Orbit.World.Get();
		AAntPawn* Ant = PlayerAnt(World);
		ATreeActor* Tree = ATreeActor::Get(World);
		if (!World || !Ant || !Tree || !Ant->GetAntMovement())
		{
			LeafDebug::AutoStrafe = 0.f;
			Orbit.Handle.Reset();
			return false;
		}
		const double Now = World->GetRealTimeSeconds();
		// After a teleport the camera needs a moment to settle behind the ant before "right" is pressed (the controls read
		// the camera when the stick is first pushed).
		if (Orbit.StrafeAt > 0.0 && Now >= Orbit.StrafeAt)
		{
			LeafDebug::AutoStrafe = 1.f;
			Orbit.StrafeAt = 0.0;
			Orbit.Start = Now;
			Orbit.NextLog = Now + 0.5;
		}
		if (Orbit.StrafeAt > 0.0) return true;
		const UAntMovementComponent* M = Ant->GetAntMovement();
		int32 Branch = INDEX_NONE;
		Tree->GetField().Distance(M->GetSurfacePosition(), &Branch);
		const TArray<FTreeBranch>& Branches = Tree->GetSkeleton().Branches;
		float Angle = 0.f, Radius = 0.f;
		if (Branches.IsValidIndex(Branch))
		{
			if (Branch != Orbit.Branch)
			{
				Orbit.Branch = Branch;
				Orbit.Ref = FVector::ZeroVector;
				OrbitAngle(Branches[Branch], M->GetSurfacePosition(), Orbit.Ref, Angle, Radius);
				Orbit.LastAngle = Angle;
			}
			else if (OrbitAngle(Branches[Branch], M->GetSurfacePosition(), Orbit.Ref, Angle, Radius))
			{
				const float Step = FMath::FindDeltaAngleDegrees(Orbit.LastAngle, Angle);
				Orbit.Total += Step;
				Orbit.SinceLog += Step;
				Orbit.LastAngle = Angle;
			}
			Orbit.Radius = Radius;
		}
		if (Now >= Orbit.NextLog)
		{
			Orbit.NextLog = Now + 0.5;
			++Orbit.Samples;
			// Stalled: under 15 cm of way round the branch in the half-second (about 15 % of walking speed).
			const float Arc = FMath::DegreesToRadians(FMath::Abs(Orbit.SinceLog)) * Radius;
			if (Arc < 15.f) ++Orbit.Stalls;
			UE_LOG(LogLeafline, Log, TEXT("Orbit %.1f s: branch %d (level %d, radius %.0f cm), angle %.0f, %+.0f deg (%.0f cm) in 0.5 s, total %+.0f, state %s"),
				Now - Orbit.Start, Branch, Branches.IsValidIndex(Branch) ? Branches[Branch].Level : -9, Radius, Angle, Orbit.SinceLog, Arc, Orbit.Total,
				*UEnum::GetValueAsString(M->GetState()));
			Orbit.SinceLog = 0.f;
		}
		if (Now - Orbit.Start >= Orbit.Duration)
		{
			LeafDebug::AutoStrafe = 0.f;
			UE_LOG(LogLeafline, Log, TEXT("Orbit done: %.0f s, last radius %.0f cm, total %+.0f deg (%.1f laps), %d of %d half-seconds with under 15 cm round"),
				Orbit.Duration, Orbit.Radius, Orbit.Total, Orbit.Total / 360.f, Orbit.Stalls, Orbit.Samples);
			Orbit.Handle.Reset();
			return false;
		}
		return true;
	}

	void StartOrbit(UWorld* World, const TArray<FString>& Args)
	{
		AAntPawn* Ant = PlayerAnt(World);
		ATreeActor* Tree = ATreeActor::Get(World);
		if (!Ant || !Tree || !Ant->GetAntMovement()) return;
		if (Orbit.Handle.IsValid()) FTSTicker::GetCoreTicker().RemoveTicker(Orbit.Handle);
		if (Args.Num() > 1)
		{
			// The limb or bough point whose radius is nearest the one asked for, away from the ends.
			const float Want = FCString::Atof(*Args[1]);
			const TArray<FTreeBranch>& Branches = Tree->GetSkeleton().Branches;
			int32 BestB = INDEX_NONE;
			float BestT = 0.5f, BestErr = TNumericLimits<float>::Max();
			const int32 Skip = Args.Num() > 2 ? FCString::Atoi(*Args[2]) : 0; // the n-th best spot, for another limb
			TArray<TPair<float, TPair<int32, float>>> Spots;
			for (int32 b = 0; b < Branches.Num(); ++b)
			{
				const FTreeBranch& B = Branches[b];
				if (B.Level < 1 || B.Samples.Num() < 8 || B.Length() <= 0.f) continue;
				float BErr = TNumericLimits<float>::Max();
				float BT = 0.5f;
				for (int32 i = B.Samples.Num() / 4; i < B.Samples.Num() * 3 / 4; ++i)
				{
					// Only where the bark round this sample belongs to this branch (not inside the trunk's blend).
					const FVector Out = B.Samples[i] + FVector::UpVector * (B.SampleRadius[i] + 5.0);
					int32 Nearest = INDEX_NONE;
					Tree->GetField().Distance(Out, &Nearest);
					if (Nearest != b) continue;
					const float Err = FMath::Abs(B.SampleRadius[i] - Want);
					if (Err < BErr) { BErr = Err; BT = B.SampleDist[i] / B.Length(); }
				}
				if (BErr < TNumericLimits<float>::Max()) Spots.Add(TPair<float, TPair<int32, float>>(BErr, TPair<int32, float>(b, BT)));
			}
			Spots.Sort([](const auto& A, const auto& B) { return A.Key < B.Key; });
			if (Spots.Num() > 0)
			{
				const auto& Pick = Spots[FMath::Clamp(Skip, 0, Spots.Num() - 1)];
				BestErr = Pick.Key;
				BestB = Pick.Value.Key;
				BestT = Pick.Value.Value;
			}
			if (BestB != INDEX_NONE && Ant->GetAntMovement()->PlaceOnBranch(BestB, BestT))
			{
				SnapCamera(World);
				UE_LOG(LogLeafline, Log, TEXT("Leaf.Orbit: on branch %d (level %d) at t %.2f, %.0f cm from the %.0f cm asked for"), BestB, Branches[BestB].Level, BestT, BestErr, Want);
			}
		}
		Orbit = FOrbitState();
		Orbit.World = World;
		Orbit.Start = World->GetRealTimeSeconds();
		Orbit.NextLog = Orbit.Start + 0.5;
		Orbit.Duration = Args.Num() > 0 ? FMath::Max(FCString::Atof(*Args[0]), 1.f) : 15.0;
		LeafDebug::AutoStrafe = 0.f;
		Orbit.StrafeAt = Orbit.Start + 0.6;
		Orbit.Handle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&OrbitTick), 0.f);
	}

	FAutoConsoleCommandWithWorldAndArgs CmdInputViz(
		TEXT("Leaf.InputViz"),
		TEXT("Leaf.InputViz [0|1]: show where W/A/S/D and the stick would take the ant, the actual movement and the rule in charge (also F3 in dev builds); no argument toggles."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld*)
		{
			LeafDebug::bInputViz = Args.Num() > 0 ? FCString::Atoi(*Args[0]) != 0 : !LeafDebug::bInputViz;
			UE_LOG(LogLeafline, Log, TEXT("Input view %s"), LeafDebug::bInputViz ? TEXT("on") : TEXT("off"));
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdNoWorld(
		TEXT("Leaf.NoWorld"),
		TEXT("Leaf.NoWorld [0|1]: stop drawing the 3D world so the HUD draws over black (pixel diffs of the HUD between builds); no argument toggles."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (!World) return;
			const bool bOff = Args.Num() > 0 ? FCString::Atoi(*Args[0]) != 0 : UGameplayStatics::GetEnableWorldRendering(World);
			UGameplayStatics::SetEnableWorldRendering(World, !bOff);
			UE_LOG(LogLeafline, Log, TEXT("Leaf.NoWorld %d"), bOff ? 1 : 0);
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdPadIcons(
		TEXT("Leaf.PadIcons"),
		TEXT("Leaf.PadIcons [-1|0|1]: prompt icons follow the device in use (-1, default), always keys and mouse (0) or always pad (1)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld*)
		{
			LeafDebug::ForcePadIcons = Args.Num() > 0 ? FMath::Clamp(FCString::Atoi(*Args[0]), -1, 1) : -1;
			UE_LOG(LogLeafline, Log, TEXT("Leaf.PadIcons %d"), LeafDebug::ForcePadIcons);
		}));

	FAutoConsoleCommandWithWorldAndArgs CmdOrbit(
		TEXT("Leaf.Orbit"),
		TEXT("Leaf.Orbit [seconds] [radius cm] [n]: hold screen-right and log the turn round the branch every 0.5 s (with a radius: first go to the limb point of about that radius on the n-th best limb, 0 first)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World) { StartOrbit(World, Args); }));
}
