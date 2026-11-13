#include "Ant/AntCamera.h"
#include "Ant/AntPawn.h"
#include "Ant/AntMovementComponent.h"
#include "Tree/TreeActor.h"
#include "Debug/LeafDebug.h"
#include "Camera/CameraComponent.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialParameterCollection.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	FVector CamProjectOnPlane(const FVector& V, const FVector& N)
	{
		return V - N * FVector::DotProduct(V, N);
	}

	FVector CamAnyPerpendicular(const FVector& N)
	{
		const FVector Ref = FMath::Abs(N.Z) < 0.9 ? FVector::UpVector : FVector::ForwardVector;
		return CamProjectOnPlane(Ref, N).GetSafeNormal();
	}

	/** Critically damped spring step (stable for any dt). */
	void SpringDamp(FVector& X, FVector& V, const FVector& Goal, double Omega, double Dt)
	{
		const double W = Omega * Dt;
		const double Exp = 1.0 / (1.0 + W + 0.48 * W * W + 0.235 * W * W * W);
		const FVector Change = X - Goal;
		const FVector Temp = (V + Change * Omega) * Dt;
		V = (V - Temp * Omega) * Exp;
		X = Goal + (Change + Temp) * Exp;
	}

	/** Signed angle from A to B about Axis, in radians. */
	double SignedAngle(const FVector& A, const FVector& B, const FVector& Axis)
	{
		return FMath::Atan2(FVector::DotProduct(FVector::CrossProduct(A, B), Axis), FVector::DotProduct(A, B));
	}
}

AAntCamera::AAntCamera()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	RootComponent = Camera;
	Camera->SetFieldOfView(Tuning.FieldOfView);
	Camera->bConstrainAspectRatio = false;
	static ConstructorHelpers::FObjectFinder<UMaterialParameterCollection> ViewMPC(TEXT("/Game/Leafline/Materials/MPC_View.MPC_View"));
	if (ViewMPC.Succeeded()) ViewCollection = ViewMPC.Object;
}

void AAntCamera::PublishView(const FVector& CamPos, const FVector& AntFocus, bool bCutaway)
{
	if (!ViewCollection) return;
	UKismetMaterialLibrary::SetVectorParameterValue(this, ViewCollection, TEXT("CamPos"), FLinearColor(CamPos));
	UKismetMaterialLibrary::SetVectorParameterValue(this, ViewCollection, TEXT("AntFocus"), FLinearColor(AntFocus));
	UKismetMaterialLibrary::SetScalarParameterValue(this, ViewCollection, TEXT("CutawayOn"), bCutaway && Tuning.CutawayRadius > 0.f ? 1.f : 0.f);
	UKismetMaterialLibrary::SetScalarParameterValue(this, ViewCollection, TEXT("CutawayRadius"), Tuning.CutawayRadius);
	UKismetMaterialLibrary::SetScalarParameterValue(this, ViewCollection, TEXT("CutawayBand"), FMath::Clamp(Tuning.CutawayBand, 0.02f, 1.f));
	UKismetMaterialLibrary::SetScalarParameterValue(this, ViewCollection, TEXT("CutawayTaper"), FMath::Max(Tuning.CutawayTaper, 0.05f));
	UKismetMaterialLibrary::SetScalarParameterValue(this, ViewCollection, TEXT("NearFadeStart"), Tuning.NearFadeStart);
	UKismetMaterialLibrary::SetScalarParameterValue(this, ViewCollection, TEXT("NearFadeEnd"), FMath::Max(Tuning.NearFadeEnd, Tuning.NearFadeStart + 1.f));
}

void AAntCamera::SetTarget(AAntPawn* NewTarget, bool bGlide)
{
	Target = NewTarget;
	if (!bGlide || !bHasSnapped)
	{
		// Snap on the next tick: at possession time the ant has not been placed on the surface yet.
		bHasSnapped = false;
	}
	else
	{
		GlideRemaining = Tuning.GlideTime;
	}
}

