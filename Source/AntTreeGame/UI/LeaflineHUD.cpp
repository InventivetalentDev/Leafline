#include "UI/LeaflineHUD.h"
#include "Game/LeaflinePlayerController.h"
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
#include "Colony/LineSystem.h"
#include "Game/Upgrades.h"
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
		case EDayPhase::Move: DrawMoveCard(D, Scale); return;
		default: break;
		}
	}
	bWasRecap = false;
	const ALineSystem* Lines = D ? D->GetLineSystem() : nullptr;
	if (D && D->IsHarvest())
	{
		// The harvest plays from the nest: no ant to prompt for, so only the colony's numbers.
		DrawDayInfo(D, Scale);
		DrawColonyCounter(D, Scale, Dt);
		DrawLineRows(D, Lines, Scale);
		DrawDeliveryPops(Lines, Scale);
		if (Lines && (Lines->IsActive() || D->IsHomeOnlyHarvest())) DrawHarvest(D, Lines, Scale);
		return;
	}
	if (D) DrawDayInfo(D, Scale);
	if (D) DrawColonyCounter(D, Scale, Dt);
	if (D) DrawLineRows(D, Lines, Scale);
	// First-day goals, instead of a tutorial.
	// The climb goal shows on the first day of each tree (day 1 only with one tree off); the riding goal on day 1 only.
	const bool bTreeFirstDay = D && (D->IsOneTree() ? D->GetDayOnTree() == 1 : D->GetDay() == 1);
	if (D && Ant && (bTreeFirstDay || D->GetDay() == 1) && D->GetPhase() == EDayPhase::Shift)
	{
		const UAntMovementComponent* M = Ant->GetAntMovement();
		FString Goal;
		// Leaves grow on boughs and twigs above LowestLeafyHeight, and the first limbs start at LimbStartHeight.
		const float LeavesFrom = D->GetLowestLeafyHeight();
		const bool bLow = M && M->GetSurfacePosition().Z < LeavesFrom - 600.f;
		if (bTreeFirstDay && Ant->GetWorkerIndex() == 0 && D->GetStats().Fragments == 0 && !Ant->IsCarrying() && M && M->GetState() == EAntState::Walking && bLow)
		{
			Goal = FString::Printf(TEXT("Climb the trunk: the leaves start about %d m up"), FMath::RoundToInt(LeavesFrom / 100.f));
		}
		else if (D->GetDay() == 1 && Ant->GetWorkerIndex() == 1 && !Ant->IsCarrying() && M && M->GetState() == EAntState::Walking)
		{
			Goal = TEXT("Your first worker walks home on its own. Hold F on the cyan trail to ride it up");
		}
		if (!Goal.IsEmpty())
		{
			DrawTextCentered(Goal, Canvas->ClipX * 0.5f, 118.f * Scale, AccentColor, 1.2f * Scale, GEngine->GetMediumFont());
		}
	}
	if (Ant)
	{
		DrawStamina(Ant, Scale, Dt);
		DrawCarry(Ant, Scale);
		DrawCutArc(Ant, Scale);
		DrawLeafTarget(Ant, Scale);
		DrawCutLine(Ant, Scale);
		DrawPatchPreview(Ant, Lines, Scale);
	}
	DrawDeliveryPops(Lines, Scale);
	DrawPrompt(Ant, D, Scale);
	if (D) DrawShiftBanner(D, Scale, Dt);
}

