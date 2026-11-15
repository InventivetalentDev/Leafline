#include "UI/LeaflineHUD.h"
#include "UI/PromptIcons.h"
#include "UI/Credits.h"
#include "Misc/App.h"
#include "Debug/LeafDebug.h"
#include "Engine/Texture2D.h"
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
#include "World/ExposureBands.h"
#include "World/BirdSystem.h"
#include "Tree/TreeActor.h"
#include "Colony/TrailNetwork.h"
#include "Colony/LineSystem.h"
#include "Game/Upgrades.h"
#include "CanvasItem.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "Ant/AntCamera.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	// All lengths are 1080p design pixels. Colours retain their existing gameplay meanings.
	struct FHUDStyle
	{
		float SecondaryHeight = 20.f; // 1.85% of screen height
		float PrimaryHeight = 30.f;   // 2.78% of screen height
		float NameHeight = 24.f;      // a nest row's upgrade name and its start-day row
		float DetailHeight = 18.f;    // a nest row's description
		float HeroHeight = 40.f;      // the recap's leaf hauled
		float MeterBlock = 128.f;     // DrawColonyMeter with its "new tree" labels, and the gap after it
		float MeterBlockShort = 96.f; // the same without them (one tree off)
		float Edge = 32.f;
		float Padding = 16.f;
		float Gap = 12.f;
		float Row = 32.f;
		float Bar = 10.f;
		float Stroke = 2.f;
		float CardWidth = 1120.f;
		float MenuRow = 64.f;
		FLinearColor Panel = FLinearColor(0.02f, 0.03f, 0.025f, 0.72f);
		FLinearColor Track = FLinearColor(0.f, 0.f, 0.f, 0.5f);
		FLinearColor Muted = FLinearColor(0.8f, 0.8f, 0.75f);
		FLinearColor Detail = FLinearColor(0.66f, 0.68f, 0.62f);      // descriptions and recap detail rows, under the main line
		FLinearColor Unaffordable = FLinearColor(0.66f, 0.68f, 0.62f, 0.5f);
		FLinearColor PipEmpty = FLinearColor(0.f, 0.f, 0.f, 0.45f);
		FLinearColor PipEdge = FLinearColor(0.8f, 0.8f, 0.75f, 0.35f);
		FLinearColor Leaf = FLinearColor(0.4f, 0.75f, 0.3f, 0.95f);
		FLinearColor Rich = FLinearColor(0.95f, 0.9f, 0.5f);
		FLinearColor Delivery = FLinearColor(0.7f, 0.98f, 0.5f);
		// ExposureOverlay converts these exact HUD hues to linear emissive. Keep the bands too.
		float ShelteredBelow = LeafExposure::ShelteredBelow; // World/ExposureBands.h, shared with the bird
		float PartlyBelow = LeafExposure::PartlyBelow;
		FLinearColor Sheltered = FLinearColor(0.6f, 0.95f, 0.5f);
		FLinearColor Partly = FLinearColor(1.f, 0.8f, 0.35f);
		FLinearColor Warning = FLinearColor(1.f, 0.72f, 0.2f);
		FLinearColor CutSafe = FLinearColor(0.45f, 0.9f, 0.4f);
		FLinearColor CutDanger = FLinearColor(0.95f, 0.25f, 0.2f);
		FLinearColor Home = FLinearColor(0.55f, 0.6f, 0.55f);
		FLinearColor Selection = FLinearColor(0.3f, 0.55f, 0.6f, 0.45f);
		// Text, accent, stamina and exposed/blown-off colours use the HUD's existing editable properties.
	};
	const FHUDStyle Style;

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

	float ScaleForHeight(float Height)
	{
		UFont* Font = GEngine->GetMediumFont();
		const float Base = MeasureCrisp(TEXT("Ag"), Font, 1.f).Y;
		const float FontSize = FMath::Max(float(Font->GetLegacySlateFontInfo().Size), 1.f);
		return FMath::CeilToInt(Height * FontSize / FMath::Max(Base, 1.f)) / FontSize;
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

namespace
{
	/** -1 follows bShowBuildLine; 0 or 1 from Leaf.BuildLine overrides it. */
	int32 GBuildLineOverride = -1;

	/** Leaf.HudIcons: the icon versions of the wind panel and the carry read-out (off: the text HUD exactly as before). */
	bool GHudIcons = true;

	FAutoConsoleCommand CmdHudIcons(
		TEXT("Leaf.HudIcons"),
		TEXT("Leaf.HudIcons [0|1]: animated icons in the wind panel and the carry read-out (1, default) or the text HUD as before (0); no argument toggles."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			GHudIcons = Args.Num() > 0 ? FCString::Atoi(*Args[0]) != 0 : !GHudIcons;
			UE_LOG(LogLeafline, Log, TEXT("Leaf.HudIcons %d"), GHudIcons ? 1 : 0);
		}));

	FAutoConsoleCommand CmdBuildLine(
		TEXT("Leaf.BuildLine"),
		TEXT("Leaf.BuildLine [0|1]: show or hide the faint build and position line in the bottom-left corner; no argument toggles."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			GBuildLineOverride = Args.Num() > 0 ? (FCString::Atoi(*Args[0]) != 0 ? 1 : 0) : (GBuildLineOverride == 0 ? 1 : 0);
		}));

	/** Content/Leafline/UIRaw/buildinfo.txt, written by Tools/write_buildinfo.sh before packaging (commit and time). */
	const FString& BuildInfo()
	{
		static FString Info;
		static bool bRead = false;
		if (!bRead)
		{
			bRead = true;
			FString Text;
			const FString Path = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Leafline/UIRaw/buildinfo.txt"));
			if (FFileHelper::LoadFileToString(Text, *Path)) Info = Text.TrimStartAndEnd();
			if (Info.IsEmpty()) Info = TEXT("dev build");
		}
		return Info;
	}
}

void ALeaflineHUD::DrawBuildLine(const AAntPawn* Ant, const ADayDirector* D, float Scale)
{
	const bool bShow = GBuildLineOverride >= 0 ? GBuildLineOverride == 1 : bShowBuildLine;
	if (!bShow || !Canvas) return;
	FString Line = BuildInfo();
	if (D) Line += FString::Printf(TEXT("  |  %s, seed %d, day %d"), *D->GetTreeName(), D->GetSeed(), D->GetDay());
	const ALeaflinePlayerController* PC = Cast<ALeaflinePlayerController>(GetOwningPlayerController());
	const AAntCamera* Cam = PC ? PC->GetAntCamera() : nullptr;
	if (Ant && Ant->GetAntMovement() && Cam)
	{
		const FVector P = Ant->GetAntMovement()->GetSurfacePosition();
		const FVector F = Cam->GetRefForward();
		const float Heading = FMath::RadiansToDegrees(FMath::Atan2(F.Y, F.X));
		Line += FString::Printf(TEXT("  |  Leaf.Goto %.0f %.0f %.0f %.0f %.0f"), P.X, P.Y, P.Z, Heading, Cam->GetPitch());
	}
	UFont* Font = GEngine->GetSmallFont();
	DrawCrisp(Canvas, Line, FLinearColor(1.f, 1.f, 1.f, 0.4f), 12.f * Scale, Canvas->ClipY - 26.f * Scale, Font, SecondaryText(Scale) * 0.75f);
}

void ALeaflineHUD::DrawHUD()
{
	MenuHits.Reset();
	if (bHideAll || !Canvas) return;
	Super::DrawHUD();

	const float Scale = Canvas->ClipY / 1080.f;
	const float Dt = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f;
	const AAntPawn* Ant = Cast<AAntPawn>(GetOwningPawn());
	const ADayDirector* D = ADayDirector::Get(this);
	DrawBuildLine(Ant, D, Scale);

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
	// Input view (F3 / Leaf.InputViz): the rule in charge and labels at the ends of the W/A/S/D paths.
	if (LeafDebug::bInputViz && LeafDebug::InputVizLines.Num() > 0)
	{
		UFont* VizFont = GEngine->GetMediumFont();
		float VY = Canvas->ClipY * 0.36f;
		for (int32 i = 0; i < LeafDebug::InputVizLines.Num(); ++i)
		{
			const FLinearColor C = i == 0 ? FLinearColor(LeafDebug::InputVizColor) : TextColor;
			DrawCrisp(Canvas, LeafDebug::InputVizLines[i], C, 30.f * Scale, VY, VizFont, i == 0 ? PrimaryText(Scale) : SecondaryText(Scale));
			VY += (i == 0 ? Style.PrimaryHeight + Style.Gap : Style.Row) * Scale;
		}
		if (const APlayerController* VPC = GetOwningPlayerController())
		{
			for (const LeafDebug::FInputVizLabel& L : LeafDebug::InputVizLabels)
			{
				FVector2D SP;
				if (VPC->ProjectWorldLocationToScreen(L.Pos, SP)) DrawCrisp(Canvas, L.Text, FLinearColor(L.Color), SP.X - 6.f * Scale, SP.Y - 12.f * Scale, VizFont, SecondaryText(Scale));
			}
		}
	}
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
	bool bGoalShown = false;
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
			// A new colony has recruits from day 1: they set out along the first trail while this goal shows.
			const bool bRecruitsOut = D->AreRecruitsOn() && Lines && (Lines->NumOut() > 0 || Lines->TripsLeft() > 0);
			Goal = bRecruitsOut
				? TEXT("Your first worker walks home on its own, and recruits follow its trail to fetch pieces. Hold {Follow} on the cyan trail to ride it up")
				: TEXT("Your first worker walks home on its own. Hold {Follow} on the cyan trail to ride it up");
		}
		if (!Goal.IsEmpty())
		{
			DrawTextPanel(Goal, Canvas->ClipX * 0.5f, 160.f * Scale, AccentColor, SecondaryText(Scale), Scale, 1.f, 700.f);
			bGoalShown = true;
		}
	}
	// The first bird day (of the session): what the bird does and what helps, while it is about.
	if (D && Ant && D->GetPhase() == EDayPhase::Shift)
	{
		const ABirdSystem* Bird = ABirdSystem::Get(this);
		if (Bird && Bird->ShouldShowHint() && !bGoalShown)
		{
			DrawTextPanel(Bird->IsBaitOn()
				? TEXT("A bird is out today. It swoops at ants carrying pieces on exposed bark: stay off the red, grip when it dives, or drop your piece in the open to draw it away.")
				: TEXT("A bird is out today. It swoops at ants carrying pieces on exposed bark: stay off the red, or grip when it dives."),
				Canvas->ClipX * 0.5f, 160.f * Scale, AccentColor, SecondaryText(Scale), Scale, 1.f, 700.f);
		}
	}
	if (Ant)
	{
		DrawStamina(Ant, Scale, Dt);
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
	const APlayerController* PC = GetOwningPlayerController();
	const AAntPawn* Watched = PC ? Cast<AAntPawn>(PC->GetPawn()) : nullptr;
	const float CX = Canvas->ClipX * 0.5f, Y = Canvas->ClipY * 0.3f;
	if (Watched && Watched->GetAntMovement() && Watched->GetAntMovement()->GetState() == EAntState::Returning && BannerTime <= 0.f)
	{
		DrawTextPanel(FString::Printf(TEXT("Worker %d heads home with its piece"), Watched->GetWorkerIndex() + 1),
			CX, Y, TextColor, SecondaryText(Scale), Scale);
	}
	if (BannerTime <= 0.f) return;
	BannerTime = FMath::Max(0.f, BannerTime - Dt);
	const float A = FMath::Clamp(BannerTime / 0.6f, 0.f, 1.f);
	UFont* Font = GEngine->GetMediumFont();
	const float TitleH = BannerTitle.IsEmpty() ? 0.f : (Style.PrimaryHeight + Style.Gap) * Scale;
	const float W = FMath::Max(float(MeasureText(BannerTitle, Font, PrimaryText(Scale)).X),
		float(MeasureText(BannerLine, Font, SecondaryText(Scale)).X));
	DrawPanel(CX - W * 0.5f, Y, W, TitleH + Style.SecondaryHeight * Scale, Scale, A);
	if (!BannerTitle.IsEmpty()) DrawTextCentered(BannerTitle, CX, Y, FLinearColor(AccentColor.R, AccentColor.G, AccentColor.B, A), PrimaryText(Scale), Font);
	DrawTextCentered(BannerLine, CX, Y + TitleH, FLinearColor(TextColor.R, TextColor.G, TextColor.B, A), SecondaryText(Scale), Font);
}

namespace
{
	/** A stretch of prompt text, or the icons of one button token. */
	struct FKeyRun
	{
		FString Text;
		TArray<UTexture2D*> Icons;
	};

	/** Width over height of an icon (the icons are cropped to the key's shape, so wide keys are wide). */
	float IconAspect(const UTexture2D* T)
	{
		return T && T->GetSizeY() > 0 ? float(T->GetSizeX()) / float(T->GetSizeY()) : 1.f;
	}

	/** "Hold {Cut}: cut" -> "Hold ", [E][LMB], ": cut". Unknown tokens stay as text; missing icons fall back to key names. */
	void SplitKeyRuns(const FString& Text, bool bPad, TArray<FKeyRun>& Out)
	{
		FString Cur;
		int32 i = 0;
		while (i < Text.Len())
		{
			const int32 Close = Text[i] == TEXT('{') ? Text.Find(TEXT("}"), ESearchCase::CaseSensitive, ESearchDir::FromStart, i) : INDEX_NONE;
			if (Close == INDEX_NONE)
			{
				Cur.AppendChar(Text[i++]);
				continue;
			}
			const FString Token = Text.Mid(i + 1, Close - i - 1);
			i = Close + 1;
			if (Token.Equals(TEXT("orclick"), ESearchCase::IgnoreCase))
			{
				if (!bPad) Cur += TEXT(" or click");
				continue;
			}
			TArray<FString> Names;
			FString Fallback;
			if (!LeafPrompts::Expand(Token, bPad, Names, Fallback))
			{
				Cur += Token;
				continue;
			}
			TArray<UTexture2D*> Icons;
			for (const FString& N : Names)
			{
				UTexture2D* T = LeafPrompts::Get(N);
				if (!T)
				{
					Icons.Reset();
					break;
				}
				Icons.Add(T);
			}
			if (Icons.Num() == 0)
			{
				Cur += Fallback;
				continue;
			}
			if (!Cur.IsEmpty()) Out.Add({ Cur, {} });
			Cur.Reset();
			Out.Add({ FString(), Icons });
		}
		if (!Cur.IsEmpty()) Out.Add({ Cur, {} });
	}
}

