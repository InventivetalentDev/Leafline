#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Colony/ColonyTypes.h"
#include "AntMovementComponent.generated.h"

class ATreeActor;
class FTreeField;

UENUM(BlueprintType)
enum class EAntState : uint8
{
	Walking,
	Cutting,
	Gripping,
	Riding,
	Returning,
	Falling,
	WalkingHome
};

USTRUCT(BlueprintType)
struct FAntMoveTuning
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speed") float WalkSpeed = 200.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speed") float SprintMultiplier = 2.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speed") float RideSpeed = 600.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speed") float Acceleration = 1200.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speed") float Deceleration = 1600.f;
	/** Exponential rate at which the body turns to face its velocity (1/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orientation") float TurnRate = 10.f;
	/** Exponential rate at which the body's up follows the surface normal (1/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orientation") float UpRate = 14.f;
	/** Height of the ant's origin (its feet) above the field surface (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface") float RideHeight = 1.5f;
	/** Longest move per substep (cm), so thin twigs are never stepped over. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface") float MaxSubstep = 10.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina") float StaminaDrain = 0.17f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina") float StaminaDrainPerMass = 0.04f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina") float StaminaRegen = 0.22f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina") float StaminaResumeThreshold = 0.25f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carry") float SpeedLossPerMass = 0.08f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carry") float MinCarrySpeedFactor = 0.45f;
	/** Walking on another worker's scent trail is this much faster (riding with F stays faster still). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speed") float TrailSpeedMultiplier = 1.7f;
	/** Ground distance from the trunk axis at which the ant is turned back (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bounds") float BoundaryRadius = 7000.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bounds") float BoundaryWidth = 800.f;
	/**
	 * Thin branches as a steering wheel: W/S along the branch (away from / toward the camera), A/D round it (clockwise /
	 * anticlockwise as seen from the camera). Which end W heads for is fixed when the stick is first held and kept until
	 * it is let go, so the camera's slow roll cannot change it mid-move. Off: camera-relative with the sideways snap.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thin branches") bool bSteeringWheel = true;
};

/**
 * Surface walker on the tree's distance field. No physics, no capsule: move tangentially, project back onto the field.
 * Drives both the player's ant and unattended ants; only the source of the wish direction differs.
 */
UCLASS(ClassGroup = (Leafline), meta = (BlueprintSpawnableComponent))
class ANTTREEGAME_API UAntMovementComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAntMovementComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	FAntMoveTuning Tuning;

	/** Mass of the carried fragment (0 when empty). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement")
	float CarriedMass = 0.f;

	/** Camera-relative move input: X right, Y forward. */
	void SetMoveInput(const FVector2D& Input, const FVector& CamForward, const FVector& CamUp);
	void SetSprintInput(bool bSprint) { bSprintInput = bSprint; }
	/** The pawn says whether the ant is on another worker's trail (checked a few times a second). */
	void SetOnTrail(bool bOn) { bOnTrail = bOn; }
	bool IsOnTrail() const { return bOnTrail; }
	bool HasMoveInput() const { return MoveInput.SizeSquared() > 0.04; }

	void TeleportTo(const FVector& Location, const FVector& Facing);
	bool PlaceOnBranch(int32 Branch, float T);

	/**
	 * Follow a recorded path: Riding goes outward (Direction +1) at RideSpeed and ends on any movement input;
	 * Returning replays it backwards (Direction -1) at the laden walking speed. The points are copied.
	 */
	void StartFollow(const TArray<FTrailPoint>& Points, int32 StartIndex, int32 Direction, EAntState FollowState);
	void StopFollow();
	/** Blown off: a ballistic drop to the ground, then WalkingHome. */
	void StartFall(const FVector& WindDirection);
	/** Where WalkingHome heads, and the trunk to walk around on the way. */
	void SetHomeTarget(const FVector& Nest, const FVector& TrunkBase, float TrunkRadius)
	{
		HomeTarget = Nest;
		HomeTrunkBase = TrunkBase;
		HomeTrunkRadius = TrunkRadius;
	}

	/** True once after a follow reached the end of its path. */
	bool ConsumeFollowFinished()
	{
		const bool bFinished = bFollowFinished;
		bFollowFinished = false;
		return bFinished;
	}

	EAntState GetState() const { return State; }
	void SetState(EAntState NewState);

	const FVector& GetSurfacePosition() const { return Pos; }
	const FVector& GetSurfaceNormal() const { return SurfaceNormal; }
	const FVector& GetBodyUp() const { return Up; }
	const FVector& GetBodyForward() const { return Forward; }
	const FVector& GetVelocity() const { return Velocity; }
	float GetStamina() const { return Stamina; }
	bool IsSprinting() const { return bSprinting; }
	bool IsSprintLocked() const { return bSprintLocked; }
	int32 GetCurrentBranch() const { return CurrentBranch; }
	double GetLastProjectionError() const { return LastProjError; }

