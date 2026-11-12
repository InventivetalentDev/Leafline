#include "Colony/TrailNetwork.h"
#include "Algo/Reverse.h"
#include "AntTreeGame.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ATrailNetwork::ATrailNetwork()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("TrailMesh"));
	Mesh->SetupAttachment(RootComponent);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMat(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (BasicMat.Succeeded()) FallbackMaterial = BasicMat.Object;
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> TrailMat(TEXT("/Game/Leafline/Materials/M_Trail.M_Trail"));
	if (TrailMat.Succeeded()) TrailMaterial = TrailMat.Object;
}

FIntVector ATrailNetwork::Cell(const FVector& P) const
{
	return FIntVector(FMath::FloorToInt(P.X / CellSize), FMath::FloorToInt(P.Y / CellSize), FMath::FloorToInt(P.Z / CellSize));
}

void ATrailNetwork::ClearAll()
{
	Paths.Reset();
	Grid.Reset();
	ChunkSection.Reset();
	NextSection = 0;
	if (Mesh) Mesh->ClearAllMeshSections();
}

int32 ATrailNetwork::StartPath(const FVector& Pos, const FVector& Normal)
{
	const int32 Index = Paths.AddDefaulted();
	AddPoint(Index, Pos, Normal);
	return Index;
}

void ATrailNetwork::AddPoint(int32 PathIndex, const FVector& Pos, const FVector& Normal)
{
	if (!Paths.IsValidIndex(PathIndex)) return;
	FPath& Path = Paths[PathIndex];

	// Only draw where no other path's ribbon already runs.
	bool bNearOther = false;
	const FIntVector C = Cell(Pos);
	const double Merge2 = MergeDistance * MergeDistance;
	for (int32 X = -1; X <= 1 && !bNearOther; ++X)
		for (int32 Y = -1; Y <= 1 && !bNearOther; ++Y)
			for (int32 Z = -1; Z <= 1 && !bNearOther; ++Z)
			{
				if (const TArray<FIntPoint>* List = Grid.Find(C + FIntVector(X, Y, Z)))
				{
					for (const FIntPoint& Ref : *List)
					{
						if (Ref.X == PathIndex || !Paths[Ref.X].Drawn[Ref.Y]) continue;
						if (FVector::DistSquared(Paths[Ref.X].Points[Ref.Y].Pos, Pos) < Merge2)
						{
							bNearOther = true;
							break;
						}
					}
				}
			}

	const float PrevDist = Path.Dist.Num() ? Path.Dist.Last() : 0.f;
	const float Step = Path.Points.Num() ? (float)FVector::Dist(Path.Points.Last().Pos, Pos) : 0.f;
	const int32 Index = Path.Points.Add({ Pos, Normal });
	Path.Drawn.Add(bNearOther ? 0 : 1);
	Path.Dist.Add(PrevDist + Step);
	Grid.FindOrAdd(C).Add(FIntPoint(PathIndex, Index));
	// The segment ending at this point belongs to the chunk of the point before it.
	const int32 Chunk = FMath::Max(Index - 1, 0) / ChunkSize;
	Path.DirtyFromChunk = Path.DirtyFromChunk == INDEX_NONE ? Chunk : FMath::Min(Path.DirtyFromChunk, Chunk);
}

void ATrailNetwork::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	RebuildTimer -= DeltaTime;
	if (RebuildTimer > 0.f) return;
	RebuildTimer = 0.2f;
	for (int32 p = 0; p < Paths.Num(); ++p)
	{
		FPath& Path = Paths[p];
		if (Path.DirtyFromChunk == INDEX_NONE) continue;
		const int32 LastChunk = FMath::Max(Path.Points.Num() - 2, 0) / ChunkSize;
		for (int32 c = Path.DirtyFromChunk; c <= LastChunk; ++c) RebuildChunk(p, c);
		Path.DirtyFromChunk = INDEX_NONE;
	}
}