bool ALeaflineHUD::UsingPad() const
{
	if (LeafDebug::ForcePadIcons >= 0) return LeafDebug::ForcePadIcons == 1;
	const ALeaflinePlayerController* PC = Cast<ALeaflinePlayerController>(GetOwningPlayerController());
	return PC && PC->IsUsingGamepad();
}

FVector2D ALeaflineHUD::MeasureText(const FString& Text, UFont* Font, float TextScale) const
{
	if (!Text.Contains(TEXT("{"))) return MeasureCrisp(Text, Font, TextScale);
	TArray<FKeyRun> Runs;
	SplitKeyRuns(Text, UsingPad(), Runs);
	const float LineH = MeasureCrisp(TEXT("Ag"), Font, TextScale).Y;
	const float IconH = LineH * 1.4f;
	float W = 0.f;
	bool bIcons = false;
	for (const FKeyRun& R : Runs)
	{
		if (R.Icons.Num() == 0)
		{
			W += MeasureCrisp(R.Text, Font, TextScale).X;
			continue;
		}
		bIcons = true;
		W += 0.2f * IconH + (R.Icons.Num() - 1) * 0.08f * IconH;
		for (const UTexture2D* T : R.Icons) W += IconH * IconAspect(T);
	}
	// A row with icons is as tall as the icons; DrawTextAt centres the text on them.
	return FVector2D(W, bIcons ? IconH : LineH);
}

void ALeaflineHUD::DrawTextAt(const FString& Text, float X, float Y, const FLinearColor& Color, float TextScale, UFont* Font)
{
	if (!Text.Contains(TEXT("{")))
	{
		DrawCrisp(Canvas, Text, Color, X, Y, Font, TextScale);
		return;
	}
	TArray<FKeyRun> Runs;
	SplitKeyRuns(Text, UsingPad(), Runs);
	const float LineH = MeasureCrisp(TEXT("Ag"), Font, TextScale).Y;
	const float IconH = LineH * 1.4f;
	bool bIcons = false;
	for (const FKeyRun& R : Runs) bIcons |= R.Icons.Num() > 0;
	const float TextY = bIcons ? Y + 0.5f * (IconH - LineH) : Y;
	for (const FKeyRun& R : Runs)
	{
		if (R.Icons.Num() == 0)
		{
			DrawCrisp(Canvas, R.Text, Color, X, TextY, Font, TextScale);
			X += MeasureCrisp(R.Text, Font, TextScale).X;
			continue;
		}
		X += 0.1f * IconH;
		for (int32 k = 0; k < R.Icons.Num(); ++k)
		{
			if (UTexture2D* T = R.Icons[k])
			{
				FCanvasTileItem Tile(FVector2D(X, Y), T->GetResource(), FVector2D(IconH * IconAspect(T), IconH), FLinearColor(1.f, 1.f, 1.f, Color.A));
				Tile.BlendMode = SE_BLEND_Translucent;
				Canvas->DrawItem(Tile);
			}
			X += IconH * IconAspect(R.Icons[k]) + (k + 1 < R.Icons.Num() ? 0.08f * IconH : 0.f);
		}
		X += 0.1f * IconH;
	}
}

void ALeaflineHUD::DrawTextCentered(const FString& Text, float CenterX, float Y, const FLinearColor& Color, float TextScale, UFont* Font)
{
	const FVector2D Size = MeasureText(Text, Font, TextScale);
	DrawTextAt(Text, CenterX - Size.X * 0.5f, Y, Color, TextScale, Font);
}

float ALeaflineHUD::SecondaryText(float Scale) const
{
	return ScaleForHeight(Style.SecondaryHeight * Scale);
}

float ALeaflineHUD::PrimaryText(float Scale) const
{
	return ScaleForHeight(Style.PrimaryHeight * Scale);
}

void ALeaflineHUD::DrawPanel(float X, float Y, float W, float H, float Scale, float Alpha, LeafSkin::EKind Kind)
{
	FLinearColor C = Style.Panel;
	C.A *= Alpha;
	const float P = Style.Padding * Scale;
	// A Group panel centred within 15% of the screen width of the screen centre (colony counter, banners) uses the
	// skin's Panel_GroupCenter when it has one.
	const LeafSkin::FImage* Image = nullptr;
	if (Kind == LeafSkin::EKind::Group && FMath::Abs(X + W * 0.5f - Canvas->ClipX * 0.5f) <= 0.15f * Canvas->ClipX)
	{
		Image = LeafSkin::Get(LeafSkin::EKind::GroupCenter);
	}
	if (!Image) Image = LeafSkin::Get(Kind);
	if (Image)
	{
		DrawNineSlice(*Image, X - P, Y - P, W + 2.f * P, H + 2.f * P, Scale, Alpha);
		return;
	}
	DrawRect(C, X - P, Y - P, W + 2.f * P, H + 2.f * P);
}

void ALeaflineHUD::DrawNineSlice(const LeafSkin::FImage& Image, float X, float Y, float W, float H, float Scale, float Alpha)
{
	if (!Canvas || !Image.Texture || Image.Size.X <= 0 || Image.Size.Y <= 0) return;
	// mirror_right: a panel centred right of the screen centre is drawn flipped left to right, its left and right margins
	// (and outsets) swapped with the image, so the nine slices still line up.
	const bool bFlip = Image.bMirrorRight && X + W * 0.5f > Canvas->ClipX * 0.5f;
	// outset: the image reaches further outside the panel (1080p pixels, scaled).
	const float OL = (bFlip ? Image.OutsetR : Image.OutsetL) * Scale, OR = (bFlip ? Image.OutsetL : Image.OutsetR) * Scale;
	const float L = FMath::RoundToFloat(X - OL), T = FMath::RoundToFloat(Y - Image.OutsetT * Scale);
	const float R = FMath::RoundToFloat(X + W + OR), B = FMath::RoundToFloat(Y + H + Image.OutsetB * Scale);
	if (R - L < 1.f || B - T < 1.f) return;
	// Corners keep their painted size (image pixels / pixels per 1080p pixel, times the screen scale), rounded to whole
	// pixels; a panel smaller than its corners squeezes them to fit.
	const float K = Scale / FMath::Max(Image.PixelsPerDesign, 0.01f);
	const int32 ML = bFlip ? Image.Right : Image.Left, MR = bFlip ? Image.Left : Image.Right;
	float CL = FMath::RoundToFloat(ML * K), CR = FMath::RoundToFloat(MR * K);
	float CT = FMath::RoundToFloat(Image.Top * K), CB = FMath::RoundToFloat(Image.Bottom * K);
	if (CL + CR > R - L)
	{
		const float F = (R - L) / (CL + CR);
		CL = FMath::FloorToFloat(CL * F);
		CR = FMath::FloorToFloat(CR * F);
	}
	if (CT + CB > B - T)
	{
		const float F = (B - T) / (CT + CB);
		CT = FMath::FloorToFloat(CT * F);
		CB = FMath::FloorToFloat(CB * F);
	}
	const float Xs[4] = { L, L + CL, R - CR, R };
	const float Ys[4] = { T, T + CT, B - CB, B };
	const float U1 = float(Image.Left) / Image.Size.X, U2 = 1.f - float(Image.Right) / Image.Size.X;
	const float Us[4] = { bFlip ? 1.f : 0.f, bFlip ? U2 : U1, bFlip ? U1 : U2, bFlip ? 0.f : 1.f };
	const float Vs[4] = { 0.f, float(Image.Top) / Image.Size.Y, 1.f - float(Image.Bottom) / Image.Size.Y, 1.f };
	const FLinearColor Tint(1.f, 1.f, 1.f, Alpha);
	for (int32 j = 0; j < 3; ++j)
	{
		for (int32 i = 0; i < 3; ++i)
		{
			const float PW = Xs[i + 1] - Xs[i], PH = Ys[j + 1] - Ys[j];
			if (PW <= 0.f || PH <= 0.f) continue;
			FCanvasTileItem Tile(FVector2D(Xs[i], Ys[j]), Image.Texture->GetResource(), FVector2D(PW, PH),
				FVector2D(Us[i], Vs[j]), FVector2D(Us[i + 1], Vs[j + 1]), Tint);
			Tile.BlendMode = SE_BLEND_Translucent;
			Canvas->DrawItem(Tile);
		}
	}
}

void ALeaflineHUD::DrawButtonBack(float X, float Y, float W, float H, float Scale, bool bSelected)
{
	if (const LeafSkin::FImage* Image = LeafSkin::Get(bSelected ? LeafSkin::EKind::ButtonSelected : LeafSkin::EKind::ButtonIdle))
	{
		DrawNineSlice(*Image, X, Y, W, H, Scale, 1.f);
		return;
	}
	if (bSelected) DrawRect(Style.Selection, X, Y, W, H);
}

TArray<FString> ALeaflineHUD::WrapText(const FString& Text, float Width, float TextScale) const
{
	TArray<FString> Rows;
	FString Row, Spaces;
	int32 Start = 0;
	while (Start < Text.Len())
	{
		const int32 Space = Text.Find(TEXT(" "), ESearchCase::CaseSensitive, ESearchDir::FromStart, Start);
		const int32 End = Space == INDEX_NONE ? Text.Len() : Space;
		const FString Word = Text.Mid(Start, End - Start);
		if (!Word.IsEmpty())
		{
			const FString Next = Row.IsEmpty() ? Word : Row + Spaces + Word;
			if (!Row.IsEmpty() && MeasureText(Next, GEngine->GetMediumFont(), TextScale).X > Width)
			{
				Rows.Add(Row);
				Row = Word;
			}
			else Row = Next;
			Spaces = TEXT(" ");
		}
		else Spaces += TEXT(" ");
		Start = End + 1;
	}
	if (!Row.IsEmpty()) Rows.Add(Row);
	return Rows;
}

float ALeaflineHUD::DrawWrappedText(const FString& Text, float CX, float Y, float Width,
	const FLinearColor& Color, float TextScale, float Scale)
{
	UFont* Font = GEngine->GetMediumFont();
	const TArray<FString> Rows = WrapText(Text, Width, TextScale);
	const float Start = Y;
	for (const FString& Row : Rows)
	{
		DrawTextCentered(Row, CX, Y, Color, TextScale, Font);
		Y += MeasureText(Row, Font, TextScale).Y + Style.Gap * Scale;
	}
	return Rows.Num() > 0 ? Y - Start - Style.Gap * Scale : 0.f;
}

float ALeaflineHUD::WrappedHeight(const FString& Text, float Width, float TextScale, float Scale) const
{
	// What DrawWrappedText returns for the same text, without drawing it.
	UFont* Font = GEngine->GetMediumFont();
	const TArray<FString> Rows = WrapText(Text, Width, TextScale);
	float H = 0.f;
	for (const FString& Row : Rows) H += MeasureText(Row, Font, TextScale).Y;
	return Rows.Num() > 0 ? H + (Rows.Num() - 1) * Style.Gap * Scale : 0.f;
}

void ALeaflineHUD::DrawTextPanel(const FString& Text, float CX, float Y, const FLinearColor& Color,
	float TextScale, float Scale, float Alpha, float MaxWidth)
{
	UFont* Font = GEngine->GetMediumFont();
	const float Limit = FMath::Min(MaxWidth * Scale, Canvas->ClipX - 2.f * (Style.Edge + Style.Padding) * Scale);
	const TArray<FString> Rows = WrapText(Text, Limit, TextScale);
	float W = 0.f, H = 0.f;
	for (const FString& Row : Rows)
	{
		const FVector2D Size = MeasureText(Row, Font, TextScale);
		W = FMath::Max(W, float(Size.X));
		H += Size.Y + Style.Gap * Scale;
	}
	if (Rows.Num() == 0) return;
	H -= Style.Gap * Scale;
	DrawPanel(CX - W * 0.5f, Y, W, H, Scale, Alpha, LeafSkin::EKind::Tag);
	for (const FString& Row : Rows)
	{
		DrawTextCentered(Row, CX, Y, Color, TextScale, Font);
		Y += MeasureText(Row, Font, TextScale).Y + Style.Gap * Scale;
	}
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
	const bool bActive = Stamina < 0.999f;
	StaminaAlpha = FMath::FInterpTo(StaminaAlpha, bActive ? 1.f : 0.f, Dt, bActive ? 10.f : 2.f);
	if (StaminaAlpha < 0.01f) return;
	const float W = 280.f * Scale, H = Style.Bar * Scale;
	const float X = (Canvas->ClipX - W) * 0.5f;
	const float BarY = Canvas->ClipY - (Style.Edge + Style.Padding + Style.Bar) * Scale;
	const float Y = BarY - Style.Row * Scale;
	FLinearColor Fill = M->IsSprintLocked() ? StaminaLockedColor : StaminaColor;
	Fill.A *= StaminaAlpha;
	DrawPanel(X, Y, W, (Style.Row + Style.Bar) * Scale, Scale, StaminaAlpha);
	DrawTextAt(M->IsSprintLocked() ? TEXT("Stamina: recovering") : TEXT("Stamina"), X, Y,
		FLinearColor(TextColor.R, TextColor.G, TextColor.B, StaminaAlpha), SecondaryText(Scale), GEngine->GetMediumFont());
	FLinearColor Track = Style.Track;
	Track.A *= StaminaAlpha;
	DrawRect(Track, X, BarY, W, H);
	DrawRect(Fill, X, BarY, W * FMath::Clamp(StaminaShown, 0.f, 1.f), H);
}

