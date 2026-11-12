#include "Ant/AntPawn.h"
#include "Ant/AntMovementComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
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
	PrimaryActorTick.bCanEverTick = false;
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