void ATrailNetwork::RebuildChunk(int32 PathIndex, int32 Chunk)
{
	FPath& Path = Paths[PathIndex];
	TArray<FVector> V, N;
	TArray<FVector2D> UV;
	TArray<FLinearColor> Col;
	TArray<FProcMeshTangent> Tan;
	TArray<int32> Tri;
	const int32 Num = Path.Points.Num();
	const int32 First = Chunk * ChunkSize;
	const int32 Last = FMath::Min((Chunk + 1) * ChunkSize, Num - 1);
	float Along = Path.Dist.IsValidIndex(First) ? Path.Dist[First] : 0.f;
	for (int32 i = First; i < Last; ++i)
	{
		const FTrailPoint& A = Path.Points[i];
		const FTrailPoint& B = Path.Points[i + 1];
		const float SegLen = (float)FVector::Dist(A.Pos, B.Pos);
		if (!Path.Drawn[i] || !Path.Drawn[i + 1] || SegLen > 60.f)
		{
			Along += SegLen;
			continue;
		}
		const FVector Dir = (B.Pos - A.Pos) / FMath::Max(SegLen, 0.01f);
		const FVector SideA = FVector::CrossProduct(A.Normal, Dir).GetSafeNormal() * (Width * 0.5f);
		const FVector SideB = FVector::CrossProduct(B.Normal, Dir).GetSafeNormal() * (Width * 0.5f);
		const int32 Base = V.Num();
		V.Append({ A.Pos + A.Normal * Lift - SideA, A.Pos + A.Normal * Lift + SideA, B.Pos + B.Normal * Lift - SideB, B.Pos + B.Normal * Lift + SideB });
		N.Append({ A.Normal, A.Normal, B.Normal, B.Normal });
		UV.Append({ FVector2D(0.0, Along), FVector2D(1.0, Along), FVector2D(0.0, Along + SegLen), FVector2D(1.0, Along + SegLen) });
		Col.Append({ FLinearColor::White, FLinearColor::White, FLinearColor::White, FLinearColor::White });
		Tan.Append({ FProcMeshTangent(Dir, false), FProcMeshTangent(Dir, false), FProcMeshTangent(Dir, false), FProcMeshTangent(Dir, false) });
		// UE front faces: (b - a) x (c - a) points away from the viewer, who looks down the normal.
		auto AddTri = [&](int32 I0, int32 I1, int32 I2)
		{
			const FVector Cross = FVector::CrossProduct(V[I1] - V[I0], V[I2] - V[I0]);
			if (FVector::DotProduct(Cross, A.Normal) > 0.0) Tri.Append({ I0, I2, I1 });
			else Tri.Append({ I0, I1, I2 });
		};
		AddTri(Base, Base + 1, Base + 2);
		AddTri(Base + 1, Base + 3, Base + 2);
		Along += SegLen;
	}
	int32* Found = ChunkSection.Find(FIntPoint(PathIndex, Chunk));
	const int32 Section = Found ? *Found : ChunkSection.Add(FIntPoint(PathIndex, Chunk), NextSection++);
	Mesh->CreateMeshSection_LinearColor(Section, V, Tri, N, UV, {}, {}, {}, Col, Tan, false, false);
	if (!MaterialInUse)
	{
		MaterialInUse = TrailMaterial;
		if (!MaterialInUse && FallbackMaterial)
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(FallbackMaterial, this);
			MID->SetVectorParameterValue(TEXT("Color"), TrailColor);
			MaterialInUse = MID;
		}
	}
	if (MaterialInUse) Mesh->SetMaterial(Section, MaterialInUse);
}

bool ATrailNetwork::FindRideStart(const FVector& P, float Reach, const FVector& PreferredDir, int32& OutPath, int32& OutIndex, int32 ExcludePath) const
{
	const int32 R = FMath::CeilToInt(Reach / CellSize);
	const FIntVector C = Cell(P);
	double BestScore = -TNumericLimits<double>::Max();
	const double Reach2 = (double)Reach * Reach;
	for (int32 X = -R; X <= R; ++X)
		for (int32 Y = -R; Y <= R; ++Y)
			for (int32 Z = -R; Z <= R; ++Z)
			{
				const TArray<FIntPoint>* List = Grid.Find(C + FIntVector(X, Y, Z));
				if (!List) continue;
				for (const FIntPoint& Ref : *List)
				{
					const TArray<FTrailPoint>& Pts = Paths[Ref.X].Points;
					const double D2 = FVector::DistSquared(Pts[Ref.Y].Pos, P);
					if (Ref.X == ExcludePath || D2 > Reach2 || Ref.Y + 1 >= Pts.Num()) continue;
					const FVector Out = (Pts[FMath::Min(Ref.Y + 2, Pts.Num() - 1)].Pos - Pts[FMath::Max(Ref.Y - 1, 0)].Pos).GetSafeNormal();
					const double Score = FVector::DotProduct(Out, PreferredDir) - FMath::Sqrt(D2) / Reach * 0.5 + Ref.X * 0.01;
					if (Score > BestScore)
					{
						BestScore = Score;
						OutPath = Ref.X;
						OutIndex = Ref.Y;
					}
				}
			}
	return BestScore > -TNumericLimits<double>::Max();
}