void ALeaflineHUD::DrawDayInfo(const ADayDirector* D, float Scale)
{
	if (GHudIcons)
	{
		DrawDayPanelIcons(D, Scale);
		if (const AWindSystem* Wind = D->GetWind()) DrawWindIcons(D, Wind, Scale);
		return;
	}
	UFont* Font = GEngine->GetMediumFont();
	const float X = (Style.Edge + Style.Padding) * Scale, Y = X;
	const FString Title = D->GetTreeName().IsEmpty() ? FString::Printf(TEXT("Day %d"), D->GetDay())
		: FString::Printf(TEXT("Day %d: %s"), D->GetDay(), *D->GetTreeName());
	const float Cell = 140.f * Scale;
	const float W = FMath::Max(FMath::Max(440.f * Scale, D->NumWorkers() * Cell), float(MeasureText(Title, Font, PrimaryText(Scale)).X));
	DrawPanel(X, Y, W, 128.f * Scale, Scale);
	DrawTextAt(Title, X, Y, TextColor, PrimaryText(Scale), Font);
	const float PY = Y + 44.f * Scale, Pip = 14.f * Scale;
	for (int32 i = 0; i < D->NumWorkers(); ++i)
	{
		const float PX = X + i * Cell;
		const EWorkerState State = D->GetWorkerState(i);
		FLinearColor C = TextColor;
		const TCHAR* Label = TEXT("Ready");
		switch (State)
		{
		case EWorkerState::Waiting: break;
		case EWorkerState::Out: C = AccentColor; Label = TEXT("Out"); break;
		case EWorkerState::Returning: C = Style.Rich; Label = TEXT("Returning"); break;
		case EWorkerState::Home: C = Style.Home; Label = TEXT("Home"); break;
		case EWorkerState::BlownOff: C = StaminaLockedColor; Label = TEXT("Blown off"); break;
		}
		// Outline = ready, solid = out, inset = returning, underline = home, cross = blown off.
		const float T = Style.Stroke * Scale, MidY = PY + 10.f * Scale;
		if (State == EWorkerState::Out) DrawRect(C, PX, MidY - Pip * 0.5f, Pip, Pip);
		else if (State == EWorkerState::Home) DrawRect(C, PX, MidY + Pip * 0.5f - T, Pip, T);
		else if (State == EWorkerState::BlownOff)
		{
			DrawLine(PX, MidY - Pip * 0.5f, PX + Pip, MidY + Pip * 0.5f, C, T);
			DrawLine(PX, MidY + Pip * 0.5f, PX + Pip, MidY - Pip * 0.5f, C, T);
		}
		else
		{
			if (LeafSkin::Get(LeafSkin::EKind::Group))
			{
				// Over a skin image the hollow cannot be painted in the panel colour: the outline as four strokes.
				const float PY0 = MidY - Pip * 0.5f;
				DrawRect(C, PX, PY0, Pip, T);
				DrawRect(C, PX, PY0 + Pip - T, Pip, T);
				DrawRect(C, PX, PY0 + T, T, Pip - 2.f * T);
				DrawRect(C, PX + Pip - T, PY0 + T, T, Pip - 2.f * T);
			}
			else
			{
				DrawRect(C, PX, MidY - Pip * 0.5f, Pip, Pip);
				DrawRect(Style.Panel, PX + T, MidY - Pip * 0.5f + T, Pip - 2.f * T, Pip - 2.f * T);
			}
			if (State == EWorkerState::Returning) DrawRect(C, PX + 5.f * Scale, MidY - 2.f * Scale, 4.f * Scale, 4.f * Scale);
		}
		DrawTextAt(FString::Printf(TEXT("%d %s"), i + 1, Label), PX + Pip + 6.f * Scale, PY, C, SecondaryText(Scale), Font);
	}
	if (const AAntPawn* Ant = Cast<AAntPawn>(GetOwningPawn())) DrawCarry(Ant, Scale);
	// Wind gauge, top right.
	if (const AWindSystem* Wind = D->GetWind())
	{
		const float GW = 260.f * Scale, GH = Style.Bar * Scale;
		const float GX = Canvas->ClipX - (Style.Edge + Style.Padding) * Scale - GW, GY = 90.f * Scale;
		const ALeaflinePlayerController* WindPC = Cast<ALeaflinePlayerController>(GetOwningPlayerController());
		const bool bMap = WindPC && WindPC->IsWindMapShown();
		// The bird's line (World/BirdSystem.*): only while it circles or dives in a shift; otherwise the panel is as before.
		const ABirdSystem* Bird = D->GetPhase() == EDayPhase::Shift ? ABirdSystem::Get(this) : nullptr;
		const bool bBirdLine = Bird && Bird->IsPresent();
		// How exposed the ant's spot is: the lee side of the trunk or a limb shelters it. Sampled before the panel,
		// which fits the rows drawn below.
		const APlayerController* PC = GetOwningPlayerController();
		const AAntPawn* Ant = PC ? Cast<AAntPawn>(PC->GetPawn()) : nullptr;
		const UAntMovementComponent* M = Ant ? Ant->GetAntMovement() : nullptr;
		ExposureTimer -= GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f;
		if (M && ExposureTimer <= 0.f)
		{
			ExposureTimer = 0.2f;
			ExposureShown = Wind->GetExposure(M->GetSurfacePosition(), M->GetSurfaceNormal());
		}
		const float Top = (Style.Edge + Style.Padding) * Scale, LineH = MeasureText(TEXT("Ag"), Font, SecondaryText(Scale)).Y;
		const float RowY = GY + GH + Style.Gap * Scale, LY = RowY + Style.Row * Scale;
		const FString MapHint = TEXT("Hold {Cycle}: wind map");
		float Bottom = GY + GH;
		if (M && ExposureShown >= 0.f) Bottom = RowY + LineH;
		if (M && D->GetPhase() == EDayPhase::Shift)
		{
			Bottom = bMap ? LY + 3.f * Style.Row * Scale + LineH : LY + MeasureText(MapHint, Font, SecondaryText(Scale)).Y;
			if (bBirdLine) Bottom = LY + ((bMap ? 4.f : 1.f) * Style.Row + 6.f) * Scale + LineH;
		}
		DrawPanel(GX - 42.f * Scale, Top, GW + 42.f * Scale, Bottom - Top, Scale);
		const EWindPhase Ph = Wind->GetPhase();
		const TCHAR* Label = Ph == EWindPhase::Calm ? TEXT("Wind: calm") : Ph == EWindPhase::Warning ? TEXT("Wind: gust coming")
			: Ph == EWindPhase::Falling ? TEXT("Wind: easing") : TEXT("Wind: GUST");
		const FLinearColor C = Wind->IsGustActive() ? StaminaLockedColor : (Ph == EWindPhase::Warning ? Style.Warning : TextColor);
		DrawTextAt(Label, GX, (Style.Edge + Style.Padding) * Scale, C, SecondaryText(Scale), Font);
		DrawRect(Style.Track, GX, GY, GW, GH);
		DrawRect(C, GX, GY, GW * FMath::Clamp(Wind->GetStrength(), 0.f, 1.f), GH);

		// Which way it blows, as seen from the camera: up on screen is away from you.
		if (PC && PC->PlayerCameraManager)
		{
			FVector F = PC->PlayerCameraManager->GetCameraRotation().Vector();
			F.Z = 0.0;
			F = F.GetSafeNormal();
			const FVector R(-F.Y, F.X, 0.0);
			const FVector WindDir = Wind->GetDirection().GetSafeNormal2D();
			FVector2D V(FVector::DotProduct(WindDir, R), -FVector::DotProduct(WindDir, F));
			if (!F.IsNearlyZero() && !V.IsNearlyZero())
			{
				V.Normalize();
				const float AR = 13.f * Scale;
				const FVector2D Ctr(GX - 30.f * Scale, GY + GH * 0.5f);
				const FVector2D Tip = Ctr + V * AR, Tail = Ctr - V * AR;
				const FVector2D Side(-V.Y, V.X);
				const float T = Style.Stroke * Scale;
				DrawLine(Tail.X, Tail.Y, Tip.X, Tip.Y, C, T);
				const FVector2D H1 = Tip - V * (AR * 0.6f) + Side * (AR * 0.45f), H2 = Tip - V * (AR * 0.6f) - Side * (AR * 0.45f);
				DrawLine(Tip.X, Tip.Y, H1.X, H1.Y, C, T);
				DrawLine(Tip.X, Tip.Y, H2.X, H2.Y, C, T);
			}
		}

		if (M && ExposureShown >= 0.f)
		{
			const bool bSheltered = ExposureShown < Style.ShelteredBelow, bPartly = ExposureShown < Style.PartlyBelow;
			const FLinearColor EC = bSheltered ? Style.Sheltered : (bPartly ? Style.Partly : StaminaLockedColor);
			const TCHAR* EL = bSheltered ? TEXT("You're sheltered") : (bPartly ? TEXT("You're partly exposed") : TEXT("You're exposed"));
			DrawTextAt(EL, GX, RowY, EC, SecondaryText(Scale), Font);
		}
		// The wind map (hold Tab / LB): its legend while shown, otherwise a quiet reminder that it exists (in play only:
		// in the harvest Tab steps the views).
		if (M && D->GetPhase() == EDayPhase::Shift)
		{
			if (bMap)
			{
				DrawCrisp(Canvas, TEXT("Wind map"), TextColor, GX, LY, Font, SecondaryText(Scale));
				DrawCrisp(Canvas, TEXT("Green: sheltered"), Style.Sheltered, GX, LY + Style.Row * Scale, Font, SecondaryText(Scale));
				DrawCrisp(Canvas, TEXT("Amber: partly exposed"), Style.Partly, GX, LY + 2.f * Style.Row * Scale, Font, SecondaryText(Scale));
				DrawCrisp(Canvas, TEXT("Red: exposed"), StaminaLockedColor, GX, LY + 3.f * Style.Row * Scale, Font, SecondaryText(Scale));
			}
			else
			{
				DrawTextAt(MapHint, GX, LY, Style.Muted, SecondaryText(Scale), Font);
			}
			if (bBirdLine)
			{
				// Plain text colour: green, amber and red in this panel mean exposure.
				DrawTextAt(Bird->IsDiving() ? TEXT("Bird diving!") : TEXT("Bird circling"), GX, LY + ((bMap ? 4.f : 1.f) * Style.Row + 6.f) * Scale,
					TextColor, SecondaryText(Scale), Font);
			}
		}
	}
}

namespace
{
	/** The piece's value multiplier as shown beside its weight ("\u00D71.6"): the leaf's Richness, as the haul uses it
	 *  (AAntPawn::GetCarriedValue), so weight times it is the "+N" on arrival. Hidden when it rounds to \u00D71.0. */
	bool ShowsMultiplier(float Richness) { return FMath::RoundToInt(Richness * 10.f) > 10; }
	FString MultiplierText(float Richness) { return FString::Printf(TEXT("\u00D7%.1f"), Richness); }
	/** The multiplier beside the carry read-out's number: a little smaller than the HUD's secondary text. */
	float MultiplierTextScale(float Scale) { return ScaleForHeight(Style.SecondaryHeight * 0.85f * Scale); }
}

void ALeaflineHUD::DrawCarry(const AAntPawn* Ant, float Scale)
{
	if (GHudIcons)
	{
		DrawCarryIcons(Ant, Scale);
		return;
	}
	// The day panel's third row, from its left edge; the size bar as wide as the panel (440 at least, as in DrawDayInfo).
	const float X = (Style.Edge + Style.Padding) * Scale, Y = (Style.Edge + Style.Padding + 84.f) * Scale;
	const FString CarryText = Ant->IsCarrying() ? FString::Printf(TEXT("Carrying %.1f"), Ant->GetCarriedMass()) : FString(TEXT("Carrying nothing"));
	DrawTextAt(CarryText, X, Y, TextColor, SecondaryText(Scale), GEngine->GetMediumFont());
	if (Ant->IsCarrying() && ShowsMultiplier(Ant->GetCarriedRichness()))
	{
		const float MX = X + MeasureText(CarryText, GEngine->GetMediumFont(), SecondaryText(Scale)).X + 6.f * Scale;
		DrawTextAt(MultiplierText(Ant->GetCarriedRichness()), MX, Y + 2.f * Scale, Style.Rich, MultiplierTextScale(Scale), GEngine->GetMediumFont());
	}
	const float W = 440.f * Scale, H = Style.Bar * Scale, BY = Y + Style.Row * Scale;
	DrawRect(Style.Track, X, BY, W, H);
	if (Ant->IsCarrying()) DrawRect(Style.Leaf, X, BY, W * FMath::Clamp(Ant->GetCarriedSize(), 0.f, 1.f), H);
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
		if (OutRatio >= 1.f) return Style.CutDanger;
		if (OutRatio >= Risky) return Style.Warning;
		return Style.CutSafe;
	};
	DrawArc(CX, CY, R, 0.f, Ant->GetCutCap(), Style.Track, Style.Bar * Scale);
	const int32 Steps = 24;
	float Ratio = 0.f;
	for (int32 i = 0; i < Steps; ++i)
	{
		const float A = Size * i / Steps, B = Size * (i + 1) / Steps;
		DrawArc(CX, CY, R, A, FMath::Max(B, A + 0.002f), RiskColor(B, Ratio), Style.Bar * Scale);
	}
	const FLinearColor Now = RiskColor(Size, Ratio);
	// The piece's weight, and under it the leaf's value multiplier when above \u00D71.0 (weight times it arrives as "+N").
	const FString Weight = FString::Printf(TEXT("%.1f"), Ant->MassForSize(Size));
	const ATreeActor* CutTree = ATreeActor::Get(this);
	const int32 CutLeaf = Ant->GetCuttingLeaf();
	const float Richness = CutTree && CutLeaf != INDEX_NONE && CutLeaf < CutTree->GetLeaves().Num()
		? FMath::Max(CutTree->GetLeaves().Get(CutLeaf).Richness, 1.f) : 1.f; // as AAntPawn sets CarriedRichness
	if (!ShowsMultiplier(Richness))
	{
		DrawTextPanel(Weight, CX, CY - Style.PrimaryHeight * 0.5f * Scale, TextColor, PrimaryText(Scale), Scale);
	}
	else
	{
		UFont* Font = GEngine->GetMediumFont();
		const FString Mul = MultiplierText(Richness);
		const float NumH = Style.PrimaryHeight * Scale, MulH = Style.SecondaryHeight * Scale, G = 2.f * Scale;
		const float Top = CY - 0.5f * (NumH + G + MulH);
		const float W = FMath::Max(float(MeasureText(Weight, Font, PrimaryText(Scale)).X), float(MeasureText(Mul, Font, SecondaryText(Scale)).X));
		DrawPanel(CX - 0.5f * W, Top, W, NumH + G + MulH, Scale, 1.f, LeafSkin::EKind::Tag);
		DrawTextCentered(Weight, CX, Top, TextColor, PrimaryText(Scale), Font);
		DrawTextCentered(Mul, CX, Top + NumH + G, Style.Rich, SecondaryText(Scale), Font);
	}
	// The label judges the size being cut as the arc grows (good, getting big, too big), not the ant or the leaf.
	const TCHAR* Label = Ratio >= 1.f ? TEXT("Too big: gusts blow it off exposed bark")
		: (Wind && Ratio >= Wind->Tuning.RiskyRatio ? TEXT("Getting big: risky in gusts") : TEXT("Good size: safe in gusts"));
	DrawTextPanel(Label, CX, CY + R + 28.f * Scale, Now, SecondaryText(Scale), Scale);
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
	for (const FDeliveryPop& P : Lines->GetRecentDeliveries())
	{
		const float T = float(Now - P.RealTime) / Life;
		if (T < 0.f || T > 1.f) continue;
		if (FVector::DotProduct(P.WorldPos - CamPos, CamFwd) < 10.0) continue; // behind the camera
		const FVector S = Canvas->Project(P.WorldPos);
		// Rises with an ease-out, holds, then fades over the last 40%.
		const float Rise = 80.f * Scale * (1.f - FMath::Square(1.f - T));
		const float A = FMath::Clamp((1.f - T) / 0.4f, 0.f, 1.f);
		const float Size = PrimaryText(Scale);
		const FString Text = P.Mass >= 10.f ? FString::Printf(TEXT("+%.0f"), P.Mass) : FString::Printf(TEXT("+%.1f"), P.Mass);
		DrawTextPanel(Text, S.X, S.Y - Rise, FLinearColor(Style.Delivery.R, Style.Delivery.G, Style.Delivery.B, A), Size, Scale, A);
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
	if (GHudIcons)
	{
		DrawColonyIcons(D, Scale);
		return;
	}
	UFont* Font = GEngine->GetMediumFont();
	const float CX = Canvas->ClipX * 0.5f, Y = (Style.Edge + Style.Padding) * Scale;
	const FString Total = FString::Printf(TEXT("%s hauled in all"), *FString::FormatAsNumber(FMath::RoundToInt(ColonyShown)));
	const FString Today = FString::Printf(TEXT("+%.1f today"), D->GetStats().MassHauled);
	const float W = FMath::Max(float(MeasureText(Total, Font, PrimaryText(Scale)).X), float(MeasureText(Today, Font, SecondaryText(Scale)).X));
	DrawPanel(CX - W * 0.5f, Y, W, (Style.PrimaryHeight + Style.Gap + Style.SecondaryHeight) * Scale, Scale);
	DrawTextCentered(Total, CX, Y, TextColor, PrimaryText(Scale), Font);
	DrawTextCentered(Today, CX, Y + (Style.PrimaryHeight + Style.Gap) * Scale, AccentColor, SecondaryText(Scale), Font);
}

