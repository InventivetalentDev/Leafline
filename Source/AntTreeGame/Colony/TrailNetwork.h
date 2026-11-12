#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Colony/ColonyTypes.h"
#include "TrailNetwork.generated.h"

class UProceduralMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/**
 * Every worker's recorded path for the day, drawn as thin ribbons. A new path only adds ribbon where it is not already
 * on an existing trail, so overlapping routes do not stack. Returning ants walk home along the nearest scent line,
 * loops cut out (BuildReturnRoute).
 */
UCLASS()
class ANTTREEGAME_API ATrailNetwork : public AActor
{
	GENERATED_BODY()

public:
	ATrailNetwork();

	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trail")
	TObjectPtr<UMaterialInterface> TrailMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trail")
	FLinearColor TrailColor = FLinearColor(0.20f, 0.80f, 0.90f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trail")
	float Width = 7.f;

	/** Height of the ribbon above the surface (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trail")
	float Lift = 1.2f;

	/** A point closer than this to another path's ribbon is not drawn again (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trail")
	float MergeDistance = 18.f;

	int32 StartPath(const FVector& Pos, const FVector& Normal);
	void AddPoint(int32 Path, const FVector& Pos, const FVector& Normal);
	void ClearAll();

	int32 NumPaths() const { return Paths.Num(); }
	const TArray<FTrailPoint>& GetPoints(int32 Path) const { return Paths[Path].Points; }

	/**
	 * Nearest point of any path within Reach of P, preferring the path whose outward direction (away from the nest)
	 * best matches PreferredDir. Returns false if there is none.
	 */
	bool FindRideStart(const FVector& P, float Reach, const FVector& PreferredDir, int32& OutPath, int32& OutIndex, int32 ExcludePath = INDEX_NONE) const;

	/**
	 * The way home from P. Starts at the trail point within Reach (on any path, facing the same way as Normal) that is
	 * nearest the nest along its own path, then follows that path back, skipping loops: wherever an earlier point of the
	 * same path lies within about 35 cm, it jumps there instead of retracing the detour. OutRoute[0] is at the nest, the
	 * last point is P. Empty if OwnPath is invalid and no trail is in reach. CanCross(A, B) says whether the straight
	 * line from A to B stays on the bark; jumps that fail it are not taken (the follower would cut through the air).
	 */
	void BuildReturnRoute(const FVector& P, const FVector& Normal, int32 OwnPath, float Reach,
		TFunctionRef<bool(const FVector&, const FVector&)> CanCross, TArray<FTrailPoint>& OutRoute) const;

	/** Total drawn trail length (cm). */
	float DrawnLength() const;

private:
	struct FPath
	{
		TArray<FTrailPoint> Points;
		TArray<uint8> Drawn;
		TArray<float> Dist;              // distance along the path to each point (cm)
		int32 DirtyFromChunk = INDEX_NONE; // only the growing tail is rebuilt
	};

	FIntVector Cell(const FVector& P) const;
	/** Ribbon geometry for points [Chunk*ChunkSize, (Chunk+1)*ChunkSize] of a path, in its own mesh section. */
	void RebuildChunk(int32 Path, int32 Chunk);

	static constexpr int32 ChunkSize = 200;
	TMap<FIntPoint, int32> ChunkSection;
	int32 NextSection = 0;

	TArray<FPath> Paths;
	TMap<FIntVector, TArray<FIntPoint>> Grid; // cell -> (path, point index), every point
	float CellSize = 60.f;
	float RebuildTimer = 0.f;

	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> Mesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> MaterialInUse;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> FallbackMaterial;
};
