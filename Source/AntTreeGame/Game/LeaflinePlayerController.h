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
UCLASS(Config = Game)
class ANTTREEGAME_API ALeaflinePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ALeaflinePlayerController();

	virtual void BeginPlay() override;
	virtual void PlayerTick(float DeltaTime) override;
	virtual void OnPossess(APawn* InPawn) override;
	/** Watches which device was used last, for the HUD's button icons. */
	virtual bool InputKey(const FInputKeyEventArgs& Params) override;

	/** The last input came from a gamepad (button, stick or trigger), not the keyboard or mouse: prompts show pad icons. */
	bool IsUsingGamepad() const { return bUsingGamepad; }

	AAntCamera* GetAntCamera() const { return AntCamera; }
	AAntPawn* GetAnt() const;

	/** Buttons held this frame / pressed this frame (LeafButtons bits). */
	uint32 GetButtonsHeld() const { return ButtonsHeld; }
	uint32 GetButtonsPressed() const { return ButtonsPressed; }

	/** Tab / LB held past this long (s) shows the wind map until let go; a shorter tap switches workers. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	float WindMapHoldSeconds = 0.25f;

	/** F3 toggles the input view (LeafDebug::bInputViz). Set False in DefaultGame.ini before the submission build. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bInputVizKey = true;
	bool IsWindMapShown() const { return bWindMapShown; }

	/** Degrees of camera turn per mouse count. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	float MouseSensitivity = 0.25f;

	/** Degrees per second at full right-stick deflection. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	float StickLookRate = 160.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	bool bInvertPitch = false;

	// Pause menu: page 0 is Resume / Options / Credits / Quit, page 1 the options (sensitivity, invert, volume, graphics,
	// back), page 2 the credits (scrolling text and Back).
	static constexpr int32 NumPauseRows = 4;
	static constexpr int32 NumOptionRows = 5;
	static constexpr int32 CreditsPage = 2;
	int32 GetPausePage() const { return PausePage; }
	int32 GetPauseRow() const { return PauseRow; }
	/** The value shown on an options row ("0.25", "On", "80%", "High"). */
	FString GetOptionValueText(int32 Row) const;
	/** Debug (Leaf.PauseMenu): pause and show a page and row of the pause menu; on the credits page the row is the scroll. */
	void DebugShowPauseMenu(int32 Page, int32 Row);
	/**
	 * Pause and show the credits page, scrolled this far (1080p design pixels; negative = the end). From the title screen
	 * Back returns to the title (unpaused), otherwise to the pause menu.
	 */
	void OpenCredits(float Scroll = 0.f);

private:
	void CreateInput();
	void EnsureCamera();
	void HandleButtons(uint32 Pressed);
	/** Tab / LB during play: tap switches workers (on release), hold shows the wind map (AExposureOverlay). */
	void UpdateWindMapButton(uint32 Pressed);
	void ShowWindMap(bool bShow);
	/** Shows the cursor in menus once the mouse moves (hidden again by keys or stick) and tracks the item under it. */
	void UpdateMenuCursor(bool bMenu, const FVector2D& MouseDelta, int32 Nav);
	/** Left click in a menu: the item under the cursor (nest row, pause button), or "continue" on the other screens. */
	void HandleMenuClick();
	void ActivatePauseRow();
	void CloseCredits();
	/** Credits page: W/S or the left stick, the arrow keys and d-pad scroll smoothly, the mouse wheel in steps. */
	void ScrollCredits(const FVector2D& Move);
	/** Step an options row up or down (Dir +1 / -1), apply and save it. */
	void ChangeOption(int32 Row, int32 Dir);
	void LoadOptions();
	void SaveOptions() const;

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
	double CycleDownAt = -1.0;
	bool bUsingGamepad = false;
	bool bWindMapShown = false;
	bool bPossessedBefore = false;
	TWeakObjectPtr<class ADayDirector> Director;
	float SpareFloat = 0.f;
	int32 SpareInt = 0;
	bool bMenuCursor = false;
	bool bCursorShown = false;
	bool bSwallowClick = false; // a menu click: the held mouse button is not "cut" until released
	bool bSwallowConfirm = false;
	int32 PausePage = 0;
	int32 PauseRow = 0;
	int32 PrevNavX = 0;
	float MasterVolume = 1.f;
	int32 HoverItem = INDEX_NONE;
	FVector2D LastCursor = FVector2D(-1.0, -1.0);
	bool bCreditsFromTitle = false; // the credits were opened from the title screen: Back unpauses to it
	int32 CreditsWheel = 0;         // mouse wheel steps since the last frame on the credits page (+ = down)
};