namespace
{
	/** A trail row's "5 pieces left" (singular at one, "Used up" at none). Counted as the bar is (FColonyLine::Fullness):
	 *  pieces still on the leaves, including those recruits are on their way to cut. */
	FString PiecesLeftText(int32 N)
	{
		return N <= 0 ? FString(TEXT("Used up")) : N == 1 ? FString(TEXT("1 piece left")) : FString::Printf(TEXT("%d pieces left"), N);
	}

	/** The leaf read-out's value words: the patch's mean richness over the tree's multiplier, as the leaf colour has it
	 *  (paler is richer). A leaf at 70 m on tree 1 is about 1.1-1.3, at 120 m 1.3-1.7, at 200 m 1.5-2.3. bSteep: the tree's
	 *  richness rises fast above the lowest leaves (FTreeParams::RichnessHeightScale > 0), so the words start higher
	 *  (1.4 and 2.0) and the lowest layers stay plain. */
	const TCHAR* ValueWords(float Richness, float TreeMul, bool bSteep = false)
	{
		const float Base = Richness / FMath::Max(TreeMul, 0.01f);
		const float More = bSteep ? 1.4f : 1.25f, Much = bSteep ? 2.f : 1.6f;
		return Base >= Much ? TEXT(", each worth much more") : (Base >= More ? TEXT(", each worth more") : TEXT(""));
	}
}

void ALeaflineHUD::DrawLineRows(const ADayDirector* D, const ALineSystem* Lines, float Scale)
{
	if (!Lines || Lines->GetLines().Num() == 0) return;
	if (D->GetPhase() != EDayPhase::Shift && D->GetPhase() != EDayPhase::Harvest) return;
	UFont* Font = GEngine->GetMediumFont();
	const float X = (Style.Edge + Style.Padding) * Scale;
	float Y = 232.f * Scale;
	const float RowH = 64.f * Scale, W = 440.f * Scale;
	const bool bIdle = D->GetPhase() == EDayPhase::Shift && Lines->IsActive() && Lines->NumIdle() > 0;
	DrawPanel(X, Y, W, (Lines->GetLines().Num() * 64.f - Style.Gap + (bIdle ? Style.Row : 0.f)) * Scale, Scale);
	for (const FColonyLine& L : Lines->GetLines())
	{
		const float Dim = L.IsLive() ? 1.f : 0.7f;
		const FLinearColor Text(TextColor.R, TextColor.G, TextColor.B, Dim);
		const FLinearColor LC = Lines->GetLineColor(L.Number, 0.3f); // the line's own colour, as on its trail
		DrawRect(FLinearColor(LC.R, LC.G, LC.B, Dim), X - 10.f * Scale, Y + 3.f * Scale, 4.f * Scale, 50.f * Scale);
		DrawTextAt(FString::Printf(TEXT("Trail %d"), L.Number), X, Y, FLinearColor(LC.R, LC.G, LC.B, Dim), SecondaryText(Scale), Font);
		DrawTextAt(FString::Printf(TEXT("Home %d   Lost %d"), L.PiecesHome, L.PiecesLost), X + 115.f * Scale, Y,
			L.PiecesLost > 0 ? FLinearColor(StaminaLockedColor.R, StaminaLockedColor.G, StaminaLockedColor.B, Dim) : Text, SecondaryText(Scale), Font);
		const float BY = Y + Style.Row * Scale, BW = 100.f * Scale, BH = Style.Bar * Scale;
		DrawRect(Style.Track, X, BY + 5.f * Scale, BW, BH);
		DrawRect(FLinearColor(Style.Leaf.R, Style.Leaf.G, Style.Leaf.B, Dim), X, BY + 5.f * Scale, BW * L.Fullness(), BH);
		DrawTextAt(PiecesLeftText(L.PatchRemaining + L.Reserved),
			X + 115.f * Scale, BY, FLinearColor(Style.Rich.R, Style.Rich.G, Style.Rich.B, Dim), SecondaryText(Scale), Font);
		Y += RowH;
	}
	if (bIdle)
	{
		const int32 N = Lines->NumIdle();
		DrawTextAt(FString::Printf(TEXT("%d recruit%s waiting for a new trail"), N, N == 1 ? TEXT("") : TEXT("s")), X, Y, Style.Partly, SecondaryText(Scale), Font);
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
		DrawTextPanel(TEXT("No leaf left here. Climb higher for fresh leaves."), Canvas->ClipX * 0.5f, Canvas->ClipY - 250.f * Scale, Style.Rich, SecondaryText(Scale), Scale);
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
	// Coloured as the trail it would become, so the trail is recognisable once laid. "These leaves" are the outlined ones;
	// nothing about recruits, since a fresh colony has none and the read-out shows anyway.
	const ATreeActor* PatchTree = ATreeActor::Get(this);
	const float TreeMul = PatchTree ? PatchTree->GetLeaves().GetRichnessMul() : 1.f;
	const TCHAR* Where = P.Leaves.Num() > 1 ? TEXT("these leaves") : TEXT("this leaf");
	const FString Text = P.Pieces == 1 ? FString::Printf(TEXT("1 piece left on %s"), Where)
		: FString::Printf(TEXT("About %d pieces on %s%s"), P.Pieces, Where, ValueWords(P.Richness, TreeMul, PatchTree && PatchTree->Params.RichnessHeightScale > 0.f));
	DrawTextPanel(Text, Canvas->ClipX * 0.5f, Canvas->ClipY - 250.f * Scale, Lines->GetLineColor(Lines->GetNextLineNumber(), 0.3f), SecondaryText(Scale), Scale);
}

void ALeaflineHUD::DrawHarvest(const ADayDirector* D, const ALineSystem* Lines, float Scale)
{
	UFont* Font = GEngine->GetMediumFont();
	const float CX = Canvas->ClipX * 0.5f, Y = 176.f * Scale;
	const bool bHomeOnly = D->IsHomeOnlyHarvest();
	const FString Title = bHomeOnly ? TEXT("Fast-forward: the last workers head home") : TEXT("Harvest: your recruits bring in the rest");
	FString Speed = D->IsSkippingHarvest() ? FString(TEXT("Skipping...")) : FString::Printf(TEXT("Speed \u00D7%d"), FMath::RoundToInt(Lines ? Lines->GetSpeedFactor() : 1.f));
	if (Lines && !bHomeOnly) Speed += FString::Printf(TEXT("   Recruits out: %d   Trips left: %d"), Lines->NumOut(), Lines->TripsLeft());
	const float W = FMath::Max(float(MeasureText(Title, Font, PrimaryText(Scale)).X), float(MeasureText(Speed, Font, SecondaryText(Scale)).X));
	DrawPanel(CX - W * 0.5f, Y, W, (Style.PrimaryHeight + Style.Gap + Style.SecondaryHeight) * Scale, Scale);
	DrawTextCentered(Title, CX, Y, AccentColor, PrimaryText(Scale), Font);
	DrawTextCentered(Speed, CX, Y + (Style.PrimaryHeight + Style.Gap) * Scale, TextColor, SecondaryText(Scale), Font);
	const bool bLineViews = Lines && Lines->NumLiveLines() > 0;
	if (!D->IsSkippingHarvest()) DrawTextPanel(bLineViews ? TEXT("{Grip}: skip      {Cycle}: next trail") : TEXT("{Grip}: skip"),
		CX, Canvas->ClipY - 190.f * Scale, TextColor, SecondaryText(Scale), Scale);
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
		const ABirdSystem* Bird = D && D->GetPhase() == EDayPhase::Shift ? ABirdSystem::Get(this) : nullptr;
		const bool bCanDrop = Bird && Bird->CanDrop(Ant);
		if (S == EAntState::Falling) Prompt = TEXT("Blown off! The piece is lost");
		else if (Bird && Bird->IsDivingAt(Ant) && S == EAntState::Gripping) Prompt = TEXT("Gripping. Hold on until the bird has passed");
		else if (Bird && Bird->IsDivingAt(Ant) && (S == EAntState::Walking || S == EAntState::Riding || S == EAntState::Cutting))
			Prompt = bCanDrop ? TEXT("Bird diving! Hold {Grip} to grip      {Drop}: drop your piece") : TEXT("Bird diving! Hold {Grip} to grip");
		else if (S == EAntState::WalkingHome) Prompt = TEXT("Walking home empty-handed.   {Cycle}: switch ant");
		else if (S == EAntState::Gripping) Prompt = TEXT("Gripping. Release {Grip} when the gust passes");
		else if (bGustSoon && (S == EAntState::Walking || S == EAntState::Riding || S == EAntState::Cutting))
		{
			// Sheltered spots need no grip; elsewhere say what helps: grip, or the side away from the arrow.
			Prompt = ExposureShown >= 0.f && ExposureShown < Style.ShelteredBelow
				? (Wind->GetPhase() == EWindPhase::Warning ? TEXT("Gust coming: you're sheltered here") : TEXT("Gust! You're sheltered here"))
				: (Wind->GetPhase() == EWindPhase::Warning ? TEXT("Gust coming: hold {Grip} to grip, or move to the sheltered side")
				                                           : TEXT("Gust! Hold {Grip} to grip"));
		}
		else if (S == EAntState::Returning) Prompt = TEXT("Heading home on the trail.   {Cycle}: switch ant");
		else if (Ant->IsCutting()) Prompt = TEXT("Release to finish the cut");
		else if (Ant->IsCarrying()) Prompt = bCanDrop ? TEXT("{Send}: send home      {Drop}: drop your piece for the bird")
			: TEXT("{Send}: send it home, or carry it to the nest yourself");
		else if (Ant->GetLeafInReach() != INDEX_NONE) Prompt = TEXT("Hold {Cut}: cut a piece");
		else if (D && S == EAntState::Walking && M && D->IsNearNest(M->GetSurfacePosition()) && D->GetWorkerState(Ant->GetWorkerIndex()) == EWorkerState::Out && Ant->GetPathIndex() != INDEX_NONE && D->GetTrails() && D->GetTrails()->GetPoints(Ant->GetPathIndex()).Num() > 30)
			Prompt = TEXT("{Send}: end this worker's trip");
		else if (S == EAntState::Riding) Prompt = TEXT("Riding the trail.   Move to step off");
		else if (Ant->IsTrailInReach()) Prompt = TEXT("Hold {Follow}: ride the trail");
	}
	else if (D && D->GetPhase() == EDayPhase::AllHome)
	{
		Prompt = TEXT("Everyone is home");
	}
	if (Prompt.IsEmpty()) return;
	DrawTextPanel(Prompt, Canvas->ClipX * 0.5f, Canvas->ClipY - 190.f * Scale, TextColor, SecondaryText(Scale), Scale, 1.f, 1200.f);
}

