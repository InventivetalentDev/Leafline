#include "Game/LeaflinePlayerController.h"
#include "Misc/App.h"
#include "AntTreeGame.h"
#include "Ant/AntPawn.h"
#include "Ant/AntCamera.h"
#include "Ant/AntMovementComponent.h"
#include "Debug/LeafDebug.h"
#include "Game/Upgrades.h"
#include "UI/LeaflineHUD.h"
#include "World/DayDirector.h"
#include "World/ExposureOverlay.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"
#include "InputAction.h"
#include "World/BirdSystem.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/GameUserSettings.h"
#include "AudioDevice.h"
#include "Audio/LeafSound.h"
#include "Engine/Engine.h"
#include "InputKeyEventArgs.h"
#include "Misc/ConfigCacheIni.h"

ALeaflinePlayerController::ALeaflinePlayerController()
{
	bAutoManageActiveCameraTarget = false;
	bShowMouseCursor = false;
	// While paused the engine only calls Tick, not PlayerTick, unless this is set; pause and resume live in PlayerTick.
	bShouldPerformFullTickWhenPaused = true;
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
	DropAction = MakeAction(TEXT("IA_Drop"), EInputActionValueType::Boolean);
	// Resume and quit must still work while the game is paused.
	PauseAction->bTriggerWhenPaused = true;
	SendHomeAction->bTriggerWhenPaused = true;
	// The pause menu is driven by the stick / WASD and confirmed with cut or grip.
	MoveAction->bTriggerWhenPaused = true;
	CutAction->bTriggerWhenPaused = true;
	GripAction->bTriggerWhenPaused = true;
	// Mouse movement shows the cursor on the pause screen (mouse look itself is ignored in menus).
	LookMouseAction->bTriggerWhenPaused = true;

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
	// The bird's bait (World/BirdSystem.*): only acts while the bird is about and the ant carries a piece.
	Ctx->MapKey(DropAction, EKeys::G);
	Ctx->MapKey(DropAction, EKeys::Gamepad_FaceButton_Right);
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
	LoadOptions();
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

	// Without focus (alt-tab, clicking another window) the input system stops getting stick updates and the last
	// analog value would stay applied: the camera kept spinning. Ignore device input until focus returns.
	const bool bHasFocus = FApp::HasFocus();
	FVector2D Move = bHasFocus ? Input->GetActionValue(MoveAction).Get<FVector2D>() : FVector2D::ZeroVector;
	Move.Y = FMath::Clamp(Move.Y + LeafDebug::AutoMove, -1.0, 1.0);
	Move.X = FMath::Clamp(Move.X + LeafDebug::AutoStrafe, -1.0, 1.0);

	if (!Director.IsValid()) Director = ADayDirector::Get(this);
	const bool bMenu = (Director.IsValid() && Director->IsMenuPhase()) || IsPaused();

	const FVector2D Mouse = bHasFocus ? Input->GetActionValue(LookMouseAction).Get<FVector2D>() : FVector2D::ZeroVector;
	FVector2D Stick = bHasFocus ? Input->GetActionValue(LookStickAction).Get<FVector2D>() : FVector2D::ZeroVector;
	// The owner's preference (2026-11-12): the right stick looks inverted (push up to look down); the mouse does not.
	// bInvertPitch below still flips both.
	Stick.Y = -Stick.Y;
	// In menus the mouse moves the cursor, not the camera.
	FVector2D Look = (bMenu ? FVector2D::ZeroVector : Mouse * MouseSensitivity) + Stick * (StickLookRate * DeltaTime);
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
	if (DropAction && Input->GetActionValue(DropAction).Get<bool>()) Held |= LeafButtons::Drop;
	// Debug: buttons held by Leaf.Hold.
	if (LeafDebug::HoldCut > 0.f) { Held |= LeafButtons::Cut; LeafDebug::HoldCut -= DeltaTime; }
	if (LeafDebug::HoldGrip > 0.f) { Held |= LeafButtons::Grip; LeafDebug::HoldGrip -= DeltaTime; }
	if (LeafDebug::HoldFollow > 0.f) { Held |= LeafButtons::Follow; LeafDebug::HoldFollow -= DeltaTime; }
	if (LeafDebug::HoldCycle > 0.f) { Held |= LeafButtons::Cycle; LeafDebug::HoldCycle -= DeltaTime; }
	if (bSwallowClick)
	{
		if (IsInputKeyDown(EKeys::LeftMouseButton)) Held &= ~LeafButtons::Cut;
		else bSwallowClick = false;
	}
	// After a menu choice with cut or grip (say Resume), those buttons count again only once released, so the ant does
	// not start a cut or a grip the moment the game continues.
	if (bSwallowConfirm)
	{
		if (Held & (LeafButtons::Cut | LeafButtons::Grip)) Held &= ~(LeafButtons::Cut | LeafButtons::Grip);
		else bSwallowConfirm = false;
	}
	ButtonsPressed = Held & ~ButtonsHeld;
	ButtonsHeld = Held;

	int32 Nav = 0;
	if (bMenu)
	{
		// Menus: the stick or W/S moves the selection; the ant stands still.
		Nav = Move.Y > 0.5 ? -1 : (Move.Y < -0.5 ? 1 : 0);
		const int32 NavX = Move.X > 0.5 ? 1 : (Move.X < -0.5 ? -1 : 0);
		if (IsPaused())
		{
			const int32 Rows = PausePage == 0 ? NumPauseRows : NumOptionRows;
			if (Nav != 0 && Nav != SpareInt)
			{
				PauseRow = (PauseRow + Nav + Rows) % Rows;
				LeafSound::Play(this, TEXT("ui_move"), 0.5f);
			}
			if (PausePage == 1 && NavX != 0 && NavX != PrevNavX && PauseRow < NumOptionRows - 1) ChangeOption(PauseRow, NavX);
		}
		else if (Nav != 0 && Nav != SpareInt && Director.IsValid())
		{
			Director->MenuNavigate(Nav);
		}
		SpareInt = Nav;
		PrevNavX = NavX;
		Move = FVector2D::ZeroVector;
		Held &= ~(LeafButtons::Sprint | LeafButtons::Follow | LeafButtons::Grip);
	}
	UpdateMenuCursor(bMenu, Mouse, Nav);
	if (bMenu && bCursorShown && WasInputKeyJustPressed(EKeys::LeftMouseButton))
	{
		// The click picks what is under the cursor; it must not also count as the confirm button.
		ButtonsPressed &= ~LeafButtons::Cut;
		bSwallowClick = true;
		HandleMenuClick();
	}
	if (AAntPawn* Ant = GetAnt())
	{
		if (UAntMovementComponent* M = Ant->GetAntMovement())
		{
			M->SetMoveInput(Move, AntCamera->GetViewForward(), AntCamera->GetViewUp(), AntCamera->GetActorLocation());
			M->SetSprintInput((Held & LeafButtons::Sprint) != 0);
		}
		Ant->SetActionInput(!bMenu && (Held & LeafButtons::Cut) != 0, (Held & LeafButtons::Follow) != 0, (Held & LeafButtons::Grip) != 0);
	}
	if (bInputVizKey && WasInputKeyJustPressed(EKeys::F3))
	{
		LeafDebug::bInputViz = !LeafDebug::bInputViz;
		UE_LOG(LogLeafline, Log, TEXT("Input view %s (F3)"), LeafDebug::bInputViz ? TEXT("on") : TEXT("off"));
	}
	UpdateWindMapButton(ButtonsPressed);
	if (ButtonsPressed) HandleButtons(ButtonsPressed);
}

