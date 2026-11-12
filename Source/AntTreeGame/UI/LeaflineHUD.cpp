#include "UI/LeaflineHUD.h"
#include "Ant/AntPawn.h"
#include "Ant/AntMovementComponent.h"
#include "World/DayDirector.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Game/LeaflineGameInstance.h"
#include "World/WindSystem.h"
#include "Tree/TreeActor.h"
#include "Colony/TrailNetwork.h"
#include "CanvasItem.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"

namespace
{
	/**
	 * The engine font rasterised at the size it is shown. Canvas text drawn with a TextScale is rasterised at the font's
	 * own size and stretched, which blurs it on large screens.
	 */
	FSlateFontInfo CrispFont(const UFont* Font, float TextScale)
	{
		FSlateFontInfo Info = Font->GetLegacySlateFontInfo();
		Info.Size = FMath::Max(1.f, FMath::RoundToFloat(Info.Size * TextScale));
		return Info;
	}

	FVector2D MeasureCrisp(const FString& Text, const UFont* Font, float TextScale)
	{
		if (!Font || !FSlateApplication::IsInitialized()) return FVector2D::ZeroVector;
		return FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Text, CrispFont(Font, TextScale));
	}

	void DrawCrisp(UCanvas* Canvas, const FString& Text, const FLinearColor& Color, float X, float Y, const UFont* Font, float TextScale)
	{
		if (!Canvas || !Font) return;
		FCanvasTextItem Item(FVector2D(X, Y), FText::FromString(Text), CrispFont(Font, TextScale), Color);
		Item.EnableShadow(FLinearColor(0.f, 0.f, 0.f, 0.5f * Color.A), FVector2D(1.f, 1.f));
		Canvas->DrawItem(Item);
	}
}

int32 ALeaflineHUD::MenuItemAt(const FVector2D& P) const
{
	for (const FMenuHit& H : MenuHits)
	{
		if (H.Box.IsInside(P)) return H.Id;
	}
	return INDEX_NONE;
}

bool ALeaflineHUD::IsHovered(int32 Id) const
{
	const APlayerController* PC = GetOwningPlayerController();
	float X = 0.f, Y = 0.f;
	return PC && PC->bShowMouseCursor && PC->GetMousePosition(X, Y) && MenuItemAt(FVector2D(X, Y)) == Id;
}

void ALeaflineHUD::DrawHUD()
{
	Super::DrawHUD();
	MenuHits.Reset();
	if (!Canvas) return;

	const float Scale = Canvas->ClipY / 1080.f;
	const float Dt = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f;
	const AAntPawn* Ant = Cast<AAntPawn>(GetOwningPawn());
	const ADayDirector* D = ADayDirector::Get(this);

	if (GetOwningPlayerController() && GetOwningPlayerController()->IsPaused())
	{
		DrawPause(Scale);
		return;
	}
	if (D)
	{
		switch (D->GetPhase())
		{
		case EDayPhase::Title: DrawTitle(D, Scale); return;
		case EDayPhase::Recap: DrawRecap(D, Scale); return;
		case EDayPhase::EndCard: DrawEndCard(D, Scale); return;
		case EDayPhase::Nest: DrawNest(D, Scale); return;
		default: break;
		}
	}
	if (D) DrawDayInfo(D, Scale);
	// First-day goals, instead of a tutorial.
	if (D && Ant && D->GetDay() == 1 && D->GetPhase() == EDayPhase::Shift)
	{
		const UAntMovementComponent* M = Ant->GetAntMovement();
		FString Goal;
		const bool bLow = M && D->GetTree() && M->GetSurfacePosition().Z < D->GetTree()->Params.LowestLeafyHeight - 600.f;
		if (Ant->GetWorkerIndex() == 0 && D->GetStats().Fragments == 0 && !Ant->IsCarrying() && M && M->GetState() == EAntState::Walking && bLow)
		{
			Goal = TEXT("Climb the trunk: the leaves start about 40 m up");
		}
		else if (Ant->GetWorkerIndex() == 1 && !Ant->IsCarrying() && M && M->GetState() == EAntState::Walking)
		{
			Goal = TEXT("Your first worker walks home on its own. Hold F on the cyan trail to ride it up");
		}
		if (!Goal.IsEmpty())
		{
			DrawTextCentered(Goal, Canvas->ClipX * 0.5f, 40.f * Scale, AccentColor, 1.2f * Scale, GEngine->GetMediumFont());
		}
	}
	if (Ant)
	{
		DrawStamina(Ant, Scale, Dt);
		DrawCarry(Ant, Scale);
		DrawCutArc(Ant, Scale);
		DrawLeafTarget(Ant, Scale);
	}
	DrawPrompt(Ant, D, Scale);
}

