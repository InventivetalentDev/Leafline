#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Tree/TreeTypes.h"
#include "Tree/TreeField.h"
#include "Tree/LeafSet.h"
#include "TreeActor.generated.h"

class UProceduralMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
struct FTrailPoint;

/** Limits for ATreeActor::BuildPatch (also spelled ATreeActor::FPatchRules). */
struct FTreePatchRules
{
	int32 MaxLeaves = 4;
	int32 MaxPieces = 10;
	/** Farthest a patch leaf's base may be from the cut leaf's base, walking over the bark (cm). */
	float MaxWalk = 700.f;
	/** Fragment size (0..1) the pieces are counted at. */
	float Template = 0.5f;
	float UsePerCut = 0.38f;
};

/** Owns the tree skeleton, its distance field and its procedural mesh. Spawned at runtime by the game mode. */
UCLASS()
class ANTTREEGAME_API ATreeActor : public AActor
{
	GENERATED_BODY()

public:
	ATreeActor();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tree")
	FTreeParams Params;

	/** Material for the wood. If empty, the engine's basic shape material tinted with BarkColor is used. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tree")
	TObjectPtr<UMaterialInterface> BarkMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tree")
	FLinearColor BarkColor = FLinearColor(0.20f, 0.13f, 0.08f);

	/** Material for leaves. If empty, the basic shape material tinted with LeafColor is used (blades are two-sided geometry). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tree")
	TObjectPtr<UMaterialInterface> LeafMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tree")
	FLinearColor LeafColor = FLinearColor(0.10f, 0.30f, 0.05f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tree")
	FLinearColor GroundColor = FLinearColor(0.09f, 0.075f, 0.05f);

	/** Procedural soil material. If empty, the basic shape material tinted with GroundColor is used. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tree")
	TObjectPtr<UMaterialInterface> GroundMaterial;

	const FLeafSet& GetLeaves() const { return Leaves; }
	int32 FindLeafNear(const FVector& P, float Reach) const { return Leaves.FindNearest(P, Reach); }
	float LeafSizeCap(int32 Leaf, float UsePerCut) const { return Leaves.SizeCap(Leaf, UsePerCut); }
	/** Bites a fragment out of a leaf and rebuilds that leaf's chunk. */
	bool BiteLeaf(int32 Leaf, float Size01, float DiameterCm, float UsePerCut);

	/** Regrows skeleton, field and mesh from a seed. */
	UFUNCTION(BlueprintCallable, Category = "Tree")
	void Grow(int32 Seed);

	// One tree per stage: the tree's bites are saved at day end and applied when the tree is grown again on load.
	/** Every leaf with at least one bite (or used up by StripBelow), in leaf order. */
	void ExportBites(TArray<FSavedLeaf>& Out) const;
	/**
	 * Grows the tree from Seed like Grow, then applies Bites before the leaf meshes are built (one build, not one per
	 * bite). If LeafCount does not match the grown tree's leaf count, or an entry is out of range or malformed, none of
	 * the bites are applied, a warning is logged and the tree stays fresh. Returns false only in that case: an empty
	 * Bites list applies nothing, logs nothing and returns true whatever LeafCount is.
	 */
	bool GrowWithBites(int32 Seed, const TArray<FSavedLeaf>& Bites, int32 LeafCount);
	/** Debug (Leaf.Strip): uses up every leaf whose base is below Z (world cm): no pieces left. Rebuilds the changed
	 *  chunks once. Returns how many leaves changed. */
	int32 StripBelow(float Z);
	/** Logs pieces left per 10 m height band, the six lowest leafy boughs (with their twigs) and the two lowest leafy limbs
	 *  with their live leaves and pieces (Leaf.TreeReport). */
	void LogTreeReport(const FString& Label) const;
	/** Nearest leaf within Reach of P that can no longer be cut (stripped or brown), or INDEX_NONE. */
	int32 FindSpentLeafNear(const FVector& P, float Reach) const;
	/** Pieces left at the patch template over the whole tree (fast enough for once a day). */
	int32 CountPiecesLeft(float Template = 0.5f, float UsePerCut = 0.38f) const;

