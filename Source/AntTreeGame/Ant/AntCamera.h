#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AntCamera.generated.h"

class UCameraComponent;
class AAntPawn;
class UMaterialParameterCollection;

USTRUCT(BlueprintType)
struct FAntCameraTuning
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") float BoomLength = 260.f;
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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") float PitchMin = -80.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") float PitchMax = 60.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") float DefaultPitch = -20.f;
	/**
	 * Pull the boom in when the trunk or a big limb blocks the view. Off by default: the camera passes through wood
	 * and the bark between camera and ant dithers away instead (CutawayRadius), which is less jarring.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") bool bAvoidThickOcclusion = false;
	/** Radius (cm) of the dithered cutaway along the line from the camera to the ant, narrowing to nothing at the ant. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") float CutawayRadius = 55.f;
	/** The camera never goes lower than this above the ground (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") float GroundClearance = 15.f;
	/** Probe radius for keeping the camera out of the tree (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") float ProbeRadius = 12.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") float BoomShrinkRate = 5.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") float BoomGrowRate = 2.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") float FieldOfView = 75.f;
	/** Mild depth of field focused on the ant sells the macro scale; 0 disables it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") float DepthOfFieldFstop = 4.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera") float ExposureBias = -0.4f;
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

	FVector GetViewForward() const { return GetActorForwardVector(); }
	FVector GetViewUp() const { return GetActorUpVector(); }

private:
	void Snap();

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<UCameraComponent> Camera;

	/** Camera and ant positions for the bark's cutaway dither. */
	UPROPERTY()
	TObjectPtr<UMaterialParameterCollection> ViewCollection;

	void PublishView(const FVector& CamPos, const FVector& AntFocus, bool bCutaway);

	TWeakObjectPtr<AAntPawn> Target;
	TWeakObjectPtr<class ATreeActor> Tree;
	FVector CamUp = FVector::UpVector;
	FVector RefForward = FVector::ForwardVector;
	float Pitch = -20.f;
	FVector Focus = FVector::ZeroVector;
	FVector FocusVelocity = FVector::ZeroVector;
	float CurrentBoom = 260.f;
	float GlideRemaining = 0.f;
	FVector2D PendingLook = FVector2D::ZeroVector;
	float WanderTimer = 0.f;
	float WanderSign = 1.f;
	bool bHasSnapped = false;
	float SmoothedRadius = 1000.f;
	float SpareFloat = 0.f;
	FVector SpareVector = FVector::ZeroVector;
};