void AAntCamera::Snap()
{
	AAntPawn* T = Target.Get();
	if (!T || !T->GetAntMovement()) return;
	const UAntMovementComponent* M = T->GetAntMovement();
	CamUp = FVector::UpVector;
	SmoothedRadius = 1000.f;
	RefForward = CamProjectOnPlane(M->GetBodyForward(), CamUp).GetSafeNormal();
	if (RefForward.IsNearlyZero()) RefForward = CamAnyPerpendicular(CamUp);
	Pitch = Tuning.DefaultPitch;
	Focus = M->GetSurfacePosition() + CamUp * Tuning.TargetHeight;
	FocusVelocity = FVector::ZeroVector;
	CurrentBoom = Tuning.BoomLength;
	Camera->SetFieldOfView(Tuning.FieldOfView);
	bHasSnapped = true;
}

void AAntCamera::SetOverview(bool bOn, const FVector& Location, const FVector& LookAt)
{
	if (!bOn)
	{
		if (bOverview)
		{
			bOverview = false;
			bHasSnapped = false; // back on the ant: snap behind it
		}
		return;
	}
	if (!bOverview) bOverviewPlaced = false;
	bOverview = true;
	OverviewLoc = Location;
	OverviewLook = LookAt;
}

void AAntCamera::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (bOverview)
	{
		// Glide (or cut, the first time) to the overview spot; no cutaway, no ant.
		const float Dt = FMath::Min(DeltaTime, 0.1f);
		const FVector Loc = bOverviewPlaced ? FMath::VInterpTo(GetActorLocation(), OverviewLoc, Dt, 2.2f) : OverviewLoc;
		const FRotator Want = (OverviewLook - Loc).Rotation();
		const FRotator Rot = bOverviewPlaced ? FMath::RInterpTo(GetActorRotation(), Want, Dt, 3.f) : Want;
		SetActorLocationAndRotation(Loc, Rot);
		bOverviewPlaced = true;
		PublishView(Loc, OverviewLook, false);
		return;
	}
	AAntPawn* T = Target.Get();
	if (!T || !T->GetAntMovement())
	{
		PublishView(GetActorLocation(), GetActorLocation(), false);
		return;
	}
	if (!bHasSnapped) Snap();
	if (!Tree.IsValid()) Tree = ATreeActor::Get(this);

	const UAntMovementComponent* M = T->GetAntMovement();
	const double Dt = FMath::Min(DeltaTime, 0.1f);
	const FVector AntUp = M->GetBodyUp();
	const FVector AntPos = M->GetSurfacePosition();

	// Blown off: the orbit camera, built around a surface, lost the tumbling ant against the bark. Watch the fall from
	// outside and above instead, on the side it was blown off (its last surface normal), and hand back on landing with
	// the orbit state matching this view.
	if (M->GetState() == EAntState::Falling)
	{
		FVector Out = M->GetSurfaceNormal();
		Out.Z = 0.0;
		if (Out.IsNearlyZero()) Out = -GetActorForwardVector().GetSafeNormal2D();
		Out = Out.GetSafeNormal();
		const FVector Want = AntPos + Out * (Tuning.BoomLength * 0.9) + FVector::UpVector * (Tuning.BoomLength * 1.2);
		const FVector CamPos = FMath::Lerp(GetActorLocation(), Want, 1.0 - FMath::Exp(-6.0 * Dt));
		const FVector View = (AntPos - CamPos).GetSafeNormal();
		if (!View.IsNearlyZero())
		{
			SetActorLocationAndRotation(CamPos, FRotationMatrix::MakeFromXZ(View, FVector::UpVector).ToQuat());
			CamUp = FVector::UpVector;
			const FVector Flat = View.GetSafeNormal2D();
			if (!Flat.IsNearlyZero()) RefForward = Flat;
			Pitch = FMath::Clamp((float)FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(View.Z, -1.0, 1.0))), Tuning.PitchMin, Tuning.PitchMax);
			CurrentBoom = (float)FVector::Dist(CamPos, AntPos);
			Focus = AntPos + CamUp * Tuning.TargetHeight;
			FocusVelocity = FVector::ZeroVector;
			PendingLook = FVector2D::ZeroVector;
		}
		PublishView(CamPos, AntPos, false);
		return;
	}

	// 1. Lazy up. The target is world up on thin branches (circling a twig must never roll the view) and the surface
	//    up on big ones (so the camera stays on the ant's side of a limb's underside). Ease toward it with a capped
	//    turn rate, and carry the view direction along so the view does not swing.
	float Radius = 100000.f; // ground and unknown: treat as flat
	if (Tree.IsValid())
	{
		const TArray<FTreeBranch>& Branches = Tree->GetSkeleton().Branches;
		const int32 B = M->GetCurrentBranch();
		if (Branches.IsValidIndex(B))
		{
			double Best = TNumericLimits<double>::Max();
			for (int32 i = 0; i < Branches[B].Samples.Num(); ++i)
			{
				const double D2 = FVector::DistSquared(AntPos, Branches[B].Samples[i]);
				if (D2 < Best)
				{
					Best = D2;
					Radius = Branches[B].SampleRadius[i];
				}
			}
		}
	}
	SmoothedRadius = FMath::FInterpTo(SmoothedRadius, FMath::Min(Radius, 5000.f), (float)Dt, 3.f);
	const float Follow = FMath::Clamp(Tuning.SurfaceFollow, 0.f, 1.f) *
		FMath::SmoothStep(Tuning.SurfaceUpRadiusMin, FMath::Max(Tuning.SurfaceUpRadiusMax, Tuning.SurfaceUpRadiusMin + 1.f), SmoothedRadius);
	const FVector TargetUp = FQuat::Slerp(FQuat::Identity, FQuat::FindBetweenNormals(FVector::UpVector, AntUp), Follow)
		.RotateVector(FVector::UpVector).GetSafeNormal();

	const double UpAlpha = 1.0 - FMath::Exp(-Dt / FMath::Max(Tuning.UpTimeConstant, 0.01f));
	FQuat UpDelta = FQuat::Slerp(FQuat::Identity, FQuat::FindBetweenNormals(CamUp, TargetUp), UpAlpha);
	{
		FVector Axis;
		double Angle;
		UpDelta.ToAxisAndAngle(Axis, Angle);
		const double MaxAngle = FMath::DegreesToRadians(Tuning.MaxUpTurnRate) * Dt;
		if (Angle > MaxAngle) UpDelta = FQuat(Axis, MaxAngle);
	}
	CamUp = UpDelta.RotateVector(CamUp).GetSafeNormal();
	RefForward = CamProjectOnPlane(UpDelta.RotateVector(RefForward), CamUp).GetSafeNormal();
	if (RefForward.IsNearlyZero()) RefForward = CamAnyPerpendicular(CamUp);

	// 2. Orbit.
	double Yaw = PendingLook.X;
	const double PitchDelta = PendingLook.Y;
	PendingLook = FVector2D::ZeroVector;
	if (LeafDebug::AutoTurn != 0.f)
	{
		double Rate = LeafDebug::AutoTurn;
		if (LeafDebug::bAutoWander)
		{
			WanderTimer -= Dt;
			if (WanderTimer <= 0.f)
			{
				WanderSign = FMath::RandBool() ? 1.f : -1.f;
				WanderTimer = FMath::FRandRange(3.f, 9.f);
			}
			Rate *= WanderSign;
		}
		Yaw += Rate * Dt;
	}
	if (Tuning.AutoAlignRate > 0.f && M->GetVelocity().SizeSquared() > 400.0)
	{
		const FVector AntFwd = CamProjectOnPlane(M->GetBodyForward(), CamUp).GetSafeNormal();
		if (!AntFwd.IsNearlyZero())
		{
			Yaw += FMath::RadiansToDegrees(SignedAngle(RefForward, AntFwd, CamUp)) * (1.0 - FMath::Exp(-Tuning.AutoAlignRate * Dt));
		}
	}
	RefForward = FQuat(CamUp, FMath::DegreesToRadians(Yaw)).RotateVector(RefForward).GetSafeNormal();
	Pitch = FMath::Clamp(Pitch + (float)PitchDelta, Tuning.PitchMin, Tuning.PitchMax);

	// 3. Spring-follow the look-at point, and keep it out of the tree when it cuts a corner.
	// Look at a point above the ant along the camera's own lazy up, not the ant's: circling a twig turns the ant's up
	// all the way round several times a second, and a look-at point riding on it made the camera jump.
	const FVector Goal = AntPos + CamUp * Tuning.TargetHeight;
	double Omega = Tuning.FollowFrequency;
	if (GlideRemaining > 0.f)
	{
		GlideRemaining -= Dt;
		Omega = Tuning.GlideFrequency;
	}
	SpringDamp(Focus, FocusVelocity, Goal, Omega, Dt);

	const FTreeField* Field = Tree.IsValid() ? &Tree->GetField() : nullptr;
	const double MinClear = Tuning.TargetHeight * 0.5;
	if (Field && Field->IsValid())
	{
		// If wood lies between the ant and the focus (after a teleport or a switch across the trunk), bring the
		// focus to the ant's side; otherwise spring and push-out can balance on the wrong side of the branch.
		double BlockT;
		if (Field->March(Goal, Focus, 2.0, BlockT) && BlockT < 1.0)
		{
			Focus = Goal + (Focus - Goal) * (BlockT * 0.8);
			FocusVelocity = FVector::ZeroVector;
		}
		double D;
		const FVector G = Field->RawGradient(Focus, &D);
		if (D < MinClear && G.SizeSquared() > 1e-8)
		{
			Focus += G.GetSafeNormal() * (MinClear - D);
		}
	}

	// 4. Boom, shortened smoothly when the tree is in the way.
	const double PitchRad = FMath::DegreesToRadians(Pitch);
	const FVector View = (RefForward * FMath::Cos(PitchRad) + CamUp * FMath::Sin(PitchRad)).GetSafeNormal();
	// Wood between camera and ant must not make the camera jump. The tree's faces are one-sided, so a camera behind or
	// inside a branch sees through it, and the bark material dithers away whatever lies along the line to the ant.
	// Optionally (bAvoidThickOcclusion) a thick occluder pulls the boom in after it has blocked the view for a moment.
	double Want = Tuning.BoomLength;
	float& OccludedTime = SpareFloat;
	bool bThickOcclusion = false;
	if (Tuning.bAvoidThickOcclusion && Field && Field->IsValid())
	{
		double HitT;
		if (Field->March(Focus, Focus - View * Tuning.BoomLength, Tuning.ProbeRadius, HitT))
		{
			const double HitDist = HitT * Tuning.BoomLength;
			// Thickness of the obstruction along the ray: step through it until outside again.
			// March reports contact within 0.5 cm of the probe, so walk while touching or inside.
			double Exit = HitDist;
			for (int32 i = 0; i < 40 && Exit < Tuning.BoomLength; ++i)
			{
				const double Gap = Field->Distance(Focus - View * Exit) - Tuning.ProbeRadius;
				if (Gap > 1.0) break;
				Exit += FMath::Max(FMath::Abs(Gap), 2.0);
			}
			if (Exit - HitDist > 60.0 || Exit >= Tuning.BoomLength)
			{
				bThickOcclusion = true;
				if (OccludedTime > 0.25f) Want = FMath::Max(HitDist - 10.0, (double)Tuning.MinBoomLength);
				else Want = CurrentBoom;
			}
		}
	}
	OccludedTime = bThickOcclusion ? OccludedTime + (float)Dt : 0.f;
	CurrentBoom = FMath::FInterpTo(CurrentBoom, (float)Want, (float)Dt, Want < CurrentBoom ? Tuning.BoomShrinkRate : Tuning.BoomGrowRate);
	// Never below the ground: looking up from low down, the boom shortens at once so the camera stays above it.
	if (View.Z > 0.01)
	{
		const double GroundZ = Field ? Field->GroundZ : 0.0;
		const double MaxBoom = (Focus.Z - GroundZ - Tuning.GroundClearance) / View.Z;
		CurrentBoom = FMath::Min(CurrentBoom, (float)FMath::Max(MaxBoom, (double)Tuning.MinBoomLength));
	}

	FVector CamPos = Focus - View * CurrentBoom;
	FVector LookDir = View;
	// Look-up assist: rather than going under the ant's own surface, the camera comes in along the view line until it
	// is LookUpClearance above that surface (no nearer than LookUpMinBoom), stays above it, and looks along the pitched
	// view, turned back toward the ant only as far as needed to keep the ant near the bottom of the frame.
	bool bAssisted = false;
	if (Tuning.bLookUpAssist)
	{
		const double Clear = Tuning.LookUpClearance;
		if (FVector::DotProduct(CamPos - AntPos, AntUp) < Clear)
		{
			const double FocusH = FVector::DotProduct(Focus - AntPos, AntUp);
			const double Rise = FVector::DotProduct(View, AntUp);
			double L = CurrentBoom;
			if (Rise > 0.01) L = FMath::Clamp((FocusH - Clear) / Rise, (double)FMath::Min(Tuning.LookUpMinBoom, CurrentBoom), (double)CurrentBoom);
			CamPos = Focus - View * L;
			const double H = FVector::DotProduct(CamPos - AntPos, AntUp);
			if (H < Clear) CamPos += AntUp * (Clear - H);
			const FVector ToAnt = (AntPos - CamPos).GetSafeNormal();
			// Half the vertical field of view at 16:9, less a margin so the ant stays inside the frame.
			const double HalfV = FMath::Atan(FMath::Tan(FMath::DegreesToRadians(Tuning.FieldOfView * 0.5)) * 9.0 / 16.0);
			const double MaxOff = HalfV * 0.75;
			const double Off = FMath::Acos(FMath::Clamp(FVector::DotProduct(ToAnt, View), -1.0, 1.0));
			LookDir = Off > MaxOff && Off > 1e-3 ? FQuat::Slerp(FQuat::Identity, FQuat::FindBetweenNormals(ToAnt, View), MaxOff / Off).RotateVector(ToAnt).GetSafeNormal() : View;
			bAssisted = true;
		}
	}
	// Not inside the trunk: looking up the trunk put the camera in the wood, and the cutaway then showed the ant
	// floating. Where the camera would be inside (or right at) thick wood, it slides out onto the bark instead.
	if (!bAssisted && Tuning.ThickWoodRadius > 0.f && Field && Field->IsValid() && Tree.IsValid())
	{
		int32 B = INDEX_NONE;
		if (Field->Distance(CamPos, &B) < Tuning.WoodClearance && Tree->GetSkeleton().Branches.IsValidIndex(B))
		{
			const FTreeBranch& Br = Tree->GetSkeleton().Branches[B];
			double Best = TNumericLimits<double>::Max();
			float R = 0.f;
			for (int32 i = 0; i < Br.Samples.Num(); ++i)
			{
				const double D2 = FVector::DistSquared(CamPos, Br.Samples[i]);
				if (D2 < Best)
				{
					Best = D2;
					R = Br.SampleRadius[i];
				}
			}
			FVector P = CamPos, N;
			if (R >= Tuning.ThickWoodRadius && Field->Project(P, N, Tuning.WoodClearance))
			{
				CamPos = P;
				const FVector ToFocus = Focus - CamPos;
				if (ToFocus.SizeSquared() > 1.0) LookDir = ToFocus.GetSafeNormal();
			}
		}
	}
	SetActorLocationAndRotation(CamPos, FRotationMatrix::MakeFromXZ(LookDir, CamUp).ToQuat());
	// The cutaway aims at the ant's body, not at the look-at point: that one sits above the ant and lags behind it on a
	// spring, so the see-through hole in the bark was often beside the ant (owner note).
	PublishView(CamPos, AntPos + AntUp * 3.0, true);
	Camera->SetFieldOfView(Tuning.FieldOfView);
	FPostProcessSettings& PP = Camera->PostProcessSettings;
	PP.bOverride_DepthOfFieldFocalDistance = Tuning.DepthOfFieldFstop > 0.f;
	PP.bOverride_DepthOfFieldFstop = Tuning.DepthOfFieldFstop > 0.f;
	PP.DepthOfFieldFocalDistance = FMath::Max(CurrentBoom, 10.f);
	PP.DepthOfFieldFstop = FMath::Max(Tuning.DepthOfFieldFstop, 0.7f);
	PP.bOverride_AutoExposureBias = true;
	PP.AutoExposureBias = Tuning.ExposureBias;
	const bool bExposureRange = Tuning.ExposureMinEV > -99.f || Tuning.ExposureMaxEV > -99.f;
	PP.bOverride_AutoExposureMinBrightness = bExposureRange;
	PP.bOverride_AutoExposureMaxBrightness = bExposureRange;
	if (bExposureRange)
	{
		PP.AutoExposureMinBrightness = Tuning.ExposureMinEV;
		PP.AutoExposureMaxBrightness = FMath::Max(Tuning.ExposureMaxEV, Tuning.ExposureMinEV);
	}
}

void AAntCamera::DebugAimDirection(const FVector& WorldDir, float NewPitch)
{
	const FVector F = CamProjectOnPlane(WorldDir, CamUp).GetSafeNormal();
	if (!F.IsNearlyZero()) RefForward = F;
	Pitch = FMath::Clamp(NewPitch, Tuning.PitchMin, Tuning.PitchMax);
}
