#include "Colony/LineSystem.h"
#include "AntTreeGame.h"
#include "Ant/AntPawn.h"
#include "Ant/AntMovementComponent.h"
#include "Tree/TreeActor.h"
#include "Colony/TrailNetwork.h"
#include "Colony/NestActor.h"
#include "World/WindSystem.h"
#include "World/DayDirector.h"
#include "World/BirdSystem.h"
#include "Game/LeaflineGameInstance.h"
#include "Audio/LeafSound.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"

namespace
{
	const TCHAR* StateName(ERecruitState S)
	{
		switch (S)
		{
		case ERecruitState::Nest: return TEXT("nest");
		case ERecruitState::Queued: return TEXT("queued");
		case ERecruitState::Out: return TEXT("out");
		case ERecruitState::Cutting: return TEXT("cutting");
		case ERecruitState::Laden: return TEXT("laden");
		case ERecruitState::Empty: return TEXT("empty");
		case ERecruitState::Fallen: return TEXT("fallen");
		}
		return TEXT("?");
	}

	double RouteLength(const TArray<FTrailPoint>& Pts)
	{
		double L = 0.0;
		for (int32 k = 1; k < Pts.Num(); ++k) L += FVector::Dist(Pts[k - 1].Pos, Pts[k].Pos);
		return L;
	}
}

ALineSystem::ALineSystem()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

