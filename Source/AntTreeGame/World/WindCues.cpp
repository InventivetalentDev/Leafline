#include "World/WindCues.h"
#include "AntTreeGame.h"
#include "World/WindSystem.h"
#include "Ant/AntPawn.h"
#include "Ant/AntMovementComponent.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Engine.h"
#include "UnrealClient.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	bool GCuesOn = true;
	// Leaf.CuesStat window.
	float GStatLeft = 0.f;
	double GStatSumMs = 0.0;
	double GStatMaxMs = 0.0;
	int32 GStatFrames = 0;
	// Leaf.CuesLog: the front and the ant's air every half second.
	float GLogTimer = -1.f; // < 0: off

	/** How much of the gust the air shows at a given wind strength (the old streaks' amount). */
	float GustAmount(float Strength) { return FMath::SmoothStep(0.22f, 0.9f, Strength); }

	float PointSegmentDistance(const FVector& P, const FVector& A, const FVector& B)
	{
		const FVector AB = B - A;
		const double L2 = AB.SizeSquared();
		const double T = L2 > 1e-4 ? FMath::Clamp(FVector::DotProduct(P - A, AB) / L2, 0.0, 1.0) : 0.0;
		return (float)FVector::Dist(P, A + AB * T);
	}

	// Unlit vertex colours, kept away from the HUD's green / amber / red (they mean sheltered / partly / exposed).
	FLinearColor PollenColor(FRandomStream& R) { return FMath::Lerp(FLinearColor(1.f, 0.95f, 0.78f), FLinearColor(0.92f, 0.9f, 0.86f), R.FRand()); }
	FLinearColor LeafBitColor(FRandomStream& R) { return FMath::Lerp(FLinearColor(0.62f, 0.55f, 0.36f), FLinearColor(0.5f, 0.42f, 0.3f), R.FRand()); }
	const FLinearColor BarkDustColor(0.6f, 0.55f, 0.45f);
	const FLinearColor BarkDustDark(0.035f, 0.03f, 0.022f);
}

bool WindCues::IsEnabled() { return GCuesOn; }
void WindCues::SetEnabled(bool bOn) { GCuesOn = bOn; }
bool WindCues::IsLogging() { return GLogTimer >= 0.f; }

void WindCues::SetLogging(bool bOn)
{
	GLogTimer = bOn ? 0.f : -1.f;
}

void WindCues::GripPose(const FWindTuning& T, float GripTime, float WindFeel, const FVector& LocalWind, float Time, FVector& Loc, FRotator& Rot, FVector& Scale)
{
	const float Press = FMath::Exp(-FMath::Square((GripTime - 0.08f) / 0.06f)); // a quick press just after the grip
	Scale.Z = FMath::Min(Scale.Z, T.CueGripSquash - 0.1f * Press);
	Scale.Y = FMath::Max(Scale.Y, T.CueGripSpread);
	Loc.Z -= T.CueGripSink + 0.5f * Press;
	Rot.Pitch -= 3.f; // head down to the bark
	const float Tug = T.CueGripTug * WindFeel;
	const float Pull = 1.f * Tug * (0.7f + 0.3f * FMath::Sin(Time * 2.f * PI * 3.3f));
	Loc.X += Pull * (float)LocalWind.X;
	Loc.Y += Pull * (float)LocalWind.Y;
	Rot.Roll += 3.f * Tug * FMath::Sin(Time * 2.f * PI * 9.f + 0.7f);
	Rot.Yaw += 4.f * Tug * FMath::Sin(Time * 2.f * PI * 5.3f);
}

void WindCues::StartStat(float Seconds)
{
	GStatLeft = FMath::Max(Seconds, 0.5f);
	GStatSumMs = 0.0;
	GStatMaxMs = 0.0;
	GStatFrames = 0;
}

AWindCues::AWindCues()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork; // after the ant has moved this frame
	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Cues"));
	RootComponent = Mesh;
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->SetCastShadow(false);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Soft(TEXT("/Game/Leafline/Materials/M_Exposure.M_Exposure"));
	if (Soft.Succeeded()) SoftMaterial = Soft.Object;
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> StreakMat(TEXT("/Game/Leafline/Materials/M_WindStreak.M_WindStreak"));
	if (StreakMat.Succeeded()) StreakMaterial = StreakMat.Object;
	Rand.Initialize(90417); // its own stream: the wind's draws (gust timing) are never touched
}

void AWindCues::HideAll()
{
	if (bHidden) return;
	Mesh->ClearAllMeshSections();
	StreakSectionMaterial = -1;
	for (TArray<FParticle>* Pool : { &Streaks, &Specks, &Local, &Scrapes, &Puffs })
	{
		for (FParticle& P : *Pool) P.MaxLife = -1.f;
	}
	bWasGripping = false;
	TrickleDebt = 0.f;
	bHidden = true;
}

