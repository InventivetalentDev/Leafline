#include "Game/LeaflinePlayerController.h"
#include "AntTreeGame.h"
#include "Ant/AntPawn.h"
#include "Ant/AntCamera.h"
#include "Ant/AntMovementComponent.h"
#include "Debug/LeafDebug.h"
#include "World/DayDirector.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Engine/LocalPlayer.h"

ALeaflinePlayerController::ALeaflinePlayerController()
{
	bAutoManageActiveCameraTarget = false;
	bShowMouseCursor = false;
}

AAntPawn* ALeaflinePlayerController::GetAnt() const
{
	return Cast<AAntPawn>(GetPawn());
}

void ALeaflinePlayerController::CreateInput()
{
	if (MappingContext) return;

	auto MakeAction = [this](const TCHAR* Name, EInputActionValueType Type)
	{
		UInputAction* A = NewObject<UInputAction>(this, Name);
		A->ValueType = Type;
		return A;
	};
	MoveAction = MakeAction(TEXT("IA_Move"), EInputActionValueType::Axis2D);
	LookMouseAction = MakeAction(TEXT("IA_LookMouse"), EInputActionValueType::Axis2D);
	LookStickAction = MakeAction(TEXT("IA_LookStick"), EInputActionValueType::Axis2D);
	SprintAction = MakeAction(TEXT("IA_Sprint"), EInputActionValueType::Boolean);
	CutAction = MakeAction(TEXT("IA_Cut"), EInputActionValueType::Boolean);
	GripAction = MakeAction(TEXT("IA_Grip"), EInputActionValueType::Boolean);
	SendHomeAction = MakeAction(TEXT("IA_SendHome"), EInputActionValueType::Boolean);
	FollowAction = MakeAction(TEXT("IA_Follow"), EInputActionValueType::Boolean);
	CycleAction = MakeAction(TEXT("IA_Cycle"), EInputActionValueType::Boolean);
	PauseAction = MakeAction(TEXT("IA_Pause"), EInputActionValueType::Boolean);
	// Resume and quit must still work while the game is paused.
	PauseAction->bTriggerWhenPaused = true;
	SendHomeAction->bTriggerWhenPaused = true;

	MappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Leafline"));
	UInputMappingContext* Ctx = MappingContext;
	auto Swizzle = [Ctx]() { return NewObject<UInputModifierSwizzleAxis>(Ctx); };
	auto Negate = [Ctx]() { return NewObject<UInputModifierNegate>(Ctx); };
	auto DeadZone = [Ctx]()
	{
		UInputModifierDeadZone* D = NewObject<UInputModifierDeadZone>(Ctx);
		D->LowerThreshold = 0.18f;
		return D;
	};

	// Move: WASD and left stick. Value is (X right, Y forward).
	Ctx->MapKey(MoveAction, EKeys::W).Modifiers.Add(Swizzle());
	{
		FEnhancedActionKeyMapping& M = Ctx->MapKey(MoveAction, EKeys::S);
		M.Modifiers.Add(Swizzle());
		M.Modifiers.Add(Negate());
	}
	Ctx->MapKey(MoveAction, EKeys::A).Modifiers.Add(Negate());
	Ctx->MapKey(MoveAction, EKeys::D);
	Ctx->MapKey(MoveAction, EKeys::Gamepad_Left2D).Modifiers.Add(DeadZone());

	Ctx->MapKey(LookMouseAction, EKeys::Mouse2D);
	Ctx->MapKey(LookStickAction, EKeys::Gamepad_Right2D).Modifiers.Add(DeadZone());

	Ctx->MapKey(SprintAction, EKeys::LeftShift);
	Ctx->MapKey(SprintAction, EKeys::Gamepad_LeftTrigger);
	Ctx->MapKey(CutAction, EKeys::LeftMouseButton);
	Ctx->MapKey(CutAction, EKeys::E);
	Ctx->MapKey(CutAction, EKeys::Gamepad_FaceButton_Left);
	Ctx->MapKey(GripAction, EKeys::SpaceBar);
	Ctx->MapKey(GripAction, EKeys::Gamepad_FaceButton_Bottom);
	Ctx->MapKey(SendHomeAction, EKeys::Q);
	Ctx->MapKey(SendHomeAction, EKeys::Gamepad_FaceButton_Top);
	Ctx->MapKey(FollowAction, EKeys::F);
	Ctx->MapKey(FollowAction, EKeys::Gamepad_RightShoulder);
	Ctx->MapKey(CycleAction, EKeys::Tab);
	Ctx->MapKey(CycleAction, EKeys::Gamepad_LeftShoulder);
	Ctx->MapKey(PauseAction, EKeys::Escape);
	Ctx->MapKey(PauseAction, EKeys::P);
	Ctx->MapKey(PauseAction, EKeys::Gamepad_Special_Right);
}

