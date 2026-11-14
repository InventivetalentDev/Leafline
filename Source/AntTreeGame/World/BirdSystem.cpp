#include "World/BirdSystem.h"
#include "AntTreeGame.h"
#include "Ant/AntPawn.h"
#include "Ant/AntMovementComponent.h"
#include "Tree/TreeActor.h"
#include "World/WindSystem.h"
#include "World/DayDirector.h"
#include "World/ExposureBands.h"
#include "Colony/LineSystem.h"
#include "Game/Upgrades.h"
#include "Audio/LeafSound.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "EngineUtils.h"
#include "Misc/Paths.h"
#include "Engine/World.h"

namespace
{
	enum EVisMode : uint8 { VisHidden, VisCircle, VisDive, VisExit, VisLeave };

	/** Display only: the day the hint showed this game session (reset in Init); never read by the schedule. */
	int32 GHintSessionDay = -1;

	/** Laden, and in a state a gust can blow off (AAntPawn::UpdateWind's check). */
	bool CanBeKnocked(const AAntPawn* A)
	{
		if (!A || A->IsVanishing() || !A->IsCarrying()) return false;
		const UAntMovementComponent* M = A->GetAntMovement();
		if (!M) return false;
		const EAntState S = M->GetState();
		return S == EAntState::Walking || S == EAntState::Riding || S == EAntState::Returning || S == EAntState::Cutting ||
			S == EAntState::Gripping;
	}

	FVector Bezier(const FVector& A, const FVector& B, const FVector& C, const FVector& D, float T)
	{
		const float U = 1.f - T;
		return A * (U * U * U) + B * (3.f * U * U * T) + C * (3.f * U * T * T) + D * (T * T * T);
	}