void AWindCues::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	AWindSystem* W = Wind.Get();
	// Off, or a skipped harvest running many gusts a second: nothing drawn.
	if (!GCuesOn || !W || W->AreCuesMuted())
	{
		HideAll();
		return;
	}
	const uint64 StartCycles = FPlatformTime::Cycles64();
	const float Dt = FMath::Min(DeltaTime, 0.1f);
	Clock += Dt;

	FAir Air;
	if (!ComputeAir(Air, Dt))
	{
		HideAll();
		return;
	}

	const FWindTuning& T = W->Tuning;
	Streaks.SetNum(FMath::Clamp(T.CueStreaks, 0, 2000));
	Specks.SetNum(FMath::Clamp(T.CueSpecks, 0, 4000));
	Local.SetNum(FMath::Clamp(T.CueLocal, 0, 1000));
	Scrapes.SetNum(48);
	Puffs.SetNum(FMath::Clamp(T.CueFrontPuffs, 0, 200));
	const int32 LocalStreakSlots = Local.Num() * 3 / 10;
	StreakQuads.Capacity = Streaks.Num() + LocalStreakSlots;
	SpeckQuads.Capacity = 3 * Puffs.Num() + Specks.Num() + (Local.Num() - LocalStreakSlots) + Scrapes.Num();
	for (FQuads* Q : { &StreakQuads, &SpeckQuads })
	{
		Q->V.SetNumUninitialized(Q->Capacity * 4);
		Q->C.SetNumUninitialized(Q->Capacity * 4);
		Q->Used = 0;
	}

	StepPuffs(Air, Dt); // first, so the faint haze is drawn under the specks
	StepStreaks(Air, Dt);
	StepSpecks(Air, Dt);
	StepLocal(Air, Dt);
	StepScrapes(Air, Dt);

	const bool bOldStreakLook = T.CueStreakMaterial == 1 && StreakMaterial;
	ParkPos = Air.CamPos;
	Flush(0, StreakQuads, bOldStreakLook ? StreakMaterial.Get() : SoftMaterial.Get(), !bOldStreakLook);
	Flush(1, SpeckQuads, SoftMaterial, true);
	bHidden = false;

	const double Ms = FPlatformTime::ToMilliseconds64(FPlatformTime::Cycles64() - StartCycles);
	if (GStatLeft > 0.f)
	{
		GStatSumMs += Ms;
		GStatMaxMs = FMath::Max(GStatMaxMs, Ms);
		++GStatFrames;
		GStatLeft -= DeltaTime;
		if (GStatLeft <= 0.f)
		{
			UE_LOG(LogLeafline, Log, TEXT("Wind cues: game thread %.3f ms average, %.3f ms max over %d frames (%d streak and %d speck quads last frame)"),
				GStatSumMs / FMath::Max(GStatFrames, 1), GStatMaxMs, GStatFrames, StreakQuads.Used, SpeckQuads.Used);
		}
	}
	if (GLogTimer >= 0.f)
	{
		GLogTimer -= DeltaTime;
		if (GLogTimer <= 0.f)
		{
			GLogTimer = 0.5f;
			const float FrontToAnt = Air.bFront ? (float)FVector::DotProduct(Air.bAnt ? Air.AntPos : Air.CamPos, Air.W) - Air.SFront : 0.f;
			UE_LOG(LogLeafline, Log, TEXT("Wind cues: %s %.1f s left, strength %.2f, front %s%.0f cm upwind of the ant, air at ant %.2f, exposure %.2f, pocket %.0f cm, quads %d + %d"),
				*UEnum::GetValueAsString(W->GetPhase()), W->GetPhaseTimeLeft(), Air.Strength, Air.bFront ? TEXT("") : TEXT("(none) "),
				FrontToAnt, Air.AntAir, Exposure, Air.Pocket, StreakQuads.Used, SpeckQuads.Used);
		}
	}
}