private:
	const FTreeField* GetField();
	void TickWalking(float Dt);
	void TickRooted(float Dt);
	void TickFollow(float Dt);
	void TickFalling(float Dt);
	FVector HomeWish(const FVector& N) const;
	FVector ComputeWish(const FVector& N) const;
	void UpdateOrientation(float Dt);
	void UpdateStamina(float Dt, bool bWantsMove);
	void UpdateTelemetry(float Dt, double Moved, bool bHadInput);
	void ApplyTransform();
	void SnapToSurface();

	TWeakObjectPtr<ATreeActor> Tree;

	FVector Pos = FVector::ZeroVector;
	FVector Up = FVector::UpVector;
	FVector Forward = FVector::ForwardVector;
	FVector Velocity = FVector::ZeroVector;
	FVector SurfaceNormal = FVector::UpVector;

	FVector2D MoveInput = FVector2D::ZeroVector;
	FVector CamForward = FVector::ForwardVector;
	FVector CamUp = FVector::UpVector;
	bool bSprintInput = false;
	bool bSprinting = false;
	bool bSprintLocked = false;
	bool bSprintReleasedSinceLock = false; // after running dry, sprint resumes only on a fresh press
	bool bOnTrail = false;
	float TrailBoost = 1.f; // eased toward TrailSpeedMultiplier while on a trail
	float Stamina = 1.f;
	EAntState State = EAntState::Walking;
	bool bPlaced = false;
	int32 CurrentBranch = INDEX_NONE;

	// Telemetry for unattended tests (Leaf.AutoMove).
	double LastProjError = 0.0;
	double MaxProjErrWindow = 0.0;
	double MovedWindow = 0.0;
	double StallMoved = 0.0;
	float StallTimer = 0.f;
	float StallInputTime = 0.f;
	float LogTimer = 0.f;
	int32 StallCount = 0;
	int32 ProjFailCount = 0;
	TSet<int32> VisitedLevels;
	int32 SpareInt = 0;
	float SpareFloat = 0.f;

	// Path following (Riding, Returning).
	TArray<FTrailPoint> FollowPoints;
	int32 FollowIndex = 0;
	double FollowFrac = 0.0;
	int32 FollowDir = 1;
	bool bFollowFinished = false;

	// Falling and walking home.
	FVector FallVelocity = FVector::ZeroVector;
	FVector FallSpinAxis = FVector::RightVector;
	FVector HomeTarget = FVector::ZeroVector;
	FVector HomeTrunkBase = FVector::ZeroVector;
	float HomeTrunkRadius = 1500.f;
	int32 SpareInt2 = 0;
	FVector SpareVector = FVector::ZeroVector;

	// Circling a thin branch: the sense of rotation is locked while the stick holds its direction, so the ant keeps going
	// round past the silhouette and behind the branch (where "screen right" would otherwise fade out and then reverse).
	FVector2D AroundStick = FVector2D::ZeroVector;
	int32 AroundSign = 0;
	float AroundKeep = 0.f;

	// Steering wheel: the world direction along the branch that W means while the stick is held; zero when released.
	FVector WheelAway = FVector::ZeroVector;
};
