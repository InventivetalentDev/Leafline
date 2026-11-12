#include "World/WindSystem.h"
#include "AntTreeGame.h"
#include "Tree/TreeActor.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "UObject/ConstructorHelpers.h"
#include "EngineUtils.h"

AWindSystem::AWindSystem()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	StreakMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Streaks"));
	StreakMesh->SetupAttachment(RootComponent);
	StreakMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StreakMesh->SetCastShadow(false);
	StreakMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Leafline/Materials/M_WindStreak.M_WindStreak")));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMat(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (BasicMat.Succeeded()) FallbackMaterial = BasicMat.Object;
}

AWindSystem* AWindSystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World) return nullptr;
	for (TActorIterator<AWindSystem> It(const_cast<UWorld*>(World)); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

void AWindSystem::BeginPlay()
{
	Super::BeginPlay();
	StreakMaterialInUse = StreakMaterial.LoadSynchronous();
	if (!StreakMaterialInUse && FallbackMaterial)
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(FallbackMaterial, this);
		MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.85f, 0.88f, 0.9f));
		StreakMaterialInUse = MID;
	}
}

void AWindSystem::StartDay(int32 InDay, int32 Seed)
{
	Day = FMath::Max(1, InDay);
	Rand.Initialize(Seed * 31 + 7);
	BaseYaw = Rand.FRandRange(0.f, 360.f);
	Direction = FRotator(0.f, BaseYaw, 0.f).Vector();
	Streaks.Reset();
	EnterPhase(EWindPhase::Calm);
	UE_LOG(LogLeafline, Log, TEXT("Wind for day %d: blowing toward yaw %.0f, peak force %.2f"), Day, BaseYaw, GetPeakForce());
}

float AWindSystem::GetPeakForce() const
{
	return Tuning.PeakForceDay1 * (1.f + Tuning.PeakForcePerDay * (Day - 1));
}

void AWindSystem::EnterPhase(EWindPhase NewPhase)
{
	Phase = NewPhase;
	switch (Phase)
	{
	case EWindPhase::Calm:
		PhaseDuration = FMath::Max(Tuning.CalmFloor, Rand.FRandRange(Tuning.CalmMin, Tuning.CalmMax) - Tuning.CalmShrinkPerDay * (Day - 1));
		GustPeak = 1.f;
		break;
	case EWindPhase::Warning: PhaseDuration = Tuning.Warning; break;
	case EWindPhase::Rising: PhaseDuration = Tuning.Rising; break;
	case EWindPhase::Holding: PhaseDuration = Rand.FRandRange(Tuning.HoldMin, Tuning.HoldMax) + Tuning.HoldGrowPerDay * (Day - 1); break;
	case EWindPhase::Falling: PhaseDuration = Tuning.Falling; break;
	}
	PhaseTimeLeft = PhaseDuration;
	UE_LOG(LogLeafline, Log, TEXT("Wind: %s for %.1f s"), *UEnum::GetValueAsString(Phase), PhaseDuration);
}

void AWindSystem::TriggerGust(float Peak)
{
	GustPeak = FMath::Clamp(Peak, 0.f, 1.f);
	const float Keep = GustPeak;
	EnterPhase(EWindPhase::Warning);
	GustPeak = Keep;
}

void AWindSystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	Clock += DeltaTime;

	PhaseTimeLeft -= DeltaTime;
	if (PhaseTimeLeft <= 0.f)
	{
		switch (Phase)
		{
		case EWindPhase::Calm: EnterPhase(EWindPhase::Warning); break;
		case EWindPhase::Warning: EnterPhase(EWindPhase::Rising); break;
		case EWindPhase::Rising: EnterPhase(EWindPhase::Holding); break;
		case EWindPhase::Holding: EnterPhase(EWindPhase::Falling); break;
		case EWindPhase::Falling: EnterPhase(EWindPhase::Calm); break;
		}
	}

	const float T = 1.f - FMath::Clamp(PhaseTimeLeft / FMath::Max(PhaseDuration, 0.01f), 0.f, 1.f);
	const float Calm = Tuning.CalmStrength * (1.f + 0.3f * FMath::Sin(Clock * 0.7f));
	const float WarnTop = FMath::Max(Calm, 0.35f * GustPeak);
	switch (Phase)
	{
	case EWindPhase::Calm: Strength = Calm; break;
	case EWindPhase::Warning: Strength = FMath::Lerp(Calm, WarnTop, T); break;
	case EWindPhase::Rising: Strength = FMath::Lerp(WarnTop, GustPeak, FMath::SmoothStep(0.f, 1.f, T)); break;
	case EWindPhase::Holding: Strength = GustPeak * (0.97f + 0.03f * FMath::Sin(Clock * 9.f)); break;
	case EWindPhase::Falling: Strength = FMath::Lerp(GustPeak, Calm, FMath::SmoothStep(0.f, 1.f, T)); break;
	}

	const float Yaw = BaseYaw + Tuning.DirectionWanderDeg * FMath::Sin(Clock * 0.11f);
	Direction = FRotator(0.f, Yaw, 0.f).Vector();
	UpdateStreaks(DeltaTime);
}