void ATrailNetwork::BuildReturnRoute(const FVector& P, const FVector& Normal, int32 OwnPath, float Reach,
	TFunctionRef<bool(const FVector&, const FVector&)> CanCross, TArray<FTrailPoint>& OutRoute) const
{
	OutRoute.Reset();
	int32 Path = INDEX_NONE, Index = INDEX_NONE;
	double Best = TNumericLimits<double>::Max();
	if (Paths.IsValidIndex(OwnPath) && Paths[OwnPath].Points.Num() > 0)
	{
		Path = OwnPath;
		Index = Paths[OwnPath].Points.Num() - 1;
		Best = Paths[OwnPath].Dist.IsValidIndex(Index) ? Paths[OwnPath].Dist[Index] : Best;
	}

	// Any trail point in reach that is nearer the nest along its path (another worker's scent line counts too).
	{
		const int32 R = FMath::CeilToInt(Reach / CellSize);
		const FIntVector C = Cell(P);
		const double Reach2 = (double)Reach * Reach;
		for (int32 X = -R; X <= R; ++X)
			for (int32 Y = -R; Y <= R; ++Y)
				for (int32 Z = -R; Z <= R; ++Z)
				{
					const TArray<FIntPoint>* List = Grid.Find(C + FIntVector(X, Y, Z));
					if (!List) continue;
					for (const FIntPoint& Ref : *List)
					{
						const FPath& Src = Paths[Ref.X];
						const FTrailPoint& Pt = Src.Points[Ref.Y];
						if (FVector::DistSquared(Pt.Pos, P) > Reach2 || FVector::DotProduct(Pt.Normal, Normal) < 0.3) continue;
						const double D = Src.Dist.IsValidIndex(Ref.Y) ? Src.Dist[Ref.Y] : TNumericLimits<double>::Max();
						if (D < Best && CanCross(P, Pt.Pos))
						{
							Best = D;
							Path = Ref.X;
							Index = Ref.Y;
						}
					}
				}
	}
	if (Path == INDEX_NONE) return;

	// Back along that path to the nest, cutting out loops.
	constexpr double LoopReach = 35.0;
	const int32 LR = FMath::CeilToInt(LoopReach / CellSize);
	const FPath& Src = Paths[Path];
	TArray<FTrailPoint> Rev;
	Rev.Add({ P, Normal });
	for (int32 i = Index; i >= 0;)
	{
		const FTrailPoint& Here = Src.Points[i];
		Rev.Add(Here);
		if (i == 0) break;
		int32 Jump = i - 1;
		const FIntVector C = Cell(Here.Pos);
		for (int32 X = -LR; X <= LR; ++X)
			for (int32 Y = -LR; Y <= LR; ++Y)
				for (int32 Z = -LR; Z <= LR; ++Z)
				{
					const TArray<FIntPoint>* List = Grid.Find(C + FIntVector(X, Y, Z));
					if (!List) continue;
					for (const FIntPoint& Ref : *List)
					{
						if (Ref.X != Path || Ref.Y >= i - 3 || Ref.Y >= Jump) continue;
						const FTrailPoint& Pt = Src.Points[Ref.Y];
						if (FVector::DistSquared(Pt.Pos, Here.Pos) <= LoopReach * LoopReach && FVector::DotProduct(Pt.Normal, Here.Normal) > 0.5
							&& CanCross(Here.Pos, Pt.Pos))
						{
							Jump = Ref.Y;
						}
					}
				}
		i = Jump;
	}
	Algo::Reverse(Rev);
	OutRoute = MoveTemp(Rev);
}

float ATrailNetwork::DrawnLength() const
{
	double Total = 0.0;
	for (const FPath& Path : Paths)
	{
		for (int32 i = 0; i + 1 < Path.Points.Num(); ++i)
		{
			const double Seg = FVector::Dist(Path.Points[i].Pos, Path.Points[i + 1].Pos);
			if (Path.Drawn[i] && Path.Drawn[i + 1] && Seg <= 60.0) Total += Seg; // same rule as the ribbon: skip jumps
		}
	}
	return (float)Total;
}
