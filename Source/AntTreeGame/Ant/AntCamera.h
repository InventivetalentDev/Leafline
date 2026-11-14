#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/Scene.h"
#include "AntCamera.generated.h"

class UCameraComponent;
class AAntPawn;
class UMaterialParameterCollection;

USTRUCT(BlueprintType)
struct FAntCameraTuning
{
	GENERATED_BODY()

	/** 170 since the look pass (13 Nov, was 220): the ant about 5.6% of the screen width instead of 4.4%. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") float BoomLength = 170.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") float MinBoomLength = 35.f;
	/** Look-at point above the ant along its up (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") float TargetHeight = 25.f;
	/** Time constant with which the camera's up eases toward its target up (s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") float UpTimeConstant = 0.8f;
	/** Fastest the camera's up may turn (degrees per second). Caps roll, the main cause of motion sickness. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") float MaxUpTurnRate = 45.f;
	/** How far the camera's up follows the surface on big branches: 0 keeps the horizon level everywhere, 1 follows fully. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") float SurfaceFollow = 1.f;
	/** Branch radius (cm) below which the camera keeps world up, so circling a twig never rolls the view. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") float SurfaceUpRadiusMin = 40.f;
	/** Branch radius (cm) above which the camera follows the surface by SurfaceFollow. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") float SurfaceUpRadiusMax = 250.f;
	/** Angular frequency of the critically damped follow spring (1/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") float FollowFrequency = 8.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") float GlideFrequency = 3.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") float GlideTime = 1.f;
	/** -65 and 80 since the owner's playtest (13 Nov evening, were -80 and 60): looking up stopped too soon on big wood,
	 *  where the camera's up follows the bark, and looking nearly straight down went through the wood above the ant. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") float PitchMin = -65.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") float PitchMax = 80.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") float DefaultPitch = -25.f;
	/**
	 * Pull the boom in when the trunk or a big limb blocks the view. Off by default: the camera passes through wood
	 * and the bark between camera and ant dithers away instead (CutawayRadius), which is less jarring.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") bool bAvoidThickOcclusion = false;
	/** Radius (cm) of the dithered cutaway along the line from the camera to the ant, narrowing to nothing at the ant. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") float CutawayRadius = 55.f;
	/** Width of the cutaway's dithered rim as a fraction of its radius (1 fades all the way from the centre). M_TreeBark2. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") float CutawayBand = 0.5f;
	/** How the cutaway narrows toward the ant: radius x (1 - t)^Taper, t = 0 at the camera, 1 at the ant. M_TreeBark2. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") float CutawayTaper = 1.f;
	/** Bark nearer the camera than this is gone; it fades back in by NearFadeEnd (cm). M_TreeBark2. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") float NearFadeStart = 8.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") float NearFadeEnd = 35.f;
	/** M_TreeBark2 hides bark buried more than this (cm) plus 0.2 x the other branch's radius inside another branch
	 *  (UV3.y, see TreeMeshBuilder.cpp). From outside, buried bark is covered by the other branch anyway. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") float BuriedDepth = 15.f;
	/**
	 * The bark's cutaway opens only when wood really lies between the camera and the ant (look pass 2): the line to the
	 * ant is traced through the tree field each frame, and the bark's cutaway radius scales with how deep it runs through
	 * wood, from 0 (clear) to full at CutawayDepthFull cm. It eases in at CutawayEaseIn and out at CutawayEaseOut (1/s),
	 * so the hole does not flicker while walking. Leaves and the near-camera fade are unaffected.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") float CutawayDepthFull = 25.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") float CutawayEaseIn = 8.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") float CutawayEaseOut = 3.f;
	/** The camera never goes lower than this above the ground (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") float GroundClearance = 15.f;
	/** The camera never sits inside wood this thick (radius, cm: the trunk, roots, big limbs); it slides out to
	 *  WoodClearance above the bark, still looking at the ant. Thinner wood it passes through. 0 turns this off. Wood
	 *  between the camera and the ant is dithered away either way. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") float ThickWoodRadius = 120.f;
	/** 30 since look pass 2 (60 in look pass 1, 12 before): the bark cutaway now opens only when wood hides the ant, so
	 *  the trench under a low camera no longer happens; 30 gives back sky when looking up from the trunk (32% of the
	 *  upper frame against 19% at 60). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") float WoodClearance = 30.f;
	/** Probe radius for keeping the camera out of the tree (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") float ProbeRadius = 12.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") float BoomShrinkRate = 5.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") float BoomGrowRate = 2.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") float FieldOfView = 75.f;
	/** Mild depth of field focused on the ant sells the macro scale; 0 disables it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") float DepthOfFieldFstop = 4.f;
	/** 0.2 since the look pass (was -0.4): frames full of sunlit bark metered the sky too dark. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") float ExposureBias = 0.2f;
	/** Auto exposure range (EV100). Both at -100 leave the level's post-process settings alone. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") float ExposureMinEV = -100.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") float ExposureMaxEV = -100.f;
	/**
	 * Look-up assist (off until the owner has tried it): when pitching up would put the camera under the ant's own
	 * surface, the camera stays LookUpClearance above that surface on a shorter boom and the view turns upward from
	 * there, so the ant slides toward the bottom of the frame and the canopy fills the rest.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") bool bLookUpAssist = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") float LookUpClearance = 10.f;
	/** Shortest boom the look-up assist may use (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") float LookUpMinBoom = 60.f;
	/** Rate at which the yaw drifts behind the ant while it moves (1/s). 0 = free orbit only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") float AutoAlignRate = 0.f;
};

/**
 * Follows the controlled ant without being attached. Keeps its own lazy up vector so sharp changes of surface
 * never swing the view; orbits around that up; stays out of the tree by marching the field.
 */
UCLASS()
class ANTTREEGAME_API AAntCamera : public AActor
{
	GENERATED_BODY()

public:
	AAntCamera();

	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	FAntCameraTuning Tuning;