	bool SoundExists(const TCHAR* Name)
	{
		return FPaths::FileExists(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Leafline/AudioRaw"), FString(Name) + TEXT(".wavraw")));
	}
}

ABirdSystem::ABirdSystem()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	BirdMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Bird"));
	BirdMesh->SetupAttachment(RootComponent);
	BirdMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BirdMesh->SetGenerateOverlapEvents(false);
	BirdMesh->SetCastShadow(true);
	BirdMesh->SetHiddenInGame(true);
	BirdMesh->SetAbsolute(true, true, true);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMat(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (SphereFinder.Succeeded()) SphereMesh = SphereFinder.Object;
	if (CylinderFinder.Succeeded()) CylinderMesh = CylinderFinder.Object;
	if (BasicMat.Succeeded()) BasicMaterial = BasicMat.Object;
}

ABirdSystem* ABirdSystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World) return nullptr;
	for (TActorIterator<ABirdSystem> It(const_cast<UWorld*>(World)); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

void ABirdSystem::Init(ATreeActor* InTree, AWindSystem* InWind, ADayDirector* InDirector, ALineSystem* InLines)
{
	Tree = InTree;
	Wind = InWind;
	Director = InDirector;
	Lines = InLines;
	GHintSessionDay = -1;
}

void ABirdSystem::BeginPlay()
{
	Super::BeginPlay();
	// The model (lk-nest's Blender script, imported by the main session) if it is there, else a dark stretched sphere.
	UStaticMesh* Model = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Leafline/Meshes/SM_Bird.SM_Bird"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (Model)
	{
		bPlaceholder = false;
		BirdMesh->SetStaticMesh(Model);
		if (UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Leafline/Materials/M_Bird.M_Bird"), nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			BirdMID = UMaterialInstanceDynamic::Create(Mat, this);
			BirdMesh->SetMaterial(0, BirdMID);
		}
		// The asset's bounds include an extension for the flapping wings (Z 35 cm); the clearances need the mesh itself.
		const FBox Ext = Model->GetBoundingBox();
		ModelBox = FBox(Ext.Min + Model->GetNegativeBoundsExtension(), Ext.Max - Model->GetPositiveBoundsExtension());
		BirdMesh->SetBoundsScale(1.f);
	}
	else
	{
		bPlaceholder = true;
		BirdMesh->SetStaticMesh(SphereMesh);
		if (BasicMaterial)
		{
			BirdMID = UMaterialInstanceDynamic::Create(BasicMaterial, this);
			BirdMID->SetVectorParameterValue(TEXT("Color"), Tuning.PlaceholderColor);
			BirdMesh->SetMaterial(0, BirdMID);
		}
	}
	bHasCallSound = SoundExists(TEXT("bird_call"));
	bHasPassSound = SoundExists(TEXT("bird_pass"));
	UE_LOG(LogLeafline, Log, TEXT("Bird: %s (box %s), sounds: call %s, pass %s"), bPlaceholder ? TEXT("placeholder shape (no SM_Bird)") : TEXT("SM_Bird"),
		*ModelBox.ToString(),
		bHasCallSound ? TEXT("bird_call") : TEXT("gust_warning (no bird_call yet)"), bHasPassSound ? TEXT("bird_pass") : TEXT("shift_whoosh (no bird_pass yet)"));
}

// ---------------------------------------------------------------------------------------------------------------------
// Schedule (sim)

bool ABirdSystem::InShift() const
{
	return Director && Director->GetPhase() == EDayPhase::Shift;
}

FString ABirdSystem::Describe(const AAntPawn* A) const
{
	if (!A) return TEXT("(gone)");
	if (A->IsRecruit())
	{
		for (int32 i = 0; Lines && i < Lines->GetPoolSize(); ++i)
		{
			if (Lines->GetRecruitPawn(i) == A) return FString::Printf(TEXT("recruit %d"), i);
		}
		return TEXT("a recruit");
	}
	return FString::Printf(TEXT("worker %d%s"), A->GetWorkerIndex(), A->IsPlayerControlled() ? TEXT(" (yours)") : TEXT(""));
}

void ABirdSystem::StartDay(int32 InDay, int32 InSeed, bool bInRecruitsOn, int32 InPool)
{
	if (bActiveToday && !bDaySummaryLogged) GoOff(TEXT("a new day starts"));
	ClearBaits();
	Day = InDay;
	Seed = InSeed;
	bRecruitsOn = bInRecruitsOn;
	Pool = InPool;
	State = EBirdState::Off;
	StateTime = 0.0;
	Clock = 0.0;
	Target = nullptr;
	TargetBait = INDEX_NONE;
	Swoops = PiecesTaken = RecruitPiecesTaken = Held = Missed = BaitsTaken = 0;
	bDaySummaryLogged = false;
	// Its own stream: the wind's gusts come out the same with the bird on and off.
	Rand.Initialize((int32)(HashCombine(GetTypeHash(Seed), GetTypeHash(Day * 7919 + 4111)) & 0x7fffffff));

	// The bird comes once the colony has recruits (with recruits off: from FirstBirdDay). The gentle first bird day follows
	// from the day and the pool alone (nothing is saved, and no session history counts): with recruits off it is
	// FirstBirdDay, with recruits on every day the pool is still the first Brood level's (or smaller, with Leaf.Pool).
	const bool bEligible = bRecruitsOn ? Pool > 0 : Day >= Tuning.FirstBirdDay;
	const int32 FirstPool = FMath::Max(FMath::RoundToInt(LeaflineUpgrades::GetEffect(static_cast<int32>(ELeaflineUpgrade::Brood), 1)), 1);
	bFirstBirdDay = bEligible && (bRecruitsOn ? Pool <= FirstPool : Day == Tuning.FirstBirdDay);
	SwoopsLeft = !bEligible ? 0 : bFirstBirdDay ? FMath::Max(Tuning.FirstDaySwoops, 0) : FMath::Max(Tuning.MaxSwoopsPerDay, 0);
	bActiveToday = Tuning.bBird && bEligible && SwoopsLeft > 0;
	if (bActiveToday && GHintSessionDay < 0) GHintSessionDay = Day;
	bHintDay = bActiveToday && GHintSessionDay == Day;
	bDiveRequested = false;
	DropRequest = nullptr;
	if (bActiveToday)
	{
		UE_LOG(LogLeafline, Log, TEXT("Bird: day %d, the bird hunts today (%d swoops%s; recruits %s, pool %d)"), Day, SwoopsLeft,
			bFirstBirdDay ? TEXT(", first bird day") : TEXT(""), bRecruitsOn ? TEXT("on") : TEXT("off"), Pool);
		EnterCircling(false);
	}
	else
	{
		UE_LOG(LogLeafline, Log, TEXT("Bird: day %d, no bird (%s)"), Day, !Tuning.bBird ? TEXT("Leaf.Bird 0")
			: !bEligible ? (bRecruitsOn ? TEXT("no recruits yet") : TEXT("recruits off, before FirstBirdDay")) : TEXT("no swoops today: MaxSwoopsPerDay or FirstDaySwoops is 0"));
	}
}

void ABirdSystem::EnterCircling(bool bFromAway)
{
	if (SwoopsLeft <= 0)
	{
		// The day's swoops are used up: it stays away (also after Leaf.Bird 0 and 1).
		State = EBirdState::Away;
		StateTime = 0.0;
		AwayFor = 1e9;
		UE_LOG(LogLeafline, Log, TEXT("Bird: no swoops left today; it stays away"));
		return;
	}
	State = EBirdState::Circling;
	StateTime = 0.0;
	bLoggedNoTarget = false;
	const float J = FMath::Clamp(Tuning.SwoopJitter, 0.f, 0.9f);
	NextLook = FMath::Max(Tuning.SwoopInterval, 1.f) * Rand.FRandRange(1.f - J, 1.f + J);
	UE_LOG(LogLeafline, Log, TEXT("Bird: circling%s, looks for a target in %.1f s (%d swoops left today)"),
		bFromAway ? TEXT(" again") : TEXT(""), NextLook, SwoopsLeft);
}

void ABirdSystem::Advance(float Dt)
{
	if (!Director) return;
	const EDayPhase Ph = Director->GetPhase();
	// Leaf.BirdNow and the drop button are taken here, at the start of a fixed step, never between steps.
	if (bDiveRequested || DropRequest.IsValid()) ConsumeRequests(Ph == EDayPhase::Shift);
	if (!bActiveToday || State == EBirdState::Off) return;
	if (Ph != EDayPhase::Shift)
	{
		// Before the day's first shift (title, move card) it waits; once the shifts are over it leaves for the day.
		if (Ph == EDayPhase::Starting || Ph == EDayPhase::Title || Ph == EDayPhase::Move) return;
		GoOff(State == EBirdState::Diving ? TEXT("the shift ended during the dive: cancelled, the bird flies off") : TEXT("the shift is over"));
		return;
	}
	Clock += Dt;
	StateTime += Dt;
	switch (State)
	{
	case EBirdState::Circling:
		if (SwoopsLeft <= 0)
		{
			EnterCircling(true); // stays away
		}
		else if (StateTime + 1e-6 >= NextLook)
		{
			AAntPawn* Ant = nullptr;
			int32 Bait = INDEX_NONE, Laden = 0;
			float Exposure = 0.f;
			if (ChooseTarget(Ant, Bait, Exposure, Laden))
			{
				StartDive(Ant, Bait);
			}
			else
			{
				if (!bLoggedNoTarget)
				{
					UE_LOG(LogLeafline, Log, TEXT("Bird: no target (%d laden ants, none on exposed bark with a clear way in); looks again every %.0f s"), Laden, Tuning.LookAgain);
					bLoggedNoTarget = true;
				}
				NextLook = StateTime + FMath::Max(Tuning.LookAgain, 0.5f);
			}
		}
		break;
	case EBirdState::Diving:
		// It follows its target as it moves.
		if (TargetBait == INDEX_NONE)
		{
			if (const AAntPawn* A = Target.Get())
			{
				if (const UAntMovementComponent* M = A->GetAntMovement())
				{
					TargetLastPos = M->GetSurfacePosition();
					TargetLastNormal = M->GetSurfaceNormal();
				}
			}
		}
		// The way in is checked again as the target moves (20 times a second): wood in it, and the bird breaks off.
		if ((DiveSteps++ % 6) == 5 && !IsCorridorClear(PassPoint(), ApproachDir))
		{
			BreakOff();
			break;
		}
		if (StateTime + 1e-6 >= Tuning.WarnSeconds) Pass();
		break;
	case EBirdState::Away:
		if (SwoopsLeft > 0 && StateTime + 1e-6 >= AwayFor) EnterCircling(true);
		break;
	default:
		break;
	}
}

void ABirdSystem::GatherLaden(TArray<AAntPawn*>& Out) const
{
	Out.Reset();
	if (Director)
	{
		for (int32 i = 0; i < Director->NumWorkers(); ++i)
		{
			AAntPawn* A = Director->GetWorkerPawn(i);
			if (CanBeKnocked(A)) Out.Add(A);
		}
	}
	if (Lines)
	{
		for (int32 i = 0; i < Lines->GetPoolSize(); ++i)
		{
			AAntPawn* A = Lines->GetRecruitPawn(i);
			if (CanBeKnocked(A)) Out.Add(A);
		}
	}
}

bool ABirdSystem::ChooseTarget(AAntPawn*& OutAnt, int32& OutBait, float& OutExposure, int32& OutLaden) const
{
	OutAnt = nullptr;
	OutBait = INDEX_NONE;
	OutExposure = 0.f;
	OutLaden = 0;
	if (!Wind) return false;
	// Candidates on exposed bark, most exposed first (ties keep the fixed order); the first one the bird has a clear way
	// in to is the target. A piece lying on exposed bark beats every ant.
	struct FCand { AAntPawn* Ant; int32 Bait; float E; int32 Order; FVector Pos; FVector Normal; };
	auto Pick = [this, &OutAnt, &OutBait, &OutExposure](TArray<FCand>& Cands)
	{
		Cands.StableSort([](const FCand& A, const FCand& B) { return A.E > B.E; });
		for (const FCand& C : Cands)
		{
			FVector Dir;
			if (FindApproach(C.Pos, C.Normal, PassGapAt(C.Pos, C.Bait != INDEX_NONE), Dir))
			{
				OutAnt = C.Ant;
				OutBait = C.Bait;
				OutExposure = C.E;
				return true;
			}
		}
		return false;
	};
	if (IsBaitOn())
	{
		TArray<FCand> BaitCands;
		for (int32 i = 0; i < Baits.Num(); ++i)
		{
			const float E = Wind->GetExposure(Baits[i].Pos, Baits[i].Normal);
			if (LeafExposure::IsExposed(E)) BaitCands.Add({ nullptr, i, E, i, Baits[i].Pos, Baits[i].Normal });
		}
		if (Pick(BaitCands)) return true;
	}
	TArray<AAntPawn*> Laden;
	GatherLaden(Laden);
	OutLaden = Laden.Num();
	TArray<FCand> AntCands;
	for (int32 i = 0; i < Laden.Num(); ++i)
	{
		const UAntMovementComponent* M = Laden[i]->GetAntMovement();
		const float E = Wind->GetExposure(M->GetSurfacePosition(), M->GetSurfaceNormal());
		if (LeafExposure::IsExposed(E)) AntCands.Add({ Laden[i], INDEX_NONE, E, i, M->GetSurfacePosition(), M->GetSurfaceNormal() });
	}
	return Pick(AntCands);
}

void ABirdSystem::StartDive(AAntPawn* Ant, int32 BaitIndex)
{
	State = EBirdState::Diving;
	StateTime = 0.0;
	Target = Ant;
	TargetBait = BaitIndex;
	float Exposure = 0.f;
	if (Baits.IsValidIndex(BaitIndex))
	{
		TargetLastPos = Baits[BaitIndex].Pos;
		TargetLastNormal = Baits[BaitIndex].Normal;
		Exposure = Wind ? Wind->GetExposure(TargetLastPos, TargetLastNormal) : 0.f;
		UE_LOG(LogLeafline, Log, TEXT("Bird: dives at the dropped piece at %s (exposure %.2f); pass in %.1f s"),
			*TargetLastPos.ToCompactString(), Exposure, Tuning.WarnSeconds);
	}
	else if (Ant && Ant->GetAntMovement())
	{
		TargetLastPos = Ant->GetAntMovement()->GetSurfacePosition();
		TargetLastNormal = Ant->GetAntMovement()->GetSurfaceNormal();
		Exposure = Wind ? Wind->GetExposure(TargetLastPos, TargetLastNormal) : 0.f;
		UE_LOG(LogLeafline, Log, TEXT("Bird: dives at %s (exposure %.2f, piece %.1f, %s) at %s; pass in %.1f s"),
			*Describe(Ant), Exposure, Ant->GetCarriedMass(), *UEnum::GetValueAsString(Ant->GetAntMovement()->GetState()),
			*TargetLastPos.ToCompactString(), Tuning.WarnSeconds);
	}
	DiveSteps = 0;
	ChooseApproach();
	++DiveSerial;
}

FVector ABirdSystem::PassPoint() const
{
	FVector N;
	const FVector T = TargetSurfacePos(&N);
	return T + N * (DiveGap + BirdBelow());
}

bool ABirdSystem::IsCorridorClear(const FVector& Pp, const FVector& Dir) const
{
	if (!Tree || !Tree->GetField().IsValid()) return true;
	double Hit = 1.0;
	if (Tree->GetField().March(Pp + Dir * 60.f, Pp + Dir * 600.f, 70.0, Hit)) return false;
	return !Tree->GetField().March(Pp + Dir * 600.f, Pp + Dir * Tuning.ApproachDistance, FMath::Min(BirdHalfSpan(), 250.f), Hit);
}

bool ABirdSystem::FindApproach(const FVector& T, const FVector& N, float Gap, FVector& OutDir) const
{
	// Come in from upwind along a corridor clear of wood (exposure means open toward the wind): between the bark's
	// normal and upwind first, then upwind, the normal, and those tilted up.
	const FVector W = Wind ? Wind->GetDirection().GetSafeNormal() : FVector::ForwardVector;
	const FVector Pp = T + N * (Gap + BirdBelow());
	const FVector Cands[] = {
		(N - W).GetSafeNormal(), (-W).GetSafeNormal2D(), N, (N - W + FVector(0, 0, 0.7)).GetSafeNormal(), (N + FVector(0, 0, 0.8)).GetSafeNormal(), FVector::UpVector
	};
	OutDir = N;
	for (const FVector& D : Cands)
	{
		if (D.IsNearlyZero() || FVector::DotProduct(D, N) < 0.15f) continue;
		if (IsCorridorClear(Pp, D))
		{
			OutDir = D;
			return true;
		}
	}
	return false;
}

void ABirdSystem::ChooseApproach()
{
	FVector N;
	const FVector T = TargetSurfacePos(&N);
	DiveGap = PassGapAt(T, TargetBait != INDEX_NONE);
	if (!FindApproach(T, N, DiveGap, ApproachDir)) UE_LOG(LogLeafline, Log, TEXT("Bird: no clear way in to its target"));
}

void ABirdSystem::BreakOff()
{
	++Swoops;
	++Missed;
	SwoopsLeft = FMath::Max(SwoopsLeft - 1, 0);
	UE_LOG(LogLeafline, Log, TEXT("Bird: pass %d breaks off: wood in its way in to %s (a miss)"), Swoops,
		TargetBait != INDEX_NONE ? TEXT("the dropped piece") : *Describe(Target.Get()));
	State = EBirdState::Away;
	StateTime = 0.0;
	AwayFor = Tuning.AwaySeconds;
	Target = nullptr;
	TargetBait = INDEX_NONE;
	++BreakSerial;
	if (SwoopsLeft <= 0) UE_LOG(LogLeafline, Log, TEXT("Bird: that was its last swoop today; it does not come back"));
}

static const TCHAR* OutcomeName(EBirdOutcome O)
{
	switch (O)
	{
	case EBirdOutcome::Knocked: return TEXT("knocked off, piece lost");
	case EBirdOutcome::Held: return TEXT("gripping: holds, keeps its piece");
	case EBirdOutcome::Missed: return TEXT("missed: not on exposed bark any more");
	case EBirdOutcome::Gone: return TEXT("missed: no longer carrying a piece on the bark");
	}
	return TEXT("?");
}

EBirdOutcome ABirdSystem::TestAnt(AAntPawn* Ant, float& OutExposure)
{
	OutExposure = 0.f;
	if (!CanBeKnocked(Ant) || !Wind) return EBirdOutcome::Gone;
	const UAntMovementComponent* M = Ant->GetAntMovement();
	OutExposure = Wind->GetExposure(M->GetSurfacePosition(), M->GetSurfaceNormal());
	if (!LeafExposure::IsExposed(OutExposure)) return EBirdOutcome::Missed;
	if (Ant->IsGripping()) return EBirdOutcome::Held;
	const bool bRecruit = Ant->IsRecruit();
	// The bird arrives along the wind, so the existing fall and the fluttering piece already point the right way.
	Ant->BlowOff(0.f, 0.f, OutExposure, EBlowOffCause::Bird);
	++PiecesTaken;
	if (bRecruit) ++RecruitPiecesTaken;
	return EBirdOutcome::Knocked;
}

void ABirdSystem::Pass()
{
	++Swoops;
	SwoopsLeft = FMath::Max(SwoopsLeft - 1, 0);
	if (TargetBait != INDEX_NONE)
	{
		if (Baits.IsValidIndex(TargetBait))
		{
			UE_LOG(LogLeafline, Log, TEXT("Bird: pass %d takes the dropped piece at %s and stays away %.0f s"),
				Swoops, *Baits[TargetBait].Pos.ToCompactString(), Tuning.BaitAwaySeconds);
			if (UStaticMeshComponent* C = Baits[TargetBait].Mesh.Get()) C->DestroyComponent();
			Baits.RemoveAt(TargetBait);
			++BaitsTaken;
		}
		AwayFor = Tuning.BaitAwaySeconds;
	}
	else
	{
		AwayFor = Tuning.AwaySeconds;
		AAntPawn* T = Target.Get();
		if (T && T->GetAntMovement())
		{
			TargetLastPos = T->GetAntMovement()->GetSurfacePosition();
			TargetLastNormal = T->GetAntMovement()->GetSurfaceNormal();
		}
		const FVector Where = TargetLastPos;
		int32 Knocked = 0;
		float E = 0.f;
		EBirdOutcome O = TestAnt(T, E);
		UE_LOG(LogLeafline, Log, TEXT("Bird: pass %d at %s: target %s, exposure %.2f: %s"), Swoops, *Where.ToCompactString(), *Describe(T), E, OutcomeName(O));
		if (O == EBirdOutcome::Knocked) ++Knocked;
		else if (O == EBirdOutcome::Held) ++Held;
		else ++Missed;
		// Its wing gust reaches other laden ants close by.
		TArray<AAntPawn*> Laden;
		GatherLaden(Laden);
		for (AAntPawn* A : Laden)
		{
			if (Knocked >= FMath::Max(Tuning.MaxHitsPerSwoop, 1)) break;
			if (A == T || FVector::Dist(A->GetAntMovement()->GetSurfacePosition(), Where) > Tuning.HitRadius) continue;
			O = TestAnt(A, E);
			UE_LOG(LogLeafline, Log, TEXT("Bird: pass %d also reaches %s, exposure %.2f: %s"), Swoops, *Describe(A), E, OutcomeName(O));
			if (O == EBirdOutcome::Knocked) ++Knocked;
			else if (O == EBirdOutcome::Held) ++Held;
		}
	}
	State = EBirdState::Away;
	StateTime = 0.0;
	Target = nullptr;
	TargetBait = INDEX_NONE;
	++PassSerial;
	if (SwoopsLeft <= 0)
	{
		UE_LOG(LogLeafline, Log, TEXT("Bird: that was its last swoop today; it does not come back"));
	}
	else
	{
		UE_LOG(LogLeafline, Log, TEXT("Bird: away for %.0f s"), AwayFor);
	}
}

void ABirdSystem::GoOff(const TCHAR* Why)
{
	if (State == EBirdState::Diving)
	{
		UE_LOG(LogLeafline, Log, TEXT("Bird: dive at %s cancelled (%s)"),
			TargetBait != INDEX_NONE ? TEXT("the dropped piece") : *Describe(Target.Get()), Why);
	}
	else if (State != EBirdState::Off)
	{
		UE_LOG(LogLeafline, Log, TEXT("Bird: leaves (%s)"), Why);
	}
	State = EBirdState::Off;
	Target = nullptr;
	TargetBait = INDEX_NONE;
	if (bActiveToday && !bDaySummaryLogged)
	{
		bDaySummaryLogged = true;
		UE_LOG(LogLeafline, Log, TEXT("Bird: day %d summary: %d swoops, %d pieces knocked off (%d of them recruits'), %d held by gripping, %d missed, %d dropped pieces taken"),
			Day, Swoops, PiecesTaken, RecruitPiecesTaken, Held, Missed, BaitsTaken);
	}
}

bool ABirdSystem::IsDivingAt(const AAntPawn* Ant) const
{
	return IsDiving() && Ant && TargetBait == INDEX_NONE && Target.Get() == Ant;
}

void ABirdSystem::SetBirdOn(bool bOn)
{
	Tuning.bBird = bOn;
	if (!bOn)
	{
		if (State != EBirdState::Off) GoOff(TEXT("Leaf.Bird 0"));
		bActiveToday = false;
		ClearBaits();
		UE_LOG(LogLeafline, Log, TEXT("Leaf.Bird 0: no bird"));
		return;
	}
	const bool bEligible = bRecruitsOn ? Pool > 0 : Day >= Tuning.FirstBirdDay;
	const EDayPhase Ph = Director ? Director->GetPhase() : EDayPhase::Starting;
	const bool bBeforeShiftEnd = Ph == EDayPhase::Shift || Ph == EDayPhase::Title || Ph == EDayPhase::Starting || Ph == EDayPhase::Move;
	// The day's remaining swoops are kept across Leaf.Bird 0 and 1: a toggle never buys more.
	if (bEligible && SwoopsLeft > 0 && !bActiveToday && bBeforeShiftEnd)
	{
		bActiveToday = true;
		bDaySummaryLogged = false;
		EnterCircling(false);
	}
	UE_LOG(LogLeafline, Log, TEXT("Leaf.Bird 1: %s"), bActiveToday ? TEXT("the bird hunts today")
		: bEligible && SwoopsLeft <= 0 ? TEXT("no swoops left today") : TEXT("from the next bird day"));
}

void ABirdSystem::SetBaitOn(bool bOn)
{
	Tuning.bBait = bOn;
	if (!bOn) ClearBaits();
	UE_LOG(LogLeafline, Log, TEXT("Leaf.Bait %d"), bOn ? 1 : 0);
}

void ABirdSystem::DiveNow()
{
	bDiveRequested = true;
}

void ABirdSystem::ConsumeRequests(bool bShift)
{
	if (AAntPawn* Ant = DropRequest.Get())
	{
		DropRequest = nullptr;
		if (bShift) DoDrop(Ant);
	}
	DropRequest = nullptr;
	if (bDiveRequested)
	{
		bDiveRequested = false;
		if (bShift) DoDiveNow();
		else UE_LOG(LogLeafline, Warning, TEXT("Leaf.BirdNow: only during a shift"));
	}
}

void ABirdSystem::DoDiveNow()
{
	if (!Tuning.bBird)
	{
		UE_LOG(LogLeafline, Warning, TEXT("Leaf.BirdNow: the bird is off (Leaf.Bird 1)"));
		return;
	}
	if (State == EBirdState::Diving)
	{
		UE_LOG(LogLeafline, Log, TEXT("Leaf.BirdNow: already diving"));
		return;
	}
	AAntPawn* Ant = nullptr;
	int32 Bait = INDEX_NONE, Laden = 0;
	float Exposure = 0.f;
	if (!ChooseTarget(Ant, Bait, Exposure, Laden))
	{
		UE_LOG(LogLeafline, Log, TEXT("Leaf.BirdNow: no target (%d laden ants, none on exposed bark with a clear way in); the bird does not dive"), Laden);
		return;
	}
	if (!bActiveToday)
	{
		bActiveToday = true;
		bDaySummaryLogged = false;
	}
	UE_LOG(LogLeafline, Log, TEXT("Leaf.BirdNow (at sim %.3f)"), Lines ? Lines->GetSimTime() : 0.0);
	StartDive(Ant, Bait);
}

void ABirdSystem::LogInfo() const
{
	UE_LOG(LogLeafline, Log, TEXT("Bird: %s, day %d %s%s, state %s for %.1f s (next look at %.1f), %d swoops left, today %d swoops, %d knocked off, %d held, %d missed, %d of %d dropped pieces taken, bait %s, %s, visible %d at %s"),
		Tuning.bBird ? TEXT("on") : TEXT("off"), Day, bActiveToday ? TEXT("active") : TEXT("inactive"), bFirstBirdDay ? TEXT(" (first bird day)") : TEXT(""),
		*UEnum::GetValueAsString(State), StateTime, NextLook, SwoopsLeft, Swoops, PiecesTaken, Held, Missed, BaitsTaken, BaitsTaken + Baits.Num(),
		IsBaitOn() ? TEXT("on") : TEXT("off"), bPlaceholder ? TEXT("placeholder") : TEXT("SM_Bird"), bVisShown ? 1 : 0, *VisPos.ToCompactString());
	UE_LOG(LogLeafline, Log, TEXT("Bird: flap amount %.2f, phase %.2f, visible mode %d"), FlapAmount, FMath::Fmod(FlapPhase, 2.f * PI), (int32)VisMode);
}

// ---------------------------------------------------------------------------------------------------------------------
// Bait (tier 2)

bool ABirdSystem::CanDrop(const AAntPawn* Ant) const
{
	if (!IsBaitOn() || !IsPresent() || !InShift() || !Ant || !Ant->IsCarrying() || Ant->IsVanishing() || Ant->IsRecruit()) return false;
	const UAntMovementComponent* M = Ant->GetAntMovement();
	const EAntState S = M ? M->GetState() : EAntState::Falling;
	return S == EAntState::Walking || S == EAntState::Gripping || S == EAntState::Riding;
}

bool ABirdSystem::DropPiece(AAntPawn* Ant)
{
	if (!CanDrop(Ant)) return false;
	DropRequest = Ant;
	return true;
}

void ABirdSystem::DoDrop(AAntPawn* Ant)
{
	if (!CanDrop(Ant))
	{
		UE_LOG(LogLeafline, Log, TEXT("Bird bait: the drop is no longer possible at this step"));
		return;
	}
	const UAntMovementComponent* M = Ant->GetAntMovement();
	FBirdBait B;
	B.Normal = M->GetSurfaceNormal();
	B.Pos = M->GetSurfacePosition();
	B.Diameter = Ant->DiameterForSize(Ant->GetCarriedSize());
	B.Exposure = Wind ? Wind->GetExposure(B.Pos, B.Normal) : 0.f;
	if (CylinderMesh)
	{
		// The piece lying flat on the bark: a thin green disc.
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
		C->SetStaticMesh(CylinderMesh);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetGenerateOverlapEvents(false);
		C->SetAbsolute(true, true, true);
		C->SetupAttachment(RootComponent);
		C->RegisterComponent();
		const float S = B.Diameter / 100.f;
		C->SetWorldLocationAndRotation(B.Pos + B.Normal * 2.5f, FRotationMatrix::MakeFromZ(B.Normal).ToQuat());
		C->SetWorldScale3D(FVector(S, S * 0.85f, 0.03f));
		if (BasicMaterial)
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BasicMaterial, C);
			MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.12f, 0.36f, 0.06f));
			C->SetMaterial(0, MID);
		}
		B.Mesh = C;
	}
	UE_LOG(LogLeafline, Log, TEXT("Bird bait: %s drops its piece (%.1f) at %s, exposure %.2f (%s)"), *Describe(Ant), Ant->GetCarriedMass(),
		*B.Pos.ToCompactString(), B.Exposure, LeafExposure::IsExposed(B.Exposure) ? TEXT("in the bird's view") : TEXT("sheltered: the bird does not see it"));
	Baits.Add(B);
	Ant->DropCarried();
	if (State == EBirdState::Diving && TargetBait == INDEX_NONE && LeafExposure::IsExposed(B.Exposure))
	{
		// A piece lying in the open beats every ant, also in the middle of a dive: the bird takes it and stays away.
		TargetBait = Baits.Num() - 1;
		Target = nullptr;
		TargetLastPos = B.Pos;
		TargetLastNormal = B.Normal;
		ChooseApproach();
		UE_LOG(LogLeafline, Log, TEXT("Bird: turns to the dropped piece (pass in %.1f s)"), FMath::Max(Tuning.WarnSeconds - StateTime, 0.0));
	}
}