void ALeaflineHUD::DrawRecap(const ADayDirector* D, float Scale)
{
	UFont* Font = GEngine->GetMediumFont();
	const ULeaflineGameInstance* GI = Cast<ULeaflineGameInstance>(GetGameInstance());
	const ALineSystem* LineSys = D->GetLineSystem();
	const FDayStats& S = D->GetStats();
	const bool bShowLines = LineSys && LineSys->GetLines().Num() > 0;
	const bool bMove = D->IsOneTree() && D->IsMoveDue();
	const int32 Shown = bShowLines ? FMath::Min(LineSys->GetLines().Num(), 8) : 0;
	const bool bMore = bShowLines && LineSys->GetLines().Num() > Shown;
	TArray<FString> Rows = {
		FString::Printf(TEXT("Pieces brought home: %d"), S.Fragments),
		FString::Printf(TEXT("Largest piece: %.1f"), S.LargestFragment),
		FString::Printf(TEXT("Workers blown off: %d"), S.BlownOff),
		FString::Printf(TEXT("Trail laid: %.0f m"), S.TrailLaid / 100.f),
	};
	const ABirdSystem* Bird = ABirdSystem::Get(this);
	const bool bBirdRow = Bird && Bird->IsBirdOn() && Bird->IsActiveToday();
	const int32 RecruitWindLost = FMath::Max(S.RecruitLost - (bBirdRow ? Bird->GetRecruitPiecesTaken() : 0), 0);
	if (bShowLines && D->AreRecruitsOn()) Rows.Insert(FString::Printf(TEXT("Recruits: %d pieces home, %d lost to the wind"), S.RecruitPieces, RecruitWindLost), 1);
	if (bBirdRow) Rows.Add(FString::Printf(TEXT("Pieces lost to the bird: %d"), Bird->GetPiecesTaken()));
	const float CX = Canvas->ClipX * 0.5f, W = Style.CardWidth * Scale;
	// The main number (leaf hauled) large, the best-day line under it, then the detail rows closer together and dimmer.
	// The panel fits what is drawn: the same steps as below.
	const float HeroText = ScaleForHeight(Style.HeroHeight * Scale);
	const float HeroH = MeasureText(TEXT("Ag"), Font, HeroText).Y, LineH = MeasureText(TEXT("Ag"), Font, SecondaryText(Scale)).Y;
	const float HeroBlock = HeroH + 6.f * Scale + LineH + 2.f * Style.Gap * Scale;
	const float DetailRow = (Style.Row - 4.f) * Scale;
	const FString Prompt = TEXT("Press {Confirm}{orclick} to go into the nest");
	const float MeterH = (GI ? (GI->IsOneTree() ? Style.MeterBlock : Style.MeterBlockShort) : 0.f) * Scale;
	const float H = (96.f + (Shown + (bMore ? 1 : 0)) * Style.Row + Style.Gap) * Scale + HeroBlock
		+ Rows.Num() * DetailRow + Style.Gap * Scale + MeterH + (bMove ? Style.Row * Scale : 0.f) + MeasureText(Prompt, Font, SecondaryText(Scale)).Y;
	float Y = FMath::RoundToFloat(FMath::Max((Style.Edge + Style.Padding) * Scale, (Canvas->ClipY - H) * 0.5f));
	DrawPanel(CX - W * 0.5f, Y, W, H, Scale, 1.f, LeafSkin::EKind::Card);

	// Reserve the whole layout while rows reveal, so the totals and continue prompt stay anchored.
	const float Dt = GetWorld() ? FMath::Min(GetWorld()->GetDeltaSeconds(), 0.1f) : 0.016f;
	if (!bWasRecap) RecapTime = 0.f;
	bWasRecap = true;
	RecapTime += Dt;
	auto Count01 = [&](float Start, float Dur) { return FMath::Clamp((RecapTime - Start) / Dur, 0.f, 1.f); };
	DrawTextCentered(FString::Printf(TEXT("Day %d: today's cut from %s"), S.Day, *D->GetTreeName()), CX, Y, AccentColor, PrimaryText(Scale), Font);
	if (D->IsOneTree()) DrawTextCentered(FString::Printf(TEXT("Tree %d: day %d on this tree"), D->GetStage() + 1, D->GetDayOnTree()),
		CX, Y + (Style.PrimaryHeight + Style.Gap) * Scale, TextColor, SecondaryText(Scale), Font);
	Y += 96.f * Scale;

	constexpr float RowGap = 0.6f, RowCount = 0.5f, FirstRow = 0.4f;
	float TotalStart = 0.3f;
	if (bShowLines)
	{
		const TArray<FColonyLine>& All = LineSys->GetLines();
		const float X = CX - 480.f * Scale;
		for (int32 i = 0; i < Shown; ++i)
		{
			const FColonyLine& L = All[i];
			const float Start = FirstRow + i * RowGap;
			if (RecapTime < Start) continue;
			const float K = Count01(Start, RowCount);
			const float Fade = FMath::Clamp((RecapTime - Start) / 0.2f, 0.f, 1.f);
			const float RY = Y + i * Style.Row * Scale;
			const FLinearColor T(TextColor.R, TextColor.G, TextColor.B, Fade);
			const FLinearColor LC = LineSys->GetLineColor(L.Number, 0.3f);
			DrawTextAt(FString::Printf(TEXT("Trail %d"), L.Number), X, RY, FLinearColor(LC.R, LC.G, LC.B, Fade), SecondaryText(Scale), Font);
			DrawTextAt(FString::Printf(TEXT("Home %d"), FMath::RoundToInt(L.PiecesHome * K)), X + 140.f * Scale, RY, T, SecondaryText(Scale), Font);
			DrawTextAt(FString::Printf(TEXT("Lost %d"), FMath::RoundToInt(L.PiecesLost * K)), X + 330.f * Scale, RY,
				L.PiecesLost > 0 ? FLinearColor(StaminaLockedColor.R, StaminaLockedColor.G, StaminaLockedColor.B, Fade) : T, SecondaryText(Scale), Font);
			DrawTextAt(FString::Printf(TEXT("Left %d"), FMath::Max(L.PatchRemaining + L.Reserved, 0)), X + 490.f * Scale, RY, T, SecondaryText(Scale), Font);
			DrawTextAt(FString::Printf(TEXT("Hauled %.1f"), L.MassHome * K), X + 750.f * Scale, RY,
				FLinearColor(Style.Delivery.R, Style.Delivery.G, Style.Delivery.B, Fade), SecondaryText(Scale), Font);
		}
		Y += Shown * Style.Row * Scale;
		if (bMore)
		{
			DrawTextCentered(FString::Printf(TEXT("and %d more trails"), All.Num() - Shown), CX, Y, Style.Muted, SecondaryText(Scale), Font);
			Y += Style.Row * Scale;
		}
		TotalStart = FirstRow + Shown * RowGap + 0.2f;
	}
	Y += Style.Gap * Scale;
	const float TK = Count01(TotalStart, 0.9f);
	if (RecapTime >= TotalStart)
	{
		DrawTextCentered(FString::Printf(TEXT("Leaf hauled: %.1f"), S.MassHauled * TK), CX, Y, Style.Delivery, HeroText, Font);
		if (TK >= 1.f)
		{
			const FString Best = S.bNewBest ? FString(TEXT("Best day yet!"))
				: FString::Printf(TEXT("Best day so far: %.1f"), GI ? GI->GetBestDayHaul() : S.PreviousBest);
			DrawTextCentered(Best, CX, Y + HeroH + 6.f * Scale, S.bNewBest ? Style.Rich : Style.Muted, SecondaryText(Scale), Font);
		}
	}
	Y += HeroBlock;
	for (const FString& Row : Rows)
	{
		DrawTextCentered(Row, CX, Y, Style.Detail, SecondaryText(Scale), Font);
		Y += DetailRow;
	}
	Y += Style.Gap * Scale;
	DrawColonyMeter(CX, Y, 640.f * Scale, Scale);
	Y += MeterH;
	if (bMove)
	{
		DrawTextCentered(TEXT("Tomorrow the colony moves to a new tree"), CX, Y, Style.Rich, SecondaryText(Scale), Font);
		Y += Style.Row * Scale;
	}
	DrawTextCentered(Prompt, CX, Y, AccentColor, SecondaryText(Scale), Font);
}

void ALeaflineHUD::DrawColonyMeter(float CX, float Y, float W, float Scale)
{
	const ULeaflineGameInstance* GI = Cast<ULeaflineGameInstance>(GetGameInstance());
	if (!GI) return;
	UFont* Medium = GEngine->GetMediumFont();
	const bool bOneTree = GI->IsOneTree();
	const float Top = GI->GetMilestoneThreshold(LeaflineUpgrades::MilestoneCount - 1);
	const float H = Style.Bar * Scale, X = CX - W * 0.5f;
	DrawTextCentered(FString::Printf(TEXT("Colony growth: %.1f hauled in all"), GI->GetTotalHauled()), CX, Y, TextColor, SecondaryText(Scale), Medium);
	const float BarY = Y + 42.f * Scale;
	DrawRect(Style.Track, X, BarY, W, H);
	DrawRect(Style.Leaf, X, BarY, W * FMath::Clamp(GI->GetTotalHauled() / Top, 0.f, 1.f), H);
	for (int32 m = 0; m < LeaflineUpgrades::MilestoneCount; ++m)
	{
		const float T = GI->GetMilestoneThreshold(m);
		const float MX = X + W * T / Top;
		const bool bReached = GI->HasReachedMilestone(m);
		const FLinearColor MarkColor = bReached ? AccentColor : TextColor;
		DrawRect(MarkColor, MX - 1.5f * Scale, BarY - 6.f * Scale, 3.f * Scale, H + 12.f * Scale);
		DrawTextCentered(FString::Printf(TEXT("%.0f"), T), MX, BarY + H + Style.Gap * Scale, MarkColor, SecondaryText(Scale), Medium);
		// With one tree on, the second and third milestones move the colony: a small tree above the mark and a label.
		if (bOneTree && m >= 1)
		{
			const float TW = 7.f * Scale, TY = BarY - 6.f * Scale;
			DrawRect(MarkColor, MX - 1.f * Scale, TY - 4.f * Scale, 2.f * Scale, 5.f * Scale);                    // trunk
			DrawRect(MarkColor, MX - TW * 0.5f, TY - 11.f * Scale, TW, 8.f * Scale);                              // crown
			DrawTextCentered(TEXT("new tree"), MX, BarY + H + (Style.Gap + Style.Row) * Scale, MarkColor, SecondaryText(Scale), Medium);
		}
	}
}

void ALeaflineHUD::DrawTitle(const ADayDirector* D, float Scale)
{
	UFont* Font = GEngine->GetMediumFont();
	const float CX = Canvas->ClipX * 0.5f, W = Style.CardWidth * Scale;
	const FString Intro1 = TEXT("You are a leafcutter ant on an enormous tree.");
	const FString Intro2 = TEXT("Cut a piece of leaf, send the worker home along your scent trail, and take over the next one.");
	const TArray<FString> Controls = {
		TEXT("{Move} move      {Look} look      {Sprint} sprint"),
		TEXT("{Cut} cut (hold)      {Send} send home      {Follow} ride a trail (hold)"),
		TEXT("{Grip} grip in a gust (hold)      {Cycle} switch ant (tap), wind map (hold)"),
		TEXT("{Pause} pause"),
	};
	const FString CreditsLine = TEXT("{Back}: credits");
	const FVector2D Size = MeasureText(CreditsLine, Font, SecondaryText(Scale));
	// The panel fits what is drawn (pad prompts are wider and can wrap a controls row): the same steps as below.
	float H = 74.f * Scale + WrappedHeight(Intro1, W, SecondaryText(Scale), Scale) + Style.Gap * Scale + WrappedHeight(Intro2, W, SecondaryText(Scale), Scale) + 44.f * Scale;
	for (const FString& C : Controls) H += WrappedHeight(C, W, SecondaryText(Scale), Scale) + Style.Padding * Scale;
	H += (28.f + 62.f) * Scale + Size.Y;
	float Y = 220.f * Scale;
	DrawPanel(CX - W * 0.5f, Y, W, H, Scale, 1.f, LeafSkin::EKind::Card);
	DrawTextCentered(TEXT("Leafline"), CX, Y, AccentColor, PrimaryText(Scale), Font);
	Y += 74.f * Scale;
	Y += DrawWrappedText(Intro1, CX, Y, W, TextColor, SecondaryText(Scale), Scale) + Style.Gap * Scale;
	Y += DrawWrappedText(Intro2, CX, Y, W, TextColor, SecondaryText(Scale), Scale) + 44.f * Scale;
	for (const FString& C : Controls)
	{
		Y += DrawWrappedText(C, CX, Y, W, Style.Muted, SecondaryText(Scale), Scale) + Style.Padding * Scale;
	}
	Y += 28.f * Scale;
	DrawTextCentered(D->GetDay() > 1 ? FString::Printf(TEXT("Press {Confirm}{orclick} to start day %d"), D->GetDay()) : FString(TEXT("Press {Confirm}{orclick} to begin")),
		CX, Y, AccentColor, SecondaryText(Scale), Font);
	// A quiet way into the credits (the controller opens them on Back, or a click on this line).
	Y += 62.f * Scale;
	const float P = Style.Padding * Scale;
	AddMenuHit(CX - Size.X * 0.5f - P, Y - 0.5f * P, Size.X + 2.f * P, Size.Y + P, MenuCredits);
	DrawTextCentered(CreditsLine, CX, Y, IsHovered(MenuCredits) ? AccentColor : Style.Muted, SecondaryText(Scale), Font);
}

