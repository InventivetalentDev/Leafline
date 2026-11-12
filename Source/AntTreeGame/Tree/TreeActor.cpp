#include "Tree/TreeActor.h"
#include "AntTreeGame.h"
#include "Tree/TreeSkeleton.h"
#include "Tree/TreeMeshBuilder.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "EngineUtils.h"

namespace
{
	UProceduralMeshComponent* MakeMeshComponent(AActor* Owner, USceneComponent* Parent, const TCHAR* Name)
	{
		UProceduralMeshComponent* C = NewObject<UProceduralMeshComponent>(Owner, Name, RF_Transient);
		C->SetupAttachment(Parent);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(true);
		C->bUseAsyncCooking = false;
		C->RegisterComponent();
		return C;
	}
}

ATreeActor::ATreeActor()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMat(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (BasicMat.Succeeded()) FallbackMaterial = BasicMat.Object;
}

void ATreeActor::Grow(int32 Seed)
{
	const double StartTime = FPlatformTime::Seconds();
	TreeSkeleton::Grow(Skeleton, Params, Seed);
	Field.Build(Skeleton);

	FTreeMeshData Data;
	TreeMeshBuilder::Build(Skeleton, Data);

	if (!WoodMesh) WoodMesh = MakeMeshComponent(this, RootComponent, TEXT("WoodMesh"));
	WoodMesh->ClearAllMeshSections();
	WoodMesh->CreateMeshSection_LinearColor(0, Data.Vertices, Data.Triangles, Data.Normals, Data.UV0, {}, {}, {},
		Data.Colors, Data.Tangents, false, false);

	UMaterialInterface* Mat = BarkMaterial;
	if (!Mat && FallbackMaterial)
	{
		if (!BarkMID) BarkMID = UMaterialInstanceDynamic::Create(FallbackMaterial, this);
		BarkMID->SetVectorParameterValue(TEXT("Color"), BarkColor);
		Mat = BarkMID;
	}
	if (Mat) WoodMesh->SetMaterial(0, Mat);
	BuildGround();

	// Leaves: one mesh section per limb, so a cut rebuilds only a small chunk.
	Leaves.Build(Skeleton, Field);
	if (!LeafMesh) LeafMesh = MakeMeshComponent(this, RootComponent, TEXT("LeafMesh"));
	LeafMesh->ClearAllMeshSections();
	LeafMaterialInUse = LeafMaterial;
	if (!LeafMaterialInUse && FallbackMaterial)
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(FallbackMaterial, this);
		MID->SetVectorParameterValue(TEXT("Color"), LeafColor);
		LeafMaterialInUse = MID;
	}
	for (int32 c = 0; c < Leaves.NumChunks(); ++c) BuildLeafChunk(c);

	UE_LOG(LogLeafline, Log, TEXT("Tree grown: seed %d in %.0f ms"), Seed, (FPlatformTime::Seconds() - StartTime) * 1000.0);
}

void ATreeActor::BuildLeafChunk(int32 Chunk)
{
	FTreeMeshData Data;
	Leaves.BuildChunkMesh(Chunk, Data);
	LeafMesh->CreateMeshSection_LinearColor(Chunk, Data.Vertices, Data.Triangles, Data.Normals, Data.UV0, {}, {}, {},
		Data.Colors, Data.Tangents, false, false);
	if (LeafMaterialInUse) LeafMesh->SetMaterial(Chunk, LeafMaterialInUse);
}

bool ATreeActor::BiteLeaf(int32 Leaf, float Size01, float DiameterCm, float UsePerCut)
{
	if (!Leaves.ApplyBite(Leaf, Size01, DiameterCm, UsePerCut)) return false;
	BuildLeafChunk(Leaves.Get(Leaf).Chunk);
	UE_LOG(LogLeafline, Log, TEXT("Leaf %d bitten: size %.2f, %d bites, %.0f%% left"), Leaf, Size01, Leaves.Get(Leaf).Bites,
		Leaves.Get(Leaf).Remaining * 100.f);
	return true;
}

void ATreeActor::BuildGround()
{
	// A polar disk at the field's ground height, rings spaced wider with distance. Wood mesh section 1.
	const FVector C(Skeleton.TrunkBase.X, Skeleton.TrunkBase.Y, Field.GroundZ);
	const int32 Around = 160;
	TArray<double> Radii;
	for (double R = 200.0; R < 60000.0; R *= 1.12) Radii.Add(R);
	TArray<FVector> V, N;
	TArray<FVector2D> UV;
	TArray<FLinearColor> Col;
	TArray<FProcMeshTangent> Tan;
	TArray<int32> Tri;
	auto AddVert = [&](const FVector& P)
	{
		V.Add(P);
		N.Add(FVector::UpVector);
		UV.Add(FVector2D(P.X, P.Y));
		Col.Add(FLinearColor::Black);
		Tan.Add(FProcMeshTangent(FVector::ForwardVector, false));
	};
	AddVert(C);
	for (double R : Radii)
	{
		for (int32 j = 0; j < Around; ++j)
		{
			const double A = 2.0 * PI * j / Around;
			AddVert(C + FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, 0.0));
		}
	}
	// UE front faces: (b - a) x (c - a) points away from the viewer, so for an upward face it points down.
	auto AddTri = [&](int32 A, int32 B, int32 Cc)
	{
		if (FVector::CrossProduct(V[B] - V[A], V[Cc] - V[A]).Z > 0.0) Tri.Append({ A, Cc, B });
		else Tri.Append({ A, B, Cc });
	};
	for (int32 j = 0; j < Around; ++j) AddTri(0, 1 + j, 1 + (j + 1) % Around);
	for (int32 k = 0; k + 1 < Radii.Num(); ++k)
	{
		const int32 R0 = 1 + k * Around, R1 = 1 + (k + 1) * Around;
		for (int32 j = 0; j < Around; ++j)
		{
			const int32 J1 = (j + 1) % Around;
			AddTri(R0 + j, R0 + J1, R1 + j);
			AddTri(R0 + J1, R1 + J1, R1 + j);
		}
	}
	WoodMesh->CreateMeshSection_LinearColor(1, V, Tri, N, UV, {}, {}, {}, Col, Tan, false, false);
	if (FallbackMaterial)
	{
		if (!GroundMID) GroundMID = UMaterialInstanceDynamic::Create(FallbackMaterial, this);
		GroundMID->SetVectorParameterValue(TEXT("Color"), GroundColor);
		WoodMesh->SetMaterial(1, GroundMID);
	}
}

FTransform ATreeActor::GetNestTransform(int32 Index) const
{
	if (Skeleton.NestSpots.Num() == 0)
	{
		return FTransform(FRotator::ZeroRotator, FVector(1500.0, 0.0, 0.0));
	}
	const FVector Spot = Skeleton.NestSpots[Index % Skeleton.NestSpots.Num()];
	// Face the trunk: the first thing a worker does is climb.
	FVector ToTrunk = Skeleton.TrunkBase - Spot;
	ToTrunk.Z = 0.0;
	return FTransform(ToTrunk.GetSafeNormal().Rotation(), Spot);
}

ATreeActor* ATreeActor::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World) return nullptr;
	for (TActorIterator<ATreeActor> It(const_cast<UWorld*>(World)); It; ++It)
	{
		return *It;
	}
	return nullptr;
}