void ALeaflineHUD::DrawTextCentered(const FString& Text, float CenterX, float Y, const FLinearColor& Color, float TextScale, UFont* Font)
{
	const FVector2D Size = MeasureCrisp(Text, Font, TextScale);
	DrawCrisp(Canvas, Text, Color, CenterX - Size.X * 0.5f, Y, Font, TextScale);
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
	DrawCrisp(Canvas, FString::Printf(TEXT("Day %d"), D->GetDay()), TextColor, X, Y, Font, 1.3f * Scale);

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
	DrawCrisp(Canvas, FString::Printf(TEXT("Hauled %.1f"), D->GetStats().MassHauled), TextColor, X, PY + Pip + 10.f * Scale, Font, 1.0f * Scale);

	// Wind gauge, top right.
	if (const AWindSystem* Wind = D->GetWind())
	{
		const float GW = 180.f * Scale, GH = 8.f * Scale;
		const float GX = Canvas->ClipX - 40.f * Scale - GW, GY = 50.f * Scale;
		const EWindPhase Ph = Wind->GetPhase();
		const TCHAR* Label = Ph == EWindPhase::Calm ? TEXT("Wind: calm") : Ph == EWindPhase::Warning ? TEXT("Wind: gust coming")
			: Ph == EWindPhase::Falling ? TEXT("Wind: easing") : TEXT("Wind: GUST");
		const FLinearColor C = Wind->IsGustActive() ? StaminaLockedColor : (Ph == EWindPhase::Warning ? FLinearColor(1.f, 0.72f, 0.2f) : TextColor);
		DrawCrisp(Canvas, Label, C, GX, GY - 26.f * Scale, Font, 1.0f * Scale);
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.45f), GX - 2.f * Scale, GY - 2.f * Scale, GW + 4.f * Scale, GH + 4.f * Scale);
		DrawRect(C, GX, GY, GW * FMath::Clamp(Wind->GetStrength(), 0.f, 1.f), GH);
	}
}

void ALeaflineHUD::DrawCarry(const AAntPawn* Ant, float Scale)
{
	if (!Ant->IsCarrying()) return;
	UFont* Font = GEngine->GetMediumFont();
	const float X = 40.f * Scale, Y = Canvas->ClipY - 110.f * Scale;
	DrawCrisp(Canvas, FString::Printf(TEXT("Carrying %.1f"), Ant->GetCarriedMass()), TextColor, X, Y, Font, 1.2f * Scale);
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
	// The arc is coloured by the wind risk of each size: safe, risky, or blown off by a full gust on an exposed route.
	const AWindSystem* Wind = AWindSystem::Get(this);
	const ULeaflineGameInstance* GI = Cast<ULeaflineGameInstance>(GetGameInstance());
	const float GripMul = GI ? GI->GetGripMultiplier() : 1.f;
	auto RiskColor = [&](float S, float& OutRatio)
	{
		OutRatio = Wind ? Wind->PredictRiskRatio(Ant->DiameterForSize(S), GripMul) : 0.f;
		const float Risky = Wind ? Wind->Tuning.RiskyRatio : 0.6f;
		if (OutRatio >= 1.f) return FLinearColor(0.95f, 0.25f, 0.2f);
		if (OutRatio >= Risky) return FLinearColor(1.f, 0.72f, 0.2f);
		return FLinearColor(0.45f, 0.9f, 0.4f);
	};
	DrawArc(CX, CY, R, 0.f, Ant->GetCutCap(), FLinearColor(0.f, 0.f, 0.f, 0.35f), 6.f * Scale);
	const int32 Steps = 24;
	float Ratio = 0.f;
	for (int32 i = 0; i < Steps; ++i)
	{
		const float A = Size * i / Steps, B = Size * (i + 1) / Steps;
		DrawArc(CX, CY, R, A, FMath::Max(B, A + 0.002f), RiskColor(B, Ratio), 6.f * Scale);
	}
	const FLinearColor Now = RiskColor(Size, Ratio);
	UFont* Font = GEngine->GetMediumFont();
	DrawTextCentered(FString::Printf(TEXT("%.1f"), Ant->MassForSize(Size)), CX, CY - 12.f * Scale, TextColor, 1.2f * Scale, Font);
	// Name the subject: the label is about the piece being cut, not the ant or the leaf.
	const TCHAR* Label = Ratio >= 1.f ? TEXT("This piece: too big for a gust")
		: (Wind && Ratio >= Wind->Tuning.RiskyRatio ? TEXT("This piece: risky in a gust") : TEXT("This piece: safe in a gust"));
	DrawTextCentered(Label, CX, CY + R + 14.f * Scale, Now, 1.0f * Scale, Font);
}