void ALeaflinePlayerController::EnsureCamera()
{
	if (AntCamera) return;
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.ObjectFlags |= RF_Transient;
	AntCamera = GetWorld()->SpawnActor<AAntCamera>(AAntCamera::StaticClass(), FTransform::Identity, Params);
}

void ALeaflinePlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (!IsLocalController()) return;

	CreateInput();
	if (UEnhancedInputLocalPlayerSubsystem* Sub = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		Sub->AddMappingContext(MappingContext, 0);
	}
	SetInputMode(FInputModeGameOnly());
	bShowMouseCursor = false;

	EnsureCamera();
	if (AntCamera)
	{
		if (AAntPawn* Ant = GetAnt()) AntCamera->SetTarget(Ant, false);
		SetViewTarget(AntCamera);
	}
}

void ALeaflinePlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	EnsureCamera();
	if (AntCamera)
	{
		AntCamera->SetTarget(Cast<AAntPawn>(InPawn), bPossessedBefore);
		SetViewTarget(AntCamera);
	}
	bPossessedBefore = true;
}

void ALeaflinePlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	UEnhancedPlayerInput* Input = Cast<UEnhancedPlayerInput>(PlayerInput);
	if (!Input || !MoveAction || !AntCamera) return;

	FVector2D Move = Input->GetActionValue(MoveAction).Get<FVector2D>();
	Move.Y = FMath::Clamp(Move.Y + LeafDebug::AutoMove, -1.0, 1.0);

	const FVector2D Mouse = Input->GetActionValue(LookMouseAction).Get<FVector2D>();
	const FVector2D Stick = Input->GetActionValue(LookStickAction).Get<FVector2D>();
	FVector2D Look = Mouse * MouseSensitivity + Stick * (StickLookRate * DeltaTime);
	if (bInvertPitch) Look.Y = -Look.Y;
	AntCamera->AddLookInput(Look);

	uint32 Held = 0;
	if (Input->GetActionValue(SprintAction).Get<bool>()) Held |= LeafButtons::Sprint;
	if (Input->GetActionValue(CutAction).Get<bool>()) Held |= LeafButtons::Cut;
	if (Input->GetActionValue(GripAction).Get<bool>()) Held |= LeafButtons::Grip;
	if (Input->GetActionValue(SendHomeAction).Get<bool>()) Held |= LeafButtons::SendHome;
	if (Input->GetActionValue(FollowAction).Get<bool>()) Held |= LeafButtons::Follow;
	if (Input->GetActionValue(CycleAction).Get<bool>()) Held |= LeafButtons::Cycle;
	if (Input->GetActionValue(PauseAction).Get<bool>()) Held |= LeafButtons::Pause;
	ButtonsPressed = Held & ~ButtonsHeld;
	ButtonsHeld = Held;

	if (!Director.IsValid()) Director = ADayDirector::Get(this);
	const bool bMenu = (Director.IsValid() && Director->IsMenuPhase()) || IsPaused();
	if (bMenu)
	{
		// Menus: the stick or W/S moves the selection; the ant stands still.
		const int32 Nav = Move.Y > 0.5 ? -1 : (Move.Y < -0.5 ? 1 : 0);
		if (Nav != 0 && Nav != SpareInt && Director.IsValid()) Director->MenuNavigate(Nav);
		SpareInt = Nav;
		Move = FVector2D::ZeroVector;
		Held &= ~(LeafButtons::Sprint | LeafButtons::Follow | LeafButtons::Grip);
	}
	if (AAntPawn* Ant = GetAnt())
	{
		if (UAntMovementComponent* M = Ant->GetAntMovement())
		{
			M->SetMoveInput(Move, AntCamera->GetViewForward(), AntCamera->GetViewUp());
			M->SetSprintInput((Held & LeafButtons::Sprint) != 0);
		}
		Ant->SetActionInput(!bMenu && (Held & LeafButtons::Cut) != 0, (Held & LeafButtons::Follow) != 0, (Held & LeafButtons::Grip) != 0);
	}
	if (ButtonsPressed) HandleButtons(ButtonsPressed);
}

void ALeaflinePlayerController::HandleButtons(uint32 Pressed)
{
	if (Pressed & LeafButtons::Pause)
	{
		SetPause(!IsPaused());
		return;
	}
	if (IsPaused())
	{
		if (Pressed & LeafButtons::SendHome) ConsoleCommand(TEXT("quit"));
		return;
	}
	if (!Director.IsValid()) Director = ADayDirector::Get(this);
	ADayDirector* D = Director.Get();
	if (!D) return;
	if (D->IsMenuPhase())
	{
		if (Pressed & (LeafButtons::Cut | LeafButtons::Grip)) D->MenuConfirm();
		if (Pressed & LeafButtons::SendHome) D->MenuBack();
		return;
	}
	if (Pressed & LeafButtons::SendHome) D->SendHome(GetAnt());
	if (Pressed & LeafButtons::Cycle) D->CycleWorker();
}
