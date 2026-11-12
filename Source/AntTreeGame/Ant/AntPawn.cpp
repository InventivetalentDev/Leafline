#include "Ant/AntPawn.h"
#include "AntTreeGame.h"
#include "Ant/AntMovementComponent.h"
#include "Tree/TreeActor.h"
#include "Colony/TrailNetwork.h"
#include "World/DayDirector.h"
#include "World/WindSystem.h"
#include "Colony/NestActor.h"
#include "Game/LeaflineGameInstance.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	UStaticMeshComponent* MakePart(AAntPawn* Owner, USceneComponent* Parent, const TCHAR* Name, UStaticMesh* Mesh, const FVector& Loc, const FVector& Scale)
	{
		UStaticMeshComponent* C = Owner->CreateDefaultSubobject<UStaticMeshComponent>(Name);
		C->SetupAttachment(Parent);
		C->SetStaticMesh(Mesh);
		C->SetRelativeLocation(Loc);
		C->SetRelativeScale3D(Scale);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetGenerateOverlapEvents(false);
		C->CanCharacterStepUpOn = ECB_No;
		return C;
	}
}

AAntPawn::AAntPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics; // after the movement component has moved the ant
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Movement = CreateDefaultSubobject<UAntMovementComponent>(TEXT("AntMovement"));

	VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VisualRoot"));
	VisualRoot->SetupAttachment(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMat(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	UStaticMesh* Sphere = SphereMesh.Succeeded() ? SphereMesh.Object : nullptr;
	if (BasicMat.Succeeded()) BasicMaterial = BasicMat.Object;

	// Placeholder: three spheres (the engine sphere is 100 cm across).
	Gaster = MakePart(this, VisualRoot, TEXT("Gaster"), Sphere, FVector(-7.f, 0.f, 5.5f), FVector(0.09f, 0.07f, 0.065f));
	Thorax = MakePart(this, VisualRoot, TEXT("Thorax"), Sphere, FVector(0.f, 0.f, 5.f), FVector(0.07f, 0.045f, 0.045f));
	Head = MakePart(this, VisualRoot, TEXT("Head"), Sphere, FVector(6.5f, 0.f, 5.5f), FVector(0.065f, 0.07f, 0.06f));

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(VisualRoot);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetVisibility(false);

	CarrySocket = CreateDefaultSubobject<USceneComponent>(TEXT("CarrySocket"));
	CarrySocket->SetupAttachment(VisualRoot);
	CarrySocket->SetRelativeLocation(MandibleOffset);

	FragmentMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Fragment"));
	FragmentMesh->SetupAttachment(CarrySocket);
	FragmentMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FragmentMesh->SetVisibility(false);
}

void AAntPawn::BeginPlay()
{
	Super::BeginPlay();
	CarrySocket->SetRelativeLocation(MandibleOffset);

	if (AntMesh)
	{
		Body->SetStaticMesh(AntMesh);
		if (AntMaterial) Body->SetMaterial(0, AntMaterial);
		Body->SetVisibility(true);
		Head->SetVisibility(false);
		Thorax->SetVisibility(false);
		Gaster->SetVisibility(false);
	}
	else if (BasicMaterial)
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BasicMaterial, this);
		MID->SetVectorParameterValue(TEXT("Color"), BodyColor);
		Head->SetMaterial(0, MID);
		Thorax->SetMaterial(0, MID);
		Gaster->SetMaterial(0, MID);
	}
}

ATreeActor* AAntPawn::GetTree()
{
	if (!TreeRef.IsValid()) TreeRef = ATreeActor::Get(this);
	return TreeRef.Get();
}

ADayDirector* AAntPawn::GetDirector()
{
	if (!DirectorRef.IsValid()) DirectorRef = ADayDirector::Get(this);
	return DirectorRef.Get();
}

ATrailNetwork* AAntPawn::GetTrails()
{
	ADayDirector* D = GetDirector();
	return D ? D->GetTrails() : nullptr;
}