void ALeaflineHUD::DrawLeafTarget(const AAntPawn* Ant, float Scale)
{
	const int32 Index = Ant->GetLeafInReach();
	if (Index == INDEX_NONE || Ant->IsCutting() || Ant->IsCarrying()) return;
	const ATreeActor* Tree = ATreeActor::Get(this);
	const APlayerController* PC = GetOwningPlayerController();
	if (!Tree || !PC || !PC->PlayerCameraManager || Index >= Tree->GetLeaves().Num()) return;
	const FTreeLeaf& L = Tree->GetLeaves().Get(Index);
	const FVector CamPos = PC->PlayerCameraManager->GetCameraLocation();
	const FVector CamFwd = PC->PlayerCameraManager->GetCameraRotation().Vector();

	// Trace the blade's edge: up one side from the base to the tip and back down the other.
	constexpr int32 Steps = 14;
	TArray<FVector2D, TInlineAllocator<2 * Steps + 2>> Screen;
	for (int32 Side = 0; Side < 2; ++Side)
	{
		for (int32 i = 0; i <= Steps; ++i)
		{
			const float Along = L.Length * (Side == 0 ? i : Steps - i) / Steps;
			const float Across = L.HalfWidthAt(Along) * (Side == 0 ? 1.f : -1.f);
			const FVector W = L.Point(Along, Across) + L.Normal * 0.5f;
			if (FVector::DotProduct(W - CamPos, CamFwd) < 10.0) return; // part of the blade is behind the camera
			const FVector S = Canvas->Project(W);
			Screen.Add(FVector2D(S.X, S.Y));
		}
	}
	const float Pulse = 0.65f + 0.35f * FMath::Sin(GetWorld() ? GetWorld()->GetRealTimeSeconds() * 5.f : 0.f);
	const FLinearColor Color(1.f, 0.88f, 0.45f, Pulse);
	for (int32 i = 0; i < Screen.Num(); ++i)
	{
		const FVector2D& A = Screen[i];
		const FVector2D& B = Screen[(i + 1) % Screen.Num()];
		DrawLine(A.X, A.Y, B.X, B.Y, Color, 2.f * Scale);
	}
}

