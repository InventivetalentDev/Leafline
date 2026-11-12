#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "LeaflineHUD.generated.h"

/** Canvas HUD: stamina for now; load, worker pips, colony meter and prompts arrive with the loop. */
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

private:
	float StaminaShown = 1.f;
	float StaminaAlpha = 0.f;
};