bool ALeaflinePlayerController::InputKey(const FInputKeyEventArgs& Params)
{
	// Prompts follow the device in use: a pad button or a clear stick or trigger push switches to pad icons; a key, a
	// mouse button or a real mouse movement switches back.
	const FKey& Key = Params.Key;
	if (Key.IsGamepadKey())
	{
		if (Params.Event == IE_Pressed || (Key.IsAnalog() && FMath::Abs(Params.AmountDepressed) > 0.35f)) bUsingGamepad = true;
	}
	else if (Key == EKeys::MouseX || Key == EKeys::MouseY)
	{
		if (FMath::Abs(Params.AmountDepressed) > 3.f) bUsingGamepad = false;
	}
	else if (Params.Event == IE_Pressed && !Params.bIsTouchEvent)
	{
		bUsingGamepad = false;
	}
	return Super::InputKey(Params);
}

void ALeaflinePlayerController::UpdateWindMapButton(uint32 Pressed)
{
	if (!Director.IsValid()) Director = ADayDirector::Get(this);
	ADayDirector* D = Director.Get();
	const bool bPlay = D && !IsPaused() && !D->IsMenuPhase() && !D->IsHarvest();
	if (!bPlay)
	{
		CycleDownAt = -1.0;
		ShowWindMap(false);
		return;
	}
	const double Now = FPlatformTime::Seconds();
	if (Pressed & LeafButtons::Cycle) CycleDownAt = Now;
	if (CycleDownAt < 0.0) return;
	if (ButtonsHeld & LeafButtons::Cycle)
	{
		if (!bWindMapShown && Now - CycleDownAt >= WindMapHoldSeconds) ShowWindMap(true);
		return;
	}
	// Let go: a short tap switches workers; after a hold it only hides the map.
	if (!bWindMapShown) D->CycleWorker();
	ShowWindMap(false);
	CycleDownAt = -1.0;
}