bool AWindCues::ComputeAir(FAir& Air, float Dt)
{
	const AWindSystem* W = Wind.Get();
	const FWindTuning& T = W->Tuning;
	APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!Cam) return false;
	Air.CamPos = Cam->GetCameraLocation();
	float ViewX = 1280.f;
	if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport)
	{
		ViewX = (float)FMath::Max(64, GEngine->GameViewport->Viewport->GetSizeXY().X);
	}
	float ViewY = ViewX * 0.5625f;
	if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport)
	{
		ViewY = (float)FMath::Max(64, GEngine->GameViewport->Viewport->GetSizeXY().Y);
	}
	Air.TanH = FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(Cam->GetFOVAngle(), 10.f, 170.f) * 0.5f));
	Air.TanV = Air.TanH * ViewY / ViewX;
	Air.PixelAngle = 2.f * Air.TanH / ViewX;
	const FRotationMatrix CamAxes(Cam->GetCameraRotation());
	Air.CamFwd = CamAxes.GetScaledAxis(EAxis::X);
	Air.CamRight = CamAxes.GetScaledAxis(EAxis::Y);
	Air.CamUp = CamAxes.GetScaledAxis(EAxis::Z);
	Air.W = W->GetDirection().GetSafeNormal();
	if (Air.W.IsNearlyZero()) Air.W = FVector::ForwardVector;
	Air.Strength = W->GetStrength();

	const AAntPawn* Ant = Cast<AAntPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
	const UAntMovementComponent* M = Ant ? Ant->GetAntMovement() : nullptr;
	Air.bAnt = M && M->GetState() != EAntState::Falling && !Ant->IsVanishing();
	if (Air.bAnt)
	{
		Air.AntPos = M->GetSurfacePosition();
		Air.AntN = M->GetSurfaceNormal().GetSafeNormal();
		if (Air.AntN.IsNearlyZero()) Air.AntN = FVector::UpVector;
	}

	// The front reaches the ant (or the camera, without one) as the gust starts rising, and keeps going downwind.
	const FVector Ref = Air.bAnt ? Air.AntPos : Air.CamPos;
	const float Left = W->GetPhaseTimeLeft(), Dur = W->GetPhaseDuration();
	float Tau = 0.f; // seconds until the front reaches the ant (negative: passed)
	Air.bFront = true;
	switch (W->GetPhase())
	{
	case EWindPhase::Warning: Tau = Left; break;
	case EWindPhase::Rising: Tau = -(Dur - Left); break;
	case EWindPhase::Holding: Tau = -(T.Rising + Dur - Left); break;
	default: Air.bFront = false; break;
	}
	Air.bFrontEdge = W->GetPhase() == EWindPhase::Warning || W->GetPhase() == EWindPhase::Rising;
	Air.SFront = (float)FVector::DotProduct(Ref, Air.W) - T.CueFrontSpeed * Tau;
	Air.GustAir = T.CueFrontLevel * W->GetGustPeak();
	Air.Band = FMath::Max(T.CueFrontBand, 10.f);
	if (W->GetPhase() == EWindPhase::Warning)
	{
		const float Progress = 1.f - FMath::Clamp(Left / FMath::Max(Dur, 0.01f), 0.f, 1.f);
		Air.Strength = FMath::Max(Air.Strength, T.CuePrecursor * Air.GustAir * Progress);
		Air.FrontHaze = FMath::SmoothStep(0.f, 0.6f, Dur - Left); // fades in at the start of the warning
	}
	else if (W->GetPhase() == EWindPhase::Rising)
	{
		Air.FrontHaze = FMath::Square(FMath::Clamp(Left / FMath::Max(Dur, 0.01f), 0.f, 1.f)); // and out as the gust rises
	}

	if (Air.bAnt)
	{
		// The ant's spot, sampled a few times a second (each sample marches through the tree's field).
		ExposureTimer -= Dt;
		if (ExposureTimer <= 0.f)
		{
			ExposureTimer = 0.2f;
			Exposure = W->GetExposure(Air.AntPos, Air.AntN);
		}
		Air.Shelter = 1.f - FMath::SmoothStep(0.15f, 0.6f, Exposure);
		Air.AntAir = GustAmount(AirAt(Air, Air.AntPos));
		PocketNow = FMath::FInterpTo(PocketNow, T.CueShelterRadius * Air.Shelter, Dt, 4.f);
		Air.Pocket = PocketNow;
		Air.PocketCentre = Air.AntPos + Air.AntN * (0.3f * PocketNow);
		const FVector Wt = Air.W - Air.AntN * FVector::DotProduct(Air.W, Air.AntN);
		// On a face the wind hits square on (or leaves square), "across" is the horizontal along the bark.
		Air.Across = FVector::CrossProduct(Air.AntN, Wt.SizeSquared() < 0.04 ? FVector::UpVector : Wt).GetSafeNormal();
		if (Air.Across.IsNearlyZero()) Air.Across = FVector::RightVector;
		const FVector Flow = Air.W - Air.AntN * FMath::Max(0.0, FVector::DotProduct(Air.W, Air.AntN));
		Air.Flow = Flow.SizeSquared() < 0.09 ? Air.Across : Flow.GetSafeNormal();
	}
	else
	{
		PocketNow = 0.f;
	}
	return true;
}

float AWindCues::AirAt(const FAir& Air, const FVector& P) const
{
	if (!Air.bFront) return Air.Strength;
	const float S = (float)FVector::DotProduct(P, Air.W);
	const float Behind = 1.f - FMath::SmoothStep(Air.SFront - Air.Band, Air.SFront + Air.Band, S);
	return FMath::Max(Air.Strength, Air.GustAir * Behind);
}

float AWindCues::FrontEdgeAt(const FAir& Air, const FVector& P) const
{
	if (!Air.bFrontEdge) return 0.f;
	const float D = ((float)FVector::DotProduct(P, Air.W) - Air.SFront) / (1.5f * Air.Band);
	return FMath::Exp(-D * D);
}

FVector AWindCues::PocketMap(const FAir& Air, const FVector& P, float Seed, const FVector& Axis) const
{
	// Streamlines round a sphere, roughly: within the pocket's slab along the wind, points are pushed out from the axis
	// so none ends inside; the push fades smoothly to nothing at the slab's ends. Never pushed into the bark.
	if (!Air.bAnt || Air.Pocket < 2.f) return P;
	const float R = Air.Pocket * 1.25f;
	const FVector D = P - Air.PocketCentre;
	const float X = (float)FVector::DotProduct(D, Axis);
	if (FMath::Abs(X) >= R) return P;
	const FVector Rho = D - Axis * X;
	const float RhoLen = (float)Rho.Size();
	FVector U = RhoLen > 0.5f ? Rho / RhoLen : Air.Across * (Seed < 0.5f ? -1.f : 1.f);
	const float Into = (float)FVector::DotProduct(U, Air.AntN);
	if (Into < 0.f)
	{
		U = (U - Air.AntN * Into).GetSafeNormal();
		if (U.IsNearlyZero()) U = Air.Across;
	}
	const float F = FMath::Square(1.f - FMath::Square(X / R));
	const float NewRho = FMath::Sqrt(RhoLen * RhoLen + R * R * F);
	return Air.PocketCentre + Axis * X + U * NewRho;
}

FVector AWindCues::ViewPoint(const FAir& Air, float Near, float Far, float DepthPow)
{
	const float D = Near + (Far - Near) * FMath::Pow(Rand.FRand(), DepthPow);
	const float X = Rand.FRandRange(-1.15f, 1.15f), Y = Rand.FRandRange(-1.15f, 1.15f);
	return Air.CamPos + Air.CamFwd * D + Air.CamRight * (X * D * Air.TanH) + Air.CamUp * (Y * D * Air.TanV);
}

