#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ExposureOverlay.generated.h"

class UProceduralMeshComponent;
class UMaterialInterface;
class ATreeActor;
class AWindSystem;

USTRUCT(BlueprintType)
struct FExposureOverlayTuning
{
	GENERATED_BODY()

	/** Spots are drawn on the bark within this distance of the controlled ant (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exposure") float Radius = 700.f;
	/** Distance between neighbouring spots, along and around a branch (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exposure") float Spacing = 30.f;
	/** Disc radius of one spot (cm). Smaller on thin branches: at most half the branch radius. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exposure") float SpotRadius = 6.f;
	/** Height of the discs above the walking surface (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exposure") float Lift = 1.5f;
	/** Most AWindSystem::GetExposure calls per tick (each is a march upwind through the tree's field). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exposure") int32 SamplesPerFrame = 40;
	/** When the ant is this far from where the area was last sampled, sample around it again; spots still inside stay (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exposure") float RecentreDistance = 150.f;

	/** Most spot positions projected onto the field per tick (each is one FTreeField::Project). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exposure") int32 PositionsPerFrame = 60;
	/** Wall-clock cap on one tick's positions and exposures together (ms). The counts above are caps too. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exposure") float WorkBudgetMs = 1.5f;
	/** Once every spot has a value, all of them are evaluated again over this many seconds (the wind wanders slowly). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exposure") float RefreshSeconds = 2.f;
	/** Shortest time between mesh rebuilds (s). The mesh is only rebuilt when a spot gains a value or changes band. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exposure") float RebuildInterval = 0.15f;
	/** Fade in and out (s), by vertex alpha. Only with SpotMaterial set: the fallback material ignores alpha. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exposure") float FadeTime = 0.15f;
	/** Branch rings thinner than this get no spots (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exposure") float MinBranchRadius = 3.f;
	/** Vertex alpha of a fully shown spot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exposure") float SpotAlpha = 0.85f;
};

/**
 * Wind exposure map ("wind map"): while shown, the bark around the controlled ant is covered in small coloured spots,
 * green sheltered, amber partly exposed, red exposed. Each spot's value is AWindSystem::GetExposure at that spot, in the
 * HUD's "Here: ..." bands (BandOf), so the map and the HUD read-out use the same function and thresholds.
 *
 * Work is spread over ticks: positions nearest the ant first, then exposures nearest first, then a slow round-robin
 * refresh. Hidden, it does nothing (tick off, mesh hidden).
 *
 * Mesh: one section, each spot an 8-triangle disc. Vertex colour RGB = band colour, A = SpotAlpha x fade.
 * UV0 is disc-local: (0, 0) at the centre, unit length at the rim (for a soft round dot in the material).
 */
UCLASS()
class ANTTREEGAME_API AExposureOverlay : public AActor
{
	GENERATED_BODY()

public:
	AExposureOverlay();

	virtual void Tick(float DeltaTime) override;

	/** The overlay in this game world, spawned on first use. Null for a non-game (editor) world. */
	static AExposureOverlay* Get(UWorld* World);

	void SetShown(bool bShow);
	bool IsShown() const { return bShown; }
	/** 0 hidden .. 1 fully shown (for a HUD legend that fades with the map). */
	float GetFade() const { return Fade; }
	int32 NumSpots() const { return Spots.Num(); }

	/** The HUD's exposure bands: 0 sheltered (< 0.25), 1 partly exposed (< 0.6), 2 exposed. */
	static int32 BandOf(float Exposure);
	/** The HUD's colour for a band (alpha 1). */
	static FLinearColor BandColor(int32 Band);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Exposure")
	FExposureOverlayTuning Tuning;