float AWindSystem::GetExposure(const FVector& P, const FVector& Normal) const
{
	// Facing: 1 on the windward face, 0.5 on top or underneath, 0 on the lee face.
	const float Facing = FMath::Clamp(FVector::DotProduct(Normal, -Direction) * 0.5f + 0.5f, 0.f, 1.f);
	float Open = 1.f;
	if (!Tree.IsValid()) const_cast<AWindSystem*>(this)->Tree = ATreeActor::Get(this);
	if (Tree.IsValid())
	{
		const FVector Start = P + Normal * 25.0;
		double HitT;
		if (Tree->GetField().March(Start, Start - Direction * Tuning.ShelterDistance, 5.0, HitT) && HitT < 1.0)
		{
			Open = Tuning.ShelterFactor;
		}
	}
	return Facing * Open;
}

float AWindSystem::FragmentArea(float DiameterCm) const
{
	return DiameterCm > 0.f ? FMath::Square(DiameterCm / FMath::Max(Tuning.FragmentAreaDiameter, 1.f)) : 0.f;
}

float AWindSystem::ComputeForce(const FVector& P, const FVector& Normal, float FragmentDiameterCm, float* OutExposure) const
{
	const float Exposure = GetExposure(P, Normal);
	if (OutExposure) *OutExposure = Exposure;
	return Strength * GetPeakForce() * Exposure * (Tuning.BodyArea + FragmentArea(FragmentDiameterCm));
}

float AWindSystem::GetHold(float GripMultiplier, bool bGripping) const
{
	return Tuning.BaseGrip * FMath::Max(GripMultiplier, 0.01f) * (bGripping ? Tuning.GripBonus : 1.f);
}

float AWindSystem::PredictRiskRatio(float FragmentDiameterCm, float GripMultiplier) const
{
	return GetPeakForce() * (Tuning.BodyArea + FragmentArea(FragmentDiameterCm)) / GetHold(GripMultiplier, false);
}

void AWindSystem::UpdateStreaks(float Dt)
{
	// Thin streaks drifting downwind around the camera; they appear as the gust builds.
	const float Amount = FMath::SmoothStep(0.22f, 0.9f, Strength);
	const int32 Wanted = FMath::RoundToInt(Tuning.StreakCount * Amount);
	APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!Cam || (Wanted == 0 && Streaks.Num() == 0))
	{
		if (StreakMesh->GetNumSections() > 0) StreakMesh->ClearAllMeshSections();
		return;
	}
	const FVector CamPos = Cam->GetCameraLocation();
	const float R = Tuning.StreakRadius;
	for (int32 i = Streaks.Num() - 1; i >= 0; --i)
	{
		FStreak& S = Streaks[i];
		S.Life += Dt;
		S.Pos += Direction * (Tuning.StreakSpeed * (0.6f + 0.4f * Strength) * Dt);
		if (S.Life >= S.MaxLife || FVector::DistSquared(S.Pos, CamPos) > 4.0 * R * R) Streaks.RemoveAtSwap(i);
	}
	while (Streaks.Num() < Wanted)
	{
		FStreak S;
		S.Pos = CamPos + FVector(Rand.FRandRange(-R, R), Rand.FRandRange(-R, R), Rand.FRandRange(-R * 0.5f, R * 0.5f)) - Direction * R * 0.5f;
		S.Length = Rand.FRandRange(120.f, 420.f);
		S.MaxLife = Rand.FRandRange(0.5f, 1.3f);
		Streaks.Add(S);
	}

	TArray<FVector> V, N;
	TArray<int32> Tri;
	TArray<FVector2D> UV;
	TArray<FLinearColor> Col;
	TArray<FProcMeshTangent> Tan;
	for (const FStreak& S : Streaks)
	{
		const float Fade = FMath::Sin(PI * FMath::Clamp(S.Life / S.MaxLife, 0.f, 1.f));
		const FVector A = S.Pos, B = S.Pos + Direction * S.Length * Fade;
		const FVector ToCam = (CamPos - A).GetSafeNormal();
		const FVector Side = FVector::CrossProduct(Direction, ToCam).GetSafeNormal() * (2.2f + 1.8f * Fade);
		const int32 Base = V.Num();
		V.Append({ A - Side, A + Side, B - Side, B + Side });
		N.Append({ ToCam, ToCam, ToCam, ToCam });
		UV.Append({ FVector2D(0, 0), FVector2D(1, 0), FVector2D(0, 1), FVector2D(1, 1) });
		const FLinearColor C(1.f, 1.f, 1.f, Fade * Amount);
		Col.Append({ C, C, C, C });
		Tan.Append({ FProcMeshTangent(Direction, false), FProcMeshTangent(Direction, false), FProcMeshTangent(Direction, false), FProcMeshTangent(Direction, false) });
		// UE front faces: (b - a) x (c - a) points away from the viewer.
		if (FVector::DotProduct(FVector::CrossProduct(V[Base + 1] - V[Base], V[Base + 2] - V[Base]), ToCam) > 0.0)
			Tri.Append({ Base, Base + 2, Base + 1, Base + 1, Base + 2, Base + 3 });
		else
			Tri.Append({ Base, Base + 1, Base + 2, Base + 1, Base + 3, Base + 2 });
	}
	StreakMesh->CreateMeshSection_LinearColor(0, V, Tri, N, UV, {}, {}, {}, Col, Tan, false, false);
	if (StreakMaterialInUse) StreakMesh->SetMaterial(0, StreakMaterialInUse);
}