void ABirdSystem::ClearBaits()
{
	for (FBirdBait& B : Baits)
	{
		if (UStaticMeshComponent* C = B.Mesh.Get()) C->DestroyComponent();
	}
	Baits.Reset();
	if (TargetBait != INDEX_NONE) TargetBait = INDEX_NONE;
}

// ---------------------------------------------------------------------------------------------------------------------
// The visible bird: follows the schedule, never feeds back into it.

void ABirdSystem::PlaySound(const TCHAR* Name, const TCHAR* Fallback, float Volume, const FVector& At) const
{
	if (Lines && Lines->GetSpeedFactor() > 20.f) return; // a fast-forward runs many seconds a frame
	const bool bHave = FCString::Strcmp(Name, TEXT("bird_call")) == 0 ? bHasCallSound : bHasPassSound;
	LeafSound::PlayAt(this, bHave ? Name : Fallback, At, Volume);
	const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	const FVector Ear = PC && PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraLocation() : At;
	UE_LOG(LogLeafline, Log, TEXT("Bird: sound %s at the bird, %.0f m from the camera"), bHave ? Name : Fallback, FVector::Dist(At, Ear) / 100.0);
}

float ABirdSystem::BirdHalfSpan() const
{
	// The placeholder has no wings, but keeps the room the real bird needs.
	const FVector Ext = ModelBox.GetExtent();
	return FMath::Max(Ext.X, Ext.Y) * FMath::Max(Tuning.BirdScale, 0.1f);
}