void ALeaflinePlayerController::ShowWindMap(bool bShow)
{
	if (bShow == bWindMapShown) return;
	bWindMapShown = bShow;
	if (AExposureOverlay* Overlay = AExposureOverlay::Get(GetWorld())) Overlay->SetShown(bShow);
	UE_LOG(LogLeafline, Log, TEXT("Wind map %s"), bShow ? TEXT("shown (Tab held)") : TEXT("hidden"));
}

void ALeaflinePlayerController::UpdateMenuCursor(bool bMenu, const FVector2D& MouseDelta, int32 Nav)
{
	if (!bMenu || Nav != 0) bMenuCursor = false;
	else if (MouseDelta.SizeSquared() > 1.0) bMenuCursor = true;

	const bool bWant = bMenu && bMenuCursor;
	if (bWant != bCursorShown)
	{
		bCursorShown = bWant;
		bShowMouseCursor = bWant;
		if (bWant)
		{
			FInputModeGameAndUI Mode;
			Mode.SetHideCursorDuringCapture(false);
			Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			SetInputMode(Mode);
		}
		else
		{
			SetInputMode(FInputModeGameOnly());
		}
		LastCursor = FVector2D(-1.0, -1.0);
	}
	HoverItem = INDEX_NONE;
	float X = 0.f, Y = 0.f;
	if (!bCursorShown || !GetMousePosition(X, Y)) return;
	const FVector2D Cursor(X, Y);
	const ALeaflineHUD* Hud = Cast<ALeaflineHUD>(GetHUD());
	HoverItem = Hud ? Hud->MenuItemAt(Cursor) : INDEX_NONE;
	// Only a moving cursor takes the selection, so a resting mouse does not fight W/S.
	if (Cursor != LastCursor && IsPaused())
	{
		if (PausePage == 0)
		{
			if (HoverItem == ALeaflineHUD::MenuResume) PauseRow = 0;
			else if (HoverItem == ALeaflineHUD::MenuOptions) PauseRow = 1;
			else if (HoverItem == ALeaflineHUD::MenuQuit) PauseRow = 2;
		}
		else if (HoverItem >= ALeaflineHUD::MenuOptRow && HoverItem < ALeaflineHUD::MenuOptRow + NumOptionRows)
		{
			PauseRow = HoverItem - ALeaflineHUD::MenuOptRow;
		}
	}
	else if (Cursor != LastCursor && HoverItem >= 0 && HoverItem <= LeaflineUpgrades::Count && Director.IsValid())
	{
		Director->SetNestSelection(HoverItem);
	}
	LastCursor = Cursor;
}