void ALeaflineHUD::DrawNest(const ADayDirector* D, float Scale)
{
	const ULeaflineGameInstance* GI = Cast<ULeaflineGameInstance>(GetGameInstance());
	UFont* Font = GEngine->GetMediumFont();
	const float CX = Canvas->ClipX * 0.5f, W = Style.CardWidth * Scale;
	const float NameText = ScaleForHeight(Style.NameHeight * Scale), DetailText = ScaleForHeight(Style.DetailHeight * Scale);
	const float RowW = 1000.f * Scale, RowH = 76.f * Scale, BoxH = RowH - Style.Gap * Scale, X = CX - RowW * 0.5f;
	const float HeadH = (Style.PrimaryHeight + Style.Gap) * Scale, SpendH = (Style.Row + 1.5f * Style.Gap) * Scale;
	const bool bMoveLine = D->IsOneTree() && D->IsMoveDue();
	const FString Hint = TEXT("{Choose}: choose      {Confirm}{orclick}: buy or start      {Back}: start the next day");

	// The panel fits what is drawn: the same steps as below (pad prompts are wider and can wrap the hint to two rows).
	int32 NumRows = 1; // the start-day row
	for (int32 i = 0; i < LeaflineUpgrades::Count; ++i) NumRows += D->IsNestRowVisible(i) ? 1 : 0;
	const float HintH = WrappedHeight(Hint, W, SecondaryText(Scale), Scale);
	const float MeterH = (GI ? (GI->IsOneTree() ? Style.MeterBlock : Style.MeterBlockShort) : 0.f) * Scale;
	const float H = HeadH + SpendH + NumRows * RowH + Style.Gap * Scale + MeterH + (bMoveLine ? Style.Row * Scale : 0.f) + HintH;
	const float Margin = (Style.Edge + Style.Padding) * Scale;
	float Y = FMath::RoundToFloat(FMath::Max(Margin, (Canvas->ClipY - H) * 0.5f));
	DrawPanel(CX - W * 0.5f, Y, W, H, Scale, 1.f, LeafSkin::EKind::Card);

	DrawTextCentered(TEXT("The nest"), CX, Y, AccentColor, PrimaryText(Scale), Font);
	Y += HeadH;
	if (GI) DrawTextCentered(FString::Printf(TEXT("Leaf to spend: %.1f"), GI->GetLeafMass()), CX, Y, TextColor, SecondaryText(Scale), Font);
	Y += SpendH;
	const float NameH = MeasureText(TEXT("Ag"), Font, NameText).Y, DetailH = MeasureText(TEXT("Ag"), Font, DetailText).Y;
	const float LineH = MeasureText(TEXT("Ag"), Font, SecondaryText(Scale)).Y;
	// Name and description as one block, centred in the row's box.
	const float NameY = 0.5f * (BoxH - (NameH + 4.f * Scale + DetailH)), DetailY = NameY + NameH + 4.f * Scale;
	const float CostRight = X + RowW - 2.f * Style.Padding * Scale;
	for (int32 i = 0; i <= LeaflineUpgrades::Count; ++i)
	{
		if (i < LeaflineUpgrades::Count && !D->IsNestRowVisible(i)) continue;
		AddMenuHit(X, Y, RowW, BoxH, i);
		const bool bSel = D->GetNestSelection() == i || IsHovered(i);
		DrawButtonBack(X, Y, RowW, BoxH, Scale, bSel);
		if (bSel) DrawRect(AccentColor, X, Y, Style.Stroke * Scale, BoxH);
		if (i < LeaflineUpgrades::Count)
		{
			const FLeaflineUpgradeInfo* Info = LeaflineUpgrades::Get(i);
			const int32 Level = GI ? GI->GetUpgradeLevel(i) : 0;
			const int32 Cost = GI ? GI->GetUpgradeCost(i) : 0;
			const bool bAfford = GI && GI->CanAfford(i);
			const bool bMaxed = Level >= LeaflineUpgrades::MaxLevel;
			DrawTextAt(Info->Name, X + Style.Padding * Scale, Y + NameY, bSel ? AccentColor : TextColor, NameText, Font);
			// On the lighter selection bar the dim colours lose contrast: the selected row uses the brighter ones.
			DrawTextAt(Info->Description, X + Style.Padding * Scale, Y + DetailY, bSel ? Style.Muted : Style.Detail, DetailText, Font);
			// Level pips: bought levels solid (gold once maxed), the rest dark with a faint edge (strokes, so they also
			// read over a skin image).
			const float PW = 18.f * Scale, PH = Style.Bar * Scale, PY = Y + 0.5f * (BoxH - PH), T = FMath::Max(1.f, FMath::RoundToFloat(Scale));
			for (int32 l = 0; l < LeaflineUpgrades::MaxLevel; ++l)
			{
				const float PX = X + 610.f * Scale + l * 30.f * Scale;
				if (l < Level)
				{
					DrawRect(bMaxed ? Style.Rich : AccentColor, PX, PY, PW, PH);
					continue;
				}
				DrawRect(Style.PipEmpty, PX, PY, PW, PH);
				DrawRect(Style.PipEdge, PX, PY, PW, T);
				DrawRect(Style.PipEdge, PX, PY + PH - T, PW, T);
				DrawRect(Style.PipEdge, PX, PY + T, T, PH - 2.f * T);
				DrawRect(Style.PipEdge, PX + PW - T, PY + T, T, PH - 2.f * T);
			}
			// The cost, right-aligned: accent when affordable, dimmed when not; maxed rows say so in gold.
			const FString CostText = Cost > 0 && !bMaxed ? FString::Printf(TEXT("Cost %d"), Cost) : FString(TEXT("Max"));
			const FLinearColor CostColor = Cost > 0 && !bMaxed ? (bAfford ? AccentColor : (bSel ? Style.Detail : Style.Unaffordable)) : Style.Rich;
			DrawTextAt(CostText, CostRight - MeasureText(CostText, Font, SecondaryText(Scale)).X, Y + 0.5f * (BoxH - LineH), CostColor, SecondaryText(Scale), Font);
		}
		else
		{
			const FString Start = D->IsOneTree() ? (D->IsMoveDue() ? FString::Printf(TEXT("Start day %d on a new tree"), D->GetDay() + 1) : FString::Printf(TEXT("Start day %d"), D->GetDay() + 1))
				: FString::Printf(TEXT("Start day %d: a new tree"), D->GetDay() + 1);
			DrawTextAt(Start, X + Style.Padding * Scale, Y + 0.5f * (BoxH - NameH), AccentColor, NameText, Font);
		}
		Y += RowH;
	}
	Y += Style.Gap * Scale;
	DrawColonyMeter(CX, Y, 640.f * Scale, Scale);
	Y += MeterH;
	if (bMoveLine)
	{
		DrawTextCentered(TEXT("Tomorrow the colony moves to a new tree"), CX, Y, Style.Rich, SecondaryText(Scale), Font);
		Y += Style.Row * Scale;
	}
	DrawWrappedText(Hint, CX, Y, W, Style.Muted, SecondaryText(Scale), Scale);
}

void ALeaflineHUD::DrawMoveCard(const ADayDirector* D, float Scale)
{
	UFont* Font = GEngine->GetMediumFont();
	const float CX = Canvas->ClipX * 0.5f, W = Style.CardWidth * Scale;
	const FString Heading = D->GetMoveCardText(), Body = TEXT("A new tree: taller and windier, and its leaves are worth more.");
	const FString Prompt = FString::Printf(TEXT("Press {Confirm}{orclick} to start day %d"), D->GetDay());
	// The panel fits what is drawn (the same steps as below).
	const float H = WrappedHeight(Heading, W, PrimaryText(Scale), Scale) + 44.f * Scale + WrappedHeight(Body, W, SecondaryText(Scale), Scale) + 62.f * Scale
		+ MeasureText(Prompt, Font, SecondaryText(Scale)).Y;
	float Y = 340.f * Scale;
	DrawPanel(CX - W * 0.5f, Y, W, H, Scale, 1.f, LeafSkin::EKind::Card);
	Y += DrawWrappedText(Heading, CX, Y, W, AccentColor, PrimaryText(Scale), Scale) + 44.f * Scale;
	Y += DrawWrappedText(Body, CX, Y, W, TextColor, SecondaryText(Scale), Scale) + 62.f * Scale;
	DrawTextCentered(Prompt, CX, Y, AccentColor, SecondaryText(Scale), Font);
}

void ALeaflineHUD::DrawEndCard(const ADayDirector* D, float Scale)
{
	UFont* Font = GEngine->GetMediumFont();
	const float CX = Canvas->ClipX * 0.5f, W = Style.CardWidth * Scale;
	const FString Line1 = TEXT("Enough leaf has come home to feed the fungus garden for a season.");
	const FString Line2 = TEXT("Thank you for playing Leafline. The trees keep growing, and so can the colony.");
	const FString Prompt = TEXT("Press {Confirm}{orclick} to keep playing");
	// The panel fits what is drawn (the same steps as below).
	const float H = 74.f * Scale + WrappedHeight(Line1, W, SecondaryText(Scale), Scale) + Style.Gap * Scale + WrappedHeight(Line2, W, SecondaryText(Scale), Scale)
		+ 62.f * Scale + MeasureText(Prompt, Font, SecondaryText(Scale)).Y;
	float Y = 320.f * Scale;
	DrawPanel(CX - W * 0.5f, Y, W, H, Scale, 1.f, LeafSkin::EKind::Card);
	DrawTextCentered(TEXT("The colony thrives"), CX, Y, AccentColor, PrimaryText(Scale), Font);
	Y += 74.f * Scale;
	Y += DrawWrappedText(Line1, CX, Y, W, TextColor, SecondaryText(Scale), Scale) + Style.Gap * Scale;
	Y += DrawWrappedText(Line2, CX, Y, W, TextColor, SecondaryText(Scale), Scale) + 62.f * Scale;
	DrawTextCentered(Prompt, CX, Y, AccentColor, SecondaryText(Scale), Font);
}

void ALeaflineHUD::DrawPause(float Scale)
{
	UFont* Font = GEngine->GetMediumFont();
	const float CX = Canvas->ClipX * 0.5f, W = Style.CardWidth * Scale;
	const ALeaflinePlayerController* PC = Cast<ALeaflinePlayerController>(GetOwningPlayerController());
	const int32 Page = PC ? PC->GetPausePage() : 0;
	const int32 Sel = PC ? PC->GetPauseRow() : 0;
	if (Page == ALeaflinePlayerController::CreditsPage)
	{
		DrawCredits(Scale);
		return;
	}
	if (Page == ALeaflinePlayerController::ControlsPage)
	{
		DrawControls(Scale);
		return;
	}
	float Y = (Page == 0 ? 170.f : 160.f) * Scale;
	const float RowH = Style.MenuRow * Scale, Gap = Style.Gap * Scale;
	// The two hint lines under the rows; the panel fits what is drawn (the same steps as below).
	const FString Hint1 = Page == 0 ? TEXT("{Choose}: choose      {Confirm}{orclick}: select") : TEXT("{Choose}: choose      {Change}{orclick}: change");
	const FString Hint2 = Page == 0 ? TEXT("{Pause}: resume      {Back}: quit") : TEXT("{Confirm}: change or back      {Back}: back");
	const int32 NumRows = Page == 0 ? ALeaflinePlayerController::NumPauseRows : ALeaflinePlayerController::NumOptionRows;
	const float H = 74.f * Scale + NumRows * (RowH + Gap) + Style.Gap * Scale + WrappedHeight(Hint1, W, SecondaryText(Scale), Scale) + Style.Padding * Scale
		+ WrappedHeight(Hint2, W, SecondaryText(Scale), Scale);
	DrawPanel(CX - W * 0.5f, Y, W, H, Scale, 1.f, LeafSkin::EKind::Card);
	DrawTextCentered(Page == 0 ? TEXT("Paused") : TEXT("Options"), CX, Y, AccentColor, PrimaryText(Scale), Font);
	Y += 74.f * Scale;
	if (Page == 0)
	{
		const float BW = 800.f * Scale;
		constexpr int32 N = ALeaflinePlayerController::NumPauseRows;
		// In the order of ALeaflinePlayerController's PauseRow constants.
		const TCHAR* Labels[N] = { TEXT("Resume"), TEXT("Controls"), TEXT("Options"), TEXT("Credits"), TEXT("Quit") };
		const int32 Ids[N] = { MenuResume, MenuControls, MenuOptions, MenuCredits, MenuQuit };
		for (int32 b = 0; b < N; ++b)
		{
			AddMenuHit(CX - BW * 0.5f, Y, BW, RowH, Ids[b]);
			const bool bOn = Sel == b || IsHovered(Ids[b]);
			DrawButtonBack(CX - BW * 0.5f, Y, BW, RowH, Scale, bOn);
			DrawTextCentered(Labels[b], CX, Y + Style.Padding * Scale, bOn ? AccentColor : TextColor, PrimaryText(Scale), Font);
			Y += RowH + Gap;
		}
		Y += Style.Gap * Scale;
		Y += DrawWrappedText(Hint1, CX, Y, W, TextColor, SecondaryText(Scale), Scale) + Style.Padding * Scale;
		DrawWrappedText(Hint2, CX, Y, W, Style.Muted, SecondaryText(Scale), Scale);
		return;
	}

	const float RW = 800.f * Scale, X = CX - RW * 0.5f;
	const TCHAR* Labels[ALeaflinePlayerController::NumOptionRows] = { TEXT("Mouse sensitivity"), TEXT("Invert look"), TEXT("Volume"), TEXT("Graphics"), TEXT("Back") };
	for (int32 r = 0; r < ALeaflinePlayerController::NumOptionRows; ++r)
	{
		const bool bBack = r == ALeaflinePlayerController::NumOptionRows - 1;
		AddMenuHit(X, Y, RW, RowH, MenuOptRow + r);
		const bool bOn = Sel == r || IsHovered(MenuOptRow + r) || IsHovered(MenuOptMinus + r) || IsHovered(MenuOptPlus + r);
		DrawButtonBack(X, Y, RW, RowH, Scale, bOn);
		if (bBack)
		{
			DrawTextCentered(Labels[r], CX, Y + Style.Padding * Scale, bOn ? AccentColor : TextColor, PrimaryText(Scale), Font);
		}
		else
		{
			DrawTextAt(Labels[r], X + Style.Padding * Scale, Y + Style.Padding * Scale, bOn ? AccentColor : TextColor, PrimaryText(Scale), Font);
			const float VX = X + RW * 0.72f, Arrow = 44.f * Scale;
			// Arrow boxes precede the whole-row box: MenuItemAt returns the first match.
			MenuHits.Insert({ FBox2D(FVector2D(VX - 100.f * Scale - Arrow * 0.5f, Y), FVector2D(VX - 100.f * Scale + Arrow * 0.5f, Y + RowH)), MenuOptMinus + r }, 0);
			MenuHits.Insert({ FBox2D(FVector2D(VX + 100.f * Scale - Arrow * 0.5f, Y), FVector2D(VX + 100.f * Scale + Arrow * 0.5f, Y + RowH)), MenuOptPlus + r }, 0);
			const bool bMinus = IsHovered(MenuOptMinus + r), bPlus = IsHovered(MenuOptPlus + r);
			DrawTextCentered(TEXT("<"), VX - 100.f * Scale, Y + Style.Padding * Scale, bMinus ? AccentColor : TextColor, PrimaryText(Scale), Font);
			DrawTextCentered(PC ? PC->GetOptionValueText(r) : FString(), VX, Y + Style.Padding * Scale, TextColor, PrimaryText(Scale), Font);
			DrawTextCentered(TEXT(">"), VX + 100.f * Scale, Y + Style.Padding * Scale, bPlus ? AccentColor : TextColor, PrimaryText(Scale), Font);
		}
		Y += RowH + Gap;
	}
	Y += Style.Gap * Scale;
	Y += DrawWrappedText(Hint1, CX, Y, W, TextColor, SecondaryText(Scale), Scale) + Style.Padding * Scale;
	DrawWrappedText(Hint2, CX, Y, W, Style.Muted, SecondaryText(Scale), Scale);
}

void ALeaflineHUD::ScrollCredits(float DesignPixels)
{
	CreditsScroll = FMath::Max(0.f, CreditsScroll + DesignPixels);
	if (CreditsMaxScroll >= 0.f) CreditsScroll = FMath::Min(CreditsScroll, CreditsMaxScroll);
}

void ALeaflineHUD::ResetCreditsScroll(float DesignPixels)
{
	CreditsScroll = DesignPixels < 0.f ? TNumericLimits<float>::Max() : DesignPixels;
	CreditsMaxScroll = -1.f;
	bCreditsJustOpened = true;
}

