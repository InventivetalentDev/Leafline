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
	// Procedural materials built over MCP (maths only, no textures).
	// M_TreeBark: world-space bark with the camera cutaway. M_Bark is the older UV-based version.
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BarkMat(TEXT("/Game/Leafline/Materials/M_TreeBark.M_TreeBark"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> OldBarkMat(TEXT("/Game/Leafline/Materials/M_Bark.M_Bark"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> LeafMat(TEXT("/Game/Leafline/Materials/M_Leaf.M_Leaf"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> GroundMat(TEXT("/Game/Leafline/Materials/M_Ground.M_Ground"));
	if (BarkMat.Succeeded()) BarkMaterial = BarkMat.Object;
	else if (OldBarkMat.Succeeded()) BarkMaterial = OldBarkMat.Object;
	if (LeafMat.Succeeded()) LeafMaterial = LeafMat.Object;
	if (GroundMat.Succeeded()) GroundMaterial = GroundMat.Object;
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
	WoodMesh->CreateMeshSection_LinearColor(0, Data.Vertices, Data.Triangles, Data.Normals, Data.UV0, Data.UV1, {}, {},
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
	BuildGrove();

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

void ATreeActor::BuildGrove()
{
	// Plain trunks with a few bare limbs in a loose ring far out, drawn with the bark material and left to the haze.
	// Built like the real tree (control nodes, samples, the same tube builder) but coarser, and never walked on.
	FTreeSkeleton Grove;
	Grove.Params = Params;
	Grove.Params.AroundSpacing = 120.f;
	Grove.Params.MaxAroundVerts = 40;
	FRandomStream R(Skeleton.Seed * 7919 + 17);
	const FVector C(Skeleton.TrunkBase.X, Skeleton.TrunkBase.Y, Field.GroundZ);
	const int32 Count = FMath::Max(0, Params.GroveCount);
	for (int32 k = 0; k < Count; ++k)
	{
		const float Az = 2.f * PI * k / FMath::Max(Count, 1) + R.FRandRange(-0.18f, 0.18f);
		const float Dist = R.FRandRange(Params.GroveMinDistance, Params.GroveMaxDistance);
		const FVector Base = C + FVector(FMath::Cos(Az), FMath::Sin(Az), 0.f) * Dist;
		const float R0 = R.FRandRange(500.f, 1100.f);
		const float H = R.FRandRange(22000.f, 38000.f);
		const FVector Lean = FVector(R.FRandRange(-1.f, 1.f), R.FRandRange(-1.f, 1.f), 0.f).GetSafeNormal() * R.FRandRange(0.01f, 0.04f);
		auto RadiusAt = [&](float Z)
		{
			const float Zc = FMath::Max(Z, 0.f);
			return (40.f + (R0 - 40.f) * FMath::Pow(FMath::Max(1.f - Zc / H, 0.f), 0.85f)) * (1.f + 0.6f * FMath::Exp(-Zc / 600.f));
		};
		FTreeBranch T;
		T.Level = 0;
		T.Random = R.FRand();
		constexpr int32 N = 16;
		for (int32 i = 0; i <= N; ++i)
		{
			const float Z = -300.f + (H + 300.f) * FMath::Pow(float(i) / N, 1.4f);
			T.Nodes.Add({ Base + Lean * FMath::Max(Z, 0.f) + FVector(0.f, 0.f, Z), RadiusAt(Z) });
		}
		TreeSkeleton::BuildSamples(T, Grove.Params);
		Grove.Branches.Add(MoveTemp(T));

		const int32 Limbs = R.RandRange(3, 5);
		for (int32 l = 0; l < Limbs; ++l)
		{
			const float Z0 = H * R.FRandRange(0.35f, 0.85f);
			const float A = R.FRandRange(0.f, 2.f * PI);
			FVector Dir = (FVector(FMath::Cos(A), FMath::Sin(A), 0.f) + FVector(0.f, 0.f, R.FRandRange(0.3f, 0.9f))).GetSafeNormal();
			const float Len = R.FRandRange(3000.f, 9000.f);
			const float LR = RadiusAt(Z0) * 0.35f;
			FTreeBranch B;
			B.Level = 1;
			B.Random = R.FRand();
			FVector P = Base + Lean * Z0 + FVector(0.f, 0.f, Z0);
			constexpr int32 M = 6;
			for (int32 i = 0; i <= M; ++i)
			{
				B.Nodes.Add({ P, FMath::Lerp(LR, 30.f, float(i) / M) });
				Dir = (Dir + FVector(0.f, 0.f, 0.06f)).GetSafeNormal();
				P += Dir * (Len / M);
			}
			TreeSkeleton::BuildSamples(B, Grove.Params);
			Grove.Branches.Add(MoveTemp(B));
		}
	}

	FTreeMeshData Data;
	TreeMeshBuilder::Build(Grove, Data);
	if (!GroveMesh)
	{
		GroveMesh = MakeMeshComponent(this, RootComponent, TEXT("GroveMesh"));
		GroveMesh->SetCastShadow(false);
	}
	GroveMesh->ClearAllMeshSections();
	if (Data.Vertices.Num() == 0) return;
	GroveMesh->CreateMeshSection_LinearColor(0, Data.Vertices, Data.Triangles, Data.Normals, Data.UV0, Data.UV1, {}, {},
		Data.Colors, Data.Tangents, false, false);
	if (UMaterialInterface* Mat = WoodMesh ? WoodMesh->GetMaterial(0) : nullptr) GroveMesh->SetMaterial(0, Mat);
	UE_LOG(LogLeafline, Log, TEXT("Grove: %d distant trunks, %d triangles"), Count, Data.Triangles.Num() / 3);
}

void ATreeActor::BuildGround()
{
	// A polar disk at the field's ground height, rings spaced wider with distance. Wood mesh section 1.
	const FVector C(Skeleton.TrunkBase.X, Skeleton.TrunkBase.Y, Field.GroundZ);
	const int32 Around = 160;
	TArray<double> Radii;
	for (double R = 200.0; R < Params.GroundRadius; R *= 1.12) Radii.Add(R);
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
	if (GroundMaterial)
	{
		WoodMesh->SetMaterial(1, GroundMaterial);
	}
	else if (FallbackMaterial)
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