bool AWindCues::InView(const FAir& Air, const FVector& P, float Near, float Far)
{
	const FVector V = P - Air.CamPos;
	const float D = (float)FVector::DotProduct(V, Air.CamFwd);
	if (D < 0.5f * Near || D > 1.15f * Far) return false;
	const float X = (float)FVector::DotProduct(V, Air.CamRight) / (D * Air.TanH);
	const float Y = (float)FVector::DotProduct(V, Air.CamUp) / (D * Air.TanV);
	return FMath::Abs(X) < 1.4f && FMath::Abs(Y) < 1.4f;
}

void AWindCues::StepPuffs(const FAir& Air, float Dt)
{
	// The dust wall: big faint puffs on the front's plane, riding with it. Each keeps its place across the wind (Pos holds
	// the point's part across the wind) and its own offset along it (Length).
	const FWindTuning& T = Wind->Tuning;
	if (Air.FrontHaze <= 0.f || T.CueFrontHaze <= 0.f)
	{
		for (FParticle& P : Puffs) P.MaxLife = -1.f;
		return;
	}
	const float Near = 0.5f * FMath::Max(T.CueFrontPuffNear, 50.f), Far = FMath::Max(0.7f * T.CueFar, 2.f * Near);
	for (FParticle& P : Puffs)
	{
		FVector At = P.Pos + Air.W * (Air.SFront + P.Length);
		if (P.MaxLife < 0.f || P.Life >= P.MaxLife || !InView(Air, At, Near, Far * 1.3f))
		{
			// A point in view, moved along the wind onto the front; tried a few times so it lands in view.
			for (int32 Try = 0; Try < 4; ++Try)
			{
				const FVector V = ViewPoint(Air, Near, Far, 0.8f);
				P.Pos = V - Air.W * FVector::DotProduct(V, Air.W);
				P.Length = Air.Band * Rand.FRandRange(-0.5f, 0.5f);
				At = P.Pos + Air.W * (Air.SFront + P.Length);
				if (InView(Air, At, Near, Far * 1.3f)) break;
			}
			P.Life = 0.f;
			P.MaxLife = Rand.FRandRange(1.2f, 2.4f);
			P.Size = T.CueFrontPuffSize * Rand.FRandRange(0.6f, 1.3f);
			P.Seed = Rand.FRand();
			P.Kind = EKind::Speck;
		}
		P.Life += Dt;
		const float Fade = FMath::Sin(PI * FMath::Clamp(P.Life / P.MaxLife, 0.f, 1.f));
		const float Alpha = T.CueFrontHaze * Air.FrontHaze * Fade;
		if (Alpha <= 0.003f || !InView(Air, At, Near, Far * 1.3f)) continue;
		// Three nested quads: the material's disc has a firm rim, so one quad reads as a blob; nested ones fade softly.
		static const float Scales[3] = { 1.f, 0.68f, 0.4f };
		static const float Shares[3] = { 0.45f, 0.35f, 0.3f };
		for (int32 k = 0; k < 3; ++k)
		{
			FLinearColor Col = P.Seed < 0.5f ? T.CueFrontPuffDark : T.CueFrontPuffColor;
			Col.A = Alpha * Shares[k];
			const float Rad = P.Size * Scales[k];
			AddQuad(SpeckQuads, Air, At - Air.W * (2.4f * Rad), At + Air.W * (2.4f * Rad), Rad, Col, 1.f, T.CueFrontPuffNear);
		}
	}
}

void AWindCues::StepStreaks(const FAir& Air, float Dt)
{
	// Streaks in the view out to CueFar, so the air behind the front shows well before the front arrives.
	const FWindTuning& T = Wind->Tuning;
	const float Near = 0.5f * FMath::Max(T.CueStreakNear, 20.f), Far = FMath::Max(T.CueFar, 500.f);
	const FVector Wd = Air.W;
	for (FParticle& P : Streaks)
	{
		if (P.MaxLife < 0.f || P.Life >= P.MaxLife || !InView(Air, P.Pos, Near, Far))
		{
			P.Pos = ViewPoint(Air, Near, Far, 1.f);
			P.Life = 0.f;
			P.MaxLife = Rand.FRandRange(0.5f, 1.3f);
			P.Length = Rand.FRandRange(0.5f, 1.2f);
			P.Seed = Rand.FRandRange(0.05f, 1.f);
			P.Kind = EKind::Streak;
		}
		const float Vs = AirAt(Air, P.Pos);
		const float Amount = GustAmount(Vs);
		P.Life += Dt;
		P.Pos += Wd * (T.StreakSpeed * (0.35f + 0.9f * Vs) * Dt);
		const float Vis = FMath::SmoothStep(P.Seed, P.Seed + 0.24f, Amount);
		if (Vis <= 0.003f) continue;
		const float Fade = FMath::Sin(PI * FMath::Clamp(P.Life / P.MaxLife, 0.f, 1.f));
		const float Len = T.CueStreakLength * P.Length * (0.5f + 0.5f * Amount) * (0.4f + 0.6f * Fade);
		const FVector A = P.Pos, B = P.Pos + Wd * Len;
		float Alpha = T.CueStreakAlpha * Vis * Fade;
		if (Air.bAnt && Air.Pocket > 2.f) Alpha *= FMath::SmoothStep(0.8f * Air.Pocket, 1.6f * Air.Pocket, PointSegmentDistance(Air.PocketCentre, A, B));
		const float HalfW = T.CueStreakWidth * (1.f + T.CueStreakThick * Amount);
		FLinearColor Col = T.CueStreakColor;
		Col.A = Alpha;
		AddQuad(StreakQuads, Air, A, B, HalfW, Col, T.CueStreakTail, T.CueStreakNear);
	}
}