float ABirdSystem::BirdBelow() const
{
	const float S = FMath::Max(Tuning.BirdScale, 0.1f);
	return bPlaceholder ? 7.f * S : FMath::Max(-ModelBox.Min.Z, 1.f) * S;
}

float ABirdSystem::BirdTipFall() const
{
	// M_Bird at FlapAmount 1: the tips fall to 30 cm below the shoulder line at the nominal size (Art/Blender/README.md).
	return 30.f * FMath::Max(Tuning.BirdScale, 0.1f);
}

float ABirdSystem::PassGapAt(const FVector& At, bool bBait) const
{
	// Never closer than PassHeight to the bark, and always clear of the tallest piece carried near the target.
	float Gap = Tuning.PassHeight;
	if (!bBait)
	{
		TArray<AAntPawn*> Laden;
		GatherLaden(Laden);
		for (const AAntPawn* A : Laden)
		{
			if (FVector::Dist(A->GetAntMovement()->GetSurfacePosition(), At) > 2.f * Tuning.HitRadius + 200.f) continue;
			Gap = FMath::Max(Gap, A->DiameterForSize(A->GetCarriedSize()) + 10.f + Tuning.PieceClearance);
		}
	}
	return Gap;
}

FVector ABirdSystem::TargetSurfacePos(FVector* OutNormal) const
{
	if (Baits.IsValidIndex(TargetBait))
	{
		if (OutNormal) *OutNormal = Baits[TargetBait].Normal;
		return Baits[TargetBait].Pos;
	}
	if (const AAntPawn* A = Target.Get())
	{
		if (const UAntMovementComponent* M = A->GetAntMovement())
		{
			if (OutNormal) *OutNormal = M->GetSurfaceNormal();
			return M->GetSurfacePosition();
		}
	}
	if (OutNormal) *OutNormal = TargetLastNormal;
	return TargetLastPos;
}

