#include "Colony/NestActor.h"
#include "AntTreeGame.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// SM_Nest (Tools/blender/make_nest.py) is authored for the default mound, 120 cm radius and 7 cm high, with its origin
	// at the mound centre on the ground. Its surface follows the walking field there; other sizes scale it.
	constexpr float NestMeshRadius = 120.f;
	constexpr float NestMeshHeight = 7.f;
}

ANestActor::ANestActor()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMat(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (BasicMat.Succeeded()) BasicMaterial = BasicMat.Object;
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SoilMat(TEXT("/Game/Leafline/Materials/M_Ground.M_Ground"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DarkMat(TEXT("/Game/Leafline/Materials/M_Dark.M_Dark"));
	if (SoilMat.Succeeded()) SoilMaterial = SoilMat.Object;
	if (DarkMat.Succeeded()) DarkMaterial = DarkMat.Object;

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
	if (EnsureNestMesh()) return;
	if (SoilMaterial && DarkMaterial)
	{
		Mound->SetMaterial(0, SoilMaterial);
		Hole->SetMaterial(0, DarkMaterial);
	}
	else if (BasicMaterial)
	{
		UMaterialInstanceDynamic* MoundMID = UMaterialInstanceDynamic::Create(BasicMaterial, this);
		MoundMID->SetVectorParameterValue(TEXT("Color"), MoundColor);
		Mound->SetMaterial(0, MoundMID);
		UMaterialInstanceDynamic* HoleMID = UMaterialInstanceDynamic::Create(BasicMaterial, this);
		HoleMID->SetVectorParameterValue(TEXT("Color"), HoleColor);
		Hole->SetMaterial(0, HoleMID);
	}
}

bool ANestActor::EnsureNestMesh()
{
	if (bNestMeshTried) return NestMesh != nullptr;
	bNestMeshTried = true;
	// Loaded by path (the folder is always cooked) so a missing asset only falls back to the sphere and disc.
	// SM_Nest2 is the same FBX imported with its vertex colours (the first import, SM_Nest, dropped them: the FBX
	// importer's default is to ignore vertex colours, and the whole mound then shaded as the black entrance).
	NestMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Leafline/Meshes/SM_Nest2.SM_Nest2"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!NestMesh) NestMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Leafline/Meshes/SM_Nest.SM_Nest"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!NestMesh)
	{
		UE_LOG(LogLeafline, Warning, TEXT("SM_Nest not found; the nest uses the sphere and disc"));
		return false;
	}
	Mound->SetStaticMesh(NestMesh);
	Mound->SetRelativeLocation(FVector::ZeroVector);
	// Slot 0 soil, slot 1 the leaf scraps lying on the mound.
	UMaterialInterface* Soil = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Leafline/Materials/M_NestSoil.M_NestSoil"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Soil)
	{
		UE_LOG(LogLeafline, Warning, TEXT("M_NestSoil not found; the nest uses M_Ground"));
		Soil = SoilMaterial;
	}
	if (Soil) Mound->SetMaterial(0, Soil);
	// The scraps get a plain dull leaf green: M_Leaf's backlight and blade maths made 5 cm scraps lying on the soil glow
	// like fireflies (look pass 2 check).
	if (BasicMaterial)
	{
		UMaterialInstanceDynamic* ScrapMID = UMaterialInstanceDynamic::Create(BasicMaterial, this);
		ScrapMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.07f, 0.13f, 0.035f));
		Mound->SetMaterial(1, ScrapMID);
	}
	// The entrance is in the mesh's vertex colours; the disc goes.
	Hole->SetVisibility(false);
	MoundScale = FVector(MoundRadius / NestMeshRadius, MoundRadius / NestMeshRadius, MoundHeight / NestMeshHeight);
	Mound->SetRelativeScale3D(MoundScale);
	UE_LOG(LogLeafline, Log, TEXT("Nest: SM_Nest for a %.0f x %.0f cm mound"), MoundRadius, MoundHeight);
	return true;
}

void ANestActor::SetMoundSize(float Radius, float Height)
{
	MoundRadius = Radius;
	MoundHeight = Height;
	if (EnsureNestMesh())
	{
		MoundScale = FVector(Radius / NestMeshRadius, Radius / NestMeshRadius, Height / NestMeshHeight);
		Mound->SetRelativeScale3D(MoundScale);
		return;
	}
	// The engine sphere has a 50 cm radius.
	MoundScale = FVector(Radius / 50.f, Radius / 50.f, Height / 50.f);
	Mound->SetRelativeScale3D(MoundScale);
	Hole->SetRelativeLocation(FVector(0.f, 0.f, Height + 0.2f));
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
	// SM_Nest follows the walking surface: a sideways pulse lifts the rim off it (about 1 cm even at 0.05), and deposits
	// come in a stream during the harvest, so the mesh does not pulse; the deposit sound and the "+N" still mark it.
	const float Amp = NestMesh ? 0.f : 0.12f;
	const float S = 1.f + Amp * FMath::Sin(Pulse * PI) * Pulse;
	Mound->SetRelativeScale3D(MoundScale * FVector(S, S, 1.f));
}