void AWindCues::StepSpecks(const FAir& Air, float Dt)
{
	// Specks in the view, most of them near: lazy motes in calm air, streaming with the gust, gathered at the front.
	const FWindTuning& T = Wind->Tuning;
	const float Near = 0.5f * FMath::Max(T.CueSpeckNear, 20.f), Far = FMath::Max(T.CueSpeckFar, 200.f);
	const FVector Wd = Air.W;
	FVector Side = FVector::CrossProduct(FVector::UpVector, Wd).GetSafeNormal();
	if (Side.IsNearlyZero()) Side = FVector::RightVector;
	const float Bright = FMath::Clamp(T.CueBrightness, 0.f, 1.f);
	for (FParticle& P : Specks)
	{
		if (P.MaxLife < 0.f || P.Life >= P.MaxLife || !InView(Air, P.Pos, Near, Far))
		{
			P.Pos = ViewPoint(Air, Near, Far, 1.6f);
			P.Life = 0.f;
			P.MaxLife = Rand.FRandRange(1.2f, 2.6f);
			P.Seed = Rand.FRandRange(0.05f, 1.f);
			P.Phase = Rand.FRandRange(0.f, 2.f * PI);
			const bool bLeaf = Rand.FRand() < T.CueLeafBitShare;
			P.Kind = bLeaf ? EKind::LeafBit : EKind::Speck;
			P.Size = bLeaf ? T.CueLeafBitSize * Rand.FRandRange(0.6f, 1.3f) : T.CueSpeckSize * Rand.FRandRange(0.6f, 1.4f);
			P.Color = bLeaf ? LeafBitColor(Rand) : PollenColor(Rand);
		}
		const float Vs = AirAt(Air, P.Pos);
		const float Amount = GustAmount(Vs);
		const float Edge = FrontEdgeAt(Air, P.Pos) * T.CueFrontDust;
		const float Speed = T.StreakSpeed * (0.03f + 0.9f * Vs * Vs);
		P.Life += Dt;
		P.Pos += Wd * (Speed * Dt);
		const float Gust = FMath::SmoothStep(P.Seed, P.Seed + 0.24f, FMath::Min(1.f, Amount + Edge));
		const float Calm = (P.Kind == EKind::Speck && P.Seed < 0.3f) ? T.CueCalmMotes : 0.f;
		const float Fade = FMath::Sin(PI * FMath::Clamp(P.Life / P.MaxLife, 0.f, 1.f));
		const float Alpha = FMath::Max(Calm, T.CueSpeckAlpha * Gust) * Fade;
		if (Alpha <= 0.003f) continue;
		// Lazy wobble in calm air, little in a gust.
		const float Wob = 4.f * (1.f - 0.6f * Amount);
		const FVector Off = Side * (Wob * FMath::Sin(Clock * 1.3f + P.Phase)) + FVector::UpVector * (0.7f * Wob * FMath::Sin(Clock * 0.9f + 1.7f * P.Phase));
		const FVector Draw = PocketMap(Air, P.Pos + Off, P.Seed, Air.W);
		float HalfW = P.Size;
		if (P.Kind == EKind::LeafBit) HalfW *= 0.3f + 0.7f * FMath::Abs(FMath::Cos(Clock * (5.f + 6.f * P.Seed) + P.Phase)); // tumbling
		const float Len = FMath::Max(2.f * P.Size, Speed * T.CueStretch);
		FLinearColor Col = P.Color * Bright;
		Col.A = Alpha;
		AddQuad(SpeckQuads, Air, Draw - Wd * (0.5f * Len), Draw + Wd * (0.5f * Len), HalfW, Col, 1.f, T.CueSpeckNear);
	}
}