void ABirdSystem::ComputeCircle(const FVector& Anchor)
{
	// Upwind of the followed ant (not the trunk: from the nest that was 40 m and more away), a little above it, out until the
	// whole circle keeps clear of branches and their leaves. The grove stands 220 m and more out, far beyond.
	const FVector Base = Tree ? Tree->GetSkeleton().TrunkBase : FVector::ZeroVector;
	CircleAnchor = Anchor;
	CircleHeight = FMath::Max(Tuning.MinCircleHeight, float(Anchor.Z - Base.Z) + Tuning.CircleAbove);
	CircleR = FMath::Max(Tuning.CircleRadius, 300.f);
	const FVector Axis(Anchor.X, Anchor.Y, Base.Z + CircleHeight);
	FVector Upwind = Wind ? -Wind->GetDirection().GetSafeNormal2D() : FVector::ForwardVector;
	if (Upwind.IsNearlyZero()) Upwind = FVector::ForwardVector;
	float Dist = Tuning.CircleDistance;
	if (Tree && Tree->GetField().IsValid())
	{
		const double Need = Tuning.ClearMargin + BirdHalfSpan();
		for (int32 k = 0; k < 24; ++k)
		{
			const FVector C = Axis + Upwind * Dist;
			bool bClear = true;
			for (int32 i = 0; i < 24 && bClear; ++i)
			{
				const float A = 2.f * PI * i / 24.f;
				bClear = Tree->GetField().DistanceMT(C + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * CircleR) >= Need;
			}
			if (bClear) break;
			Dist += 500.f;
		}
	}
	CircleCenter = Axis + Upwind * Dist;
}