void AAntPawn::SetWorker(int32 Index, int32 Path)
{
	WorkerIndex = Index;
	PathIndex = Path;
	LastRecordPos = GetActorLocation();
}

void AAntPawn::SetActionInput(bool bInCutHeld, bool bInFollowHeld, bool bInGripHeld)
{
	bCutHeld = bInCutHeld;
	bFollowHeld = bInFollowHeld;
	bGripHeld = bInGripHeld;
}

void AAntPawn::UnPossessed()
{
	Super::UnPossessed();
	bCutHeld = false;
	bFollowHeld = false;
	bGripHeld = false;
	if (Movement)
	{
		Movement->SetMoveInput(FVector2D::ZeroVector, FVector::ForwardVector, FVector::UpVector);
		Movement->SetSprintInput(false);
		if (Movement->GetState() == EAntState::Riding) Movement->StopFollow();
		if (Movement->GetState() == EAntState::Gripping) Movement->SetState(EAntState::Walking);
	}
}

void AAntPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Movement) return;

	// A leaf within reach of the mouth, for the prompt and for cutting.
	LeafInReach = INDEX_NONE;
	const EAntState State = Movement->GetState();
	if (IsPlayerControlled() && !IsCarrying() && (State == EAntState::Walking || State == EAntState::Cutting))
	{
		if (ATreeActor* Tree = GetTree())
		{
			LeafInReach = Tree->FindLeafNear(GetActorTransform().TransformPosition(MandibleOffset), Cut.Reach);
		}
	}

	UpdateCutting(DeltaSeconds);
	UpdateRiding(DeltaSeconds);
	UpdateGrip();
	UpdateRecording();
	UpdateWind(DeltaSeconds);

	ADayDirector* Director = GetDirector();
	if (!Director) return;
	if (Movement->ConsumeFollowFinished() && Movement->GetState() == EAntState::Returning)
	{
		Director->OnAntReachedNest(this);
		return;
	}
	if (Movement->GetState() == EAntState::WalkingHome)
	{
		if (ANestActor* Nest = Director->GetNest())
		{
			const FVector TrunkBase = GetTree() ? GetTree()->GetSkeleton().TrunkBase : FVector::ZeroVector;
			Movement->SetHomeTarget(Nest->GetActorLocation(), TrunkBase, (float)FVector::Dist2D(Nest->GetActorLocation(), TrunkBase) - 300.f);
		}
		if (Director->IsNearNest(Movement->GetSurfacePosition()))
		{
			Director->OnAntReachedNest(this);
			return;
		}
	}
	// Walking home yourself with a fragment also counts.
	if (IsPlayerControlled() && IsCarrying() && Movement->GetState() == EAntState::Walking &&
		Director->IsNearNest(Movement->GetSurfacePosition()))
	{
		Director->OnAntReachedNest(this);
	}
}

void AAntPawn::UpdateCutting(float Dt)
{
	ATreeActor* Tree = GetTree();
	if (!Tree)
	{
		bPrevCutHeld = bCutHeld;
		return;
	}
	if (CuttingLeaf == INDEX_NONE)
	{
		const bool bPressed = bCutHeld && !bPrevCutHeld;
		if (bPressed && LeafInReach != INDEX_NONE && !IsCarrying() && Movement->GetState() == EAntState::Walking)
		{
			CuttingLeaf = LeafInReach;
			CutSize = 0.f;
			CutCap = Tree->LeafSizeCap(CuttingLeaf, Cut.LeafUsePerCut);
			Movement->SetState(EAntState::Cutting);
			UE_LOG(LogLeafline, Log, TEXT("%s: cutting leaf %d (size cap %.2f)"), *GetName(), CuttingLeaf, CutCap);
		}
	}
	else if (bCutHeld && IsPlayerControlled())
	{
		CutSize = FMath::Min(CutCap, CutSize + Cut.CutRate * CutRateMultiplier * Dt);
	}
	else
	{
		if (CutSize >= Cut.MinSize && Tree->BiteLeaf(CuttingLeaf, CutSize, DiameterForSize(CutSize), Cut.LeafUsePerCut))
		{
			SetFragment(CutSize);
			UE_LOG(LogLeafline, Log, TEXT("%s: cut a fragment of size %.2f (mass %.1f, %.0f cm) from leaf %d"),
				*GetName(), CutSize, CarriedMass, DiameterForSize(CutSize), CuttingLeaf);
		}
		else
		{
			UE_LOG(LogLeafline, Log, TEXT("%s: cut cancelled at size %.2f"), *GetName(), CutSize);
		}
		CuttingLeaf = INDEX_NONE;
		CutSize = 0.f;
		if (Movement->GetState() == EAntState::Cutting) Movement->SetState(EAntState::Walking);
	}
	bPrevCutHeld = bCutHeld;
}

