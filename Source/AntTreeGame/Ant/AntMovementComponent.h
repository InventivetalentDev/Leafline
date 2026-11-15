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
	/** Height of the ant's origin above the field surface (cm); the mesh's feet are drawn MeshSinkCm below it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface") float RideHeight = 1.5f;
	/** The ant mesh and the carried piece are drawn this far below the origin (cm), so the feet touch the bark instead of
	 *  hovering RideHeight above it (playtest, 13 Nov). Visual only: RideHeight, the routes and the trail keep theirs. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface") float MeshSinkCm = 1.2f;
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
	/**
	 * 0: screen direction everywhere (owner's choice, 13 Nov). The stick is a direction on screen, read from the camera
	 * when you start pushing and kept (carried along the bark) while you hold it, so the camera is free meanwhile. On
	 * branches it snaps to along or around, whichever is closer. 1: the older steering wheel on branches under
	 * WheelFadeRadius, camera-relative elsewhere.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Controls") int32 ControlStyle = 0;
	/** Screen direction: on thin twigs a diagonal snaps to along or around (no fast corkscrews). False: diagonals go diagonally. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Controls") bool bSnapOnBranches = false;
	/** Screen direction on a branch: how the branch lies on screen is its perspective projection at the ant; this much of
	 *  "away from the camera" is added as up, so a branch pointing straight at the camera still reads as running up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Controls") float AxisDepthWeight = 0.25f;
	/** Going round a branch at most this many laps a second (walking along it is not limited); thin twigs only, since
	 *  from about 30 cm radius full speed is under a lap a second anyway. 0: no limit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Controls") float MaxLapsPerSecond = 1.f;
	/** Screen direction: the snap applies fully on wood up to this radius and fades out by SnapFadeRadius (cm). Thicker wood
	 *  keeps the held direction free, so you can steer onto a twig (a snap on limbs turned the ant round curving boughs). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Controls") float SnapFullRadius = 25.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Controls") float SnapFadeRadius = 60.f;
	/** Screen direction: the stick centred this long (s) counts as a pause, and the next push reads the camera again.
	 *  0.05 since 14 Nov (owner: 0.15 felt like waiting before the controls followed the camera). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Controls") float PauseSeconds = 0.05f;
	/** Screen direction: while you hold a direction and turn the camera yourself (mouse or right stick), the held
	 *  direction is read from the camera again every frame, so W follows your look at once; the camera's own roll and
	 *  follow still cannot change it. Off by default (owner's trial, 14 Nov): Leaf.Tune move bHeldFollowsLook True. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Controls") bool bHeldFollowsLook = false;
	/** Screen direction: a jump of the stick direction larger than this (deg, frame to frame) is a new input: the camera is read again. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Controls") float NewInputAngleDeg = 40.f;
	/** Screen direction: how much "away from the camera" counts as up on screen, besides screen-up itself. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Controls") float ScreenDepthWeight = 0.35f;
	/** Screen direction, exact (on by default since 15 Nov): W/A/S/D are the bark directions whose image on
	 *  screen at the ant points exactly up/left/down/right (the perspective projection of the bark's tangent plane,
	 *  inverted), instead of the branch-on-screen rule and the "away counts as up" weights. Where the bark is seen nearly
	 *  edge-on the inverse is unstable, so it blends back to the old rule (see ExactMinFit). On by default since the owner tried it
	 *  (15 Nov); Leaf.Tune move bExactScreenDir False gives the old branch-on-screen rule. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Controls") bool bExactScreenDir = true;
	/** Exact screen direction: the sine of the angle between the exact W and D on the bark (1 = square; a rolled,
	 *  grazing view folds D toward W): the old rule under ExactMinFit, the exact mapping over ExactFullFit, blended. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Controls") float ExactMinFit = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Controls") float ExactFullFit = 0.8f;
	/** Screen direction: another of along / around must fit the held direction this much better (dot) before the snap changes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Controls") float SnapStickiness = 0.15f;
	/** The wheel applies fully on branches up to this radius and fades out by WheelFadeRadius (cm); thicker wood
	 *  (the trunk, over 5 m radius where play happens) stays camera-relative. Was 90 / 200: on limbs of 140 to 220 cm
	 *  the half-faded wheel let camera-relative "right" point into the wood at the far side, and the ant turned back
	 *  along the limb (Leaf.Orbit 12 150). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thin branches") float WheelFullRadius = 200.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thin branches") float WheelFadeRadius = 320.f;
	/**
	 * Walking seen from behind (playtest: the legs barely read from there). The body sways with the stride: it wags
	 * (yaw) and rocks (roll) once per stride as the two tripods alternate, and bobs twice. Scaled by walking speed;
	 * all zero gives the old rigid body.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gait look") float GaitWagDeg = 3.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gait look") float GaitRollDeg = 4.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gait look") float GaitBobCm = 0.45f;
	/** Distance walked per leg cycle (cm) and the fastest cycle (Hz). Was 14 cm and no cap: 14 cycles a second at a walk
	 *  and 28 sprinting, 2 to 4 frames per cycle at 60 fps, so the legs aliased and looked still. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gait look") float GaitStrideCm = 36.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gait look") float GaitMaxHz = 7.f;
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
	/** The player turned the camera this frame (mouse or right stick); read by bHeldFollowsLook. */
	void SetPlayerLooking(bool bLooking) { bPlayerLooking = bLooking; }
	void SetMoveInput(const FVector2D& Input, const FVector& CamForward, const FVector& CamUp, const FVector& CamLocation = FVector::ZeroVector);
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
	/** Recruits: Riding at this speed instead of RideSpeed (0 = RideSpeed), and Returning at the laden walking speed
	 *  times ReturnScale. */
	void SetFollowSpeed(float RideOverride, float ReturnScale) { FollowRideOverride = RideOverride; FollowReturnScale = ReturnScale; }
	int32 GetFollowIndex() const { return FollowIndex; }
	int32 GetFollowCount() const { return FollowPoints.Num(); }
	bool IsFollowFinished() const { return bFollowFinished; }
	/** Times a follower was pinned and stepped onto its route point (diagnostics). */
	int32 GetFollowSnaps() const { return FollowSnaps; }

	/** One movement step of Dt seconds. Ticking calls it with the frame time unless the owner steps it itself. */
	void Step(float Dt, bool bApplyTransform = true);
	/** The owner calls Step (fixed sim steps) instead of the component ticking. */
	void SetExternalStep(bool bExternal) { bExternalStep = bExternal; }
	void ApplyTransformNow() { ApplyTransform(); }
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
	/** Where the ant is drawn: its position and up lifted onto a junction's skirt when one covers it (fillets are looks
	 *  only, the sim walks on the field without them), else the surface position and body up. Set by ApplyTransform. */
	const FVector& GetShownPosition() const { return bShownLifted ? ShownPos : Pos; }
	const FVector& GetShownUp() const { return bShownLifted ? ShownUp : Up; }
	/** The component holding the ant's visuals (meshes and the carried piece): ApplyTransform moves it onto the skirt. */
	void SetShownComponent(USceneComponent* C) { ShownComponent = C; }
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
	/** Screen-direction controls: take or keep the snapshot of the camera (the held basis) for this frame. */
	void UpdateHeldInput(float Dt, const FVector& N, const FVector& Axis);
	/** The surface directions that "up" and "right" on screen mean at a point with normal N, from the live camera.
	 *  Mode -1 follows bExactScreenDir; 0 forces the branch-on-screen rule, 1 the exact mapping (the log compares them). */
	void MakeScreenBasis(const FVector& N, const FVector& Axis, FVector& OutFwd, FVector& OutRight, int32 Mode = -1) const;
	/** Exact screen direction: fills the tangent directions whose screen image points up and right; returns the fit
	 *  (see ExactMinFit), 0 when the bark is seen edge-on or there is no camera. */
	float MakeExactScreenBasis(const FVector& N, const FVector& Axis, FVector& OutFwd, FVector& OutRight) const;
	/** Along / around candidates on a branch for a direction, with the current snap kept unless another fits clearly better. */
	int32 ChooseSnap(const FVector& Dir, const FVector& A, const FVector& Around, int32 Current) const;
	/** What a pretend stick input would do here, without changing any state (the input view). */
	FVector PreviewWish(const FVector2D& Input, const FVector& N, const FVector& Axis, float BranchBlend, float RadiusHere, float DistToEnd) const;
	/** Input view drawing and the control trace in the log (player's ant only). */
	void UpdateInputViz(float Dt, const FVector& Axis, float RadiusHere, float ThinBlend, float BranchBlend, bool bWheel, bool bWantsMove,
		const FVector& LastWish, const FVector& LastIntent, float DistToEnd);
	void UpdateOrientation(float Dt);
	void UpdateStamina(float Dt, bool bWantsMove);
	void UpdateTelemetry(float Dt, double Moved, bool bHadInput);
	void ApplyTransform();
	void SnapToSurface();

	TWeakObjectPtr<ATreeActor> Tree;

	FVector Pos = FVector::ZeroVector;
	FVector Up = FVector::UpVector;
	FVector ShownPos = FVector::ZeroVector;
	FVector ShownUp = FVector::UpVector;
	bool bShownLifted = false;
	TWeakObjectPtr<USceneComponent> ShownComponent;
	FVector Forward = FVector::ForwardVector;
	FVector Velocity = FVector::ZeroVector;
	FVector SurfaceNormal = FVector::UpVector;

	FVector2D MoveInput = FVector2D::ZeroVector;
	FVector CamForward = FVector::ForwardVector;
	FVector CamUp = FVector::UpVector;
	FVector CamPos = FVector::ZeroVector;
	bool bHasCamPos = false;
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

	// Screen-direction controls: the surface directions that screen-up and screen-right meant when the stick was pushed,
	// carried along the bark while it is held (zero when released for PauseSeconds); the last stick direction; the snap
	// on a branch (0 / 1 along +axis / -axis, 2 / 3 around, -1 none).
	FVector HeldFwd = FVector::ZeroVector;
	FVector HeldRight = FVector::ZeroVector;
	FVector2D HeldStickDir = FVector2D::ZeroVector;
	float HeldIdle = 0.f;
	bool bPlayerLooking = false;
	int32 SnapChoice = -1;

	float FollowWait = 0.f;
	double FollowLag = 0.0;
	int32 FollowSnaps = 0;
	float FollowRideOverride = 0.f;
	float FollowReturnScale = 1.f;
	bool bExternalStep = false;
};