void AWindCues::StepLocal(const FAir& Air, float Dt)
{
	// The ant's own air: specks and short streaks aimed past the ant, bending round the calm pocket where it is sheltered;
	// where they reach the bark near the ant (a face the wind hits) they slide along it. Motes hang in the pocket.
	const FWindTuning& T = Wind->Tuning;
	const int32 N = Local.Num();
	const int32 NStreak = N * 3 / 10, NMote = N * 15 / 100;
	const float R = FMath::Max(T.CueLocalRadius, 20.f);
	const float Spread = FMath::Max(T.CueLocalSpread, 5.f);
	const float Bright = FMath::Clamp(T.CueBrightness, 0.f, 1.f);
	// Two axes across the wind for the spawn disc.
	FVector P1 = Air.Across - Air.Flow * FVector::DotProduct(Air.Across, Air.Flow);
	P1 = P1.IsNearlyZero() ? FVector::CrossProduct(Air.Flow, FVector::UpVector).GetSafeNormal() : P1.GetSafeNormal();
	if (P1.IsNearlyZero()) P1 = FVector::RightVector;
	const FVector P2 = FVector::CrossProduct(Air.Flow, P1).GetSafeNormal();
	for (int32 i = 0; i < N; ++i)
	{
		FParticle& P = Local[i];
		if (!Air.bAnt)
		{
			P.MaxLife = -1.f;
			continue;
		}
		const bool bStreakSlot = i < NStreak;
		const bool bMoteSlot = !bStreakSlot && i < NStreak + NMote;
		FVector Rel = P.Pos - Air.AntPos;
		bool bDead = P.MaxLife < 0.f || P.Life >= P.MaxLife;
		if (!bDead && !bMoteSlot)
		{
			const bool bPast = P.bSlid || FVector::DotProduct(Rel, Air.Flow) > 0.0;
			bDead = (bPast && Rel.SizeSquared() > FMath::Square(1.15 * R)) || FVector::DotProduct(Rel, Air.AntN) < -2.0 || Rel.SizeSquared() > FMath::Square(3.0 * R);
		}
		if (!bDead && bMoteSlot) bDead = Rel.SizeSquared() > FMath::Square(2.0 * FMath::Max(Air.Pocket, 30.f));
		if (bDead)
		{
			P.Life = 0.f;
			P.Seed = Rand.FRandRange(0.05f, 1.f);
			P.Phase = Rand.FRandRange(0.f, 2.f * PI);
			P.bSlid = false;
			if (bMoteSlot)
			{
				P.Kind = EKind::Mote;
				P.Pos = Air.PocketCentre + Rand.GetUnitVector() * (0.8f * FMath::Max(Air.Pocket, 5.f) * FMath::Pow(Rand.FRand(), 0.33f));
				P.Dir = Rand.GetUnitVector();
				P.Speed = Rand.FRandRange(3.f, 10.f);
				P.MaxLife = Rand.FRandRange(1.5f, 3.5f);
				P.Size = T.CueSpeckSize * Rand.FRandRange(0.6f, 1.2f);
				P.Color = PollenColor(Rand);
			}
			else
			{
				// Through a disc across the wind round a point just above the ant (more of them near its axis), from D0 upwind.
				// Where the disc dips under the bark (wind along it), that part is mirrored above.
				const float D0 = R * Rand.FRandRange(0.8f, 1.2f);
				const float Rad = Spread * FMath::Pow(Rand.FRand(), 1.5f);
				const float Ang = Rand.FRandRange(0.f, 2.f * PI);
				FVector Off = P1 * (Rad * FMath::Cos(Ang)) + P2 * (Rad * FMath::Sin(Ang));
				const float Cross = T.CueLocalLift + (float)FVector::DotProduct(Off, Air.AntN);
				if (Cross < 1.5f) Off += Air.AntN * (2.f * (1.5f - Cross));
				P.Pos = Air.AntPos + Air.AntN * T.CueLocalLift + Off - Air.Flow * D0;
				P.Dir = Air.Flow;
				P.MaxLife = 4.f;
				P.Length = Rand.FRandRange(0.6f, 1.f);
				const bool bLeaf = !bStreakSlot && Rand.FRand() < T.CueLeafBitShare;
				P.Kind = bStreakSlot ? EKind::Streak : (bLeaf ? EKind::LeafBit : EKind::Speck);
				P.Size = bLeaf ? T.CueLeafBitSize * Rand.FRandRange(0.5f, 1.f) : T.CueSpeckSize * Rand.FRandRange(0.6f, 1.2f);
				P.Color = bLeaf ? LeafBitColor(Rand) : PollenColor(Rand);
			}
			Rel = P.Pos - Air.AntPos;
			const float Hgt = (float)FVector::DotProduct(Rel, Air.AntN);
			if (Hgt < 1.5f) P.Pos += Air.AntN * (1.5f - Hgt); // never start inside the bark
		}
		P.Life += Dt;

		if (P.Kind == EKind::Mote)
		{
			P.Pos += P.Dir * (P.Speed * Dt);
			const float Fade = FMath::Sin(PI * FMath::Clamp(P.Life / P.MaxLife, 0.f, 1.f));
			const float Alpha = T.CueMoteAlpha * Air.Shelter * Air.AntAir * Fade * FMath::SmoothStep(5.f, 20.f, Air.Pocket);
			if (Alpha <= 0.003f) continue;
			FLinearColor Col = P.Color * Bright;
			Col.A = Alpha;
			AddQuad(SpeckQuads, Air, P.Pos - P.Dir * P.Size, P.Pos + P.Dir * P.Size, P.Size, Col, 1.f, T.CueLocalNear);
			continue;
		}

		const float Vs = AirAt(Air, P.Pos);
		const float Amount = GustAmount(Vs);
		const float Speed = T.StreakSpeed * (0.03f + 0.9f * Vs * Vs) * (P.bSlid ? 0.6f : 1.f);
		P.Pos += P.Dir * (Speed * Dt);
		Rel = P.Pos - Air.AntPos;
		float Hgt = (float)FVector::DotProduct(Rel, Air.AntN);
		// Reaching the bark near the ant: slide along it (away from where it struck, on a face the wind hits square on).
		const FVector RelT = Rel - Air.AntN * Hgt;
		if (!P.bSlid && Hgt < 1.5f && FVector::DotProduct(P.Dir, Air.AntN) < -0.05 && RelT.SizeSquared() < FMath::Square(1.2f * Spread))
		{
			FVector Slide = P.Dir - Air.AntN * FVector::DotProduct(P.Dir, Air.AntN);
			Slide = Slide.SizeSquared() < 0.09 ? Air.Across : Slide.GetSafeNormal(); // square on: one way along the bark, so it reads as flow
			const FVector SideAxis = FVector::CrossProduct(Air.AntN, Slide);
			P.Dir = (Slide + SideAxis * ((P.Seed - 0.5f) * 0.7f) + Air.AntN * 0.08f).GetSafeNormal();
			P.Pos += Air.AntN * (1.5f - Hgt);
			P.bSlid = true;
			Rel = P.Pos - Air.AntPos;
			Hgt = 1.5f;
		}
		const float U = (float)FVector::DotProduct(Rel, Air.Flow) / R; // about -1 at the start
		const float Past = (P.bSlid || U > 0.f) ? (float)Rel.Size() / R : 0.f;
		const float In = P.bSlid ? 1.f : FMath::SmoothStep(-1.25f, -0.8f, U);
		const float Out = 1.f - FMath::SmoothStep(0.7f, 1.1f, Past);
		const float Vis = FMath::SmoothStep(P.Seed, P.Seed + 0.24f, Amount);
		float Alpha = Vis * In * Out * FMath::SmoothStep(-1.f, 2.f, Hgt) * FMath::Min(1.f, P.Life / 0.06f);
		if (Alpha <= 0.003f) continue;
		if (P.Kind == EKind::Streak)
		{
			const float Len = 0.15f * T.CueStreakLength * P.Length * (0.5f + 0.5f * Amount);
			const FVector A = PocketMap(Air, P.Pos, P.Seed, Air.Flow), B = PocketMap(Air, P.Pos + P.Dir * Len, P.Seed, Air.Flow);
			if (Air.Pocket > 2.f) Alpha *= FMath::SmoothStep(0.8f * Air.Pocket, 1.2f * Air.Pocket, PointSegmentDistance(Air.PocketCentre, A, B));
			const float HalfW = 0.3f * T.CueStreakWidth * (1.f + T.CueStreakThick * Amount);
			FLinearColor Col = T.CueStreakColor;
			Col.A = Alpha * T.CueStreakAlpha;
			AddQuad(StreakQuads, Air, A, B, HalfW, Col, T.CueStreakTail, T.CueLocalNear);
		}
		else
		{
			const FVector Draw = PocketMap(Air, P.Pos, P.Seed, Air.Flow);
			float HalfW = P.Size;
			if (P.Kind == EKind::LeafBit) HalfW *= 0.3f + 0.7f * FMath::Abs(FMath::Cos(Clock * (5.f + 6.f * P.Seed) + P.Phase));
			const float Len = FMath::Max(2.f * P.Size, Speed * T.CueStretch);
			FLinearColor Col = P.Color * Bright;
			Col.A = Alpha * T.CueSpeckAlpha;
			AddQuad(SpeckQuads, Air, Draw - P.Dir * (0.5f * Len), Draw + P.Dir * (0.5f * Len), HalfW, Col, 1.f, T.CueLocalNear);
		}
	}
}

