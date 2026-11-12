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
	/** Arc length from the branch start to each sample (cm). */
	TArray<float> SampleDist;

	int32 Parent = INDEX_NONE;
	float ParentT = 0.f;   // 0..1 along the parent's samples
	int32 Level = 0;       // 0 trunk, 1 limb, 2 bough, 3 twig, -1 root
	FBox Bounds = FBox(ForceInit); // sample bounds expanded by radius
	float BlendK = 0.f;    // smooth-min radius used when this branch joins the field
	float Random = 0.f;    // 0..1, per-branch random for shading
	bool bLeafy = false;   // leaves may grow here (levels 2 and 3 above LowestLeafyHeight)

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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaves") float LeafSpacingTwig = 70.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaves") float LeafSpacingBough = 160.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaves") float LeafLengthMin = 170.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaves") float LeafLengthMax = 230.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaves") float LeafWidthRatio = 0.46f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Leaves") float LeafStalk = 18.f;

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

	// Mesh
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh") float AroundSpacing = 30.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh") int32 MaxAroundVerts = 128;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh") int32 MinAroundVerts = 7;
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