void ALeaflineHUD::DrawShiftBanner(const ADayDirector* D, float Scale, float Dt)
{
	const int32 Shift = D->GetShiftsStarted();
	if (D->GetPhase() == EDayPhase::Shift && Shift > 0 && (Shift != BannerShift || D->GetDay() != BannerDay))
	{
		BannerTitle = Shift == 1 ? FString::Printf(TEXT("Day %d: %s"), D->GetDay(), *D->GetTreeName()) : FString();
		BannerLine = FString::Printf(TEXT("Worker %d of %d sets out"), Shift, D->NumWorkers());
		BannerTime = Shift == 1 ? 3.5f : 2.5f;
	}
	BannerShift = Shift;
	BannerDay = D->GetDay();
	// While still watching a worker that was just sent home: say so (the next worker comes out a moment later).
	const APlayerController* PC = GetOwningPlayerController();
	const AAntPawn* Watched = PC ? Cast<AAntPawn>(PC->GetPawn()) : nullptr;
	if (Watched && Watched->GetAntMovement() && Watched->GetAntMovement()->GetState() == EAntState::Returning && BannerTime <= 0.f)
	{
		const float CX = Canvas->ClipX * 0.5f;
		DrawTextCentered(FString::Printf(TEXT("Worker %d heads home with its piece"), Watched->GetWorkerIndex() + 1), CX, Canvas->ClipY * 0.26f,
			TextColor, 1.25f * Scale, GEngine->GetMediumFont());
	}
	if (BannerTime <= 0.f) return;
	BannerTime -= Dt;
	const float A = FMath::Clamp(BannerTime / 0.6f, 0.f, 1.f);
	const float CX = Canvas->ClipX * 0.5f;
	float Y = Canvas->ClipY * 0.26f;
	if (!BannerTitle.IsEmpty())
	{
		DrawTextCentered(BannerTitle, CX, Y, FLinearColor(AccentColor.R, AccentColor.G, AccentColor.B, A), 1.8f * Scale, GEngine->GetLargeFont());
		Y += 70.f * Scale;
	}
	DrawTextCentered(BannerLine, CX, Y, FLinearColor(TextColor.R, TextColor.G, TextColor.B, A), 1.25f * Scale, GEngine->GetMediumFont());
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
	DrawCrisp(Canvas, D->GetTreeName().IsEmpty() ? FString::Printf(TEXT("Day %d"), D->GetDay())
		: FString::Printf(TEXT("Day %d: %s"), D->GetDay(), *D->GetTreeName()), TextColor, X, Y, Font, 1.3f * Scale);

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

		// Which way it blows, as seen from the camera: up on screen is away from you.
		const APlayerController* PC = GetOwningPlayerController();
		if (PC && PC->PlayerCameraManager)
		{
			FVector F = PC->PlayerCameraManager->GetCameraRotation().Vector();
			F.Z = 0.0;
			F = F.GetSafeNormal();
			const FVector R(-F.Y, F.X, 0.0);
			const FVector W = Wind->GetDirection().GetSafeNormal2D();
			FVector2D V(FVector::DotProduct(W, R), -FVector::DotProduct(W, F));
			if (!F.IsNearlyZero() && !V.IsNearlyZero())
			{
				V.Normalize();
				const float AR = 13.f * Scale;
				const FVector2D Ctr(GX - 30.f * Scale, GY - 6.f * Scale);
				const FVector2D Tip = Ctr + V * AR, Tail = Ctr - V * AR;
				const FVector2D Side(-V.Y, V.X);
				const float T = 2.f * Scale;
				DrawLine(Tail.X, Tail.Y, Tip.X, Tip.Y, C, T);
				const FVector2D H1 = Tip - V * (AR * 0.6f) + Side * (AR * 0.45f), H2 = Tip - V * (AR * 0.6f) - Side * (AR * 0.45f);
				DrawLine(Tip.X, Tip.Y, H1.X, H1.Y, C, T);
				DrawLine(Tip.X, Tip.Y, H2.X, H2.Y, C, T);
			}
		}

		// How exposed the ant's spot is: the lee side of the trunk or a limb shelters it.
		const AAntPawn* Ant = PC ? Cast<AAntPawn>(PC->GetPawn()) : nullptr;
		const UAntMovementComponent* M = Ant ? Ant->GetAntMovement() : nullptr;
		ExposureTimer -= GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f;
		if (M && ExposureTimer <= 0.f)
		{
			ExposureTimer = 0.2f;
			ExposureShown = Wind->GetExposure(M->GetSurfacePosition(), M->GetSurfaceNormal());
		}
		if (M && ExposureShown >= 0.f)
		{
			const bool bSheltered = ExposureShown < 0.25f, bPartly = ExposureShown < 0.6f;
			const FLinearColor EC = bSheltered ? FLinearColor(0.6f, 0.95f, 0.5f) : (bPartly ? FLinearColor(1.f, 0.8f, 0.35f) : StaminaLockedColor);
			const TCHAR* EL = bSheltered ? TEXT("Here: sheltered") : (bPartly ? TEXT("Here: partly exposed") : TEXT("Here: exposed"));
			DrawCrisp(Canvas, EL, EC, GX, GY + GH + 10.f * Scale, Font, 1.0f * Scale);
		}
		// The wind map (hold Tab / LB): its legend while shown, otherwise a quiet reminder that it exists (in play only:
		// in the harvest Tab steps the views).
		if (M && D->GetPhase() == EDayPhase::Shift)
		{
			const ALeaflinePlayerController* LPC = Cast<ALeaflinePlayerController>(PC);
			const float LY = GY + GH + 32.f * Scale;
			if (LPC && LPC->IsWindMapShown())
			{
				DrawCrisp(Canvas, TEXT("Wind map:"), TextColor, GX, LY, Font, 0.9f * Scale);
				DrawCrisp(Canvas, TEXT("green sheltered"), FLinearColor(0.6f, 0.95f, 0.5f), GX, LY + 20.f * Scale, Font, 0.9f * Scale);
				DrawCrisp(Canvas, TEXT("amber partly exposed"), FLinearColor(1.f, 0.8f, 0.35f), GX, LY + 38.f * Scale, Font, 0.9f * Scale);
				DrawCrisp(Canvas, TEXT("red exposed"), StaminaLockedColor, GX, LY + 56.f * Scale, Font, 0.9f * Scale);
			}
			else
			{
				DrawCrisp(Canvas, TEXT("Hold Tab / LB: wind map"), FLinearColor(TextColor.R, TextColor.G, TextColor.B, 0.55f), GX, LY, Font, 0.85f * Scale);
			}
		}
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

void ALeaflineHUD::DrawCutLine(const AAntPawn* Ant, float Scale)
{
	const int32 Index = Ant->GetCuttingLeaf();
	if (Index == INDEX_NONE) return;
	const ATreeActor* Tree = ATreeActor::Get(this);
	const APlayerController* PC = GetOwningPlayerController();
	if (!Tree || !PC || !PC->PlayerCameraManager || Index >= Tree->GetLeaves().Num()) return;
	const FTreeLeaf& L = Tree->GetLeaves().Get(Index);
	FVector2f C(0.f, 0.f);
	float R = 0.f;
	Tree->GetLeaves().PlanBite(Index, Ant->DiameterForSize(Ant->GetCutSize()), C, R);
	const FVector CamPos = PC->PlayerCameraManager->GetCameraLocation();
	const FVector CamFwd = PC->PlayerCameraManager->GetCameraRotation().Vector();
	const FVector Lift = L.Normal * (FVector::DotProduct(L.Normal, CamPos - L.Origin) >= 0.0 ? 0.5 : -0.5);

	// Pale and pulsing with the sawing, like a fresh cut catching the light.
	const float Pulse = 0.7f + 0.3f * FMath::Sin(GetWorld() ? GetWorld()->GetRealTimeSeconds() * 28.f : 0.f);
	const FLinearColor Color(1.f, 0.96f, 0.75f, Pulse);
	constexpr int32 Steps = 48;
	FVector2D Prev(0.0, 0.0);
	bool bPrev = false;
	for (int32 i = 0; i <= Steps; ++i)
	{
		const float A = 2.f * PI * i / Steps;
		const float Along = C.X + R * FMath::Cos(A);
		const float Across = C.Y + R * FMath::Sin(A);
		const bool bInside = Along >= 0.f && Along <= L.Length && FMath::Abs(Across) <= L.HalfWidthAt(Along);
		const FVector W = L.Point(Along, Across) + Lift;
		const bool bVisible = bInside && FVector::DotProduct(W - CamPos, CamFwd) > 10.0;
		FVector2D S(0.0, 0.0);
		if (bVisible)
		{
			const FVector P = Canvas->Project(W);
			S = FVector2D(P.X, P.Y);
			if (bPrev) DrawLine(Prev.X, Prev.Y, S.X, S.Y, Color, 2.5f * Scale);
		}
		Prev = S;
		bPrev = bVisible;
	}
}

void ALeaflineHUD::DrawLeafTarget(const AAntPawn* Ant, float Scale)
{
	const int32 Index = Ant->GetLeafInReach();
	if (Index == INDEX_NONE || Ant->IsCutting() || Ant->IsCarrying()) return;
	const float Pulse = 0.65f + 0.35f * FMath::Sin(GetWorld() ? GetWorld()->GetRealTimeSeconds() * 5.f : 0.f);
	DrawLeafOutline(Index, FLinearColor(1.f, 0.88f, 0.45f, Pulse), 2.f * Scale);
}

bool ALeaflineHUD::DrawLeafOutline(int32 Index, const FLinearColor& Color, float Thickness)
{
	const ATreeActor* Tree = ATreeActor::Get(this);
	const APlayerController* PC = GetOwningPlayerController();
	if (Index == INDEX_NONE || !Tree || !PC || !PC->PlayerCameraManager || Index >= Tree->GetLeaves().Num()) return false;
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
			if (FVector::DotProduct(W - CamPos, CamFwd) < 10.0) return false; // part of the blade is behind the camera
			const FVector S = Canvas->Project(W);
			Screen.Add(FVector2D(S.X, S.Y));
		}
	}
	for (int32 i = 0; i < Screen.Num(); ++i)
	{
		const FVector2D& A = Screen[i];
		const FVector2D& B = Screen[(i + 1) % Screen.Num()];
		DrawLine(A.X, A.Y, B.X, B.Y, Color, Thickness);
	}
	return true;
}