void ALeaflineHUD::DrawCredits(float Scale)
{
	UFont* Font = GEngine->GetMediumFont();
	const float CX = Canvas->ClipX * 0.5f, W = Style.CardWidth * Scale;
	const float Top = 110.f * Scale, H = 860.f * Scale;
	const float TextScale = SecondaryText(Scale);
	DrawPanel(CX - W * 0.5f, Top, W, H, Scale, 1.f, LeafSkin::EKind::Card);
	DrawTextCentered(TEXT("Credits"), CX, Top, AccentColor, PrimaryText(Scale), Font);

	// Bottom up: the prompt line, the Back button, then the window the rows scroll in.
	const float PromptH = MeasureText(TEXT("{Back}: back"), Font, TextScale).Y;
	const float PromptY = Top + H - PromptH;
	const float BackH = Style.MenuRow * Scale, BackY = PromptY - Style.Padding * Scale - BackH;
	const float ViewTop = Top + (Style.PrimaryHeight + Style.Gap) * Scale, ViewBottom = BackY - 2.f * Style.Gap * Scale;
	// The canvas cannot clip, so rows fade out over a band of one row at each edge of the window and are left out once
	// they reach it. The text starts and ends one band inside the window, so nothing is faded before scrolling.
	const float Step = Style.Row * Scale, Band = Style.Row * Scale;
	const float WrapWidth = W - 96.f * Scale; // clear of the scroll bar on the right

	struct FLine
	{
		FString Text;
		FLinearColor Color;
		float Y = 0.f; // from the top of the text
	};
	TArray<FLine> Lines;
	float ContentH = 0.f;
	const TArray<LeafCredits::FRow>& Rows = LeafCredits::Get();
	for (const LeafCredits::FRow& R : Rows)
	{
		if (R.Kind == LeafCredits::ERowKind::Gap)
		{
			ContentH += Step;
			continue;
		}
		const FLinearColor& Color = R.Kind == LeafCredits::ERowKind::Heading ? AccentColor : TextColor;
		for (const FString& Part : WrapText(R.Text, WrapWidth, TextScale))
		{
			Lines.Add({ Part, Color, ContentH });
			ContentH += Step;
		}
	}
	const float Visible = ViewBottom - ViewTop - 2.f * Band;
	const float MaxScroll = FMath::Max(0.f, ContentH - Visible) / Scale;
	CreditsMaxScroll = MaxScroll;
	CreditsScroll = FMath::Clamp(CreditsScroll, 0.f, MaxScroll);
	if (bCreditsJustOpened)
	{
		bCreditsJustOpened = false;
		CreditsScrollShown = CreditsScroll;
		UE_LOG(LogLeafline, Log, TEXT("Credits page: %d lines from %d rows, text %.0f px, window %.0f px, scroll %.0f of %.0f (design px), canvas %.0fx%.0f"),
			Lines.Num(), Rows.Num(), ContentH / Scale, Visible / Scale, CreditsScroll, MaxScroll, Canvas->ClipX, Canvas->ClipY);
	}
	else
	{
		CreditsScrollShown = FMath::FInterpTo(CreditsScrollShown, CreditsScroll, FApp::GetDeltaTime(), 16.f);
	}

	const float Origin = ViewTop + Band - CreditsScrollShown * Scale;
	for (const FLine& L : Lines)
	{
		const float Y = Origin + L.Y;
		const float A = FMath::Min((Y - ViewTop) / Band, (ViewBottom - (Y + Step)) / Band);
		if (A <= 0.01f) continue;
		const float Fade = FMath::Min(A, 1.f);
		DrawTextCentered(L.Text, CX, Y, FLinearColor(L.Color.R, L.Color.G, L.Color.B, L.Color.A * Fade), TextScale, Font);
	}
	if (MaxScroll > 0.f)
	{
		const float TrackW = 4.f * Scale, TrackX = CX + W * 0.5f - TrackW, TrackH = ViewBottom - ViewTop;
		const float ThumbH = FMath::Max(TrackH * Visible / FMath::Max(ContentH, 1.f), 24.f * Scale);
		DrawRect(Style.Track, TrackX, ViewTop, TrackW, TrackH);
		DrawRect(AccentColor, TrackX, ViewTop + (TrackH - ThumbH) * FMath::Clamp(CreditsScrollShown / MaxScroll, 0.f, 1.f), TrackW, ThumbH);
	}

	// Back is the only button, so it is always the selected one.
	const float BW = 800.f * Scale;
	AddMenuHit(CX - BW * 0.5f, BackY, BW, BackH, MenuPageBack);
	DrawButtonBack(CX - BW * 0.5f, BackY, BW, BackH, Scale, true);
	DrawTextCentered(TEXT("Back"), CX, BackY + Style.Padding * Scale, AccentColor, PrimaryText(Scale), Font);
	DrawTextCentered(MaxScroll > 0.f ? TEXT("{Choose}: scroll      {Back}: back") : TEXT("{Back}: back"), CX, PromptY, TextColor, TextScale, Font);
}

void ALeaflineHUD::DrawControls(float Scale)
{
	UFont* Font = GEngine->GetMediumFont();
	const float CX = Canvas->ClipX * 0.5f, W = Style.CardWidth * Scale;
	const float TextScale = SecondaryText(Scale);
	const ABirdSystem* Bird = ABirdSystem::Get(this);

	// The buttons come from the prompt tokens, so they show the device in use like every other prompt; PromptIcons.cpp
	// keeps the tokens in step with the bindings in ALeaflinePlayerController::CreateInput.
	struct FControlRow
	{
		const TCHAR* Keys;
		const TCHAR* Label;
	};
	TArray<FControlRow> Rows = {
		{ TEXT("{Move}"), TEXT("move") },
		{ TEXT("{Look}"), TEXT("look") },
		{ TEXT("{Sprint}"), TEXT("sprint") },
		{ TEXT("{Cut}"), TEXT("cut (hold)") },
		{ TEXT("{Send}"), TEXT("send home") },
		{ TEXT("{Follow}"), TEXT("ride a trail (hold)") },
		{ TEXT("{Grip}"), Bird && Bird->IsBirdOn() ? TEXT("grip in a gust or a bird dive (hold)") : TEXT("grip in a gust (hold)") },
		{ TEXT("{Cycle}"), TEXT("switch ant (tap), wind map (hold)") },
		{ TEXT("{Pause}"), TEXT("pause") },
	};
	if (Bird && Bird->IsBaitOn()) Rows.Add({ TEXT("{Drop}"), TEXT("drop your piece for the bird") });

	// Two columns centred as a block: the buttons right-aligned, the action beside them.
	const float LineH = MeasureText(TEXT("Ag"), Font, TextScale).Y;
	float KeysW = 0.f, LabelW = 0.f, RowH = LineH;
	for (const FControlRow& R : Rows)
	{
		const FVector2D K = MeasureText(R.Keys, Font, TextScale);
		KeysW = FMath::Max(KeysW, float(K.X));
		RowH = FMath::Max(RowH, float(K.Y));
		LabelW = FMath::Max(LabelW, float(MeasureText(R.Label, Font, TextScale).X));
	}
	const float ColGap = 2.f * Style.Gap * Scale;
	const float ColX = CX - 0.5f * (KeysW + ColGap + LabelW) + KeysW;
	const float Step = RowH + Style.Gap * Scale;
	const FString Device = UsingPad() ? TEXT("Showing the controller. Press a key to show the keyboard.")
		: TEXT("Showing keyboard and mouse. Press a controller button to show the controller.");
	const FString Prompt = TEXT("{Back}: back");
	const float PromptH = MeasureText(Prompt, Font, TextScale).Y;
	const float BackH = Style.MenuRow * Scale;

	// Title, the rows, the device line, Back and the prompt: the same steps as drawn below.
	const float H = (Style.PrimaryHeight + Style.Gap + Style.Row) * Scale + Rows.Num() * Step + 2.f * Style.Row * Scale
		+ BackH + Style.Padding * Scale + PromptH;
	float Y = FMath::RoundToFloat((Canvas->ClipY - H) * 0.5f);
	DrawPanel(CX - W * 0.5f, Y, W, H, Scale, 1.f, LeafSkin::EKind::Card);
	DrawTextCentered(TEXT("Controls"), CX, Y, AccentColor, PrimaryText(Scale), Font);
	Y += (Style.PrimaryHeight + Style.Gap + Style.Row) * Scale;
	for (const FControlRow& R : Rows)
	{
		DrawTextAt(R.Keys, ColX - MeasureText(R.Keys, Font, TextScale).X, Y, TextColor, TextScale, Font);
		DrawTextAt(R.Label, ColX + ColGap, Y + 0.5f * (RowH - LineH), TextColor, TextScale, Font);
		Y += Step;
	}
	Y += Style.Row * Scale - Style.Gap * Scale;
	DrawTextCentered(Device, CX, Y, Style.Muted, TextScale, Font);
	Y += Style.Row * Scale + Style.Gap * Scale;

	// Back is the only button, so it is always the selected one (as on the credits page).
	const float BW = 800.f * Scale;
	AddMenuHit(CX - BW * 0.5f, Y, BW, BackH, MenuPageBack);
	DrawButtonBack(CX - BW * 0.5f, Y, BW, BackH, Scale, true);
	DrawTextCentered(TEXT("Back"), CX, Y + Style.Padding * Scale, AccentColor, PrimaryText(Scale), Font);
	Y += BackH + Style.Padding * Scale;
	DrawTextCentered(Prompt, CX, Y, TextColor, TextScale, Font);
}

bool ALeaflineHUD::DrawIcon(const TCHAR* Name, float CX, float CY, float Size, const FLinearColor& Color, float AngleDeg)
{
	UTexture2D* Tex = LeafPrompts::Get(Name);
	if (!Tex || !Canvas) return false;
	FCanvasTileItem Tile(FVector2D(CX - 0.5f * Size, CY - 0.5f * Size), Tex->GetResource(), FVector2D(Size, Size), Color);
	Tile.BlendMode = SE_BLEND_Translucent;
	if (AngleDeg != 0.f)
	{
		Tile.Rotation = FRotator(0.f, AngleDeg, 0.f);
		Tile.PivotPoint = FVector2D(0.5f, 0.5f);
	}
	Canvas->DrawItem(Tile);
	return true;
}

void ALeaflineHUD::DrawWindIcons(const ADayDirector* D, const AWindSystem* Wind, float Scale)
{
	// Same panel, rows and colours as the text version; the left column (42 px) holds the icons, a short word beside each.
	UFont* Font = GEngine->GetMediumFont();
	const float Now = GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.f;
	const float GW = 260.f * Scale, GH = Style.Bar * Scale;
	const float GX = Canvas->ClipX - (Style.Edge + Style.Padding) * Scale - GW, GY = 90.f * Scale;
	const float Top = (Style.Edge + Style.Padding) * Scale;
	const float IconX = GX - 22.f * Scale, IconSize = 28.f * Scale;
	const ALeaflinePlayerController* LPC = Cast<ALeaflinePlayerController>(GetOwningPlayerController());
	const bool bMap = LPC && LPC->IsWindMapShown();
	const ABirdSystem* Bird = D->GetPhase() == EDayPhase::Shift ? ABirdSystem::Get(this) : nullptr;
	const bool bBirdLine = Bird && Bird->IsPresent();
	// How exposed the ant's spot is (sampled as in the text version; DrawPrompt reads ExposureShown too). Sampled before
	// the panel, which fits the rows drawn below.
	const APlayerController* PC = GetOwningPlayerController();
	const AAntPawn* Ant = PC ? Cast<AAntPawn>(PC->GetPawn()) : nullptr;
	const UAntMovementComponent* M = Ant ? Ant->GetAntMovement() : nullptr;
	ExposureTimer -= GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f;
	if (M && ExposureTimer <= 0.f)
	{
		ExposureTimer = 0.2f;
		ExposureShown = Wind->GetExposure(M->GetSurfacePosition(), M->GetSurfaceNormal());
	}
	const float RowY = GY + GH + Style.Gap * Scale, LY = RowY + Style.Row * Scale;
	const float LineH = MeasureText(TEXT("Ag"), Font, SecondaryText(Scale)).Y;
	const FString MapHint = TEXT("Hold {Cycle}: wind map");
	float Bottom = GY + GH;
	if (M && ExposureShown >= 0.f) Bottom = RowY + FMath::Max(LineH, 10.f * Scale + 0.5f * IconSize);
	if (M && D->GetPhase() == EDayPhase::Shift)
	{
		Bottom = bMap ? LY + 3.f * Style.Row * Scale + LineH : LY + MeasureText(MapHint, Font, SecondaryText(Scale)).Y;
		if (bBirdLine) Bottom = LY + ((bMap ? 4.f : 1.f) * Style.Row + 6.f) * Scale + FMath::Max(LineH, 14.f * Scale + 0.55f * IconSize);
	}
	DrawPanel(GX - 42.f * Scale, Top, GW + 42.f * Scale, Bottom - Top, Scale);

	const EWindPhase Ph = Wind->GetPhase();
	const bool bGust = Wind->IsGustActive(), bWarn = Ph == EWindPhase::Warning;
	const FLinearColor C = bGust ? StaminaLockedColor : (bWarn ? Style.Warning : TextColor);
	const float Pulse = 0.5f + 0.5f * FMath::Sin(Now * 2.f * PI * 2.f); // 2 Hz, before a gust
	DrawTextAt(Ph == EWindPhase::Calm ? TEXT("calm") : bWarn ? TEXT("gust coming") : Ph == EWindPhase::Falling ? TEXT("easing") : TEXT("GUST!"),
		GX, Top, C, SecondaryText(Scale), Font);
	FLinearColor BarC = C;
	if (bWarn) BarC.A *= 0.35f + 0.65f * Pulse;
	DrawRect(Style.Track, GX, GY, GW, GH);
	DrawRect(BarC, GX, GY, GW * FMath::Clamp(Wind->GetStrength(), 0.f, 1.f), GH);

	// The wind arrow as seen from the camera (up on screen is away from you), with streaks flowing along it: slow when
	// calm, faster and pulsing before a gust, fast and shaking in it.
	if (PC && PC->PlayerCameraManager)
	{
		FVector F = PC->PlayerCameraManager->GetCameraRotation().Vector();
		F.Z = 0.0;
		F = F.GetSafeNormal();
		const FVector R(-F.Y, F.X, 0.0);
		const FVector WindDir = Wind->GetDirection().GetSafeNormal2D();
		FVector2D V(FVector::DotProduct(WindDir, R), -FVector::DotProduct(WindDir, F));
		if (!F.IsNearlyZero() && !V.IsNearlyZero())
		{
			V.Normalize();
			const FVector2D Side(-V.Y, V.X);
			FVector2D Ctr(IconX, Top + 26.f * Scale);
			if (bGust) Ctr += FVector2D(FMath::Sin(Now * 53.f), FMath::Cos(Now * 41.f)) * (3.5f * Scale);
			const float AR = 14.f * Scale * (bWarn ? 1.f + 0.3f * Pulse : 1.f);
			FLinearColor AC = C;
			if (bWarn) AC.A *= 0.4f + 0.6f * Pulse;
			const float T = (bGust ? 3.f : 2.f) * Scale;
			const FVector2D Tip = Ctr + V * AR, Tail = Ctr - V * AR;
			const FVector2D H1 = Tip - V * (AR * 0.6f) + Side * (AR * 0.45f), H2 = Tip - V * (AR * 0.6f) - Side * (AR * 0.45f);
			DrawLine(Tail.X, Tail.Y, Tip.X, Tip.Y, AC, T);
			DrawLine(Tip.X, Tip.Y, H1.X, H1.Y, AC, T);
			DrawLine(Tip.X, Tip.Y, H2.X, H2.Y, AC, T);
			const float Speed = bGust ? 2.6f : (bWarn ? 1.6f : (Ph == EWindPhase::Falling ? 0.9f : 0.45f));
			for (int32 k = 0; k < 2; ++k)
			{
				const float Phase = FMath::Frac(Now * Speed + k * 0.5f);
				const float Along = FMath::Lerp(-1.3f * AR, 1.1f * AR, Phase), Len = 7.f * Scale;
				const FVector2D Base = Ctr + Side * ((k == 0 ? 1.f : -1.f) * 10.f * Scale);
				const FVector2D P0 = Base + V * (Along - 0.5f * Len), P1 = Base + V * (Along + 0.5f * Len);
				FLinearColor SC = AC;
				SC.A *= 0.8f * FMath::Sin(Phase * PI) * (Ph == EWindPhase::Calm ? 0.6f : 1.f);
				DrawLine(P0.X, P0.Y, P1.X, P1.Y, SC, 1.5f * Scale);
			}
		}
	}

	if (M && ExposureShown >= 0.f)
	{
		const bool bSheltered = ExposureShown < Style.ShelteredBelow, bPartly = ExposureShown < Style.PartlyBelow;
		const FLinearColor EC = bSheltered ? Style.Sheltered : (bPartly ? Style.Partly : StaminaLockedColor);
		DrawIcon(bSheltered ? TEXT("Icon_Sheltered") : (bPartly ? TEXT("Icon_Partly") : TEXT("Icon_Exposed")), IconX, RowY + 10.f * Scale, IconSize, EC);
		DrawTextAt(bSheltered ? TEXT("sheltered") : (bPartly ? TEXT("partly exposed") : TEXT("exposed")), GX, RowY, EC, SecondaryText(Scale), Font);
	}
	if (M && D->GetPhase() == EDayPhase::Shift)
	{
		if (bMap)
		{
			DrawCrisp(Canvas, TEXT("Wind map"), TextColor, GX, LY, Font, SecondaryText(Scale));
			DrawCrisp(Canvas, TEXT("Green: sheltered"), Style.Sheltered, GX, LY + Style.Row * Scale, Font, SecondaryText(Scale));
			DrawCrisp(Canvas, TEXT("Amber: partly exposed"), Style.Partly, GX, LY + 2.f * Style.Row * Scale, Font, SecondaryText(Scale));
			DrawCrisp(Canvas, TEXT("Red: exposed"), StaminaLockedColor, GX, LY + 3.f * Style.Row * Scale, Font, SecondaryText(Scale));
		}
		else
		{
			DrawTextAt(MapHint, GX, LY, Style.Muted, SecondaryText(Scale), Font);
		}
		if (bBirdLine)
		{
			// Plain text colour: green, amber and red in this panel mean exposure. Circling: the bird glides and banks;
			// diving: it stoops and blinks.
			const float BY = LY + ((bMap ? 4.f : 1.f) * Style.Row + 6.f) * Scale, BCY = BY + 10.f * Scale;
			if (Bird->IsDiving())
			{
				const float Blink = FMath::Frac(Now * 4.f) < 0.5f ? 1.f : 0.25f;
				const float Stoop = FMath::Frac(Now * 2.f) * 6.f * Scale;
				DrawIcon(TEXT("Icon_BirdDive"), IconX, BCY - 2.f * Scale + Stoop, IconSize * 1.1f, FLinearColor(TextColor.R, TextColor.G, TextColor.B, Blink));
				DrawTextAt(TEXT("diving!"), GX, BY, TextColor, SecondaryText(Scale), Font);
			}
			else
			{
				const float Bob = FMath::Sin(Now * 2.f * PI * 0.6f);
				DrawIcon(TEXT("Icon_BirdGlide"), IconX + FMath::Cos(Now * 2.f * PI * 0.3f) * 4.f * Scale, BCY + Bob * 3.f * Scale, IconSize * 1.1f, TextColor, Bob * 12.f);
				DrawTextAt(TEXT("circling"), GX, BY, TextColor, SecondaryText(Scale), Font);
			}
		}
	}
}