void ALeaflinePlayerController::HandleMenuClick()
{
	if (IsPaused())
	{
		if (HoverItem == ALeaflineHUD::MenuResume) SetPause(false);
		else if (HoverItem == ALeaflineHUD::MenuQuit) ConsoleCommand(TEXT("quit"));
		else if (HoverItem == ALeaflineHUD::MenuOptions)
		{
			PausePage = 1;
			PauseRow = 0;
		}
		else if (HoverItem >= ALeaflineHUD::MenuOptMinus && HoverItem < ALeaflineHUD::MenuOptMinus + NumOptionRows) ChangeOption(HoverItem - ALeaflineHUD::MenuOptMinus, -1);
		else if (HoverItem >= ALeaflineHUD::MenuOptPlus && HoverItem < ALeaflineHUD::MenuOptPlus + NumOptionRows) ChangeOption(HoverItem - ALeaflineHUD::MenuOptPlus, 1);
		else if (HoverItem >= ALeaflineHUD::MenuOptRow && HoverItem < ALeaflineHUD::MenuOptRow + NumOptionRows)
		{
			PauseRow = HoverItem - ALeaflineHUD::MenuOptRow;
			ActivatePauseRow();
		}
		return;
	}
	ADayDirector* D = Director.Get();
	if (!D || !D->IsMenuPhase()) return;
	if (D->GetPhase() == EDayPhase::Nest)
	{
		// Only a click on a row buys or starts; a stray click beside the list does nothing.
		if (HoverItem >= 0 && HoverItem <= LeaflineUpgrades::Count)
		{
			D->SetNestSelection(HoverItem);
			D->MenuConfirm();
		}
		return;
	}
	D->MenuConfirm(); // title, recap and end card: a click anywhere continues
}

void ALeaflinePlayerController::DebugShowPauseMenu(int32 Page, int32 Row)
{
	if (!IsPaused()) SetPause(true);
	PausePage = FMath::Clamp(Page, 0, 1);
	PauseRow = FMath::Clamp(Row, 0, (PausePage == 0 ? NumPauseRows : NumOptionRows) - 1);
}

void ALeaflinePlayerController::ActivatePauseRow()
{
	LeafSound::Play(this, TEXT("ui_confirm"), 0.6f);
	bSwallowConfirm = true;
	if (PausePage == 0)
	{
		if (PauseRow == 0) SetPause(false);
		else if (PauseRow == 1)
		{
			PausePage = 1;
			PauseRow = 0;
		}
		else ConsoleCommand(TEXT("quit"));
		return;
	}
	if (PauseRow >= NumOptionRows - 1)
	{
		PausePage = 0;
		PauseRow = 1;
		return;
	}
	ChangeOption(PauseRow, 1);
}

namespace
{
	const TCHAR* OptionsSection = TEXT("Leafline.Options");
	int32 QualityLevel()
	{
		const UGameUserSettings* S = GEngine ? GEngine->GetGameUserSettings() : nullptr;
		const int32 Q = S ? S->GetOverallScalabilityLevel() : 3;
		return Q < 0 ? 3 : FMath::Min(Q, 3); // custom or cinematic reads as Epic
	}
}

FString ALeaflinePlayerController::GetOptionValueText(int32 Row) const
{
	switch (Row)
	{
	case 0: return FString::Printf(TEXT("%.2f"), MouseSensitivity);
	case 1: return bInvertPitch ? TEXT("On") : TEXT("Off");
	case 2: return FString::Printf(TEXT("%d%%"), FMath::RoundToInt(MasterVolume * 100.f));
	case 3:
	{
		static const TCHAR* Names[4] = { TEXT("Low"), TEXT("Medium"), TEXT("High"), TEXT("Epic") };
		return Names[QualityLevel()];
	}
	default: return FString();
	}
}