void ALeaflineHUD::DrawDeliveryPops(const ALineSystem* Lines, float Scale)
{
	const APlayerController* PC = GetOwningPlayerController();
	if (!Lines || !PC || !PC->PlayerCameraManager || !GetWorld()) return;
	const FVector CamPos = PC->PlayerCameraManager->GetCameraLocation();
	const FVector CamFwd = PC->PlayerCameraManager->GetCameraRotation().Vector();
	const double Now = GetWorld()->GetRealTimeSeconds();
	constexpr float Life = 1.5f;
	UFont* Font = GEngine->GetMediumFont();
	for (const FDeliveryPop& P : Lines->GetRecentDeliveries())
	{
		const float T = float(Now - P.RealTime) / Life;
		if (T < 0.f || T > 1.f) continue;
		if (FVector::DotProduct(P.WorldPos - CamPos, CamFwd) < 10.0) continue; // behind the camera
		const FVector S = Canvas->Project(P.WorldPos);
		// Rises with an ease-out, holds, then fades over the last 40%.
		const float Rise = 80.f * Scale * (1.f - FMath::Square(1.f - T));
		const float A = FMath::Clamp((1.f - T) / 0.4f, 0.f, 1.f);
		const float Size = (1.1f + 0.08f * FMath::Min(P.Chain, 10)) * Scale;
		const FString Text = P.Mass >= 10.f ? FString::Printf(TEXT("+%.0f"), P.Mass) : FString::Printf(TEXT("+%.1f"), P.Mass);
		DrawTextCentered(Text, S.X, S.Y - Rise, FLinearColor(0.7f, 0.98f, 0.5f, A), Size, Font);
	}
}

