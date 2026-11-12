#include "Ant/AntCamera.h"
#include "Ant/AntPawn.h"
#include "Ant/AntMovementComponent.h"
#include "Tree/TreeActor.h"
#include "Debug/LeafDebug.h"
#include "Camera/CameraComponent.h"

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

void AAntCamera::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	AAntPawn* T = Target.Get();
	if (!T || !T->GetAntMovement()) return;
	if (!bHasSnapped) Snap();
	if (!Tree.IsValid()) Tree = ATreeActor::Get(this);

	const UAntMovementComponent* M = T->GetAntMovement();
	const double Dt = FMath::Min(DeltaTime, 0.1f);
	const FVector AntUp = M->GetBodyUp();
	const FVector AntPos = M->GetSurfacePosition();

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
	const FVector Goal = AntPos + AntUp * Tuning.TargetHeight;
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
	// Branches passing between camera and ant must not make the camera jump. The tree's faces are one-sided, so a
	// camera briefly behind or even inside a branch simply sees through it. Thin occluders are ignored; a thick one
	// (trunk, big limb) pulls the boom in smoothly after it has blocked the view for a moment.
	double Want = Tuning.BoomLength;
	float& OccludedTime = SpareFloat;
	bool bThickOcclusion = false;
	if (Field && Field->IsValid())
	{
		double HitT;
		if (Field->March(Focus, Focus - View * Tuning.BoomLength, Tuning.ProbeRadius, HitT))
		{
			const double HitDist = HitT * Tuning.BoomLength;
			// Thickness of the obstruction along the ray: step through it until outside again.
			double Exit = HitDist;
			for (int32 i = 0; i < 24 && Exit < Tuning.BoomLength; ++i)
			{
				const double D = Field->Distance(Focus - View * Exit);
				if (D > Tuning.ProbeRadius) break;
				Exit += FMath::Max(-D, 0.0) + 4.0;
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

	const FVector CamPos = Focus - View * CurrentBoom;
	SetActorLocationAndRotation(CamPos, FRotationMatrix::MakeFromXZ(View, CamUp).ToQuat());
	Camera->SetFieldOfView(Tuning.FieldOfView);
}