void ALeaflineHUD::DrawCarryIcons(const AAntPawn* Ant, float Scale)
{
	// The text version's "Carrying ..." row, from the panel's left edge: a leaf sized by the piece (swaying while carried) and its mass, or a
	// faint leaf and "empty"; a claw and "gripping" while the ant grips; the size bar as before.
	UFont* Font = GEngine->GetMediumFont();
	const float Now = GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.f;
	const float X = (Style.Edge + Style.Padding) * Scale, Y = (Style.Edge + Style.Padding + 84.f) * Scale;
	const float Icon = 26.f * Scale, ICY = Y + 10.f * Scale, TextX = X + Icon + 6.f * Scale;
	const bool bCarry = Ant->IsCarrying();
	const float Size01 = FMath::Clamp(Ant->GetCarriedSize(), 0.f, 1.f);
	if (bCarry)
	{
		DrawIcon(TEXT("Icon_Leaf"), X + 0.5f * Icon, ICY, Icon * (0.6f + 0.4f * Size01), Style.Leaf, FMath::Sin(Now * 2.f * PI * 0.7f) * 8.f);
		const FString Weight = FString::Printf(TEXT("%.1f"), Ant->GetCarriedMass());
		DrawTextAt(Weight, TextX, Y, TextColor, SecondaryText(Scale), Font);
		if (ShowsMultiplier(Ant->GetCarriedRichness()))
		{
			DrawTextAt(MultiplierText(Ant->GetCarriedRichness()), TextX + MeasureText(Weight, Font, SecondaryText(Scale)).X + 5.f * Scale, Y + 2.f * Scale,
				Style.Rich, MultiplierTextScale(Scale), Font);
		}
	}
	else
	{
		DrawIcon(TEXT("Icon_Leaf"), X + 0.5f * Icon, ICY, Icon * 0.8f, FLinearColor(Style.Home.R, Style.Home.G, Style.Home.B, 0.6f));
		DrawTextAt(TEXT("empty"), TextX, Y, Style.Muted, SecondaryText(Scale), Font);
	}
	const UAntMovementComponent* M = Ant->GetAntMovement();
	if (M && M->GetState() == EAntState::Gripping)
	{
		const float GXp = X + 220.f * Scale;
		DrawIcon(TEXT("Icon_Grip"), GXp + 0.5f * Icon, ICY, Icon * (1.f + 0.08f * FMath::Sin(Now * 2.f * PI * 3.f)), AccentColor);
		DrawTextAt(TEXT("gripping"), GXp + Icon + 6.f * Scale, Y, AccentColor, SecondaryText(Scale), Font);
	}
	const float W = 440.f * Scale, H = Style.Bar * Scale, BY = Y + Style.Row * Scale;
	DrawRect(Style.Track, X, BY, W, H);
	if (bCarry) DrawRect(Style.Leaf, X, BY, W * Size01, H);
}

void ALeaflineHUD::DrawDayPanelIcons(const ADayDirector* D, float Scale)
{
	// The text version's panel and rows; each worker is its number, an ant in its state's colour and pose, and the word.
	UFont* Font = GEngine->GetMediumFont();
	const float X = (Style.Edge + Style.Padding) * Scale, Y = X;
	const FString Title = D->GetTreeName().IsEmpty() ? FString::Printf(TEXT("Day %d"), D->GetDay())
		: FString::Printf(TEXT("Day %d: %s"), D->GetDay(), *D->GetTreeName());
	const float Cell = 140.f * Scale;
	const float W = FMath::Max(FMath::Max(440.f * Scale, D->NumWorkers() * Cell), float(MeasureText(Title, Font, PrimaryText(Scale)).X));
	DrawPanel(X, Y, W, 128.f * Scale, Scale);
	DrawTextAt(Title, X, Y, TextColor, PrimaryText(Scale), Font);
	const float PY = Y + 44.f * Scale, Icon = 24.f * Scale, T = Style.Stroke * Scale;
	for (int32 i = 0; i < D->NumWorkers(); ++i)
	{
		const float PX = X + i * Cell;
		const EWorkerState State = D->GetWorkerState(i);
		FLinearColor C = TextColor;
		const TCHAR* Label = TEXT("Ready");
		switch (State)
		{
		case EWorkerState::Waiting: break;
		case EWorkerState::Out: C = AccentColor; Label = TEXT("Out"); break;
		case EWorkerState::Returning: C = Style.Rich; Label = TEXT("Returning"); break;
		case EWorkerState::Home: C = Style.Home; Label = TEXT("Home"); break;
		case EWorkerState::BlownOff: C = StaminaLockedColor; Label = TEXT("Blown off"); break;
		}
		const FString Num = FString::FromInt(i + 1);
		DrawTextAt(Num, PX, PY, C, SecondaryText(Scale), Font);
		const float ICX = PX + MeasureText(Num, Font, SecondaryText(Scale)).X + 4.f * Scale + 0.5f * Icon, ICY = PY + 10.f * Scale;
		// Ready: faint at the nest; out: heading up; returning: heading down with its piece; home: on the nest line;
		// blown off: tumbling in the wind.
		switch (State)
		{
		case EWorkerState::Waiting:
			DrawIcon(TEXT("Icon_Ant"), ICX, ICY, Icon, FLinearColor(C.R, C.G, C.B, 0.45f));
			break;
		case EWorkerState::Out:
			DrawIcon(TEXT("Icon_Ant"), ICX, ICY, Icon, C);
			break;
		case EWorkerState::Returning:
			DrawIcon(TEXT("Icon_Ant"), ICX, ICY, Icon, C, 180.f);
			DrawIcon(TEXT("Icon_Leaf"), ICX + 0.15f * Icon, ICY + 0.1f * Icon, 0.7f * Icon, Style.Leaf);
			break;
		case EWorkerState::Home:
			DrawIcon(TEXT("Icon_Ant"), ICX, ICY - 0.1f * Icon, 0.8f * Icon, C);
			DrawRect(C, ICX - 0.5f * Icon, ICY + 0.4f * Icon, Icon, T);
			break;
		case EWorkerState::BlownOff:
			DrawIcon(TEXT("Icon_Ant"), ICX, ICY, Icon, C, 40.f);
			DrawLine(ICX - 0.75f * Icon, ICY - 0.2f * Icon, ICX - 0.4f * Icon, ICY - 0.2f * Icon, C, T);
			DrawLine(ICX - 0.8f * Icon, ICY + 0.15f * Icon, ICX - 0.45f * Icon, ICY + 0.15f * Icon, C, T);
			break;
		}
		DrawTextAt(Label, ICX + 0.5f * Icon + 4.f * Scale, PY, C, SecondaryText(Scale), Font);
	}
	if (const AAntPawn* Ant = Cast<AAntPawn>(GetOwningPawn())) DrawCarry(Ant, Scale);
}

void ALeaflineHUD::DrawColonyIcons(const ADayDirector* D, float Scale)
{
	// What the milestones count: everything hauled since the colony began (not what can be spent; that shows at the
	// nest). A leaf pile and the total, a bar toward the next milestone, today's +N and where the next one is.
	const ULeaflineGameInstance* GI = Cast<ULeaflineGameInstance>(GetGameInstance());
	if (!GI) return;
	UFont* Font = GEngine->GetMediumFont();
	const float CX = Canvas->ClipX * 0.5f, Y = (Style.Edge + Style.Padding) * Scale;
	const FString Num = FString::FormatAsNumber(FMath::RoundToInt(ColonyShown));
	const FString Word = TEXT("hauled in all");
	const FString Today = FString::Printf(TEXT("+%.1f today"), D->GetStats().MassHauled);
	const int32 Reached = GI->GetReachedMilestoneCount();
	const bool bNext = Reached < LeaflineUpgrades::MilestoneCount;
	const float NextAt = bNext ? GI->GetMilestoneThreshold(Reached) : 0.f;
	const float PrevAt = Reached > 0 ? GI->GetMilestoneThreshold(Reached - 1) : 0.f;
	// With one tree per stage the second and third milestones move the colony to a new tree; the first does not.
	const FString NextNum = FString::FormatAsNumber(FMath::RoundToInt(NextAt));
	const FString Next = !bNext ? FString() : (GI->IsOneTree() && Reached >= 1 ? FString::Printf(TEXT("new tree at %s"), *NextNum)
		: FString::Printf(TEXT("next milestone at %s"), *NextNum));
	const float Icon = 30.f * Scale, Gap = 8.f * Scale;
	const float NumW = MeasureText(Num, Font, PrimaryText(Scale)).X, WordW = MeasureText(Word, Font, SecondaryText(Scale)).X;
	const float TodayW = MeasureText(Today, Font, SecondaryText(Scale)).X, NextW = Next.IsEmpty() ? 0.f : MeasureText(Next, Font, SecondaryText(Scale)).X;
	const float Row1W = Icon + Gap + NumW + Gap + WordW, Row3W = TodayW + (Next.IsEmpty() ? 0.f : 2.f * Style.Padding * Scale + NextW);
	const float W = FMath::Max(Row1W, Row3W);
	const float BarY = Y + (Style.PrimaryHeight + 8.f) * Scale, BarH = 4.f * Scale, Row3Y = BarY + BarH + 8.f * Scale;
	DrawPanel(CX - 0.5f * W, Y, W, Row3Y + Style.SecondaryHeight * Scale - Y, Scale);
	float RX = CX - 0.5f * Row1W;
	DrawIcon(TEXT("Icon_LeafPile"), RX + 0.5f * Icon, Y + 0.5f * Style.PrimaryHeight * Scale, Icon, Style.Leaf);
	RX += Icon + Gap;
	DrawTextAt(Num, RX, Y, TextColor, PrimaryText(Scale), Font);
	DrawTextAt(Word, RX + NumW + Gap, Y + (Style.PrimaryHeight - Style.SecondaryHeight) * 0.7f * Scale, Style.Muted, SecondaryText(Scale), Font);
	if (bNext && NextAt > PrevAt)
	{
		const float F = FMath::Clamp((ColonyShown - PrevAt) / (NextAt - PrevAt), 0.f, 1.f);
		DrawRect(Style.Track, CX - 0.5f * W, BarY, W, BarH);
		DrawRect(Style.Leaf, CX - 0.5f * W, BarY, W * F, BarH);
	}
	RX = CX - 0.5f * Row3W;
	DrawTextAt(Today, RX, Row3Y, AccentColor, SecondaryText(Scale), Font);
	if (!Next.IsEmpty()) DrawTextAt(Next, RX + TodayW + 2.f * Style.Padding * Scale, Row3Y, Style.Muted, SecondaryText(Scale), Font);
}