void AWindCues::SpawnScrape(const FAir& Air, float Strength01, float BurstAngle)
{
	FParticle* Slot = nullptr;
	for (FParticle& P : Scrapes)
	{
		if (P.MaxLife < 0.f || P.Life >= P.MaxLife)
		{
			Slot = &P;
			break;
		}
	}
	if (!Slot) return;
	FParticle& P = *Slot;
	FVector Tan = Air.W - Air.AntN * FVector::DotProduct(Air.W, Air.AntN);
	Tan = Tan.SizeSquared() < 0.04 ? Air.Across : Tan.GetSafeNormal();
	const FVector Tan2 = FVector::CrossProduct(Air.AntN, Tan);
	if (BurstAngle >= 0.f)
	{
		// The claws dig in: grains burst out along the bark all round the ant, then the wind takes them.
		const FVector Out = Tan * FMath::Cos(BurstAngle) + Tan2 * FMath::Sin(BurstAngle);
		P.Pos = Air.AntPos + Air.AntN * 0.5f + Out * Rand.FRandRange(2.f, 5.f);
		P.Dir = (Out + Air.AntN * Rand.FRandRange(0.15f, 0.5f)).GetSafeNormal();
		P.Speed = Rand.FRandRange(60.f, 110.f);
	}
	else
	{
		// Scraped off while the wind pulls: blown downwind.
		P.Pos = Air.AntPos + Air.AntN * 0.5f + Tan2 * Rand.FRandRange(-4.f, 4.f) + Tan * Rand.FRandRange(-4.f, 4.f);
		P.Dir = (Tan * (0.4f + Strength01) + Air.AntN * Rand.FRandRange(0.4f, 1.f) + Tan2 * Rand.FRandRange(-0.5f, 0.5f)).GetSafeNormal();
		P.Speed = Rand.FRandRange(25.f, 60.f) + 260.f * Strength01;
	}
	P.Life = 0.f;
	P.MaxLife = Rand.FRandRange(0.5f, 0.9f);
	P.Size = Rand.FRandRange(0.5f, 0.9f);
	// Half light, half dark, so the dust shows on pale boughs and dark trunk bark alike (colours as drawn, not dimmed).
	P.Color = Rand.FRand() < 0.5f ? BarkDustColor : BarkDustDark;
	P.Kind = EKind::Scrape;
}