	/** Unlit material using vertex colour and alpha. Null: the engine's vertex colour debug material. Read at every
	 *  mesh build, so it can be set after the actor has spawned. */
	UPROPERTY(EditAnywhere, Category = "Exposure")
	TObjectPtr<UMaterialInterface> SpotMaterial;

private:
	struct FSpot
	{
		FVector P = FVector::ZeroVector; // on the walking surface (D = 0)
		FVector N = FVector::UpVector;   // field normal there
		FVector ShownP = FVector::ZeroVector; // display only: P lifted onto a junction's skirt where one covers it
		FVector ShownN = FVector::UpVector;
		float Size = 6.f;                // disc radius (cm)
		float Exposure = -1.f;           // -1: no value yet (not drawn)
		int8 Band = -1;
	};
	struct FCandidate
	{
		FVector P = FVector::ZeroVector; // on the branch's ring, before projection
		float Size = 6.f;
		double Dist2 = 0.0;              // to the sampling centre
	};
	/** Identifies the grown tree; when it changes the spots are sampled again. */
	struct FTreeKey
	{
		int32 Seed = 0;
		int32 Branches = 0;
		int32 Leaves = 0;
		bool operator==(const FTreeKey& O) const { return Seed == O.Seed && Branches == O.Branches && Leaves == O.Leaves; }
		bool operator!=(const FTreeKey& O) const { return !(*this == O); }
	};

	/** Spots, candidates and the hash emptied; the next tick samples from scratch. */
	void ResetSpots();
	/** Drops spots outside the sphere round NewCentre (and their values if bClearValues), then queues candidates for the
	 *  part of the new sphere that the last completed pass did not cover. */
	void Recentre(const FVector& NewCentre, bool bClearValues);
	/** Ring points round every branch near Centre, cheap filters only, sorted nearest first. Skips points well inside the
	 *  sphere round Covered (already sampled) when it is given. */
	void GenerateCandidates(const ATreeActor& Tree, const FVector& Centre, const FVector* Covered);
	/** Projects queued candidates until a cap is reached. Returns how many were processed. */
	int32 ProcessCandidates(const ATreeActor& Tree, double StartTime, double Budget);
	/** Evaluates pending spots, then refreshes round-robin. Returns how many exposures were evaluated. */
	int32 ProcessExposures(const AWindSystem& Wind, float DeltaTime, double StartTime, double Budget);
	void Evaluate(const AWindSystem& Wind, FSpot& Spot);

	FIntVector HashCell(const FVector& P) const;
	bool HasSpotNear(const FVector& P, double Radius) const;
	void AddToHash(int32 Index);
	void RebuildHash();

	void RebuildMesh();
	/** Rewrites only the vertex alpha of the built section (fading). */
	void ApplyFade();
	/** Mesh hidden and cleared, tick off. */
	void FinishHide();
	bool CanFade() const;
	UMaterialInterface* MaterialInUse() const;
	/** Tree and wind, cached; looked up again at most once a second while missing. */
	bool ResolveWorld(ATreeActor*& OutTree, AWindSystem*& OutWind);
	void LogFill() const;

	UPROPERTY(VisibleAnywhere, Category = "Exposure")
	TObjectPtr<UProceduralMeshComponent> Mesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> FallbackMaterial;

	TWeakObjectPtr<ATreeActor> TreeRef;
	TWeakObjectPtr<AWindSystem> WindRef;
	double NextLookupTime = 0.0;

	TArray<FSpot> Spots;
	TMap<FIntVector, TArray<int32>> Hash;
	double HashCellSize = 18.0;

	TArray<FCandidate> Candidates;
	int32 NextCandidate = 0;
	/** A candidate pass is queued or running. */
	bool bPassActive = false;
	/** The last pass completed around CoveredCentre: every surface point within Radius of it was considered. */
	bool bCovered = false;
	FVector CoveredCentre = FVector::ZeroVector;

	/** Spot indices without a value, roughly nearest first. */
	TArray<int32> Pending;
	int32 NextPending = 0;
	int32 RefreshCursor = 0;
	float RefreshCarry = 0.f;

	FVector SampleCentre = FVector::ZeroVector;
	bool bSampled = false;
	FTreeKey TreeKey;

	bool bShown = false;
	/** Shown again, not yet sampled for it. */
	bool bShowPending = false;
	/** That show started from fully hidden: old values are dropped. */
	bool bClearOnShow = true;
	float Fade = 0.f;
	bool bMeshDirty = false;
	float RebuildTimer = 0.f;

	/** Positions and colours of the built section (alpha before fade), for ApplyFade. */
	TArray<FVector> MeshVerts;
	TArray<FLinearColor> MeshColors;

	// Log bookkeeping for the current show.
	double ShowStartTime = 0.0;
	double PositionSeconds = 0.0;
	bool bFillLogged = false;
	double LastWarnTime = -100.0;
};
