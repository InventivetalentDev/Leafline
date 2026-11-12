#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
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
	/** Ground distance from the trunk axis at which the ant is turned back (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bounds") float BoundaryRadius = 7000.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bounds") float BoundaryWidth = 800.f;
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

	void TeleportTo(const FVector& Location, const FVector& Facing);
	bool PlaceOnBranch(int32 Branch, float T);

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
};
