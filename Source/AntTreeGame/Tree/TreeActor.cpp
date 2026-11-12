#include "Tree/TreeActor.h"
#include "AntTreeGame.h"
#include "Tree/TreeSkeleton.h"
#include "Tree/TreeMeshBuilder.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "EngineUtils.h"

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

	if (!WoodMesh)
	{
		WoodMesh = NewObject<UProceduralMeshComponent>(this, TEXT("WoodMesh"), RF_Transient);
		WoodMesh->SetupAttachment(RootComponent);
		WoodMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		WoodMesh->SetCastShadow(true);
		WoodMesh->bUseAsyncCooking = false;
		WoodMesh->RegisterComponent();
	}
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

	// Ground: a polar disk at the field's ground height, rings spaced wider with distance. Section 1.
	{
		const FVector C(Skeleton.TrunkBase.X, Skeleton.TrunkBase.Y, Field.GroundZ);
		const int32 Around = 160;
		TArray<double> Radii;
		for (double R = 200.0; R < 60000.0; R *= 1.12) Radii.Add(R);
		TArray<FVector> V, N;
		TArray<FVector2D> UV;
		TArray<FLinearColor> Col;
		TArray<FProcMeshTangent> Tan;
		TArray<int32> Tri;
		V.Add(C);
		N.Add(FVector::UpVector);
		UV.Add(FVector2D(C.X, C.Y));
		Col.Add(FLinearColor::Black);
		Tan.Add(FProcMeshTangent(FVector::ForwardVector, false));
		for (double R : Radii)
		{
			for (int32 j = 0; j < Around; ++j)
			{
				const double A = 2.0 * PI * j / Around;
				const FVector P = C + FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, 0.0);
				V.Add(P);
				N.Add(FVector::UpVector);
				UV.Add(FVector2D(P.X, P.Y));
				Col.Add(FLinearColor::Black);
				Tan.Add(FProcMeshTangent(FVector::ForwardVector, false));
			}
		}
		// UE front faces: (b - a) x (c - a) points away from the viewer, so for an upward face it points down.
		for (int32 j = 0; j < Around; ++j)
		{
			const int32 A = 1 + j, B = 1 + (j + 1) % Around;
			Tri.Append({ 0, B, A });
		}
		for (int32 k = 0; k + 1 < Radii.Num(); ++k)
		{
			const int32 R0 = 1 + k * Around, R1 = 1 + (k + 1) * Around;
			for (int32 j = 0; j < Around; ++j)
			{
				const int32 J1 = (j + 1) % Around;
				Tri.Append({ R0 + j, R0 + J1, R1 + j, R0 + J1, R1 + J1, R1 + j });
			}
		}
		// Check winding once and flip if needed, so the ground is never back-facing.
		if (Tri.Num() >= 3 && FVector::CrossProduct(V[Tri[1]] - V[Tri[0]], V[Tri[2]] - V[Tri[0]]).Z > 0.0)
		{
			for (int32 t = 0; t + 2 < Tri.Num(); t += 3) Swap(Tri[t + 1], Tri[t + 2]);
		}
		WoodMesh->CreateMeshSection_LinearColor(1, V, Tri, N, UV, {}, {}, {}, Col, Tan, false, false);
		if (FallbackMaterial)
		{
			UMaterialInstanceDynamic* GroundMID = UMaterialInstanceDynamic::Create(FallbackMaterial, this);
			GroundMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.09f, 0.075f, 0.05f));
			WoodMesh->SetMaterial(1, GroundMID);
		}
	}

	UE_LOG(LogLeafline, Log, TEXT("Tree grown: seed %d in %.0f ms"), Seed, (FPlatformTime::Seconds() - StartTime) * 1000.0);
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