void ALeaflineHUD::DrawColonyCounter(const ADayDirector* D, float Scale, float Dt)
{
	const ULeaflineGameInstance* GI = Cast<ULeaflineGameInstance>(GetGameInstance());
	if (!GI) return;
	const float Target = GI->GetTotalHauled();
	if (ColonyShown < 0.f || Target < ColonyShown)
	{
		ColonyShown = Target;
	}
	else
	{
		// Eases toward the real value, but never crawls: at least a few units a second until it arrives.
		const float Diff = Target - ColonyShown;
		ColonyShown = Diff < 0.05f ? Target : ColonyShown + FMath::Min(Diff, FMath::Max(Diff * 5.f * Dt, 6.f * Dt));
	}
	UFont* Font = GEngine->GetMediumFont();
	const float CX = Canvas->ClipX * 0.5f;
	DrawTextCentered(FString::Printf(TEXT("Colony  %s"), *FString::FormatAsNumber(FMath::RoundToInt(ColonyShown))), CX, 22.f * Scale, TextColor, 1.6f * Scale, Font);
	DrawTextCentered(FString::Printf(TEXT("Today  +%.1f"), D->GetStats().MassHauled), CX, 62.f * Scale, AccentColor, 1.05f * Scale, Font);
}

void ALeaflineHUD::DrawLineRows(const ADayDirector* D, const ALineSystem* Lines, float Scale)
{
	if (!Lines || Lines->GetLines().Num() == 0) return;
	if (D->GetPhase() != EDayPhase::Shift && D->GetPhase() != EDayPhase::Harvest) return;
	UFont* Font = GEngine->GetMediumFont();
	const float X = 40.f * Scale;
	float Y = 150.f * Scale;
	const float RowH = 28.f * Scale;
	const float BarW = 70.f * Scale, BarH = 8.f * Scale;
	for (const FColonyLine& L : Lines->GetLines())
	{
		const bool bLive = L.IsLive();
		const float Dim = bLive ? 1.f : 0.5f;
		const FLinearColor Text(TextColor.R, TextColor.G, TextColor.B, Dim);
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.35f * Dim), X - 8.f * Scale, Y - 3.f * Scale, 400.f * Scale, RowH);
		DrawCrisp(Canvas, FString::Printf(TEXT("Line %d"), L.Number), FLinearColor(AccentColor.R, AccentColor.G, AccentColor.B, Dim), X, Y, Font, 0.95f * Scale);
		DrawCrisp(Canvas, FString::Printf(TEXT("home %d"), L.PiecesHome), Text, X + 78.f * Scale, Y, Font, 0.95f * Scale);
		DrawCrisp(Canvas, FString::Printf(TEXT("lost %d"), L.PiecesLost), L.PiecesLost > 0 ? FLinearColor(StaminaLockedColor.R, StaminaLockedColor.G, StaminaLockedColor.B, Dim) : Text,
			X + 156.f * Scale, Y, Font, 0.95f * Scale);
		// Patch remaining: a small bar and the count.
		const float BX = X + 232.f * Scale, BY = Y + 7.f * Scale;
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.5f * Dim), BX - 1.f * Scale, BY - 1.f * Scale, BarW + 2.f * Scale, BarH + 2.f * Scale);
		DrawRect(FLinearColor(0.4f, 0.75f, 0.3f, 0.95f * Dim), BX, BY, BarW * L.Fullness(), BarH);
		DrawCrisp(Canvas, FString::Printf(TEXT("%d"), FMath::Max(L.PatchRemaining, 0)), Text, BX + BarW + 8.f * Scale, Y, Font, 0.95f * Scale);
		DrawCrisp(Canvas, FString::Printf(TEXT("\u00D7%.1f"), L.Richness), FLinearColor(0.95f, 0.9f, 0.5f, Dim), BX + BarW + 44.f * Scale, Y, Font, 0.95f * Scale);
		Y += RowH;
	}
	if (D->GetPhase() == EDayPhase::Shift && Lines->IsActive() && Lines->NumIdle() > 0)
	{
		const int32 N = Lines->NumIdle();
		DrawCrisp(Canvas, FString::Printf(TEXT("%d worker%s idle: lay a new line"), N, N == 1 ? TEXT("") : TEXT("s")), FLinearColor(1.f, 0.8f, 0.35f, 1.f),
			X, Y + 4.f * Scale, Font, 1.0f * Scale);
	}
}