void ALeaflineHUD::DrawPrompt(const AAntPawn* Ant, const ADayDirector* D, float Scale)
{
	FString Prompt;
	if (Ant)
	{
		const UAntMovementComponent* M = Ant->GetAntMovement();
		const EAntState S = M ? M->GetState() : EAntState::Walking;
		const AWindSystem* Wind = D ? D->GetWind() : nullptr;
		const bool bGustSoon = Wind && (Wind->GetPhase() == EWindPhase::Warning || Wind->IsGustActive());
		if (S == EAntState::Falling) Prompt = TEXT("Blown off! The fragment is lost");
		else if (S == EAntState::WalkingHome) Prompt = TEXT("Walking home empty-handed.   Tab: switch ant");
		else if (S == EAntState::Gripping) Prompt = TEXT("Gripping. Release Space / A when the gust passes");
		else if (bGustSoon && (S == EAntState::Walking || S == EAntState::Riding || S == EAntState::Cutting)) Prompt = TEXT("Gust! Hold Space / A to grip");
		else if (S == EAntState::Returning) Prompt = TEXT("Heading home on the trail.   Tab: switch ant");
		else if (Ant->IsCutting()) Prompt = TEXT("Release to finish the cut");
		else if (Ant->IsCarrying()) Prompt = TEXT("Q / Y: send home      or carry it to the nest yourself");
		else if (Ant->GetLeafInReach() != INDEX_NONE) Prompt = TEXT("Hold E / LMB / X: cut");
		else if (D && S == EAntState::Walking && M && D->IsNearNest(M->GetSurfacePosition()) && D->GetWorkerState(Ant->GetWorkerIndex()) == EWorkerState::Out && Ant->GetPathIndex() != INDEX_NONE && D->GetTrails() && D->GetTrails()->GetPoints(Ant->GetPathIndex()).Num() > 30)
			Prompt = TEXT("Q / Y: end this worker's trip");
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
	const FVector2D Size = MeasureCrisp(Prompt, Font, 1.15f * Scale);
	const float W = Size.X, H = Size.Y;
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
	Y += 24.f * Scale;
	DrawColonyMeter(CX, Y, 520.f * Scale, Scale);
	Y += 90.f * Scale;
	DrawTextCentered(TEXT("Press E / Space / A or click to go into the nest"), CX, Y, AccentColor, 1.2f * Scale, Medium);
}

void ALeaflineHUD::DrawColonyMeter(float CX, float Y, float W, float Scale)
{
	const ULeaflineGameInstance* GI = Cast<ULeaflineGameInstance>(GetGameInstance());
	if (!GI) return;
	UFont* Medium = GEngine->GetMediumFont();
	const float Top = LeaflineUpgrades::GetMilestoneThreshold(LeaflineUpgrades::MilestoneCount - 1);
	const float H = 12.f * Scale, X = CX - W * 0.5f;
	DrawTextCentered(FString::Printf(TEXT("Colony: %.1f hauled in all"), GI->GetTotalHauled()), CX, Y, TextColor, 1.1f * Scale, Medium);
	const float BarY = Y + 34.f * Scale;
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.5f), X - 2.f * Scale, BarY - 2.f * Scale, W + 4.f * Scale, H + 4.f * Scale);
	DrawRect(FLinearColor(0.4f, 0.75f, 0.3f, 0.95f), X, BarY, W * FMath::Clamp(GI->GetTotalHauled() / Top, 0.f, 1.f), H);
	for (int32 m = 0; m < LeaflineUpgrades::MilestoneCount; ++m)
	{
		const float T = LeaflineUpgrades::GetMilestoneThreshold(m);
		const float MX = X + W * T / Top;
		const bool bReached = GI->HasReachedMilestone(m);
		DrawRect(bReached ? AccentColor : TextColor, MX - 1.5f * Scale, BarY - 6.f * Scale, 3.f * Scale, H + 12.f * Scale);
		DrawTextCentered(FString::Printf(TEXT("%.0f"), T), MX, BarY + H + 8.f * Scale, bReached ? AccentColor : TextColor, 0.9f * Scale, Medium);
	}
}