bool AAntPawn::DebugCutNow(float Size01)
{
	ATreeActor* Tree = GetTree();
	if (!Tree || IsCarrying()) return false;
	const int32 Leaf = Tree->FindLeafNear(GetActorTransform().TransformPosition(MandibleOffset), Cut.Reach);
	if (Leaf == INDEX_NONE) return false;
	const float Size = FMath::Min(Size01, Tree->LeafSizeCap(Leaf, Cut.LeafUsePerCut));
	if (!Tree->BiteLeaf(Leaf, Size, DiameterForSize(Size), Cut.LeafUsePerCut)) return false;
	SetFragment(Size);
	UE_LOG(LogLeafline, Log, TEXT("%s: cut a fragment of size %.2f (mass %.1f) from leaf %d"), *GetName(), Size, CarriedMass, Leaf);
	return true;
}

void AAntPawn::UpdateRiding(float Dt)
{
	ATrailNetwork* Trails = GetTrails();
	const EAntState State = Movement->GetState();
	if (!Trails || !IsPlayerControlled())
	{
		bTrailInReach = false;
		return;
	}
	TrailCheckTimer -= Dt;
	if (TrailCheckTimer <= 0.f && State == EAntState::Walking)
	{
		TrailCheckTimer = 0.2f;
		int32 P, I;
		ADayDirector* D = GetDirector();
		bTrailInReach = Trails->FindRideStart(Movement->GetSurfacePosition(), D ? D->Tuning.RideReach : 60.f, Movement->GetBodyForward(), P, I, PathIndex);
	}
	if (bFollowHeld && State == EAntState::Walking)
	{
		int32 Path, Index;
		ADayDirector* D = GetDirector();
		if (Trails->FindRideStart(Movement->GetSurfacePosition(), D ? D->Tuning.RideReach : 60.f, Movement->GetBodyForward(), Path, Index, PathIndex))
		{
			Movement->StartFollow(Trails->GetPoints(Path), Index, 1, EAntState::Riding);
			UE_LOG(LogLeafline, Log, TEXT("%s: riding trail %d from point %d of %d"), *GetName(), Path, Index, Trails->GetPoints(Path).Num());
		}
	}
	else if (!bFollowHeld && State == EAntState::Riding)
	{
		Movement->StopFollow();
	}
}

bool AAntPawn::IsGripping() const
{
	return Movement && Movement->GetState() == EAntState::Gripping;
}

void AAntPawn::UpdateGrip()
{
	if (!IsPlayerControlled()) return;
	const EAntState State = Movement->GetState();
	if (bGripHeld && (State == EAntState::Walking || State == EAntState::Riding))
	{
		if (State == EAntState::Riding) Movement->StopFollow();
		Movement->SetState(EAntState::Gripping);
	}
	else if (!bGripHeld && State == EAntState::Gripping)
	{
		Movement->SetState(EAntState::Walking);
	}
}

float AAntPawn::GetGripMultiplier() const
{
	const ULeaflineGameInstance* GI = Cast<ULeaflineGameInstance>(GetGameInstance());
	return GI ? FMath::Max(GI->GetGripMultiplier(), 0.01f) : 1.f;
}