void ALeaflineHUD::DrawPatchPreview(const AAntPawn* Ant, const ALineSystem* Lines, float Scale)
{
	if (Ant->IsCarrying()) return;
	int32 Leaf = Ant->GetLeafInReach();
	if (Leaf == INDEX_NONE && Ant->IsCutting()) Leaf = Ant->GetCuttingLeaf();
	// The read-out is measured at the mouth, like the cut reach. No cuttable leaf there but a spent one: the bough is stripped.
	auto DrawStripped = [&]()
	{
		UFont* F = GEngine->GetMediumFont();
		DrawTextCentered(TEXT("Stripped. Fresh leaves are higher up."), Canvas->ClipX * 0.5f, Canvas->ClipY - 175.f * Scale, FLinearColor(0.95f, 0.9f, 0.5f, 1.f), 1.1f * Scale, F);
	};
	if (Leaf == INDEX_NONE)
	{
		const ATreeActor* Tree = ATreeActor::Get(this);
		if (Tree && !Ant->IsCutting() && Ant->IsPlayerControlled()
			&& Tree->FindSpentLeafNear(Ant->GetActorTransform().TransformPosition(Ant->MandibleOffset), Ant->Cut.Reach) != INDEX_NONE)
		{
			DrawStripped();
		}
		return;
	}
	if (!Lines) return;
	const float Template = Ant->IsCutting() ? Ant->GetCutSize() : Lines->Tuning.PreviewTemplate;
	FPatchPreview P;
	if (!Lines->GetPatchPreview(Leaf, Template, P)) return;
	if (P.Pieces <= 0)
	{
		if (!Ant->IsCutting()) DrawStripped();
		return;
	}
	// The scout's own leaf is outlined by DrawLeafTarget; the patch's other leaves get a fainter outline.
	const FLinearColor Faint(1.f, 0.88f, 0.45f, 0.35f);
	for (const int32 Other : P.Leaves)
	{
		if (Other != Leaf) DrawLeafOutline(Other, Faint, 1.5f * Scale);
	}
	UFont* Font = GEngine->GetMediumFont();
	const FString Text = FString::Printf(TEXT("This patch: about %d pieces, rich \u00D7%.1f"), P.Pieces, P.Richness);
	DrawTextCentered(Text, Canvas->ClipX * 0.5f, Canvas->ClipY - 175.f * Scale, FLinearColor(0.95f, 0.9f, 0.5f, 1.f), 1.1f * Scale, Font);
}