	/** Follow a new ant. With bGlide the camera eases over instead of cutting. */
	void SetTarget(AAntPawn* NewTarget, bool bGlide);
	AAntPawn* GetTarget() const { return Target.Get(); }

	/** Yaw (X) and pitch (Y) in degrees, applied on the next tick. */
	void AddLookInput(const FVector2D& DeltaDegrees) { PendingLook += DeltaDegrees; }

	/** Debug: aim the orbit toward a world heading (degrees, 0 = +X) at a pitch, for repeatable screenshots. */
	/** Orbit state for the HUD's reproduction line (Leaf.Goto): the reference forward and the pitch above it. */
	const FVector& GetRefForward() const { return RefForward; }
	float GetPitch() const { return Pitch; }
	void DebugAim(float WorldYawDeg, float NewPitch) { DebugAimDirection(FRotator(0.f, WorldYawDeg, 0.f).Vector(), NewPitch); }
	void DebugAimDirection(const FVector& WorldDir, float NewPitch);

	/** Harvest view: glide to a fixed spot looking at a point, ignoring the ant. Off: back to the ant. */
	void SetOverview(bool bOn, const FVector& Location = FVector::ZeroVector, const FVector& LookAt = FVector::ZeroVector);
	bool IsOverview() const { return bOverview; }

	FVector GetViewForward() const { return GetActorForwardVector(); }
	FVector GetViewUp() const { return GetActorUpVector(); }

	/** Recording only: damp input and follow without changing the normal tuning. */
	void SetCinematic(bool bOn) { bCinematic = bOn; CinematicLook = FVector2D::ZeroVector; }
	bool IsCinematic() const { return bCinematic; }
	/** External driver freezes the orbit state and restores the exact incoming view on release. */
	void BeginExternalDrive();
	void DriveExternal(const FTransform& View, float FOV, const FVector& LookAt);
	void EndExternalDrive();
	float GetFieldOfView() const;

private:
	void Snap();
	bool bCinematic = false;
	FVector2D CinematicLook = FVector2D::ZeroVector;
	bool bExternalDrive = false;
	bool bExternalJustReleased = false;
	FTransform SavedExternalView;
	float SavedExternalFOV = 75.f;
	FPostProcessSettings SavedExternalPostProcess;

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<UCameraComponent> Camera;

	/** Camera and ant positions for the bark's cutaway dither. */
	UPROPERTY()
	TObjectPtr<UMaterialParameterCollection> ViewCollection;

	void PublishView(const FVector& CamPos, const FVector& AntFocus, bool bCutaway);
	/** How deep (cm) the segment runs through wood, ignoring EndSkip cm at its far end (the ant stands on the bark). */
	float OcclusionDepth(const FVector& From, const FVector& To, float EndSkip) const;
	/** The bark's cutaway factor (0 clear .. 1 full), eased; published to MPC_View as BarkCut. */
	float BarkCut = 0.f;
	float CutLogTimer = 0.f;

	TWeakObjectPtr<AAntPawn> Target;
	TWeakObjectPtr<class ATreeActor> Tree;
	FVector CamUp = FVector::UpVector;
	FVector RefForward = FVector::ForwardVector;
	float Pitch = -20.f;
	FVector Focus = FVector::ZeroVector;
	FVector FocusVelocity = FVector::ZeroVector;
	float CurrentBoom = 220.f;
	float GlideRemaining = 0.f;
	FVector2D PendingLook = FVector2D::ZeroVector;
	float WanderTimer = 0.f;
	float WanderSign = 1.f;
	bool bHasSnapped = false;
	float SmoothedRadius = 1000.f;
	float SpareFloat = 0.f;
	FVector SpareVector = FVector::ZeroVector;
	bool bOverview = false;
	bool bOverviewPlaced = false;
	FVector OverviewLoc = FVector::ZeroVector;
	FVector OverviewLook = FVector::ZeroVector;
};