	const FTreeSkeleton& GetSkeleton() const { return Skeleton; }
	/** The walking field: what the whole sim asks (walking, recruits, trails, falls, wind, exposure). With fillets and
	 *  FTreeParams::bFilletWalk it has the fillets (the ants walk on the skirts' surface); otherwise it has none. Leaves are
	 *  placed against the field without fillets either way, so leaf count and order do not depend on them. */
	const FTreeField& GetField() const { return Field; }
	/** The surface the bark mesh shows: with fillets, the field with them (the skirts lie on it; the walking field itself
	 *  when the ants walk on the fillets); otherwise the walking field. The skirts, the camera's bark cutaway, fillet debug. */
	const FTreeField& GetVisualField() const { return SkirtField.HasFillets() ? SkirtField : Field; }
	/** Display only: moves P (Height above the walking field) onto the skirts, Height above the skirt field, with its normal,
	 *  where a junction's skirt covers it; false (P and N untouched) where nothing covers it or fillets are off. Ants, trail
	 *  ribbons and exposure spots are drawn there while the sim keeps the walking position. When the ants walk on the
	 *  fillets this only lifts what lies under them (a trail recorded on the field without fillets). */
	bool LiftOntoSkirts(FVector& P, FVector& N, double Height) const;
	/** Counts tree regrowths (GrowShape), so display caches built on the old shape can tell. */
	int32 GetShapeVersion() const { return ShapeVersion; }
	int32 GetSeed() const { return Skeleton.Seed; }

	using FPatchRules = FTreePatchRules;

	/**
	 * The patch around a leaf: CutLeaf first (always, even with no pieces left), then the leaves on the same branch, then
	 * those on the rest of its bough (the bough and its twigs), each group nearest first along the bark. Leaves with no
	 * pieces left at Rules.Template, farther than Rules.MaxWalk, or whose approach from CutLeaf's base cannot be built are
	 * left out, and so is a leaf that would take the pieces (sum of PiecesLeft, CutLeaf included; the scout's own piece is
	 * not counted, the caller adds it) past Rules.MaxPieces. Stops at Rules.MaxLeaves or when the pieces reach MaxPieces.
	 */
	void BuildPatch(int32 CutLeaf, const FPatchRules& Rules, TArray<int32>& OutLeaves) const;

	/**
	 * A walkable route over the bark from From (on branch FromBranch, or the nearest branch if INDEX_NONE) to the base of
	 * a leaf, following the branch graph (along FromBranch to the junction, along the parent, up the leaf's branch).
	 * OutRoute[0] is From on the bark; the last point is at the leaf's Base. Points at most 12 cm apart, each projected onto the
	 * walking field 1.5 cm up (the ants' ride height), normals from the field; every consecutive pair passes StaysOnBark.
	 * Runs over the upper side of branches where it can and turns to the leaf's side near the end. False if no valid
	 * route (a step leaves the bark, the route is longer than MaxWalk, or the leaf is invalid).
	 */
	bool BuildLeafApproach(const FVector& From, int32 FromBranch, int32 Leaf, float MaxWalk, TArray<FTrailPoint>& OutRoute) const;

	/** The straight line A-B stays within 10 cm of bark (sampled at five inner points). */
	bool StaysOnBark(const FVector& A, const FVector& B) const;
	/** Approach outcomes so far on this tree, e.g. "ok 120, left the branches 2" (logs). */
	FString DescribeApproachStats() const;

	/** Debug: logs typical patch sizes (leaves, pieces) at templates 0.3 / 0.6 / 1.0 over Samples cuttable leaves spread
	 *  over the tree, and how many approaches failed and why. Clears the patch reachability cache first. Slow. */
	void LogPatchSurvey(int32 Samples = 40) const;

	/** A point on the ground beside the trunk, between two roots, and the direction facing away from the trunk. */
	FTransform GetNestTransform(int32 Index = 0) const;

	/** The tree in this world, or null. */
	static ATreeActor* Get(const UObject* WorldContext);

	/** Bark material: M_TreeBark2 (the look pass, default) or the older M_TreeBark. Applies at once, to the grove too. */
	void SetNewBark(bool bOn);
	static bool IsNewBark();