void ALeaflineHUD::DrawHarvest(const ADayDirector* D, const ALineSystem* Lines, float Scale)
{
	UFont* Font = GEngine->GetMediumFont();
	const float CX = Canvas->ClipX * 0.5f;
	float Y = 120.f * Scale;
	// Only scouts walking home (no recruit trips): the same fast-forward, about the walk home.
	const bool bHomeOnly = D->IsHomeOnlyHarvest();
	DrawTextCentered(bHomeOnly ? TEXT("Fast-forward: the last workers head home") : TEXT("Harvest: the colony brings in the rest"), CX, Y, AccentColor, 1.4f * Scale, Font);
	Y += 40.f * Scale;
	FString Speed = D->IsSkippingHarvest() ? FString(TEXT("skipping...")) : FString::Printf(TEXT("Speed \u00D7%d"), FMath::RoundToInt(Lines ? Lines->GetSpeedFactor() : 1.f));
	if (Lines && !bHomeOnly)
	{
		Speed += FString::Printf(TEXT("      Recruits out: %d      Trips left: %d"), Lines->NumOut(), Lines->TripsLeft());
	}
	DrawTextCentered(Speed, CX, Y, TextColor, 1.1f * Scale, Font);
	const bool bLineViews = Lines && Lines->NumLiveLines() > 0;
	const FString Controls = D->IsSkippingHarvest() ? FString() : FString(bLineViews ? TEXT("Space / A: skip      Tab / LB: next line") : TEXT("Space / A: skip"));
	if (!Controls.IsEmpty())
	{
		const FVector2D Size = MeasureCrisp(Controls, Font, 1.15f * Scale);
		const float BY = Canvas->ClipY - 130.f * Scale;
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.4f), CX - Size.X * 0.5f - 14.f * Scale, BY - 6.f * Scale, Size.X + 28.f * Scale, Size.Y + 12.f * Scale);
		DrawTextCentered(Controls, CX, BY, TextColor, 1.15f * Scale, Font);
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
		else if (bGustSoon && (S == EAntState::Walking || S == EAntState::Riding || S == EAntState::Cutting))
		{
			// Sheltered spots need no grip; elsewhere say what helps: grip, or the side away from the arrow.
			Prompt = ExposureShown >= 0.f && ExposureShown < 0.25f ? TEXT("Gust! You are sheltered here")
				: (Wind->GetPhase() == EWindPhase::Warning ? TEXT("Gust coming: hold Space / A to grip, or get to the side away from the wind arrow")
				                                           : TEXT("Gust! Hold Space / A to grip"));
		}
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
	const ULeaflineGameInstance* GI = Cast<ULeaflineGameInstance>(GetGameInstance());
	const ALineSystem* LineSys = D->GetLineSystem();
	const float CX = Canvas->ClipX * 0.5f;
	float Y = Canvas->ClipY * 0.11f;
	const FDayStats& S = D->GetStats();

	// Reveal clock: restarts each time the recap opens.
	const float Dt = GetWorld() ? FMath::Min(GetWorld()->GetDeltaSeconds(), 0.1f) : 0.016f;
	if (!bWasRecap) RecapTime = 0.f;
	bWasRecap = true;
	RecapTime += Dt;
	auto Count01 = [&](float Start, float Dur) { return FMath::Clamp((RecapTime - Start) / Dur, 0.f, 1.f); };

	DrawTextCentered(FString::Printf(TEXT("Day %d: today's cut from %s"), S.Day, *D->GetTreeName()), CX, Y, AccentColor, 2.0f * Scale, Large);
	Y += 80.f * Scale;
	if (D->IsOneTree())
	{
		DrawTextCentered(FString::Printf(TEXT("Tree %d, %s: day %d on this tree"), D->GetStage() + 1, *D->GetTreeName(), D->GetDayOnTree()), CX, Y - 30.f * Scale, TextColor, 1.0f * Scale, Medium);
	}

	// One row per line, in turn, each counting up; then the day total.
	const bool bShowLines = LineSys && LineSys->GetLines().Num() > 0;
	constexpr float RowGap = 0.6f, RowCount = 0.5f, FirstRow = 0.4f;
	float TotalStart = 0.3f;
	if (bShowLines)
	{
		const TArray<FColonyLine>& All = LineSys->GetLines();
		constexpr int32 MaxRows = 8;
		const int32 Shown = FMath::Min(All.Num(), MaxRows);
		const float RowH = 30.f * Scale;
		const float RW = 760.f * Scale, X = CX - RW * 0.5f;
		for (int32 i = 0; i < Shown; ++i)
		{
			const FColonyLine& L = All[i];
			const float Start = FirstRow + i * RowGap;
			if (RecapTime < Start) break;
			const float K = Count01(Start, RowCount);
			const float Fade = FMath::Clamp((RecapTime - Start) / 0.2f, 0.f, 1.f);
			DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.35f * Fade), X, Y - 2.f * Scale, RW, RowH);
			DrawCrisp(Canvas, FString::Printf(TEXT("Line %d"), L.Number), FLinearColor(AccentColor.R, AccentColor.G, AccentColor.B, Fade), X + 14.f * Scale, Y, Medium, 1.05f * Scale);
			const FLinearColor T(TextColor.R, TextColor.G, TextColor.B, Fade);
			DrawCrisp(Canvas, FString::Printf(TEXT("pieces home %d"), FMath::RoundToInt(L.PiecesHome * K)), T, X + 120.f * Scale, Y, Medium, 1.05f * Scale);
			DrawCrisp(Canvas, FString::Printf(TEXT("lost %d"), FMath::RoundToInt(L.PiecesLost * K)), L.PiecesLost > 0 ? FLinearColor(StaminaLockedColor.R, StaminaLockedColor.G, StaminaLockedColor.B, Fade) : T,
				X + 330.f * Scale, Y, Medium, 1.05f * Scale);
			DrawCrisp(Canvas, FString::Printf(TEXT("patch left %d"), FMath::Max(L.PatchRemaining, 0)), T, X + 430.f * Scale, Y, Medium, 1.05f * Scale);
			DrawCrisp(Canvas, FString::Printf(TEXT("%.1f"), L.MassHome * K), FLinearColor(0.7f, 0.98f, 0.5f, Fade), X + RW - 90.f * Scale, Y, Medium, 1.05f * Scale);
			Y += RowH + 4.f * Scale;
		}
		if (All.Num() > Shown)
		{
			DrawTextCentered(FString::Printf(TEXT("and %d more"), All.Num() - Shown), CX, Y, TextColor, 0.9f * Scale, Medium);
			Y += 28.f * Scale;
		}
		TotalStart = FirstRow + Shown * RowGap + 0.2f;
		Y = FMath::Max(Y, Canvas->ClipY * 0.11f + 80.f * Scale + Shown * (RowH + 4.f * Scale));
	}
	Y += 10.f * Scale;

	// Day total, counting up, with the best-day mark.
	const float TK = Count01(TotalStart, 0.9f);
	if (RecapTime >= TotalStart)
	{
		DrawTextCentered(FString::Printf(TEXT("Leaf hauled: %.1f"), S.MassHauled * TK), CX, Y, TextColor, 1.5f * Scale, Medium);
		Y += 46.f * Scale;
		if (TK >= 1.f)
		{
			if (S.bNewBest) DrawTextCentered(TEXT("Best day yet!"), CX, Y, FLinearColor(0.95f, 0.9f, 0.5f, 1.f), 1.25f * Scale, Medium);
			else DrawTextCentered(FString::Printf(TEXT("Best day so far: %.1f"), GI ? GI->GetBestDayHaul() : S.PreviousBest), CX, Y, FLinearColor(0.8f, 0.8f, 0.75f, 1.f), 1.05f * Scale, Medium);
		}
		Y += 40.f * Scale;
	}
	else
	{
		Y += 86.f * Scale;
	}

	TArray<FString> Rows = {
		FString::Printf(TEXT("Fragments brought home: %d"), S.Fragments),
		FString::Printf(TEXT("Largest fragment: %.1f"), S.LargestFragment),
		FString::Printf(TEXT("Workers blown off: %d"), S.BlownOff),
		FString::Printf(TEXT("Trail laid: %.0f m"), S.TrailLaid / 100.f),
	};
	if (bShowLines && D->AreRecruitsOn()) Rows.Insert(FString::Printf(TEXT("Recruits: %d pieces home, %d lost to the wind"), S.RecruitPieces, S.RecruitLost), 1);
	for (const FString& L : Rows)
	{
		DrawTextCentered(L, CX, Y, TextColor, 1.1f * Scale, Medium);
		Y += 34.f * Scale;
	}
	Y += 14.f * Scale;
	DrawColonyMeter(CX, Y, 520.f * Scale, Scale);
	Y += 90.f * Scale;
	if (D->IsOneTree() && D->IsMoveDue())
	{
		DrawTextCentered(TEXT("Tomorrow the colony moves to a new tree"), CX, Y, FLinearColor(0.95f, 0.9f, 0.5f, 1.f), 1.15f * Scale, Medium);
		Y += 40.f * Scale;
	}
	DrawTextCentered(TEXT("Press E / Space / A or click to go into the nest"), CX, Y, AccentColor, 1.2f * Scale, Medium);
}

