#pragma once

#include "CoreMinimal.h"
#include "TreeTypes.generated.h"

/** One control point of a branch centreline. */
struct FTreeNode
{
	FVector Pos = FVector::ZeroVector;
	float Radius = 10.f;
};

/** A branch: control nodes plus the dense centreline samples that both the field and the mesh use. */
struct FTreeBranch
{
	TArray<FTreeNode> Nodes;

	/** Dense samples of the Catmull-Rom curve through Nodes. Field segments and mesh rings sit on these. */
	TArray<FVector> Samples;
	TArray<float> SampleRadius;
	/** Radius without the trunk's ground flare and without the branch collar, one per sample, for the bark shader (its
	 *  cylinder-coordinate noise must not stretch with either). Empty means "use SampleRadius". */
	TArray<float> BarkRadius;
	/** Arc length from the branch start to each sample (cm). */
	TArray<float> SampleDist;

	int32 Parent = INDEX_NONE;
	float ParentT = 0.f;   // 0..1 along the parent's samples
	int32 Level = 0;       // 0 trunk, 1 limb, 2 bough, 3 twig, -1 root
	FBox Bounds = FBox(ForceInit); // sample bounds expanded by radius
	float BlendK = 0.f;    // smooth-min radius used when this branch joins the field
	/** Fillet where the branch leaves its parent (FTreeParams::bFillets; limbs, boughs and roots; 0 = none). Inside the
	 *  junction zone the field blends the branch in with FilletK instead of BlendK: fully within FilletInner of
	 *  FilletCenter (where the axis leaves the parent), easing back to BlendK at FilletOuter. Set after growth from the
	 *  grown shape (no random numbers). */
	float FilletK = 0.f;
	FVector FilletCenter = FVector::ZeroVector;
	float FilletInner = 0.f;
	float FilletOuter = 0.f;
	float Random = 0.f;    // 0..1, per-branch random for shading
	bool bLeafy = false;   // leaves may grow here (levels 2 and 3 above LowestLeafyHeight)
	bool bStub = false;    // a limb broken off short: no boughs grow on it (a landmark)

	float Length() const { return SampleDist.Num() ? SampleDist.Last() : 0.f; }
};

/** Every knob for growing a tree. Units are cm. */
USTRUCT(BlueprintType)
struct FTreeParams
{
	GENERATED_BODY()