void AAntPawn::UpdateWind(float Dt)
{
	// Deterministic blow-off: Force = strength * peak * exposure * (body + fragment area) against the grip.
	const AWindSystem* Wind = AWindSystem::Get(this);
	if (!Wind) return;
	const EAntState State = Movement->GetState();
	const bool bCanBeBlown = State == EAntState::Walking || State == EAntState::Riding || State == EAntState::Returning ||
		State == EAntState::Cutting || State == EAntState::Gripping;
	if (!Wind->IsGustActive() || !bCanBeBlown)
	{
		if (bGustSeen && bCanBeBlown)
		{
			UE_LOG(LogLeafline, Log, TEXT("%s: rode out the gust (worst force/hold %.2f, exposure %.2f, fragment %.1f, gripped %d)"),
				*GetName(), GustMaxRatio, GustMaxExposure, CarriedMass, bGustGripped ? 1 : 0);
		}
		bGustSeen = false;
		GustMaxRatio = 0.f;
		GustMaxExposure = 0.f;
		bGustGripped = false;
		return;
	}
	WindCheckTimer -= Dt;
	if (WindCheckTimer > 0.f) return;
	WindCheckTimer = 0.25f;
	float Exposure = 0.f;
	const float Force = Wind->ComputeForce(Movement->GetSurfacePosition(), Movement->GetSurfaceNormal(),
		IsCarrying() ? DiameterForSize(CarriedSize) : 0.f, &Exposure);
	const float Hold = Wind->GetHold(GetGripMultiplier(), State == EAntState::Gripping);
	bGustSeen = true;
	bGustGripped |= State == EAntState::Gripping;
	if (Hold > 0.f && Force / Hold > GustMaxRatio)
	{
		GustMaxRatio = Force / Hold;
		GustMaxExposure = Exposure;
	}
	if (Force > Hold) BlowOff(Force, Hold, Exposure);
}

void AAntPawn::BlowOff(float Force, float Hold, float Exposure)
{
	UE_LOG(LogLeafline, Log, TEXT("%s: blown off (force %.2f > hold %.2f, exposure %.2f, fragment %.1f, state %s)"),
		*GetName(), Force, Hold, Exposure, CarriedMass, *UEnum::GetValueAsString(Movement->GetState()));
	if (IsCutting())
	{
		CuttingLeaf = INDEX_NONE;
		CutSize = 0.f;
	}
	ClearFragment();
	bGustSeen = false;
	const AWindSystem* Wind = AWindSystem::Get(this);
	if (ADayDirector* D = GetDirector())
	{
		if (ANestActor* Nest = D->GetNest())
		{
			const FVector TrunkBase = GetTree() ? GetTree()->GetSkeleton().TrunkBase : FVector::ZeroVector;
			Movement->SetHomeTarget(Nest->GetActorLocation(), TrunkBase, (float)FVector::Dist2D(Nest->GetActorLocation(), TrunkBase) - 300.f);
		}
	}
	Movement->StartFall(Wind ? Wind->GetDirection() : FVector::ForwardVector);
	if (ADayDirector* D = GetDirector()) D->OnBlownOff(this);
}

void AAntPawn::UpdateRecording()
{
	ATrailNetwork* Trails = GetTrails();
	if (PathIndex == INDEX_NONE || !Trails) return;
	const EAntState State = Movement->GetState();
	if (State == EAntState::Returning || State == EAntState::Falling || State == EAntState::WalkingHome) return;
	const ADayDirector* D = GetDirector();
	const float Spacing = D ? D->Tuning.PathSpacing : 10.f;
	const FVector P = Movement->GetSurfacePosition();
	if (FVector::DistSquared(P, LastRecordPos) >= Spacing * Spacing)
	{
		Trails->AddPoint(PathIndex, P, Movement->GetSurfaceNormal());
		LastRecordPos = P;
	}
}

