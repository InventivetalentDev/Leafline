#include "UI/LeaflineHUD.h"
#include "Ant/AntPawn.h"
#include "Ant/AntMovementComponent.h"
#include "World/DayDirector.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Game/LeaflineGameInstance.h"

void ALeaflineHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas) return;

	const float Scale = Canvas->ClipY / 1080.f;
	const float Dt = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f;
	const AAntPawn* Ant = Cast<AAntPawn>(GetOwningPawn());
	const ADayDirector* D = ADayDirector::Get(this);

	if (D && D->GetPhase() == EDayPhase::Recap)
	{
		DrawRecap(D, Scale);
		return;
	}
	if (D) DrawDayInfo(D, Scale);
	if (Ant)
	{
		DrawStamina(Ant, Scale, Dt);
		DrawCarry(Ant, Scale);
		DrawCutArc(Ant, Scale);
	}
	DrawPrompt(Ant, D, Scale);
}

void ALeaflineHUD::DrawTextCentered(const FString& Text, float CenterX, float Y, const FLinearColor& Color, float TextScale, UFont* Font)
{
	float W = 0.f, H = 0.f;
	GetTextSize(Text, W, H, Font, TextScale);
	DrawText(Text, Color, CenterX - W * 0.5f, Y, Font, TextScale);
}

void ALeaflineHUD::DrawArc(float CX, float CY, float Radius, float From01, float To01, const FLinearColor& Color, float Thickness)
{
	const int32 Segs = FMath::Max(2, FMath::CeilToInt((To01 - From01) * 64.f));
	for (int32 i = 0; i < Segs; ++i)
	{
		const float A0 = 2.f * PI * FMath::Lerp(From01, To01, float(i) / Segs) - 0.5f * PI;
		const float A1 = 2.f * PI * FMath::Lerp(From01, To01, float(i + 1) / Segs) - 0.5f * PI;
		DrawLine(CX + FMath::Cos(A0) * Radius, CY + FMath::Sin(A0) * Radius, CX + FMath::Cos(A1) * Radius, CY + FMath::Sin(A1) * Radius, Color, Thickness);
	}
}

void ALeaflineHUD::DrawStamina(const AAntPawn* Ant, float Scale, float Dt)
{
	const UAntMovementComponent* M = Ant->GetAntMovement();
	if (!M) return;
	const float Stamina = M->GetStamina();
	StaminaShown = FMath::FInterpTo(StaminaShown, Stamina, Dt, 12.f);
	// Fades out when full and idle, in while it is being used.
	const bool bActive = Stamina < 0.999f || M->IsSprinting();
	StaminaAlpha = FMath::FInterpTo(StaminaAlpha, bActive ? 1.f : 0.f, Dt, bActive ? 10.f : 2.f);
	if (StaminaAlpha < 0.01f) return;

	const float W = 280.f * Scale, H = 9.f * Scale;
	const float X = (Canvas->ClipX - W) * 0.5f;
	const float Y = Canvas->ClipY - 70.f * Scale;
	FLinearColor Fill = M->IsSprintLocked() ? StaminaLockedColor : StaminaColor;
	Fill.A *= StaminaAlpha;
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.45f * StaminaAlpha), X - 2.f * Scale, Y - 2.f * Scale, W + 4.f * Scale, H + 4.f * Scale);
	DrawRect(Fill, X, Y, W * FMath::Clamp(StaminaShown, 0.f, 1.f), H);
}

void ALeaflineHUD::DrawDayInfo(const ADayDirector* D, float Scale)
{
	UFont* Font = GEngine->GetMediumFont();
	const float X = 40.f * Scale, Y = 34.f * Scale;
	DrawText(FString::Printf(TEXT("Day %d"), D->GetDay()), TextColor, X, Y, Font, 1.3f * Scale);

	// Worker pips: filled = waiting in the nest, ring = out or returning, dim = home.
	const float Pip = 14.f * Scale, Gap = 8.f * Scale;
	const float PY = Y + 34.f * Scale;
	for (int32 i = 0; i < D->NumWorkers(); ++i)
	{
		const float PX = X + i * (Pip + Gap);
		const EWorkerState S = D->GetWorkerState(i);
		FLinearColor C = TextColor;
		if (S == EWorkerState::Home) C = FLinearColor(0.4f, 0.4f, 0.35f, 0.8f);
		if (S == EWorkerState::BlownOff) C = StaminaLockedColor;
		if (S == EWorkerState::Out || S == EWorkerState::Returning) C = AccentColor;
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.4f), PX - 2.f * Scale, PY - 2.f * Scale, Pip + 4.f * Scale, Pip + 4.f * Scale);
		const float Inset = (S == EWorkerState::Returning) ? 4.f * Scale : 0.f;
		DrawRect(C, PX + Inset, PY + Inset, Pip - 2.f * Inset, Pip - 2.f * Inset);
	}
	DrawText(FString::Printf(TEXT("Hauled %.1f"), D->GetStats().MassHauled), TextColor, X, PY + Pip + 10.f * Scale, Font, 1.0f * Scale);
}

