#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "LeaflinePlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
class AAntCamera;
class AAntPawn;

/** Bit per button action, for edge detection. */
namespace LeafButtons
{
	enum : uint32
	{
		Sprint = 1 << 0,
		Cut = 1 << 1,
		Grip = 1 << 2,
		SendHome = 1 << 3,
		Follow = 1 << 4,
		Cycle = 1 << 5,
		Pause = 1 << 6,
	};
}

/** Creates input in code (no input assets), owns the ant camera and feeds input to the controlled ant. */
UCLASS()
class ANTTREEGAME_API ALeaflinePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ALeaflinePlayerController();

	virtual void BeginPlay() override;
	virtual void PlayerTick(float DeltaTime) override;
	virtual void OnPossess(APawn* InPawn) override;

	AAntCamera* GetAntCamera() const { return AntCamera; }
	AAntPawn* GetAnt() const;

	/** Buttons held this frame / pressed this frame (LeafButtons bits). */
	uint32 GetButtonsHeld() const { return ButtonsHeld; }
	uint32 GetButtonsPressed() const { return ButtonsPressed; }

	/** Degrees of camera turn per mouse count. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	float MouseSensitivity = 0.12f;

	/** Degrees per second at full right-stick deflection. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	float StickLookRate = 160.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	bool bInvertPitch = false;

private:
	void CreateInput();
	void EnsureCamera();
	void HandleButtons(uint32 Pressed);

	UPROPERTY(Transient) TObjectPtr<AAntCamera> AntCamera;
	UPROPERTY(Transient) TObjectPtr<UInputMappingContext> MappingContext;
	UPROPERTY(Transient) TObjectPtr<UInputAction> MoveAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> LookMouseAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> LookStickAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> SprintAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> CutAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> GripAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> SendHomeAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> FollowAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> CycleAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> PauseAction;

	uint32 ButtonsHeld = 0;
	uint32 ButtonsPressed = 0;
	bool bPossessedBefore = false;
	float SpareFloat = 0.f;
	int32 SpareInt = 0;
};