void ALeaflineHUD::DrawTitle(const ADayDirector* D, float Scale)
{
	DrawRect(FLinearColor(0.02f, 0.03f, 0.02f, 0.55f), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);
	UFont* Large = GEngine->GetLargeFont();
	UFont* Medium = GEngine->GetMediumFont();
	const float CX = Canvas->ClipX * 0.5f;
	float Y = Canvas->ClipY * 0.22f;
	DrawTextCentered(TEXT("Leafline"), CX, Y, AccentColor, 3.6f * Scale, Large);
	Y += 120.f * Scale;
	DrawTextCentered(TEXT("You are a leafcutter ant on an enormous tree."), CX, Y, TextColor, 1.3f * Scale, Medium);
	Y += 40.f * Scale;
	DrawTextCentered(TEXT("Cut a piece of leaf, send the worker home along your scent trail, and take over the next one."), CX, Y, TextColor, 1.1f * Scale, Medium);
	Y += 70.f * Scale;
	const TArray<FString> Controls = {
		TEXT("Move: WASD / left stick      Look: mouse / right stick      Sprint: Shift / LT"),
		TEXT("Cut (hold): E or left mouse / X      Send home: Q / Y      Follow a trail (hold): F / RB"),
		TEXT("Grip in a gust (hold): Space / A      Switch ant: Tab / LB      Pause: Esc or P / Start"),
	};
	for (const FString& C : Controls)
	{
		DrawTextCentered(C, CX, Y, FLinearColor(0.85f, 0.85f, 0.8f, 1.f), 1.0f * Scale, Medium);
		Y += 34.f * Scale;
	}
	Y += 50.f * Scale;
	DrawTextCentered(D->GetDay() > 1 ? FString::Printf(TEXT("Press E / Space / A or click to continue: day %d"), D->GetDay()) : FString(TEXT("Press E / Space / A or click to begin")),
		CX, Y, AccentColor, 1.4f * Scale, Medium);
}

void ALeaflineHUD::DrawNest(const ADayDirector* D, float Scale)
{
	DrawRect(FLinearColor(0.03f, 0.025f, 0.015f, 0.78f), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);
	const ULeaflineGameInstance* GI = Cast<ULeaflineGameInstance>(GetGameInstance());
	UFont* Large = GEngine->GetLargeFont();
	UFont* Medium = GEngine->GetMediumFont();
	const float CX = Canvas->ClipX * 0.5f;
	float Y = Canvas->ClipY * 0.12f;
	DrawTextCentered(TEXT("The nest"), CX, Y, AccentColor, 2.2f * Scale, Large);
	Y += 80.f * Scale;
	if (GI)
	{
		DrawTextCentered(FString::Printf(TEXT("Leaf to spend: %.1f"), GI->GetLeafMass()), CX, Y, TextColor, 1.3f * Scale, Medium);
		Y += 50.f * Scale;
	}
	const float RowW = 760.f * Scale, RowH = 56.f * Scale, X = CX - RowW * 0.5f;
	for (int32 i = 0; i <= LeaflineUpgrades::Count; ++i)
	{
		const bool bSel = D->GetNestSelection() == i;
		DrawRect(bSel ? FLinearColor(0.3f, 0.55f, 0.6f, 0.45f) : FLinearColor(0.f, 0.f, 0.f, 0.35f), X, Y, RowW, RowH - 6.f * Scale);
		AddMenuHit(X, Y, RowW, RowH - 6.f * Scale, i);
		if (i < LeaflineUpgrades::Count)
		{
			const FLeaflineUpgradeInfo* Info = LeaflineUpgrades::Get(i);
			const int32 Level = GI ? GI->GetUpgradeLevel(i) : 0;
			const int32 Cost = GI ? GI->GetUpgradeCost(i) : 0;
			const bool bAfford = GI && GI->CanAfford(i);
			DrawCrisp(Canvas, Info->Name, TextColor, X + 18.f * Scale, Y + 6.f * Scale, Medium, 1.25f * Scale);
			DrawCrisp(Canvas, Info->Description, FLinearColor(0.8f, 0.8f, 0.75f, 1.f), X + 18.f * Scale, Y + 30.f * Scale, Medium, 0.9f * Scale);
			for (int32 l = 0; l < LeaflineUpgrades::MaxLevel; ++l)
			{
				DrawRect(l < Level ? AccentColor : FLinearColor(0.25f, 0.25f, 0.22f, 1.f), X + RowW * 0.55f + l * 26.f * Scale, Y + 18.f * Scale, 18.f * Scale, 14.f * Scale);
			}
			const FString CostText = Cost > 0 ? FString::Printf(TEXT("%d"), Cost) : FString(TEXT("max"));
			DrawCrisp(Canvas, CostText, Cost > 0 ? (bAfford ? FLinearColor(0.6f, 0.95f, 0.5f, 1.f) : StaminaLockedColor) : TextColor, X + RowW - 90.f * Scale, Y + 12.f * Scale, Medium, 1.2f * Scale);
		}
		else
		{
			DrawCrisp(Canvas, FString::Printf(TEXT("Start day %d: a new tree"), D->GetDay() + 1), AccentColor, X + 18.f * Scale, Y + 14.f * Scale, Medium, 1.25f * Scale);
		}
		Y += RowH;
	}
	Y += 30.f * Scale;
	DrawColonyMeter(CX, Y, 520.f * Scale, Scale);
	Y += 100.f * Scale;
	DrawTextCentered(TEXT("W / S, stick or mouse: choose      E / Space / A or click: buy or start      Q / Y: start the next day"), CX, Y, FLinearColor(0.8f, 0.8f, 0.75f, 1.f), 1.0f * Scale, Medium);
}