void ALeaflineHUD::DrawCarry(const AAntPawn* Ant, float Scale)
{
	if (!Ant->IsCarrying()) return;
	UFont* Font = GEngine->GetMediumFont();
	const float X = 40.f * Scale, Y = Canvas->ClipY - 110.f * Scale;
	DrawText(FString::Printf(TEXT("Carrying %.1f"), Ant->GetCarriedMass()), TextColor, X, Y, Font, 1.2f * Scale);
	const float W = 160.f * Scale, H = 8.f * Scale;
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.45f), X - 2.f * Scale, Y + 30.f * Scale, W + 4.f * Scale, H + 4.f * Scale);
	DrawRect(FLinearColor(0.35f, 0.75f, 0.25f, 0.95f), X, Y + 32.f * Scale, W * Ant->GetCarriedSize(), H);
}

void ALeaflineHUD::DrawCutArc(const AAntPawn* Ant, float Scale)
{
	if (!Ant->IsCutting()) return;
	const float CX = Canvas->ClipX * 0.5f, CY = Canvas->ClipY * 0.5f;
	const float R = 60.f * Scale;
	const float Size = Ant->GetCutSize();
	// Risk colour comes with wind (M3); until then the arc warms from green to amber with size.
	const FLinearColor Col = FMath::Lerp(FLinearColor(0.45f, 0.9f, 0.4f), FLinearColor(1.f, 0.7f, 0.2f), Size);
	DrawArc(CX, CY, R, 0.f, Ant->GetCutCap(), FLinearColor(0.f, 0.f, 0.f, 0.35f), 6.f * Scale);
	DrawArc(CX, CY, R, 0.f, FMath::Max(Size, 0.01f), Col, 6.f * Scale);
	UFont* Font = GEngine->GetMediumFont();
	DrawTextCentered(FString::Printf(TEXT("%.1f"), Ant->MassForSize(Size)), CX, CY - 12.f * Scale, TextColor, 1.2f * Scale, Font);
}

void ALeaflineHUD::DrawPrompt(const AAntPawn* Ant, const ADayDirector* D, float Scale)
{
	FString Prompt;
	if (Ant)
	{
		const UAntMovementComponent* M = Ant->GetAntMovement();
		const EAntState S = M ? M->GetState() : EAntState::Walking;
		if (S == EAntState::Returning) Prompt = TEXT("Heading home on the trail.   Tab: switch ant");
		else if (Ant->IsCutting()) Prompt = TEXT("Release to finish the cut");
		else if (Ant->IsCarrying()) Prompt = TEXT("Q / Y: send home      or carry it to the nest yourself");
		else if (Ant->GetLeafInReach() != INDEX_NONE) Prompt = TEXT("Hold E / LMB / X: cut");
		else if (S == EAntState::Riding) Prompt = TEXT("Riding the trail.   Move to step off");
		else if (Ant->IsTrailInReach()) Prompt = TEXT("Hold F / RB: follow the trail");
	}
	else if (D && D->GetPhase() == EDayPhase::AllHome)
	{
		Prompt = TEXT("Everyone is home");
	}
	if (Prompt.IsEmpty()) return;
	UFont* Font = GEngine->GetMediumFont();
	const float Y = Canvas->ClipY - 130.f * Scale;
	float W = 0.f, H = 0.f;
	GetTextSize(Prompt, W, H, Font, 1.15f * Scale);
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.4f), (Canvas->ClipX - W) * 0.5f - 14.f * Scale, Y - 6.f * Scale, W + 28.f * Scale, H + 12.f * Scale);
	DrawTextCentered(Prompt, Canvas->ClipX * 0.5f, Y, TextColor, 1.15f * Scale, Font);
}

void ALeaflineHUD::DrawRecap(const ADayDirector* D, float Scale)
{
	DrawRect(FLinearColor(0.02f, 0.03f, 0.02f, 0.72f), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);
	UFont* Large = GEngine->GetLargeFont();
	UFont* Medium = GEngine->GetMediumFont();
	const float CX = Canvas->ClipX * 0.5f;
	float Y = Canvas->ClipY * 0.28f;
	const FDayStats& S = D->GetStats();
	DrawTextCentered(FString::Printf(TEXT("Day %d: today's cut"), S.Day), CX, Y, AccentColor, 2.0f * Scale, Large);
	Y += 90.f * Scale;
	const TArray<FString> Lines = {
		FString::Printf(TEXT("Leaf hauled: %.1f"), S.MassHauled),
		FString::Printf(TEXT("Fragments brought home: %d"), S.Fragments),
		FString::Printf(TEXT("Largest fragment: %.1f"), S.LargestFragment),
		FString::Printf(TEXT("Workers blown off: %d"), S.BlownOff),
		FString::Printf(TEXT("Trail laid: %.0f m"), S.TrailLaid / 100.f),
	};
	for (const FString& L : Lines)
	{
		DrawTextCentered(L, CX, Y, TextColor, 1.3f * Scale, Medium);
		Y += 44.f * Scale;
	}
	if (const ULeaflineGameInstance* GI = Cast<ULeaflineGameInstance>(GetGameInstance()))
	{
		Y += 16.f * Scale;
		DrawTextCentered(FString::Printf(TEXT("Colony: %.1f hauled in all, %.1f to spend, milestone %d of %d"),
			GI->GetTotalHauled(), GI->GetLeafMass(), GI->GetReachedMilestoneCount(), LeaflineUpgrades::MilestoneCount), CX, Y, AccentColor, 1.1f * Scale, Medium);
		Y += 40.f * Scale;
	}
	Y += 40.f * Scale;
	DrawTextCentered(FString::Printf(TEXT("Press E / Space / X for day %d: a new tree"), S.Day + 1), CX, Y, AccentColor, 1.2f * Scale, Medium);
}