void ABirdSystem::SetVisible(bool bVisible)
{
	if (bVisShown == bVisible) return;
	bVisShown = bVisible;
	BirdMesh->SetHiddenInGame(!bVisible);
}

void ABirdSystem::BeginVisualDive()
{
	// The way in (ApproachDir) and the pull-up distance (DiveGap) are the schedule's (ChooseApproach): the visible bird
	// only flies them.
	FVector N;
	TargetSurfacePos(&N);
	const FVector Pp = PassPoint();
	UpdateExitTangent(N);
	if (!bVisShown || VisMode == VisHidden)
	{
		// Out of sight (away): it comes in from far upwind of its target.
		VisPos = Pp + ApproachDir * (Tuning.ApproachDistance * 2.5f) + FVector(0, 0, 900);
		VisVel = -ApproachDir * Tuning.GlideSpeed;
		VisRot = FRotationMatrix::MakeFromXZ(-ApproachDir, FVector::UpVector).ToQuat();
	}
	DiveFrom = VisPos;
	DiveFromVel = VisVel;
	DiveCtrl = DiveFrom + DiveFromVel.GetSafeNormal() * FMath::Min(FVector::Dist(DiveFrom, Pp) * 0.35f, 1500.f);
	if (Tree && Tree->GetField().IsValid())
	{
		// Sweep the turn-in (the curve out to the start of the straight run in) with the bird's half span: if it would
		// pass near wood, swing wide, out along the approach first.
		const FVector B2 = Pp + ApproachDir * Tuning.ApproachDistance;
		const float Need = BirdHalfSpan() * 0.8f;
		for (int32 i = 0; i <= 24; ++i)
		{
			const float U = i / 24.f;
			if (Tree->GetField().DistanceMT(Bezier(DiveFrom, DiveCtrl, B2, B2, U)) < Need)
			{
				DiveCtrl = Pp + ApproachDir * (Tuning.ApproachDistance * 2.f) + FVector(0, 0, 600);
				break;
			}
		}
	}
	VisMode = VisDive;
	VisModeTime = 0.f;
	SetVisible(true);
	PlaySound(TEXT("bird_call"), TEXT("gust_warning"), Tuning.CallVolume, VisPos);
}

void ABirdSystem::UpdateExitTangent(const FVector& N)
{
	// Where it leaves after the pass: upward along the surface (or up and upwind off a flat top), never on through the wood.
	const FVector W = Wind ? Wind->GetDirection().GetSafeNormal() : FVector::ForwardVector;
	const FVector Tup = FVector::UpVector - N * FVector::DotProduct(N, FVector::UpVector);
	ExitNormal = N;
	ExitTangent = Tup.SizeSquared() > 0.09 ? Tup.GetSafeNormal() : FVector::CrossProduct(N, FVector::CrossProduct(-W, N)).GetSafeNormal();
	if (ExitTangent.IsNearlyZero()) ExitTangent = FVector::CrossProduct(N, FVector::RightVector).GetSafeNormal();
}