	// Trunk
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trunk") float TrunkHeight = 30000.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trunk") float TrunkRadius = 800.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trunk") float TrunkTopRadius = 40.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trunk") float TrunkTaperExp = 0.85f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trunk") float TrunkLeanDeg = 3.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trunk") float TrunkWobble = 0.04f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trunk") float FlareMul = 0.75f;      // extra radius fraction at the ground
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trunk") float FlareHeight = 600.f;   // e-folding height of the flare
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trunk") float TrunkSinkDepth = 300.f; // trunk starts below ground

	// Roots
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots") int32 NumRoots = 4;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots") float RootLength = 2400.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots") float RootRadiusMul = 0.32f;  // of flared trunk radius
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Roots") float RootDipDeg = 18.f;

	// Limbs (level 1)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Limbs") int32 NumLimbs = 22;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Limbs") float LimbStartHeight = 2600.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Limbs") float LimbEndFrac = 0.93f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Limbs") float LimbLengthMax = 6000.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Limbs") float LimbLengthMin = 1600.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Limbs") float LimbPitchMinDeg = 12.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Limbs") float LimbPitchMaxDeg = 42.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Limbs") float LimbRadiusRatio = 0.36f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Limbs") float LimbTipRadius = 22.f;
	/** Landmark: one of the lowest limbs is broken off short, a thick bare stub. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Limbs") bool bBrokenLimb = true;

	// Boughs (level 2)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boughs") int32 BoughsPerLimb = 7;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boughs") float BoughLengthFrac = 0.38f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boughs") float BoughRadiusRatio = 0.45f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boughs") float BoughTipRadius = 8.f;

	// Twigs (level 3)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Twigs") int32 TwigsPerBough = 7;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Twigs") float TwigLengthMin = 300.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Twigs") float TwigLengthMax = 750.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Twigs") float TwigRadiusMin = 6.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Twigs") float TwigRadiusMax = 16.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Twigs") float TwigTipRadius = 4.f;

	// Leaves
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaves") float LowestLeafyHeight = 4000.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaves") float LeafSpacingTwig = 50.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaves") float LeafSpacingBough = 115.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaves") float LeafLengthMin = 170.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaves") float LeafLengthMax = 230.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaves") float LeafWidthRatio = 0.46f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaves") float LeafStalk = 18.f;
	/** Leaf spacing multiplier at the lowest leafy branch: the tree is sparse low down (one tree per stage). It eases back
	 *  to 1 over SparseBlendHeight above LowestLeafyHeight. 1 (the default) leaves placement exactly as before. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaves") float SparseSpacingMul = 1.f;
	/** Height above LowestLeafyHeight (cm) over which SparseSpacingMul eases back to 1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaves") float SparseBlendHeight = 0.f;
	/** Where the spacing multiplier M at a twig's start is above 1, the whole twig is left bare with probability
	 *  SparseTwigDrop * (1 - 1 / M): 1 thins low twigs as much as the spacing does, 0 turns it off. Only used when
	 *  SparseSpacingMul != 1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaves") float SparseTwigDrop = 1.f;
	/** Multiplier on every leaf's Richness (later trees have richer leaves). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaves") float RichnessMul = 1.f;
	/** How Richness rises with height. 0: linearly over the whole crown (1x at the lowest leaf, the height share of 3x at
	 *  the top). Above 0: metres over which the height bonus builds up from the lowest leaf, 1 - exp(-dz / scale), so the
	 *  first layers above the lowest are worth clearly more and the top still reaches 3x. Set from FDayTuning. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaves") float RichnessHeightScale = 0.f;

	// Growth
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Growth") float AvoidMargin = 40.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Growth") float Gravitropism = 0.08f;

	// Field
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Field") float BlendFrac = 0.3f;   // k as fraction of child radius
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Field") float BlendMax = 30.f;    // cap on k (1.5 AL)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Field") float GroundBlend = 12.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Field") float SampleSpacingFrac = 0.6f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Field") float SampleSpacingMin = 20.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Field") float SampleSpacingMax = 300.f;

	// Nest: a low soil mound around the entrance. Part of the walking field, so ants walk over it instead of through it.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nest") float NestMoundRadius = 120.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nest") float NestMoundHeight = 7.f;

	// Grove: plain trunks far off in the haze, so the world reads as a forest instead of a disk with an edge.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grove") int32 GroveCount = 14;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grove") float GroveMinDistance = 22000.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grove") float GroveMaxDistance = 70000.f;
	/** Leaf clumps per grove crown (around the top and at the limb ends); 0 leaves the trunks bare. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grove") int32 GroveCrownClumps = 9;
	/** Leaves per clump; each is about a quarter of the clump radius long, so this sets how closed the crown looks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grove") int32 GroveLeavesPerClump = 260;
	/** Radius of the ground disk (cm). Far enough that the height fog swallows its edge. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grove") float GroundRadius = 500000.f;

	// Mesh
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh") float AroundSpacing = 30.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh") int32 MaxAroundVerts = 128;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh") int32 MinAroundVerts = 7;

	// Collars: a branch swells where it leaves its parent (draws no random numbers; the tree stays the same tree).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh") bool bBranchCollars = false;
	/** Extra radius at the point where the branch leaves its parent, as a fraction of the branch's base radius (roots and
	 *  limbs; boughs get 0.8x, twigs 0.5x). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh") float CollarAmount = 0.45f;
	/** How far the swelling fades out along the branch, in base radii. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh") float CollarLength = 1.2f;

	// Fillets: where a limb, bough, twig or root leaves its parent, the mesh gets a skirt that rounds the corner (a field with a
	// larger blend round the junction, which the skirt lies on). Twigs thinner than FilletTwigMinRadius keep their crease. Draws no random numbers, and
	// leaves are placed against the field without fillets, so the tree, its leaf count and leaf order are the same either
	// way (Leaf.Fillets). With bFilletWalk the sim walks on them; without, they are looks only.
	// 14 Nov: with fillets in the walking field, laden recruits stuck at the same junctions every day (69 hauled in twenty
	// Leaf.AutoPlay days against 470 without, Saved/RefShots/bird-baseline/summary.md): followers crept at 1 cm/s along
	// route steps that run through the wood. Fixed in UAntMovementComponent::TickFollow (488 hauled, no rescues).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fillets") bool bFillets = true;
	/** With fillets on, the sim walks on them: the walking field is the field with fillets, so ants climb over the rounded
	 *  junction they see (owner: the fillets add a lot to the visuals). Off: the skirts are looks only and the ants are drawn
	 *  lifted onto them while the sim walks on the field without fillets (Leaf.FilletWalk). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fillets") bool bFilletWalk = true;
	/** Fillet blend radius as a fraction of the child's base radius (the rounding reaches about this far along each surface). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fillets") float FilletFrac = 0.7f;
	/** Multiplier on FilletFrac for boughs. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fillets") float FilletBoughMul = 1.f;
	/** Twigs get fillets too (owner, 14 Nov: "small branches should have fillets too"), sized like the others (FilletFrac of
	 *  their base radius), when their base radius is at least this (cm). Thinner ones keep their crease: their fillet would
	 *  be under a centimetre and there are thousands of them (skirt build time). A huge value turns twig fillets off. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fillets") float FilletTwigMinRadius = 3.f;
	/** Cap on the fillet blend radius (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fillets") float FilletMax = 150.f;
	/** Blend radius (cm) where the trunk and roots meet the ground, with fillets on (GroundBlend without), and a skirt there.
	 *  0 keeps GroundBlend and builds no ground skirt. Off by default (13 Nov): where a root meets the trunk at the ground the
	 *  ground skirt and the junction skirt overlap and leave a lip; 80 is the tried value (Leaf.Fillets 1 -1 -1 -1 80). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Fillets") float FilletGroundK = 0.f;
};

/** A bitten leaf, for the colony save (one tree per stage). Leaf indices are only stable for the same seed, params and
 *  code; the save also stores the tree's leaf count and drops the bites when it does not match. */
USTRUCT()
struct FSavedLeaf
{
	GENERATED_BODY()

	UPROPERTY(SaveGame) int32 Leaf = INDEX_NONE;
	UPROPERTY(SaveGame) int32 Bites = 0;
	UPROPERTY(SaveGame) float Remaining = 1.f;
	/** Per bite: centre along, centre across (leaf-local cm), radius (cm); 3 floats each, Bites entries. */
	UPROPERTY(SaveGame) TArray<float> BiteData;
};

struct FTreeSkeleton
{
	TArray<FTreeBranch> Branches;
	FTreeParams Params;
	int32 Seed = 0;

	/** Ground-level points between pairs of roots, where a nest can sit. */
	TArray<FVector> NestSpots;
	/** Trunk axis at ground level. */
	FVector TrunkBase = FVector::ZeroVector;
};