void ALeaflinePlayerController::ChangeOption(int32 Row, int32 Dir)
{
	switch (Row)
	{
	case 0:
		MouseSensitivity = FMath::Clamp(FMath::RoundToFloat((MouseSensitivity + 0.05f * Dir) / 0.05f) * 0.05f, 0.05f, 1.f);
		break;
	case 1:
		bInvertPitch = !bInvertPitch;
		break;
	case 2:
		MasterVolume = FMath::Clamp(FMath::RoundToFloat((MasterVolume + 0.1f * Dir) * 10.f) / 10.f, 0.f, 1.f);
		if (UWorld* World = GetWorld())
		{
			if (FAudioDeviceHandle Device = World->GetAudioDevice()) Device->SetTransientPrimaryVolume(MasterVolume);
		}
		break;
	case 3:
		if (UGameUserSettings* S = GEngine ? GEngine->GetGameUserSettings() : nullptr)
		{
			// Low turns off Lumen and most shadows, for weaker machines.
			S->SetOverallScalabilityLevel((QualityLevel() + Dir + 4) % 4);
			S->ApplySettings(false);
		}
		break;
	default:
		return;
	}
	LeafSound::Play(this, TEXT("ui_move"), 0.5f);
	UE_LOG(LogLeafline, Log, TEXT("Options: row %d set to %s"), Row, *GetOptionValueText(Row));
	SaveOptions();
}

void ALeaflinePlayerController::LoadOptions()
{
	if (!GConfig) return;
	float F = 0.f;
	bool B = false;
	if (GConfig->GetFloat(OptionsSection, TEXT("MouseSensitivity"), F, GGameUserSettingsIni)) MouseSensitivity = FMath::Clamp(F, 0.05f, 1.f);
	if (GConfig->GetBool(OptionsSection, TEXT("InvertPitch"), B, GGameUserSettingsIni)) bInvertPitch = B;
	if (GConfig->GetFloat(OptionsSection, TEXT("Volume"), F, GGameUserSettingsIni)) MasterVolume = FMath::Clamp(F, 0.f, 1.f);
	if (UWorld* World = GetWorld())
	{
		if (FAudioDeviceHandle Device = World->GetAudioDevice()) Device->SetTransientPrimaryVolume(MasterVolume);
	}
}

void ALeaflinePlayerController::SaveOptions() const
{
	if (GConfig)
	{
		GConfig->SetFloat(OptionsSection, TEXT("MouseSensitivity"), MouseSensitivity, GGameUserSettingsIni);
		GConfig->SetBool(OptionsSection, TEXT("InvertPitch"), bInvertPitch, GGameUserSettingsIni);
		GConfig->SetFloat(OptionsSection, TEXT("Volume"), MasterVolume, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
	if (UGameUserSettings* S = GEngine ? GEngine->GetGameUserSettings() : nullptr) S->SaveSettings();
}

void ALeaflinePlayerController::HandleButtons(uint32 Pressed)
{
	if (Pressed & LeafButtons::Pause)
	{
		SetPause(!IsPaused());
		PausePage = 0;
		PauseRow = 0;
		return;
	}
	if (IsPaused())
	{
		if (Pressed & LeafButtons::SendHome)
		{
			if (PausePage == 0) ConsoleCommand(TEXT("quit"));
			else
			{
				PausePage = 0;
				PauseRow = 1;
			}
		}
		else if (Pressed & (LeafButtons::Cut | LeafButtons::Grip))
		{
			ActivatePauseRow();
		}
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
	if (D->IsHarvest())
	{
		// The harvest plays by itself: confirm skips it, cycle hops between the nest and the lines' patches.
		if (Pressed & (LeafButtons::Cut | LeafButtons::Grip)) D->SkipHarvest();
		if (Pressed & LeafButtons::Cycle) D->CycleHarvestView();
		return;
	}
	if (Pressed & LeafButtons::SendHome) D->SendHome(GetAnt());
	if (Pressed & LeafButtons::Drop)
	{
		if (ABirdSystem* Bird = ABirdSystem::Get(this)) Bird->DropPiece(GetAnt());
	}
	// Cycle (Tab / LB) during play is UpdateWindMapButton's: a tap switches workers on release, a hold shows the wind map.
}