void AAntPawn::StartReturn()
{
	ATrailNetwork* Trails = GetTrails();
	if (!Trails || PathIndex == INDEX_NONE) return;
	Trails->AddPoint(PathIndex, Movement->GetSurfacePosition(), Movement->GetSurfaceNormal());
	const TArray<FTrailPoint>& Points = Trails->GetPoints(PathIndex);
	Movement->StartFollow(Points, Points.Num() - 1, -1, EAntState::Returning);
	UE_LOG(LogLeafline, Log, TEXT("%s: returning along %d path points"), *GetName(), Points.Num());
}

void AAntPawn::SetFragment(float Size01)
{
	CarriedSize = FMath::Clamp(Size01, 0.f, 1.f);
	CarriedMass = MassForSize(CarriedSize);
	Movement->CarriedMass = CarriedMass;
	BuildFragmentMesh(DiameterForSize(CarriedSize));
	FragmentMesh->SetVisibility(true);
}

void AAntPawn::ClearFragment()
{
	CarriedSize = 0.f;
	CarriedMass = 0.f;
	if (Movement) Movement->CarriedMass = 0.f;
	FragmentMesh->SetVisibility(false);
}

void AAntPawn::BuildFragmentMesh(float Diameter)
{
	// A rough disc of leaf, upright in the ant's XZ plane, held by its lower edge like a sail.
	const int32 Segs = 20;
	const float R = Diameter * 0.5f;
	const FVector Center(-2.f, 0.f, R * 0.95f);
	const float Phase = FMath::FRandRange(0.f, 2.f * PI);
	TArray<FVector> V, N;
	TArray<FVector2D> UV;
	TArray<FLinearColor> Col;
	TArray<FProcMeshTangent> Tan;
	TArray<int32> Tri;
	for (int32 Face = 0; Face < 2; ++Face)
	{
		const FVector Normal = Face == 0 ? FVector::RightVector : FVector::LeftVector;
		const int32 Base = V.Num();
		V.Add(Center);
		N.Add(Normal);
		UV.Add(FVector2D(0.5, 0.5));
		Col.Add(FLinearColor::White);
		Tan.Add(FProcMeshTangent(FVector::ForwardVector, false));
		for (int32 i = 0; i < Segs; ++i)
		{
			const float A = 2.f * PI * i / Segs;
			const float Rr = R * (1.f + 0.07f * FMath::Sin(3.f * A + Phase) + 0.04f * FMath::Sin(7.f * A));
			V.Add(Center + FVector(FMath::Cos(A) * Rr, 0.f, FMath::Sin(A) * Rr));
			N.Add(Normal);
			UV.Add(FVector2D(0.5 + 0.5 * FMath::Cos(A), 0.5 + 0.5 * FMath::Sin(A)));
			Col.Add(FLinearColor::White);
			Tan.Add(FProcMeshTangent(FVector::ForwardVector, false));
		}
		for (int32 i = 0; i < Segs; ++i)
		{
			const int32 A = Base + 1 + i, B = Base + 1 + (i + 1) % Segs;
			// UE front faces: (b - a) x (c - a) points away from the viewer, who looks down the normal.
			const FVector Cross = FVector::CrossProduct(V[A] - V[Base], V[B] - V[Base]);
			if (FVector::DotProduct(Cross, Normal) > 0.0) Tri.Append({ Base, B, A });
			else Tri.Append({ Base, A, B });
		}
	}
	FragmentMesh->CreateMeshSection_LinearColor(0, V, Tri, N, UV, {}, {}, {}, Col, Tan, false, false);
	FragmentMesh->SetRelativeRotation(FRotator(12.f, 35.f, 0.f)); // yawed so the camera behind sees its face, not its edge; // leans back slightly over the body (positive pitch tilts +Z toward -X)
	if (BasicMaterial)
	{
		if (!FragmentMID) FragmentMID = UMaterialInstanceDynamic::Create(BasicMaterial, this);
		FragmentMID->SetVectorParameterValue(TEXT("Color"), FragmentColor);
		FragmentMesh->SetMaterial(0, FragmentMID);
	}
}