ALineSystem* ALineSystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World) return nullptr;
	for (TActorIterator<ALineSystem> It(const_cast<UWorld*>(World)); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

void ALineSystem::Init(ATreeActor* InTree, ATrailNetwork* InTrails, ANestActor* InNest, AWindSystem* InWind, ADayDirector* InDirector)
{
	Tree = InTree;
	Trails = InTrails;
	Nest = InNest;
	Wind = InWind;
	Director = InDirector;
	// The wind's gust clock runs in the same fixed steps as the recruits (see Tick).
	if (Wind) Wind->SetExternalClock(true);
}

void ALineSystem::StartDay()
{
	for (FRecruit& R : Recruits)
	{
		if (AAntPawn* P = R.Pawn.Get()) P->Destroy();
	}
	Recruits.Reset();
	DrivenAnts.Reset();
	Lines.Reset();
	Deliveries.Reset();
	LeafReserved.Reset();
	PreviewCache.Reset();
	RecruitLosses = 0;
	Fallbacks = 0;
	LastDepart = -1000.0;
	SimTime = 0.0;
	Accumulator = 0.0;
	SpeedFactor = 1.f;
	DeliveryChain = 0;

	if (Director && Director->WorkerClass)
	{
		const AAntPawn* CDO = Director->WorkerClass->GetDefaultObject<AAntPawn>();
		UsePerCut = CDO->Cut.LeafUsePerCut;
		CutReach = CDO->Cut.Reach;
	}

	int32 Pool = 0;
	if (Director && Director->AreRecruitsOn())
	{
		const ULeaflineGameInstance* GI = Cast<ULeaflineGameInstance>(GetGameInstance());
		Pool = PoolOverride >= 0 ? PoolOverride : (GI ? GI->GetRecruitPool() : 0);
	}
	bOnToday = Director && Director->AreRecruitsOn();
	Recruits.SetNum(Pool);
	for (FRecruit& R : Recruits) R.TripsLeft = Tuning.TripsPerRecruit;
	UE_LOG(LogLeafline, Log, TEXT("Recruits: pool of %d, %d trips each%s"), Pool, Tuning.TripsPerRecruit,
		Director && !Director->AreRecruitsOn() ? TEXT(" (recruits off)") : PoolOverride >= 0 ? TEXT(" (Leaf.Pool)") : TEXT(""));
	if (Bird && Director) Bird->StartDay(Director->GetDay(), Director->GetSeed(), bOnToday, Pool);
	bGlowDirty = true;
	UpdateTrailGlow();
}

bool ALineSystem::IsActive() const
{
	return bOnToday && Recruits.Num() > 0;
}

// ---------------------------------------------------------------------------------------------------------------------
// Lines

int32 ALineSystem::PiecesOn(int32 Leaf, float Template) const
{
	return Tree ? Tree->GetLeaves().PiecesLeft(Leaf, Template, UsePerCut) : 0;
}

void ALineSystem::AddLine(AAntPawn* Scout, bool bPieceAlreadyHome)
{
	if (!Tree || !Trails || !Scout) return;
	const FAntCutRecord& C = Scout->GetLastCut();
	if (!C.IsValid())
	{
		UE_LOG(LogLeafline, Log, TEXT("Line: worker %d has no cut on record; no line"), Scout->GetWorkerIndex());
		return;
	}
	for (const FColonyLine& Existing : Lines)
	{
		if (Existing.Worker == Scout->GetWorkerIndex() && Existing.CutLeaf == C.Leaf && Existing.EndPos().Equals(C.Pos, 1.0))
		{
			return; // this cut already has its line
		}
	}

	FColonyLine L;
	L.Number = Lines.Num() + 1;
	L.Worker = Scout->GetWorkerIndex();
	L.CutLeaf = C.Leaf;
	L.CutBranch = C.Branch;
	L.Template = C.Size;
	const ATreeActor* T = Tree;
	Trails->BuildReturnRoute(C.Pos, C.Normal, Scout->GetPathIndex(), 100.f,
		[T](const FVector& A, const FVector& B) { return T->StaysOnBark(A, B); }, L.Route);
	if (L.Route.Num() < 2)
	{
		UE_LOG(LogLeafline, Warning, TEXT("Line: no route home from worker %d's cut (leaf %d); no line"), L.Worker, C.Leaf);
		return;
	}

	FTreePatchRules Rules;
	Rules.MaxLeaves = Tuning.PatchMaxLeaves;
	Rules.MaxPieces = FMath::Max(Tuning.PatchMaxPieces - 1, 0); // the scout's own piece is the patch's first
	Rules.MaxWalk = Tuning.PatchMaxWalk;
	Rules.Template = L.Template;
	Rules.UsePerCut = UsePerCut;
	TArray<int32> Candidates;
	Tree->BuildPatch(C.Leaf, Rules, Candidates);
	float RichSum = 0.f;
	for (int32 Leaf : Candidates)
	{
		TArray<FTrailPoint> Approach;
		// The patch measures walking distance from the cut leaf; the line ends where the scout stood, so allow slack.
		if (!Tree->BuildLeafApproach(L.EndPos(), L.CutBranch, Leaf, Rules.MaxWalk + 300.f, Approach))
		{
			UE_LOG(LogLeafline, Log, TEXT("Line %d: leaf %d left out of the patch (no approach)"), L.Number, Leaf);
			continue;
		}
		{
			double MaxGap = 0.0;
			for (int32 k = 1; k < Approach.Num(); ++k) MaxGap = FMath::Max(MaxGap, FVector::Dist(Approach[k - 1].Pos, Approach[k].Pos));
			UE_LOG(LogLeafline, Verbose, TEXT("Line %d approach to leaf %d: %d points, %.0f cm, starts %.0f cm from the line end, ends %.0f cm from the leaf base, max step %.0f cm"),
				L.Number, Leaf, Approach.Num(), RouteLength(Approach), Approach.Num() ? FVector::Dist(Approach[0].Pos, L.EndPos()) : -1.0,
				Approach.Num() ? FVector::Dist(Approach.Last().Pos, Tree->GetLeaves().Get(Leaf).Base) : -1.0, MaxGap);
		}
		L.Patch.Add(Leaf);
		L.Approach.Add(MoveTemp(Approach));
		RichSum += Tree->GetLeaves().Get(Leaf).Richness;
	}
	L.Richness = L.Patch.Num() ? RichSum / L.Patch.Num() : C.Richness;
	RecountLine(L);
	L.PatchTotal = L.PatchRemaining + 1;
	L.bScoutPieceInFlight = true;
	Lines.Add(MoveTemp(L));
	const FColonyLine& Added = Lines.Last();
	UE_LOG(LogLeafline, Log, TEXT("Line %d laid by worker %d: %d route points, %.0f m, template %.2f, patch %d leaves, %d pieces left (+1 the scout's), rich x%.2f%s"),
		Added.Number, Added.Worker, Added.Route.Num(), RouteLength(Added.Route) / 100.0, Added.Template, Added.Patch.Num(),
		Added.PatchRemaining, Added.Richness, bPieceAlreadyHome ? TEXT(", scout's piece already home") : TEXT(""));
	bGlowDirty = true;
	PreviewCache.Reset();
}

void ALineSystem::OnScoutPieceResolved(int32 Worker, bool bHome, float Mass)
{
	for (int32 i = Lines.Num() - 1; i >= 0; --i)
	{
		FColonyLine& L = Lines[i];
		if (L.Worker != Worker || !L.bScoutPieceInFlight) continue;
		L.bScoutPieceInFlight = false;
		if (bHome)
		{
			L.PiecesHome += 1;
			L.MassHome += Mass;
		}
		else
		{
			L.PiecesLost += 1;
		}
		bGlowDirty = true;
		return;
	}
}

void ALineSystem::RecountLine(FColonyLine& L)
{
	int32 Remaining = 0;
	for (int32 Leaf : L.Patch)
	{
		const int32* Res = LeafReserved.Find(Leaf);
		Remaining += FMath::Max(PiecesOn(Leaf, L.Template) - (Res ? *Res : 0), 0);
	}
	L.PatchRemaining = Remaining;
}

void ALineSystem::RecountAll()
{
	for (FColonyLine& L : Lines)
	{
		const int32 Before = L.PatchRemaining + L.Reserved;
		RecountLine(L);
		if (L.PatchRemaining + L.Reserved != Before) bGlowDirty = true;
	}
}

int32 ALineSystem::NumLiveLines() const
{
	int32 N = 0;
	for (const FColonyLine& L : Lines) N += L.IsLive() ? 1 : 0;
	return N;
}

int32 ALineSystem::ChooseLine() const
{
	// The live line with the most remaining pieces per ant already on it; richer patches win ties.
	int32 Best = INDEX_NONE;
	float BestScore = -1.f;
	for (int32 i = 0; i < Lines.Num(); ++i)
	{
		const FColonyLine& L = Lines[i];
		if (L.PatchRemaining <= 0) continue;
		const float Score = float(L.PatchRemaining) / float(L.Assigned + 1);
		if (Best == INDEX_NONE || Score > BestScore + 1e-4f || (FMath::IsNearlyEqual(Score, BestScore, 1e-4f) && L.Richness > Lines[Best].Richness))
		{
			Best = i;
			BestScore = Score;
		}
	}
	return Best;
}

int32 ALineSystem::ChoosePatchSlot(const FColonyLine& L) const
{
	for (int32 s = 0; s < L.Patch.Num(); ++s)
	{
		const int32* Res = LeafReserved.Find(L.Patch[s]);
		if (PiecesOn(L.Patch[s], L.Template) - (Res ? *Res : 0) > 0) return s;
	}
	return INDEX_NONE;
}

bool ALineSystem::GetPatchPreview(int32 Leaf, float Template, FPatchPreview& Out) const
{
	if (!Tree || Leaf < 0 || Leaf >= Tree->GetLeaves().Num()) return false;
	// Bites change what a patch holds, so the cache only lives a moment.
	const double Now = GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.0;
	if (Now - PreviewCacheTime > 1.0)
	{
		PreviewCache.Reset();
		PreviewCacheTime = Now;
	}
	const int32 Quant = FMath::Clamp(FMath::RoundToInt(Template * 20.f), 1, 20);
	const uint64 Key = (uint64(uint32(Leaf)) << 8) | uint64(Quant);
	if (const FPatchPreview* Hit = PreviewCache.Find(Key))
	{
		Out = *Hit;
		return true;
	}
	FPatchPreview P;
	P.Leaf = Leaf;
	P.Template = Quant / 20.f;
	FTreePatchRules Rules;
	Rules.MaxLeaves = Tuning.PatchMaxLeaves;
	Rules.MaxPieces = Tuning.PatchMaxPieces;
	Rules.MaxWalk = Tuning.PatchMaxWalk;
	Rules.Template = P.Template;
	Rules.UsePerCut = UsePerCut;
	Tree->BuildPatch(Leaf, Rules, P.Leaves);
	float RichSum = 0.f;
	for (int32 L : P.Leaves)
	{
		P.Pieces += PiecesOn(L, P.Template);
		RichSum += Tree->GetLeaves().Get(L).Richness;
	}
	P.Richness = P.Leaves.Num() ? RichSum / P.Leaves.Num() : Tree->GetLeaves().Get(Leaf).Richness;
	PreviewCache.Add(Key, P);
	Out = MoveTemp(P);
	return true;
}

// ---------------------------------------------------------------------------------------------------------------------
// Pool read-outs

bool ALineSystem::HasWork() const
{
	bool bCanGo = false;
	for (const FRecruit& R : Recruits)
	{
		if (R.State != ERecruitState::Nest) return true;
		bCanGo |= R.TripsLeft > 0;
	}
	return bCanGo && ChooseLine() != INDEX_NONE;
}

int32 ALineSystem::NumIdle() const
{
	if (ChooseLine() != INDEX_NONE) return 0;
	int32 N = 0;
	for (const FRecruit& R : Recruits) N += R.State == ERecruitState::Nest && R.TripsLeft > 0 ? 1 : 0;
	return N;
}

int32 ALineSystem::NumOut() const
{
	int32 N = 0;
	for (const FRecruit& R : Recruits) N += R.State != ERecruitState::Nest ? 1 : 0;
	return N;
}

int32 ALineSystem::TripsLeft() const
{
	int32 N = 0;
	for (const FRecruit& R : Recruits) N += R.TripsLeft;
	return N;
}

int32 ALineSystem::FindRecruit(const AAntPawn* Ant) const
{
	for (int32 i = 0; i < Recruits.Num(); ++i)
	{
		if (Recruits[i].Pawn.Get() == Ant) return i;
	}
	return INDEX_NONE;
}

// ---------------------------------------------------------------------------------------------------------------------
// Deliveries

void ALineSystem::RecordDelivery(const FVector& At, float Mass, int32 Line)
{
	const double Now = GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.0;
	DeliveryChain = Now - LastDeliveryReal < 2.5 ? DeliveryChain + 1 : 0;
	LastDeliveryReal = Now;
	FDeliveryPop Pop;
	Pop.WorldPos = At;
	Pop.Mass = Mass;
	Pop.RealTime = Now;
	Pop.Chain = DeliveryChain;
	Deliveries.Add(Pop);
	while (Deliveries.Num() > 0 && (Now - Deliveries[0].RealTime > 4.0 || Deliveries.Num() > 40)) Deliveries.RemoveAt(0);

	if (Lines.IsValidIndex(Line))
	{
		Lines[Line].PiecesHome += 1;
		Lines[Line].MassHome += Mass;
		bGlowDirty = true;
	}
	if (Nest) Nest->PlayDeposit(Mass);
	// The chime rises while deliveries come in close together and falls back after a pause. While skipping, many
	// arrive in one frame: let only the first few of a frame sound.
	static uint64 LastSoundFrame = 0;
	static int32 SoundsThisFrame = 0;
	if (LastSoundFrame != GFrameCounter)
	{
		LastSoundFrame = GFrameCounter;
		SoundsThisFrame = 0;
	}
	if (SoundsThisFrame++ < 2)
	{
		const float Base = FMath::Clamp(1.15f - 0.06f * Mass / FMath::Max(1.f, Lines.IsValidIndex(Line) ? Lines[Line].Richness : 1.f), 0.8f, 1.2f);
		const float Rise = 1.f + 0.06f * FMath::Min(DeliveryChain, 10);
		LeafSound::Play(this, TEXT("deposit"), 0.7f, Base * Rise);
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Recruits

void ALineSystem::SetState(FRecruit& R, ERecruitState NewState)
{
	R.State = NewState;
	R.StateSince = SimTime;
}

void ALineSystem::ReleaseReservation(FRecruit& R)
{
	if (!Lines.IsValidIndex(R.Line) || !Lines[R.Line].Patch.IsValidIndex(R.PatchSlot)) return;
	FColonyLine& L = Lines[R.Line];
	const int32 Leaf = L.Patch[R.PatchSlot];
	if (int32* Res = LeafReserved.Find(Leaf))
	{
		if (--(*Res) <= 0) LeafReserved.Remove(Leaf);
	}
	L.Reserved = FMath::Max(L.Reserved - 1, 0);
	R.PatchSlot = INDEX_NONE;
	RecountAll();
	bGlowDirty = true;
}

void ALineSystem::AssignIdle()
{
	for (int32 i = 0; i < Recruits.Num(); ++i)
	{
		FRecruit& R = Recruits[i];
		if (R.State != ERecruitState::Nest || R.TripsLeft <= 0) continue;
		const int32 LineIdx = ChooseLine();
		if (LineIdx == INDEX_NONE) return;
		FColonyLine& L = Lines[LineIdx];
		const int32 Slot = ChoosePatchSlot(L);
		if (Slot == INDEX_NONE)
		{
			RecountLine(L); // counts were stale; try again next step
			continue;
		}
		LeafReserved.FindOrAdd(L.Patch[Slot]) += 1;
		L.Reserved += 1;
		L.Assigned += 1;
		RecountAll();
		R.Line = LineIdx;
		R.PatchSlot = Slot;
		R.DepartAt = FMath::Max(SimTime, LastDepart + Tuning.DepartSpacing);
		LastDepart = R.DepartAt;
		SetState(R, ERecruitState::Queued);
		bGlowDirty = true;
	}
}

AAntPawn* ALineSystem::SpawnRecruitPawn()
{
	if (!Tree || !Director || !GetWorld()) return nullptr;
	FTransform SpawnXf = Tree->GetNestTransform(0);
	SpawnXf.AddToTranslation(FVector(0.0, 0.0, 40.0));
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags |= RF_Transient;
	UClass* Class = Director->WorkerClass ? *Director->WorkerClass : AAntPawn::StaticClass();
	AAntPawn* Ant = GetWorld()->SpawnActor<AAntPawn>(Class, SpawnXf, Params);
	if (!Ant) return nullptr;
	if (ULeaflineGameInstance* GI = Cast<ULeaflineGameInstance>(GetGameInstance())) GI->ApplyUpgradesToAnt(Ant);
	Ant->MakeRecruit(this);
	Ant->AddTickPrerequisiteActor(this);
	if (Ant->GetAntMovement()) Ant->GetAntMovement()->AddTickPrerequisiteActor(this);
	return Ant;
}

void ALineSystem::Depart(int32 Index)
{
	FRecruit& R = Recruits[Index];
	if (!Lines.IsValidIndex(R.Line) || !Lines[R.Line].Patch.IsValidIndex(R.PatchSlot))
	{
		FinishTrip(Index, false);
		return;
	}
	const FColonyLine& L = Lines[R.Line];
	AAntPawn* Ant = SpawnRecruitPawn();
	if (!Ant)
	{
		ReleaseReservation(R);
		FinishTrip(Index, false);
		return;
	}
	R.Pawn = Ant;
	R.Route = L.Route;
	R.LegStart = R.Route.Num() - 1;
	const TArray<FTrailPoint>& Approach = L.Approach[R.PatchSlot];
	for (int32 k = 0; k < Approach.Num(); ++k)
	{
		if (k == 0 && FVector::DistSquared(Approach[0].Pos, R.Route.Last().Pos) < 4.0 * 4.0) continue;
		R.Route.Add(Approach[k]);
	}
	UAntMovementComponent* M = Ant->GetAntMovement();
	const FVector Facing = R.Route.Num() > 1 ? (R.Route[1].Pos - R.Route[0].Pos) : FVector::ForwardVector;
	M->TeleportTo(R.Route[0].Pos, Facing);
	M->SetFollowSpeed(Tuning.OutSpeed, Tuning.HomeSpeedScale);
	M->StartFollow(R.Route, 0, 1, EAntState::Riding);
	R.OutSince = SimTime;
	R.LegSince = -1.0;
	R.ReturnFrom = R.Route.Num() - 1;
	SetState(R, ERecruitState::Out);
	UE_LOG(LogLeafline, Log, TEXT("Recruit %d sets out on line %d for leaf %d (%.0f m out, %d trips left)"),
		Index, L.Number, L.Patch[R.PatchSlot], RouteLength(R.Route) / 100.0, R.TripsLeft);
}

void ALineSystem::ArriveAtLeaf(int32 Index, bool bFallback)
{
	FRecruit& R = Recruits[Index];
	AAntPawn* Ant = R.Pawn.Get();
	if (!Ant || !Lines.IsValidIndex(R.Line))
	{
		FinishTrip(Index, false);
		return;
	}
	FColonyLine& L = Lines[R.Line];
	UAntMovementComponent* M = Ant->GetAntMovement();
	R.ReturnFrom = FMath::Clamp(M->GetFollowIndex(), 0, R.Route.Num() - 1);
	int32 Leaf = L.Patch.IsValidIndex(R.PatchSlot) ? L.Patch[R.PatchSlot] : INDEX_NONE;
	const FVector Here = M->GetSurfacePosition();
	if (Leaf == INDEX_NONE || PiecesOn(Leaf, L.Template) < 1)
	{
		// Someone took its piece meanwhile: take a free piece from another patch leaf within reach, if there is one.
		ReleaseReservation(R);
		Leaf = INDEX_NONE;
		for (int32 s = 0; s < L.Patch.Num(); ++s)
		{
			const int32* Res = LeafReserved.Find(L.Patch[s]);
			if (PiecesOn(L.Patch[s], L.Template) - (Res ? *Res : 0) > 0 &&
				FVector::Dist(Tree->GetLeaves().Get(L.Patch[s]).Base, Here) < CutReach * 1.5f)
			{
				R.PatchSlot = s;
				Leaf = L.Patch[s];
				LeafReserved.FindOrAdd(Leaf) += 1;
				L.Reserved += 1;
				RecountAll();
				break;
			}
		}
		if (Leaf == INDEX_NONE)
		{
			UE_LOG(LogLeafline, Log, TEXT("Recruit %d: patch of line %d has nothing left here; turns round empty (trip kept)"), Index, L.Number);
			StartHome(R, false);
			return;
		}
	}
	const float Dist = FVector::Dist(Tree->GetLeaves().Get(Leaf).Base, Here);
	if (bFallback || Dist > CutReach * 1.5f)
	{
		UE_LOG(LogLeafline, Log, TEXT("Recruit %d fallback detail: follow index %d of %d (leg starts %d), %.0f cm from the route's last point, %.0f cm from the leg start"),
			Index, M->GetFollowIndex(), R.Route.Num(), R.LegStart, R.Route.Num() ? FVector::Dist(R.Route.Last().Pos, Here) : -1.0,
			R.Route.IsValidIndex(R.LegStart) ? FVector::Dist(R.Route[R.LegStart].Pos, Here) : -1.0);
		++Fallbacks;
		UE_LOG(LogLeafline, Warning, TEXT("Recruit %d: fallback cut on line %d, leaf %d from %.0f cm away (%s); %d fallbacks today"),
			Index, L.Number, Leaf, Dist, bFallback ? TEXT("timed out") : TEXT("route ended short"), Fallbacks);
	}
	Ant->StartAutoCut(Leaf, L.Template);
	SetState(R, ERecruitState::Cutting);
}

void ALineSystem::StartHome(FRecruit& R, bool bLaden)
{
	AAntPawn* Ant = R.Pawn.Get();
	if (!Ant) return;
	UAntMovementComponent* M = Ant->GetAntMovement();
	M->SetFollowSpeed(Tuning.OutSpeed, Tuning.HomeSpeedScale);
	M->StartFollow(R.Route, FMath::Clamp(R.ReturnFrom, 0, R.Route.Num() - 1), -1, EAntState::Returning);
	SetState(R, bLaden ? ERecruitState::Laden : ERecruitState::Empty);
}

void ALineSystem::FinishTrip(int32 Index, bool bUsedTrip, bool bQuiet)
{
	FRecruit& R = Recruits[Index];
	ReleaseReservation(R);
	if (Lines.IsValidIndex(R.Line)) Lines[R.Line].Assigned = FMath::Max(Lines[R.Line].Assigned - 1, 0);
	if (AAntPawn* Ant = R.Pawn.Get())
	{
		if (bQuiet)
		{
			Ant->Destroy();
		}
		else
		{
			Ant->PushVisuals();
			Ant->DepositAndVanish(Nest ? Nest->GetActorLocation() + FVector(0.f, 0.f, 6.f) : Ant->GetActorLocation());
		}
	}
	R.Pawn = nullptr;
	R.Line = INDEX_NONE;
	R.PatchSlot = INDEX_NONE;
	R.LegSince = -1.0;
	R.Route.Reset();
	if (bUsedTrip) R.TripsLeft = FMath::Max(R.TripsLeft - 1, 0);
	SetState(R, ERecruitState::Nest);
	bGlowDirty = true;
}

void ALineSystem::OnRecruitBlownOff(AAntPawn* Ant, bool bHadPiece)
{
	const int32 i = FindRecruit(Ant);
	if (i == INDEX_NONE) return;
	FRecruit& R = Recruits[i];
	const ERecruitState Was = R.State;
	// Count the piece from what the ant held, not the state: a cut can finish and blow away in the same step.
	if (bHadPiece)
	{
		++RecruitLosses;
		if (Lines.IsValidIndex(R.Line)) Lines[R.Line].PiecesLost += 1;
	}
	ReleaseReservation(R);
	if (Lines.IsValidIndex(R.Line)) Lines[R.Line].Assigned = FMath::Max(Lines[R.Line].Assigned - 1, 0);
	UE_LOG(LogLeafline, Log, TEXT("Recruit %d blown off while %s on line %d%s; it walks home and loses this trip"),
		i, StateName(Was), Lines.IsValidIndex(R.Line) ? Lines[R.Line].Number : 0, bHadPiece ? TEXT(" (piece lost)") : TEXT(""));
	R.Line = INDEX_NONE;
	SetState(R, ERecruitState::Fallen);
	bGlowDirty = true;
}

void ALineSystem::UpdateRecruit(int32 Index, float Dt)
{
	FRecruit& R = Recruits[Index];
	if (R.State == ERecruitState::Nest) return;
	if (R.State == ERecruitState::Queued)
	{
		if (SimTime >= R.DepartAt) Depart(Index);
		return;
	}
	AAntPawn* Ant = R.Pawn.Get();
	if (!Ant)
	{
		UE_LOG(LogLeafline, Warning, TEXT("Recruit %d lost its ant while %s; back in the nest"), Index, StateName(R.State));
		FinishTrip(Index, R.State == ERecruitState::Laden || R.State == ERecruitState::Fallen);
		return;
	}
	const ERecruitState Before = R.State;
	Ant->SimStep(Dt, false); // may call OnRecruitBlownOff
	if (R.State != Before) return;
	UAntMovementComponent* M = Ant->GetAntMovement();

	// A fallen recruit walking home over the ground can stall against a root: if it makes no headway for a while it
	// is counted home (it vanishes where it is; it is usually far below the camera).
	if (R.State == ERecruitState::Fallen && M->GetState() != EAntState::Falling)
	{
		if (FVector::DistSquared(M->GetSurfacePosition(), R.ProgressPos) > 40.0 * 40.0 || R.ProgressTime < R.StateSince)
		{
			R.ProgressPos = M->GetSurfacePosition();
			R.ProgressTime = SimTime;
		}
		else if (SimTime - R.ProgressTime > 8.0)
		{
			UE_LOG(LogLeafline, Log, TEXT("Recruit %d stalled on its walk home after a fall (at %s); counted home"), Index, *M->GetSurfacePosition().ToCompactString());
			FinishTrip(Index, true, true);
			return;
		}
	}

	// Safety net: nobody stays out forever.
	if (SimTime - R.OutSince > Tuning.RescueAfter || (R.State == ERecruitState::Fallen && SimTime - R.StateSince > 150.0))
	{
		UE_LOG(LogLeafline, Warning, TEXT("Recruit %d rescued after %.0f s out (stuck while %s at %s)"),
			Index, SimTime - R.OutSince, StateName(R.State), *M->GetSurfacePosition().ToCompactString());
		if (R.State == ERecruitState::Laden)
		{
			++RecruitLosses;
			if (Lines.IsValidIndex(R.Line)) Lines[R.Line].PiecesLost += 1;
		}
		FinishTrip(Index, true);
		return;
	}

	switch (R.State)
	{
	case ERecruitState::Out:
		if (R.LegSince < 0.0 && M->GetFollowIndex() >= R.LegStart) R.LegSince = SimTime;
		if (M->IsFollowFinished() || M->GetState() != EAntState::Riding)
		{
			ArriveAtLeaf(Index, false);
		}
		else if (R.LegSince >= 0.0 && SimTime - R.LegSince > Tuning.ReachTimeout)
		{
			ArriveAtLeaf(Index, true);
		}
		break;
	case ERecruitState::Cutting:
		if (Ant->IsCarrying() && !Ant->IsCutting())
		{
			// The piece is taken: its reservation is used up.
			const int32 Leaf = Lines.IsValidIndex(R.Line) && Lines[R.Line].Patch.IsValidIndex(R.PatchSlot) ? Lines[R.Line].Patch[R.PatchSlot] : INDEX_NONE;
			ReleaseReservation(R);
			StartHome(R, true);
			UE_LOG(LogLeafline, Log, TEXT("Recruit %d cut its piece from leaf %d on line %d (patch: %d left)"),
				Index, Leaf, Lines.IsValidIndex(R.Line) ? Lines[R.Line].Number : 0, Lines.IsValidIndex(R.Line) ? Lines[R.Line].PatchRemaining : 0);
		}
		else if (!Ant->IsCutting())
		{
			ReleaseReservation(R);
			UE_LOG(LogLeafline, Log, TEXT("Recruit %d: the cut failed (leaf gone); turns round empty"), Index);
			StartHome(R, false);
		}
		break;
	case ERecruitState::Laden:
	case ERecruitState::Empty:
		if (M->IsFollowFinished() || (M->GetState() != EAntState::Returning && M->GetState() != EAntState::Falling))
		{
			if (R.State == ERecruitState::Laden && Ant->IsCarrying())
			{
				const float Value = Ant->GetCarriedValue();
				const int32 LineIdx = R.Line;
				if (Director) Director->CreditHaul(Value, true);
				RecordDelivery(Nest ? Nest->GetActorLocation() : M->GetSurfacePosition(), Value, LineIdx);
				UE_LOG(LogLeafline, Log, TEXT("Recruit %d home with %.1f from line %d"), Index, Value, Lines.IsValidIndex(LineIdx) ? Lines[LineIdx].Number : 0);
				FinishTrip(Index, true);
			}
			else
			{
				FinishTrip(Index, false);
			}
		}
		break;
	case ERecruitState::Fallen:
		if (Director && Director->IsNearNest(M->GetSurfacePosition()) && M->GetState() != EAntState::Falling)
		{
			UE_LOG(LogLeafline, Log, TEXT("Recruit %d walked home after its fall"), Index);
			FinishTrip(Index, true);
		}
		break;
	default:
		break;
	}
}

void ALineSystem::AddDrivenAnt(AAntPawn* Ant)
{
	if (!Ant || Ant->IsVanishing()) return;
	Ant->SetSimDriven(true);
	DrivenAnts.AddUnique(Ant);
}

void ALineSystem::StepAnts(float Dt)
{
	for (int32 k = 0; k < Scheduled.Num(); ++k)
	{
		if (SimTime + 1e-6 < Scheduled[k].Key) continue;
		const FString Cmd = Scheduled[k].Value;
		Scheduled.RemoveAt(k--);
		UE_LOG(LogLeafline, Log, TEXT("Leaf.AtSim %.3f: %s"), SimTime, *Cmd);
		if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr) PC->ConsoleCommand(Cmd);
	}
	if (Wind) Wind->Advance(Dt);
	SimTime += Dt;
	// Counts are refreshed every step (patches can share leaves, and a scout may bite one), never per rendered frame,
	// so assignments come out the same at any speed.
	RecountAll();
	AssignIdle();
	for (int32 i = 0; i < Recruits.Num(); ++i) UpdateRecruit(i, Dt);
	for (int32 i = DrivenAnts.Num() - 1; i >= 0; --i)
	{
		AAntPawn* Ant = DrivenAnts[i].Get();
		if (!Ant || !Ant->IsSimDriven())
		{
			DrivenAnts.RemoveAt(i);
			continue;
		}
		Ant->SimStep(Dt, false);
	}
	// The bird's schedule and its hits, after the ants have moved (it only acts during the Shift).
	if (Bird) Bird->Advance(Dt);
}

void ALineSystem::SimStep(float Dt)
{
	StepAnts(Dt);
}

void ALineSystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	const float Step = FMath::Max(Tuning.Step, 1.f / 480.f);
	if (Wind) Wind->SetMuteCues(SpeedFactor > 20.f);
	Accumulator += double(DeltaTime) * SpeedFactor;
	const double Start = FPlatformTime::Seconds();
	int32 Steps = 0;
	bool bOverBudget = false;
	while (Accumulator >= Step)
	{
		SimStep(Step);
		Accumulator -= Step;
		++Steps;
		if ((Steps & 3) == 0 && (FPlatformTime::Seconds() - Start) * 1000.0 > Tuning.FrameBudgetMs)
		{
			bOverBudget = true;
			break;
		}
	}
	// Over budget: drop the backlog instead of piling it up. The sim then runs slower than asked, but every step is
	// the same fixed step, so the outcome does not change.
	if (bOverBudget) Accumulator = FMath::Min(Accumulator, double(Step));
	if (Accumulator > 0.25) Accumulator = 0.25; // a hitch at normal speed

	for (FRecruit& R : Recruits)
	{
		if (AAntPawn* Ant = R.Pawn.Get()) Ant->PushVisuals();
	}
	for (const TWeakObjectPtr<AAntPawn>& A : DrivenAnts)
	{
		if (AAntPawn* Ant = A.Get()) Ant->PushVisuals();
	}

	if (bGlowDirty) UpdateTrailGlow();
}

FLinearColor ALineSystem::GetLineColor(int32 Number, float Lighten) const
{
	const FLinearColor C = Tuning.LineColors.Num() > 0 ? Tuning.LineColors[FMath::Max(Number - 1, 0) % Tuning.LineColors.Num()] : Tuning.NoLineColor;
	return FMath::Lerp(C, FLinearColor::White, FMath::Clamp(Lighten, 0.f, 1.f));
}

void ALineSystem::UpdateTrailGlow()
{
	bGlowDirty = false;
	if (!Trails) return;
	TArray<const TArray<FTrailPoint>*> Routes;
	TArray<float> Glow;
	TArray<FLinearColor> Colors;
	for (const FColonyLine& L : Lines)
	{
		Routes.Add(&L.Route);
		Glow.Add(L.IsLive() || L.Reserved > 0 ? FMath::Lerp(Tuning.GlowEmpty, Tuning.GlowFull, L.Fullness()) : Tuning.GlowEmpty);
		Colors.Add(GetLineColor(L.Number));
	}
	Trails->SetLineGlow(Routes, Glow, Tuning.GlowNoLine, Colors, Tuning.NoLineColor);
}
