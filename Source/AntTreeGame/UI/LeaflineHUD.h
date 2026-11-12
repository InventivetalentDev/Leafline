#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "LeaflineHUD.generated.h"

class AAntPawn;
class ADayDirector;

/** Canvas HUD: stamina, carried load, worker pips, cut arc, context prompts and the end-of-day recap. */
UCLASS()
class ANTTREEGAME_API ALeaflineHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

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
	void DrawPrompt(const AAntPawn* Ant, const ADayDirector* D, float Scale);
	void DrawRecap(const ADayDirector* D, float Scale);
	void DrawTitle(const ADayDirector* D, float Scale);
	void DrawNest(const ADayDirector* D, float Scale);
	void DrawEndCard(const ADayDirector* D, float Scale);
	void DrawPause(float Scale);
	void DrawColonyMeter(float CenterX, float Y, float Width, float Scale);
	void DrawTextCentered(const FString& Text, float CenterX, float Y, const FLinearColor& Color, float TextScale, UFont* Font);
	void DrawArc(float CX, float CY, float Radius, float From01, float To01, const FLinearColor& Color, float Thickness);

	float StaminaShown = 1.f;
	float StaminaAlpha = 0.f;
	float SpareFloat = 0.f;
};