void ALeaflineHUD::DrawEndCard(const ADayDirector* D, float Scale)
{
	DrawRect(FLinearColor(0.02f, 0.04f, 0.03f, 0.82f), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);
	UFont* Large = GEngine->GetLargeFont();
	UFont* Medium = GEngine->GetMediumFont();
	const float CX = Canvas->ClipX * 0.5f;
	float Y = Canvas->ClipY * 0.3f;
	DrawTextCentered(TEXT("The colony thrives"), CX, Y, AccentColor, 2.4f * Scale, Large);
	Y += 100.f * Scale;
	DrawTextCentered(TEXT("Enough leaf has come home to feed the fungus garden for a season."), CX, Y, TextColor, 1.25f * Scale, Medium);
	Y += 42.f * Scale;
	DrawTextCentered(TEXT("Thank you for playing Leafline. The trees keep growing, and so can the colony."), CX, Y, TextColor, 1.25f * Scale, Medium);
	Y += 90.f * Scale;
	DrawTextCentered(TEXT("Press E / Space / A or click to keep playing"), CX, Y, AccentColor, 1.3f * Scale, Medium);
}

void ALeaflineHUD::DrawPause(float Scale)
{
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);
	UFont* Large = GEngine->GetLargeFont();
	UFont* Medium = GEngine->GetMediumFont();
	const float CX = Canvas->ClipX * 0.5f;
	const float Top = Canvas->ClipY * 0.38f;
	DrawTextCentered(TEXT("Paused"), CX, Top, AccentColor, 2.4f * Scale, Large);
	// Buttons for the mouse; keys and pad work as before.
	const float BW = 220.f * Scale, BH = 54.f * Scale, Gap = 30.f * Scale, BY = Top + 95.f * Scale;
	const TCHAR* Labels[2] = { TEXT("Resume"), TEXT("Quit") };
	const int32 Ids[2] = { MenuResume, MenuQuit };
	for (int32 b = 0; b < 2; ++b)
	{
		const float BX = CX - BW - Gap * 0.5f + b * (BW + Gap);
		const bool bHover = IsHovered(Ids[b]);
		DrawRect(bHover ? FLinearColor(0.3f, 0.55f, 0.6f, 0.6f) : FLinearColor(0.f, 0.f, 0.f, 0.45f), BX, BY, BW, BH);
		DrawTextCentered(Labels[b], BX + BW * 0.5f, BY + 12.f * Scale, bHover ? AccentColor : TextColor, 1.3f * Scale, Medium);
		AddMenuHit(BX, BY, BW, BH, Ids[b]);
	}
	DrawTextCentered(TEXT("Esc, P or Start: resume      Q / Y: quit"), CX, BY + BH + 30.f * Scale, TextColor, 1.1f * Scale, Medium);
}
