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

	/** M_Trail and M_TrailSmoke show vertex RGB times this (vertex colours stop at 1): line colours are written divided by
	 *  it. Must match the materials' multiplier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trail")
	float ColorGain = 1.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trail")
	float Width = 7.f;

	/** Height of the ribbon above the surface (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trail")
	float Lift = 1.2f;

	/**
	 * Smoke look: a second, upright ribbon along the trail, this tall (cm), crossing the flat one so the trail reads as a
	 * thin haze from the side too. Its UV.x runs 2 (bark) to 3 (top) so M_Trail can tell it from the flat ribbon (0..1).
	 * 0: no upright ribbon (the flat ribbon only, as before).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trail")
	float SmokeHeight = 6.f;
	/** SmokeHeight that SetSmoke(true) restores. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trail")
	float SmokeLookHeight = 6.f;

	/** Smoke look (M_TrailSmoke and the upright ribbon) or the plain ribbon (M_Trail, no upright); rebuilds every chunk. */
	void SetSmoke(bool bOn);
	bool IsSmoke() const { return SmokeMaterial && TrailMaterial == SmokeMaterial; }

	/** A point closer than this to another path's ribbon is not drawn again (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trail")
	float MergeDistance = 18.f;

	/** Ribbon brightness of yesterday's trails (0..1). SetLineGlow never changes it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trail")
	float YesterdayGlow = 0.3f;

	int32 StartPath(const FVector& Pos, const FVector& Normal);
	void AddPoint(int32 Path, const FVector& Pos, const FVector& Normal);
	void ClearAll();

	// Yesterday's trails (one tree per stage): at day end today's routes are kept; the next day they load as one faint,
	// ridable set with no lines attached, and they are dropped when that day ends or on a move.
	/** Today's routes (not yesterday's), for the save. */
	void ExportToday(TArray<FSavedTrail>& Out) const;
	/** Clears everything, then adds Routes as yesterday's paths (faint, ridable). Call before any worker starts a path. */
	void LoadYesterday(const TArray<FSavedTrail>& Routes);
	bool IsYesterdayPath(int32 Path) const;
	int32 NumYesterdayPaths() const;

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

	/**
	 * Brightness of the ribbon by line: every drawn stretch within MergeDistance of a line's route takes the brightest
	 * glow of the lines that use it (0..1, vertex alpha in M_Trail); stretches on no line take NoLine. Only chunks whose
	 * brightness changed are rebuilt.
	 */
	void SetLineGlow(const TArray<const TArray<FTrailPoint>*>& Routes, const TArray<float>& Glow, float NoLine);
	/** The same, also colouring each line's stretches (vertex RGB: Colors parallel to Routes; NoLineColor elsewhere and on
	 *  yesterday's trails). Where lines share a stretch, the brightest takes it, the lower-numbered on a tie. */
	void SetLineGlow(const TArray<const TArray<FTrailPoint>*>& Routes, const TArray<float>& Glow, float NoLine,
		const TArray<FLinearColor>& Colors, const FLinearColor& NoLineColor);

private:
	struct FPath
	{
		TArray<FTrailPoint> Points;
		TArray<uint8> Drawn;
		TArray<float> Dist;              // distance along the path to each point (cm)
		TArray<float> Glow;              // ribbon brightness at each point (SetLineGlow)
		TArray<FLinearColor> Color;      // ribbon colour at each point: its line's (SetLineGlow), vertex RGB
		TSet<int32> GlowDirtyChunks;
		int32 DirtyFromChunk = INDEX_NONE; // only the growing tail is rebuilt
		bool bYesterday = false;         // loaded from the save: faint, drawn lower, ignored by today's merge test
		TArray<FTrailPoint> Shown;       // display only: the points lifted onto junction skirts (filled as chunks are built)
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
	float NoLineGlow = 1.f;
	FLinearColor NoLineColor = FLinearColor(0.30f, 0.95f, 1.10f);
	int32 ShownShape = INDEX_NONE;   // the tree shape the Shown points were lifted on (ATreeActor::GetShapeVersion)
	float RebuildTimer = 0.f;

	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> Mesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> MaterialInUse;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> FallbackMaterial;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> PlainMaterial;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> SmokeMaterial;
};