void ABirdSystem::BeginVisualExit()
{
	// Climb away along the surface and off it; fall back to the approach corridor if that is blocked.
	ExitFrom = VisPos;
	ExitDir = (ExitTangent + ExitNormal * 0.6f).GetSafeNormal();
	if (Tree && Tree->GetField().IsValid())
	{
		double Hit = 1.0;
		if (Tree->GetField().March(ExitFrom + ExitDir * 80.f, ExitFrom + ExitDir * Tuning.ExitDistance, 70.0, Hit))
		{
			ExitDir = Tree->GetField().March(ExitFrom + ExitNormal * 80.f, ExitFrom + ExitNormal * Tuning.ExitDistance, 70.0, Hit) ? ApproachDir : ExitNormal;
		}
	}
	VisMode = VisExit;
	VisModeTime = 0.f;
	SetVisible(true);
	PlaySound(TEXT("bird_pass"), TEXT("shift_whoosh"), Tuning.PassVolume, VisPos);
}

float ABirdSystem::GetCameraNudge(const AAntPawn* Ant, float Dt, bool bPlayerLook, FVector& OutLookAt)
{
	if (NudgeDiveSerial != DiveSerial)
	{
		NudgeDiveSerial = DiveSerial; // a new dive: the player's earlier look input no longer counts
		bNudgeCancelled = false;
		NudgeAnt = IsDivingAt(Ant) ? Ant : nullptr;
	}
	// On while the dive targets this ant and until the visible pass is over (its exit); off for any other dive.
	const bool bOurDive = Ant && NudgeAnt.Get() == Ant && (IsDivingAt(Ant) || (VisMode == VisExit && State == EBirdState::Away));
	if (bPlayerLook && NudgeWeight > 0.f) bNudgeCancelled = true;
	const float Want = (bOurDive && !bNudgeCancelled && bVisShown && Tuning.bBird) ? FMath::Clamp(Tuning.NudgeMax, 0.f, 1.f) : 0.f;
	const float Rate = Want > NudgeWeight ? FMath::Max(Tuning.NudgeMax, 0.01f) / FMath::Max(Tuning.NudgeIn, 0.05f)
		: FMath::Max(Tuning.NudgeMax, 0.01f) / FMath::Max(Tuning.NudgeOut, 0.05f);
	NudgeWeight = FMath::FInterpConstantTo(NudgeWeight, Want, Dt, Rate);
	if (NudgeWeight <= 0.f || !Ant) return 0.f;
	OutLookAt = FMath::Lerp(Ant->GetActorLocation(), VisPos, FMath::Clamp(Tuning.NudgeBlend, 0.f, 1.f));
	return FMath::SmoothStep(0.f, 1.f, NudgeWeight);
}

void ABirdSystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateVisual(FMath::Min(DeltaTime, 0.1f));
}

