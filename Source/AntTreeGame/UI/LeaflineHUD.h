#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "LeaflineHUD.generated.h"

class AAntPawn;
class ADayDirector;
class ALineSystem;

/** Canvas HUD: stamina, carried load, worker pips, cut arc, context prompts and the end-of-day recap. */
UCLASS()
class ANTTREEGAME_API ALeaflineHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;
	/** Video recording: suppress every canvas element, including menus and the build line. */
	bool bHideAll = false;

	/** Menu item under P (viewport pixels) as drawn last frame: a nest row 0..upgrade count, MenuResume or MenuQuit. */
	int32 MenuItemAt(const FVector2D& P) const;
	static constexpr int32 MenuResume = 1000;
	static constexpr int32 MenuQuit = 1001;
	static constexpr int32 MenuOptions = 1002;
	static constexpr int32 MenuOptRow = 1100;   // + row: the whole options row (hover selects)
	static constexpr int32 MenuOptMinus = 1200; // + row: the "<" on a value row
	static constexpr int32 MenuOptPlus = 1300;  // + row: the ">" on a value row
	static constexpr int32 MenuCredits = 1003;     // the pause menu's Credits button, and the title's "credits" line
	static constexpr int32 MenuPageBack = 1004;    // the Back button of the credits and controls pages
	static constexpr int32 MenuControls = 1005;    // the pause menu's Controls button

	/** Credits page: scroll by this many 1080p design pixels (clamped to the text when drawn). */
	void ScrollCredits(float DesignPixels);
	/** Credits page just opened: start at this scroll (design pixels; negative = the end), snapped, and log the layout. */
	void ResetCreditsScroll(float DesignPixels);

	/**
	 * A faint line in the bottom-left corner for reproducing what a screenshot shows: build, tree, seed, day and a
	 * pasteable Leaf.Goto with the ant's position and the camera's heading and pitch. Set False in DefaultGame.ini
	 * ([/Script/AntTreeGame.LeaflineHUD]) before the submission build; Leaf.BuildLine toggles it while playing.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "HUD")
	bool bShowBuildLine = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD")
	FLinearColor StaminaColor = FLinearColor(0.78f, 0.86f, 0.45f, 0.9f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD")
	FLinearColor StaminaLockedColor = FLinearColor(0.85f, 0.35f, 0.25f, 0.9f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD")
	FLinearColor TextColor = FLinearColor(0.95f, 0.93f, 0.85f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD")
	FLinearColor AccentColor = FLinearColor(0.45f, 0.85f, 0.95f, 1.f);

private:
	void DrawStamina(const AAntPawn* Ant, float Scale, float Dt);
	void DrawDayInfo(const ADayDirector* D, float Scale);
	void DrawCarry(const AAntPawn* Ant, float Scale);
	void DrawCutArc(const AAntPawn* Ant, float Scale);
	/** Outline of the leaf the ant would cut, so the player can see the target before pressing cut. */
	void DrawLeafTarget(const AAntPawn* Ant, float Scale);
	/** Outline of one leaf's blade. False (nothing drawn) if part of it is behind the camera. */
	bool DrawLeafOutline(int32 LeafIndex, const FLinearColor& Color, float Thickness);
	/** The line: "+N" at the nest, colony counter, line rows, patch read-out and the harvest overlay. */
	void DrawDeliveryPops(const ALineSystem* Lines, float Scale);
	void DrawColonyCounter(const ADayDirector* D, float Scale, float Dt);
	void DrawLineRows(const ADayDirector* D, const ALineSystem* Lines, float Scale);
	void DrawPatchPreview(const AAntPawn* Ant, const ALineSystem* Lines, float Scale);
	void DrawHarvest(const ADayDirector* D, const ALineSystem* Lines, float Scale);
	/** While cutting: the line the piece will tear along (the part of the bite circle inside the blade), growing with it. */
	void DrawCutLine(const AAntPawn* Ant, float Scale);
	void DrawPrompt(const AAntPawn* Ant, const ADayDirector* D, float Scale);
	/** A short banner when a worker sets out ("Worker 2 of 3 sets out"; the first of the day also names the tree). */
	void DrawShiftBanner(const ADayDirector* D, float Scale, float Dt);
	void DrawRecap(const ADayDirector* D, float Scale);
	void DrawTitle(const ADayDirector* D, float Scale);
	void DrawNest(const ADayDirector* D, float Scale);
	void DrawEndCard(const ADayDirector* D, float Scale);
	void DrawMoveCard(const ADayDirector* D, float Scale);
	void DrawPause(float Scale);
	/** Pause page 2: the rows of credits.txt (UI/Credits.h) in a scrolling window, and Back. */
	void DrawCredits(float Scale);
	/** Pause page 3: each action's buttons for the device in use (prompt tokens, PromptIcons.h) and Back. */
	void DrawControls(float Scale);
	void DrawColonyMeter(float CenterX, float Y, float Width, float Scale);
	/** Text may hold button tokens ("Hold {Cut}: cut", "{orclick}"): drawn as icons for the device in use (PromptIcons.h). */
	void DrawTextCentered(const FString& Text, float CenterX, float Y, const FLinearColor& Color, float TextScale, UFont* Font);
	void DrawTextAt(const FString& Text, float X, float Y, const FLinearColor& Color, float TextScale, UFont* Font);
	FVector2D MeasureText(const FString& Text, UFont* Font, float TextScale) const;
	/** Two measured text heights, in 1080p design pixels; both use the existing engine font. */
	float SecondaryText(float Scale) const;
	float PrimaryText(float Scale) const;
	/** Coordinates describe the content; the shared padding is added outside it. */
	void DrawPanel(float X, float Y, float W, float H, float Scale, float Alpha = 1.f);
	TArray<FString> WrapText(const FString& Text, float Width, float TextScale) const;
	float DrawWrappedText(const FString& Text, float CX, float Y, float Width, const FLinearColor& Color, float TextScale, float Scale);
	void DrawTextPanel(const FString& Text, float CX, float Y, const FLinearColor& Color, float TextScale, float Scale,
		float Alpha = 1.f, float MaxWidth = 900.f);
	bool UsingPad() const;
	void DrawBuildLine(const class AAntPawn* Ant, const class ADayDirector* D, float Scale);
	void DrawArc(float CX, float CY, float Radius, float From01, float To01, const FLinearColor& Color, float Thickness);

	struct FMenuHit
	{
		FBox2D Box;
		int32 Id = INDEX_NONE;
	};
	TArray<FMenuHit> MenuHits; // rebuilt every frame by the menu screens
	void AddMenuHit(float X, float Y, float W, float H, int32 Id) { MenuHits.Add({ FBox2D(FVector2D(X, Y), FVector2D(X + W, Y + H)), Id }); }
	/** The mouse cursor is showing and over this item. */
	bool IsHovered(int32 Id) const;

	// Credits page scroll in 1080p design pixels: the target, the eased value drawn, and the most there is to scroll
	// (from the last frame drawn; negative before that).
	float CreditsScroll = 0.f;
	float CreditsScrollShown = 0.f;
	float CreditsMaxScroll = -1.f;
	bool bCreditsJustOpened = false;

	// Wind exposure of the ant's spot, sampled a few times a second (it marches the field).
	float ExposureShown = -1.f;
	float ExposureTimer = 0.f;

	int32 BannerShift = -1;
	int32 BannerDay = -1;
	float BannerTime = 0.f;
	FString BannerTitle;
	FString BannerLine;

	// Colony counter value shown (ticks up to the real total); negative until first drawn.
	float ColonyShown = -1.f;
	// Recap reveal clock (real seconds since the recap opened) and whether the last frame was the recap.
	float RecapTime = 0.f;
	bool bWasRecap = false;

	float StaminaShown = 1.f;
	float StaminaAlpha = 0.f;
	float SpareFloat = 0.f;
};