void ALeaflineHUD::DrawColonyMeter(float CX, float Y, float W, float Scale)
{
	const ULeaflineGameInstance* GI = Cast<ULeaflineGameInstance>(GetGameInstance());
	if (!GI) return;
	UFont* Medium = GEngine->GetMediumFont();
	const bool bOneTree = GI->IsOneTree();
	const float Top = GI->GetMilestoneThreshold(LeaflineUpgrades::MilestoneCount - 1);
	const float H = 12.f * Scale, X = CX - W * 0.5f;
	DrawTextCentered(FString::Printf(TEXT("Colony: %.1f hauled in all"), GI->GetTotalHauled()), CX, Y, TextColor, 1.1f * Scale, Medium);
	const float BarY = Y + 34.f * Scale;
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.5f), X - 2.f * Scale, BarY - 2.f * Scale, W + 4.f * Scale, H + 4.f * Scale);
	DrawRect(FLinearColor(0.4f, 0.75f, 0.3f, 0.95f), X, BarY, W * FMath::Clamp(GI->GetTotalHauled() / Top, 0.f, 1.f), H);
	for (int32 m = 0; m < LeaflineUpgrades::MilestoneCount; ++m)
	{
		const float T = GI->GetMilestoneThreshold(m);
		const float MX = X + W * T / Top;
		const bool bReached = GI->HasReachedMilestone(m);
		const FLinearColor MarkColor = bReached ? AccentColor : TextColor;
		DrawRect(MarkColor, MX - 1.5f * Scale, BarY - 6.f * Scale, 3.f * Scale, H + 12.f * Scale);
		DrawTextCentered(FString::Printf(TEXT("%.0f"), T), MX, BarY + H + 8.f * Scale, MarkColor, 0.9f * Scale, Medium);
		// With one tree on, the second and third milestones move the colony: a small tree above the mark and a label.
		if (bOneTree && m >= 1)
		{
			const float TW = 7.f * Scale, TY = BarY - 12.f * Scale;
			DrawRect(MarkColor, MX - 1.f * Scale, TY - 4.f * Scale, 2.f * Scale, 5.f * Scale);                    // trunk
			DrawRect(MarkColor, MX - TW * 0.5f, TY - 11.f * Scale, TW, 8.f * Scale);                              // crown
			DrawTextCentered(TEXT("new tree"), MX, BarY + H + 30.f * Scale, MarkColor, 0.8f * Scale, Medium);
		}
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
		TEXT("Grip in a gust (hold): Space / A      Switch ant (tap): Tab / LB      Wind map (hold): Tab / LB"),
		TEXT("Pause: Esc or P / Start"),
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
		// Row indices and hit ids stay the upgrade index; a hidden row (Brood with recruits off) just takes no space.
		if (i < LeaflineUpgrades::Count && !D->IsNestRowVisible(i)) continue;
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
			DrawCrisp(Canvas, (D->IsOneTree() ? (D->IsMoveDue() ? FString::Printf(TEXT("Start day %d on a new tree"), D->GetDay() + 1) : FString::Printf(TEXT("Start day %d"), D->GetDay() + 1))
					: FString::Printf(TEXT("Start day %d: a new tree"), D->GetDay() + 1)), AccentColor, X + 18.f * Scale, Y + 14.f * Scale, Medium, 1.25f * Scale);
		}
		Y += RowH;
	}
	Y += 30.f * Scale;
	DrawColonyMeter(CX, Y, 520.f * Scale, Scale);
	Y += 100.f * Scale;
	if (D->IsOneTree() && D->IsMoveDue())
	{
		DrawTextCentered(TEXT("Tomorrow the colony moves to a new tree"), CX, Y, FLinearColor(0.95f, 0.9f, 0.5f, 1.f), 1.15f * Scale, Medium);
		Y += 40.f * Scale;
	}
	DrawTextCentered(TEXT("W / S, stick or mouse: choose      E / Space / A or click: buy or start      Q / Y: start the next day"), CX, Y, FLinearColor(0.8f, 0.8f, 0.75f, 1.f), 1.0f * Scale, Medium);
}