void ABirdSystem::UpdateVisual(float Dt)
{
	if (!Tree || !Wind) return;
	if (DiveSerial != VisDiveSerial)
	{
		VisDiveSerial = DiveSerial;
		if (State == EBirdState::Diving) BeginVisualDive();
	}
	if (PassSerial != VisPassSerial)
	{
		VisPassSerial = PassSerial;
		if (VisMode == VisDive || bVisShown) BeginVisualExit();
	}
	if (BreakSerial != VisBreakSerial)
	{
		// Broke off: it pulls back out the way it came, and up.
		VisBreakSerial = BreakSerial;
		if (bVisShown)
		{
			VisMode = VisLeave;
			VisModeTime = 0.f;
			LeaveDir = (ApproachDir + FVector(0, 0, 0.8)).GetSafeNormal();
		}
	}
	VisModeTime += Dt;
	const bool bShift = InShift();
	const FVector Base = Tree->GetSkeleton().TrunkBase;

	// What the visible bird should be doing for the schedule's state.
	if (State == EBirdState::Circling && bShift)
	{
		if (VisMode == VisHidden || VisMode == VisLeave)
		{
			APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
			const APawn* Followed = PC ? PC->GetPawn() : nullptr;
			ComputeCircle(Followed ? Followed->GetActorLocation() : Base);
			const FVector Out = (CircleCenter - FVector(Base.X, Base.Y, CircleCenter.Z)).GetSafeNormal2D();
			if (VisMode == VisHidden)
			{
				// It glides in from far out on the upwind side.
				VisPos = CircleCenter + Out * (CircleR * 2.5f) + FVector(0, 0, 1200);
				VisVel = -Out * Tuning.GlideSpeed;
				VisRot = FRotationMatrix::MakeFromXZ(-Out, FVector::UpVector).ToQuat();
			}
			CircleAngle = FMath::Atan2(VisPos.Y - CircleCenter.Y, VisPos.X - CircleCenter.X);
			CircleSign = ((Day + Seed) & 1) ? 1.f : -1.f;
			VisMode = VisCircle;
			VisModeTime = 0.f;
			SetVisible(true);
		}
	}
	else if (VisMode == VisCircle || (VisMode == VisDive && State != EBirdState::Diving))
	{
		// The shift ended (or the switch went off): it flies off.
		VisMode = VisLeave;
		VisModeTime = 0.f;
		LeaveDir = ((VisPos - FVector(Base.X, Base.Y, VisPos.Z)).GetSafeNormal2D() + FVector(0, 0, 0.5)).GetSafeNormal();
	}

	FVector NewPos = VisPos;
	float FlapWant = 0.f;
	float FlareW = 0.f;
	switch (VisMode)
	{
	case VisHidden:
		SetVisible(false);
		return;
	case VisCircle:
	{
		APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
		const APawn* Followed = PC ? PC->GetPawn() : nullptr;
		const FVector Anchor = Followed ? Followed->GetActorLocation() : CircleAnchor;
		CircleCheckTimer -= Dt;
		if (CircleHeight < 0.f || FVector::Dist(Anchor, CircleAnchor) > 800.f || CircleCheckTimer <= 0.f)
		{
			ComputeCircle(Anchor);
			CircleCheckTimer = 2.f;
		}
		CircleAngle += CircleSign * Tuning.GlideSpeed / CircleR * Dt;
		const FVector Goal = CircleCenter + FVector(FMath::Cos(CircleAngle), FMath::Sin(CircleAngle), 0.f) * CircleR +
			FVector(0.f, 0.f, 150.f * FMath::Sin(VisModeTime * 0.5f));
		const FVector ToGoal = Goal - VisPos;
		const float D = ToGoal.Size();
		const FVector WantVel = D > 1.f ? ToGoal / D * FMath::Min(Tuning.GlideSpeed * 2.f, Tuning.GlideSpeed * 0.8f + D * 0.8f) : FVector::ZeroVector;
		VisVel = FMath::VInterpTo(VisVel, WantVel, Dt, 2.f);
		NewPos = VisPos + VisVel * Dt;
		// Mostly gliding, a few wing beats now and then, and while it catches up.
		FlapWant = (FMath::Sin(VisModeTime * 0.8f) > 0.65f || D > CircleR) ? 1.f : 0.f;
		break;
	}
	case VisDive:
	{
		FVector N;
		const FVector T = TargetSurfacePos(&N);
		const FVector Pp = T + N * (DiveGap + BirdBelow());
		UpdateExitTangent(N);
		const float S = FMath::Clamp(float(StateTime / FMath::Max(Tuning.WarnSeconds, 0.1f)), 0.f, 1.f);
		const FVector B2 = Pp + ApproachDir * Tuning.ApproachDistance;
		const float L = FMath::Max(FVector::Dist(DiveFrom, Pp), 1.f);
		const float M0 = FMath::Clamp(DiveFromVel.Size() * Tuning.WarnSeconds / L, 0.f, 1.5f);
		// Hermite in time: it leaves the circle at its gliding speed, speeds up, and flares to a stop at the pass.
		const float U = FMath::Clamp((3.f * S * S - 2.f * S * S * S) + M0 * (S * S * S - 2.f * S * S + S), 0.f, 1.f);
		NewPos = Bezier(DiveFrom, DiveCtrl, B2, Pp, U);
		VisVel = (NewPos - VisPos) / FMath::Max(Dt, 1e-3f);
		FlareW = FMath::SmoothStep(0.55f, 0.88f, S);
		FlapWant = (S < 0.35f || S > 0.75f) ? 1.f : 0.f;
		break;
	}
	case VisExit:
	{
		const float S = FMath::Clamp(VisModeTime / FMath::Max(Tuning.ExitSeconds, 0.1f), 0.f, 1.f);
		NewPos = ExitFrom + ExitDir * Tuning.ExitDistance * S * S;
		VisVel = ExitDir * (2.f * Tuning.ExitDistance * FMath::Max(S, 0.15f) / FMath::Max(Tuning.ExitSeconds, 0.1f));
		FlareW = 1.f - FMath::SmoothStep(0.f, 0.5f, S);
		FlapWant = 1.f;
		if (S >= 1.f)
		{
			VisMode = VisLeave;
			VisModeTime = 0.f;
			LeaveDir = (ExitDir + (ExitFrom - FVector(Base.X, Base.Y, ExitFrom.Z)).GetSafeNormal2D() * 0.6f + FVector(0, 0, 0.3)).GetSafeNormal();
		}
		break;
	}
	case VisLeave:
	{
		VisVel = FMath::VInterpTo(VisVel, LeaveDir * Tuning.GlideSpeed * 2.f, Dt, 1.5f);
		NewPos = VisPos + VisVel * Dt;
		FlapWant = 1.f;
		if (VisModeTime > Tuning.LeaveSeconds)
		{
			VisMode = VisHidden;
			SetVisible(false);
			return;
		}
		break;
	}
	default:
		break;
	}
	VisPos = NewPos;

	// Attitude: nose along the flight, banked into turns; near the pass it flares, belly to the bark and wings along it.
	const FVector Fwd = VisVel.SizeSquared() > 400.f ? VisVel.GetSafeNormal() : VisRot.GetForwardVector();
	const FVector PrevFwd = VisRot.GetForwardVector();
	const float YawRate = FMath::Atan2(FVector::CrossProduct(PrevFwd.GetSafeNormal2D(), Fwd.GetSafeNormal2D()).Z,
		FVector::DotProduct(PrevFwd.GetSafeNormal2D(), Fwd.GetSafeNormal2D())) / FMath::Max(Dt, 1e-3f);
	const float Bank = FMath::Clamp(YawRate * 25.f, -40.f, 40.f);
	FQuat Want = FRotationMatrix::MakeFromXZ(Fwd, FVector::UpVector).ToQuat() * FQuat(FVector::ForwardVector, FMath::DegreesToRadians(Bank));
	if (FlareW > 0.f)
	{
		const FQuat Flare = FRotationMatrix::MakeFromXZ(ExitTangent, ExitNormal).ToQuat();
		Want = FQuat::Slerp(Want, Flare, FlareW);
	}
	VisRot = FQuat::Slerp(VisRot, Want, 1.f - FMath::Exp(-Dt * (FlareW > 0.f ? 14.f : 5.f)));
	VisRot.Normalize();

	// Wing beats only where the wings have room: never a downstroke into the bark.
	const float Room = float(Tree->GetField().DistanceMT(VisPos));
	FlapWant = FMath::Min(FlapWant, FMath::Clamp((Room - 30.f) / FMath::Max(BirdTipFall(), 1.f), 0.f, 1.f));
	FlapAmount = FMath::FInterpTo(FlapAmount, FlapWant, Dt, 6.f);
	FlapPhase = FMath::Fmod(FlapPhase + 2.f * PI * Tuning.FlapHz * Dt, 2.f * PI * 64.f);

	// Never within CameraClearance of the camera (visual only): pushed off the bark, or sideways if the camera is the
	// farther out. Points of the bird: beak, tail, wing tips, mid wings, belly, centre.
	if (const APlayerController* CamPC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		if (CamPC->PlayerCameraManager && Tuning.CameraClearance > 0.f)
		{
			const FVector Cam = CamPC->PlayerCameraManager->GetCameraLocation();
			const float Sc = FMath::Max(Tuning.BirdScale, 0.1f);
			const FVector Local[] = { FVector(ModelBox.Max.X, 0, 0), FVector(ModelBox.Min.X, 0, 0), FVector(0, ModelBox.Max.Y, 0), FVector(0, ModelBox.Min.Y, 0),
				FVector(-5, ModelBox.Max.Y * 0.5f, 0), FVector(-5, ModelBox.Min.Y * 0.5f, 0), FVector(0, 0, ModelBox.Min.Z), FVector::ZeroVector };
			float Nearest = TNumericLimits<float>::Max();
			for (const FVector& L : Local) Nearest = FMath::Min(Nearest, float(FVector::Dist(VisPos + VisRot.RotateVector(L * Sc), Cam)));
			if (Nearest < Tuning.CameraClearance)
			{
				const FVector N = ExitNormal.GetSafeNormal();
				const FVector Away = FVector::DotProduct(Cam - VisPos, N) <= 0.0 ? N
					: FVector::VectorPlaneProject(VisPos - Cam, N).GetSafeNormal();
				VisPos += (Away.IsNearlyZero() ? N : Away) * (Tuning.CameraClearance - Nearest);
			}
		}
	}

	const float S = FMath::Max(Tuning.BirdScale, 0.1f);
	if (bPlaceholder)
	{
		// A dark sphere stretched along the flight, as long and deep as the real bird's body (62 x 20 x 14 cm nominal).
		BirdMesh->SetWorldScale3D(FVector(0.62f, 0.2f, 0.14f) * S);
	}
	else
	{
		BirdMesh->SetWorldScale3D(FVector(S));
		if (BirdMID)
		{
			BirdMID->SetScalarParameterValue(TEXT("FlapPhase"), FlapPhase);
			BirdMID->SetScalarParameterValue(TEXT("FlapAmount"), FlapAmount);
		}
	}
	// SM_Bird's origin is the middle of its shoulder line, roughly its centre of mass: the position is that point.
	BirdMesh->SetWorldLocationAndRotation(VisPos, VisRot);
	SetVisible(true);
}