	/** Ground material (Leaf.Ground): M_Ground2 (the floor pass: a far look of leaf-sized flecks beyond about 20 m, the
	 *  default when the asset exists) or M_Ground. Applies at once. */
	void SetGroundLook(bool bNew);
	static bool IsGroundLook2();
	/** Sets a scalar parameter on M_Ground2's dynamic instance (tuning without rebuilding the material); false if none. */
	bool SetGroundParam(FName Name, float Value);
	/** Rebuilds the ground rings, the grove and its crowns (after Leaf.Relief); the ground skirt is left out. */
	void RebuildGroundAndGrove();

	/** Branch collars (Leaf.Collars): overrides FTreeParams::bBranchCollars for every tree grown from now on. */
	static void SetBranchCollarsOverride(bool bOn);
	static bool AreBranchCollarsOn(const FTreeParams& P);

	/** Junction fillets (Leaf.Fillets): override FTreeParams::bFillets, and FilletFrac / FilletMax / FilletBoughMul / FilletGroundK where
	 *  the value is >= 0, for every tree grown from now on. */
	static void SetFilletsOverride(bool bOn, float Frac = -1.f, float Max = -1.f, float BoughMul = -1.f, float GroundK = -1.f, float TwigMinRadius = -1.f);
	/** P with the fillet overrides applied. */
	static FTreeParams WithFilletOverrides(const FTreeParams& P);
	/** Leaf.FilletWalk: the sim walks on the fillets (1) or they are looks only (0); -1 leaves it to FTreeParams::bFilletWalk. */
	static void SetFilletWalkOverride(int32 Walk);

private:
	/** M_TreeBark2 if the new bark is on and the asset exists, else BarkMaterial. */
	UMaterialInterface* PickBark();

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> NewBarkMaterial;

	/** M_Ground2's dynamic instance if the ground look 2 is on and the asset exists, else GroundMaterial. */
	UMaterialInterface* PickGround();

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> Ground2MID;

	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> WoodMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BarkMID;

	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> LeafMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> LeafMaterialInUse;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> GroundMID;

	/** Distant trunks (no shadows, not walkable). */
	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> GroveMesh;

	/** Their crowns: big leaf clumps in the leaf material, scenery only. */
	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> GroveLeafMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> FallbackMaterial;

	/** Rebuilds one leaf chunk's mesh section. With bHitchCheck, warns when it takes over 4 ms (off for the full build). */
	void BuildLeafChunk(int32 Chunk, bool bHitchCheck = true);
	/** Grow, part 1: skeleton, field, wood, ground, grove and the leaf set (no leaf meshes yet). */
	void GrowShape(int32 Seed);
	/** Grow, part 2: every leaf chunk's mesh and the grove crowns. */
	void BuildLeafMeshes();
	/** Checks every saved entry against the freshly built leaf set, then applies all of them or none (see GrowWithBites). */
	bool ApplySavedBites(const TArray<FSavedLeaf>& Bites, int32 LeafCount);
	/** Whether BuildLeafApproach from CutLeaf's base to Leaf works within MaxWalk (cached for the tree's life). */
	bool PatchLeafReachable(int32 CutLeaf, int32 Leaf, float MaxWalk) const;
	/** Skirt: the ground half of the trunk and roots' ground skirt (TreeMeshBuilder), added to the ground section. */
	void BuildGround(const FTreeMeshData* Skirt = nullptr);
	void BuildGrove();
	/** The grove crowns' mesh from GroveLeaves (made in BuildGrove). */
	void BuildGroveCrowns();

	FTreeSkeleton Skeleton;
	FTreeField Field;
	/** With fillets on but not walked on: the field with them, which the skirts are built on (GetVisualField). Empty otherwise. */
	FTreeField SkirtField;
	/** With fillets in the walking field: the field without them, which the leaves are placed against. Empty otherwise. */
	FTreeField PlainField;
	int32 ShapeVersion = 0;
	FLeafSet Leaves;
	FLeafSet GroveLeaves;

	/** Child branches of each skeleton branch. */
	TArray<TArray<int32>> BranchChildren;
	/** (CutLeaf << 32 | Leaf) -> approach length (cm) from CutLeaf's base, or -MaxWalk if it failed at that MaxWalk. */
	mutable TMap<uint64, float> PatchWalkCache;
	/** BuildLeafApproach results by outcome (see TreeActor.cpp). */
	mutable TArray<int32> ApproachStats;
};