void ALeaflineHUD::DrawMoveCard(const ADayDirector* D, float Scale)
{
	DrawRect(FLinearColor(0.02f, 0.04f, 0.03f, 0.55f), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY); // lighter: the new tree shows behind it
	UFont* Large = GEngine->GetLargeFont();
	UFont* Medium = GEngine->GetMediumFont();
	const float CX = Canvas->ClipX * 0.5f;
	float Y = Canvas->ClipY * 0.3f;
	DrawTextCentered(D->GetMoveCardText(), CX, Y, AccentColor, 2.4f * Scale, Large);
	Y += 100.f * Scale;
	DrawTextCentered(TEXT("A new tree: taller, with richer leaves and stronger gusts."), CX, Y, TextColor, 1.25f * Scale, Medium);
	Y += 90.f * Scale;
	DrawTextCentered(FString::Printf(TEXT("Press E / Space / A or click to begin day %d"), D->GetDay()), CX, Y, AccentColor, 1.3f * Scale, Medium);
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
	const ALeaflinePlayerController* PC = Cast<ALeaflinePlayerController>(GetOwningPlayerController());
	const int32 Page = PC ? PC->GetPausePage() : 0;
	const int32 Sel = PC ? PC->GetPauseRow() : 0;
	float Y = Canvas->ClipY * (Page == 0 ? 0.32f : 0.24f);
	DrawTextCentered(Page == 0 ? TEXT("Paused") : TEXT("Options"), CX, Y, AccentColor, 2.4f * Scale, Large);
	Y += 100.f * Scale;
	const FLinearColor RowSel(0.3f, 0.55f, 0.6f, 0.6f), RowIdle(0.f, 0.f, 0.f, 0.45f);
	const float RowH = 54.f * Scale, Gap = 10.f * Scale;

	if (Page == 0)
	{
		// Resume / Options / Quit, for keys, pad and mouse.
		const float BW = 300.f * Scale;
		const TCHAR* Labels[3] = { TEXT("Resume"), TEXT("Options"), TEXT("Quit") };
		const int32 Ids[3] = { MenuResume, MenuOptions, MenuQuit };
		for (int32 b = 0; b < 3; ++b)
		{
			const bool bOn = Sel == b || IsHovered(Ids[b]);
			DrawRect(bOn ? RowSel : RowIdle, CX - BW * 0.5f, Y, BW, RowH);
			DrawTextCentered(Labels[b], CX, Y + 12.f * Scale, bOn ? AccentColor : TextColor, 1.3f * Scale, Medium);
			AddMenuHit(CX - BW * 0.5f, Y, BW, RowH, Ids[b]);
			Y += RowH + Gap;
		}
		Y += 20.f * Scale;
		DrawTextCentered(TEXT("W / S or stick: choose      E / Space / A or click: select      Esc, P or Start: resume      Q / Y: quit"), CX, Y, TextColor, 1.0f * Scale, Medium);
		return;
	}

	// Options: label on the left, "<  value  >" on the right; the last row goes back.
	const float RW = 640.f * Scale, X = CX - RW * 0.5f;
	const TCHAR* Labels[ALeaflinePlayerController::NumOptionRows] = { TEXT("Mouse sensitivity"), TEXT("Invert look"), TEXT("Volume"), TEXT("Graphics"), TEXT("Back") };
	for (int32 r = 0; r < ALeaflinePlayerController::NumOptionRows; ++r)
	{
		const bool bBack = r == ALeaflinePlayerController::NumOptionRows - 1;
		const bool bOn = Sel == r;
		DrawRect(bOn ? RowSel : RowIdle, X, Y, RW, RowH);
		AddMenuHit(X, Y, RW, RowH, MenuOptRow + r);
		if (bBack)
		{
			DrawTextCentered(Labels[r], CX, Y + 12.f * Scale, bOn ? AccentColor : TextColor, 1.25f * Scale, Medium);
		}
		else
		{
			DrawCrisp(Canvas, Labels[r], bOn ? AccentColor : TextColor, X + 22.f * Scale, Y + 12.f * Scale, Medium, 1.25f * Scale);
			const float VX = X + RW * 0.72f, Arrow = 34.f * Scale;
			const bool bMinus = IsHovered(MenuOptMinus + r), bPlus = IsHovered(MenuOptPlus + r);
			DrawTextCentered(TEXT("<"), VX - 90.f * Scale, Y + 12.f * Scale, bMinus ? AccentColor : TextColor, 1.3f * Scale, Medium);
			DrawTextCentered(PC ? PC->GetOptionValueText(r) : FString(), VX, Y + 12.f * Scale, TextColor, 1.25f * Scale, Medium);
			DrawTextCentered(TEXT(">"), VX + 90.f * Scale, Y + 12.f * Scale, bPlus ? AccentColor : TextColor, 1.3f * Scale, Medium);
			// The arrow hit boxes come after the row's, so they win (MenuItemAt returns the first match: check them first).
			MenuHits.Insert({ FBox2D(FVector2D(VX - 90.f * Scale - Arrow * 0.5f, Y), FVector2D(VX - 90.f * Scale + Arrow * 0.5f, Y + RowH)), MenuOptMinus + r }, 0);
			MenuHits.Insert({ FBox2D(FVector2D(VX + 90.f * Scale - Arrow * 0.5f, Y), FVector2D(VX + 90.f * Scale + Arrow * 0.5f, Y + RowH)), MenuOptPlus + r }, 0);
		}
		Y += RowH + Gap;
	}
	Y += 20.f * Scale;
	DrawTextCentered(TEXT("W / S: choose      A / D or click < >: change      E / Space / A: change or back      Q / Y: back"), CX, Y, TextColor, 1.0f * Scale, Medium);
}
