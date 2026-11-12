#include "UI/LeaflineHUD.h"
#include "Ant/AntPawn.h"
#include "Ant/AntMovementComponent.h"
#include "Engine/Canvas.h"

void ALeaflineHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas) return;

	const AAntPawn* Ant = Cast<AAntPawn>(GetOwningPawn());
	const UAntMovementComponent* M = Ant ? Ant->GetAntMovement() : nullptr;
	if (!M) return;

	const float Dt = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f;
	const float Stamina = M->GetStamina();
	StaminaShown = FMath::FInterpTo(StaminaShown, Stamina, Dt, 12.f);

	// The bar fades out when full and idle, and in while it is being used.
	const bool bActive = Stamina < 0.999f || M->IsSprinting();
	StaminaAlpha = FMath::FInterpTo(StaminaAlpha, bActive ? 1.f : 0.f, Dt, bActive ? 10.f : 2.f);
	if (StaminaAlpha < 0.01f) return;

	const float Scale = Canvas->ClipY / 1080.f;
	const float W = 280.f * Scale, H = 9.f * Scale;
	const float X = (Canvas->ClipX - W) * 0.5f;
	const float Y = Canvas->ClipY - 70.f * Scale;

	FLinearColor Back(0.f, 0.f, 0.f, 0.45f * StaminaAlpha);
	FLinearColor Fill = M->IsSprintLocked() ? StaminaLockedColor : StaminaColor;
	Fill.A *= StaminaAlpha;
	DrawRect(Back, X - 2.f * Scale, Y - 2.f * Scale, W + 4.f * Scale, H + 4.f * Scale);
	DrawRect(Fill, X, Y, W * FMath::Clamp(StaminaShown, 0.f, 1.f), H);
}
