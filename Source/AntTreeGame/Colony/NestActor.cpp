#include "Colony/NestActor.h"
#include "AntTreeGame.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ANestActor::ANestActor()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMat(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (BasicMat.Succeeded()) BasicMaterial = BasicMat.Object;

	// Placeholder: a low soil mound (a flattened sphere) around a dark entrance.
	Mound = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mound"));
	Mound->SetupAttachment(RootComponent);
	if (SphereMesh.Succeeded()) Mound->SetStaticMesh(SphereMesh.Object);
	Mound->SetRelativeScale3D(MoundScale);
	Mound->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Hole = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Hole"));
	Hole->SetupAttachment(RootComponent);
	if (CylinderMesh.Succeeded()) Hole->SetStaticMesh(CylinderMesh.Object);
	Hole->SetRelativeLocation(FVector(0.f, 0.f, 7.2f));
	Hole->SetRelativeScale3D(FVector(0.8f, 0.8f, 0.01f));
	Hole->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ANestActor::BeginPlay()
{
	Super::BeginPlay();
	if (BasicMaterial)
	{
		UMaterialInstanceDynamic* MoundMID = UMaterialInstanceDynamic::Create(BasicMaterial, this);
		MoundMID->SetVectorParameterValue(TEXT("Color"), MoundColor);
		Mound->SetMaterial(0, MoundMID);
		UMaterialInstanceDynamic* HoleMID = UMaterialInstanceDynamic::Create(BasicMaterial, this);
		HoleMID->SetVectorParameterValue(TEXT("Color"), HoleColor);
		Hole->SetMaterial(0, HoleMID);
	}
}

void ANestActor::PlayDeposit(float Mass)
{
	Pulse = FMath::Clamp(0.4f + Mass * 0.12f, 0.f, 1.f);
	UE_LOG(LogLeafline, Log, TEXT("Nest: deposit of %.1f"), Mass);
}

void ANestActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (Pulse <= 0.f) return;
	Pulse = FMath::Max(0.f, Pulse - DeltaTime * 1.5f);
	const float S = 1.f + 0.12f * FMath::Sin(Pulse * PI) * Pulse;
	Mound->SetRelativeScale3D(MoundScale * FVector(S, S, 1.f));
}