void AWindCues::StepScrapes(const FAir& Air, float Dt)
{
	// Gripping: a puff of bark dust as the claws dig in, then a trickle blown off while the wind pulls at the ant.
	const FWindTuning& T = Wind->Tuning;
	const AAntPawn* Ant = Cast<AAntPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
	const bool bGripping = Air.bAnt && Ant && Ant->IsGripping();
	if (bGripping)
	{
		const float Pull = FMath::Clamp(AirAt(Air, Air.AntPos) * Exposure, 0.f, 1.f);
		if (!bWasGripping)
		{
			for (int32 i = 0; i < T.CueGripPuff; ++i) SpawnScrape(Air, FMath::Max(Pull, 0.2f), (i + Rand.FRand()) * 2.f * PI / FMath::Max(T.CueGripPuff, 1));
		}
		TrickleDebt += Dt * T.CueGripTrickle * FMath::SmoothStep(0.1f, 0.6f, Pull);
		while (TrickleDebt >= 1.f)
		{
			TrickleDebt -= 1.f;
			SpawnScrape(Air, Pull, -1.f);
		}
	}
	else
	{
		TrickleDebt = 0.f;
	}
	bWasGripping = bGripping;

	for (FParticle& P : Scrapes)
	{
		if (P.MaxLife < 0.f || P.Life >= P.MaxLife) continue;
		P.Life += Dt;
		P.Speed *= FMath::Exp(-1.2f * Dt);
		P.Pos += P.Dir * (P.Speed * Dt) + Air.W * (180.f * Air.Strength * Dt);
		const float Alpha = 0.95f * FMath::Square(1.f - FMath::Clamp(P.Life / P.MaxLife, 0.f, 1.f));
		const float Len = FMath::Max(2.f * P.Size, P.Speed * T.CueStretch);
		FLinearColor Col = P.Color;
		Col.A = Alpha;
		AddQuad(SpeckQuads, Air, P.Pos - P.Dir * (0.5f * Len), P.Pos + P.Dir * (0.5f * Len), P.Size, Col, 1.f, T.CueLocalNear);
	}
}

void AWindCues::AddQuad(FQuads& Q, const FAir& Air, const FVector& A, const FVector& B, float HalfWidth, const FLinearColor& Col, float TailAlpha, float FadeNear)
{
	if (Q.Used >= Q.Capacity) return;
	const FVector Mid = (A + B) * 0.5;
	const FVector ToCam = Air.CamPos - Mid;
	const float Dist = (float)ToCam.Size();
	if (Dist < 0.5f * FadeNear || Dist < 8.f) return;
	const FVector S0 = FVector::CrossProduct(B - A, ToCam).GetSafeNormal();
	if (S0.IsNearlyZero()) return;
	// Never thinner than CueMinPixels on screen; nothing right in front of the lens.
	const float HW = FMath::Max(HalfWidth, 0.5f * Wind->Tuning.CueMinPixels * Air.PixelAngle * Dist);
	const FVector S = S0 * HW;
	FLinearColor Head = Col;
	Head.A *= FMath::SmoothStep(0.5f * FadeNear, FadeNear, Dist);
	FLinearColor Tail = Head;
	Tail.A *= TailAlpha;
	const int32 I = Q.Used * 4;
	Q.V[I] = A - S;
	Q.V[I + 1] = A + S;
	Q.V[I + 2] = B - S;
	Q.V[I + 3] = B + S;
	Q.C[I] = Tail;
	Q.C[I + 1] = Tail;
	Q.C[I + 2] = Head;
	Q.C[I + 3] = Head;
	++Q.Used;
}

void AWindCues::Flush(int32 Section, FQuads& Q, UMaterialInterface* Material, bool bCentredUV)
{
	// Unused quads collapse onto one point (no pixels), so the vertex count stays fixed and the section is only updated.
	const int32 NumV = Q.Capacity * 4;
	for (int32 i = Q.Used * 4; i < NumV; ++i)
	{
		Q.V[i] = ParkPos;
		Q.C[i] = FLinearColor::Transparent;
	}
	const int32 MaterialKey = bCentredUV ? 0 : 1;
	const FProcMeshSection* Existing = Mesh->GetProcMeshSection(Section);
	const bool bRebuild = !Existing || Existing->ProcVertexBuffer.Num() != NumV || (Section == 0 && StreakSectionMaterial != MaterialKey);
	if (!bRebuild)
	{
		Mesh->UpdateMeshSection_LinearColor(Section, Q.V, {}, {}, {}, {}, {}, Q.C, {}, false);
		return;
	}
	TArray<int32> Tri;
	TArray<FVector> N;
	TArray<FVector2D> UV;
	Tri.Reserve(Q.Capacity * 6);
	N.Init(FVector::UpVector, NumV);
	UV.Reserve(NumV);
	for (int32 k = 0; k < Q.Capacity; ++k)
	{
		const int32 Base = k * 4;
		// UE front faces: (b - a) x (c - a) points away from the viewer; AddQuad builds the sides so this order faces it.
		Tri.Append({ Base, Base + 2, Base + 1, Base + 1, Base + 2, Base + 3 });
		// M_Exposure: UV 0 at the centre, length 1 at the rim (a soft dot, an ellipse when stretched). M_WindStreak: 0..1.
		if (bCentredUV) UV.Append({ FVector2D(-1, -1), FVector2D(1, -1), FVector2D(-1, 1), FVector2D(1, 1) });
		else UV.Append({ FVector2D(0, 0), FVector2D(1, 0), FVector2D(0, 1), FVector2D(1, 1) });
	}
	Mesh->CreateMeshSection_LinearColor(Section, Q.V, Tri, N, UV, {}, {}, {}, Q.C, {}, false, false);
	if (Material) Mesh->SetMaterial(Section, Material);
	if (Section == 0) StreakSectionMaterial = MaterialKey;
}
