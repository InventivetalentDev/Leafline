#include "Ant/AntCamera.h"
#include "Ant/AntPawn.h"
#include "Ant/AntMovementComponent.h"
#include "Tree/TreeActor.h"
#include "Debug/LeafDebug.h"
#include "Camera/CameraComponent.h"

namespace
{
	FVector ProjectOnPlane(const FVector& V, const FVector& N)
	{
		return V - N * FVector::DotProduct(V, N);
	}

	FVector AnyPerpendicular(const FVector& N)
	{
		const FVector Ref = FMath::Abs(N.Z) < 0.9 ? FVector::UpVector : FVector::ForwardVector;
		return ProjectOnPlane(Ref, N).GetSafeNormal();
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
	CamUp = M->GetBodyUp();
	RefForward = ProjectOnPlane(M->GetBodyForward(), CamUp).GetSafeNormal();
	if (RefForward.IsNearlyZero()) RefForward = AnyPerpendicular(CamUp);
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
	const double Dt = FMath::Min(DeltaTime, 0.05f);
	const FVector AntUp = M->GetBodyUp();
	const FVector AntPos = M->GetSurfacePosition();

	// 1. Lazy up: ease toward the ant's up and carry the view direction along, so the view does not swing.
	const double UpAlpha = 1.0 - FMath::Exp(-Dt / FMath::Max(Tuning.UpTimeConstant, 0.01f));
	const FQuat UpDelta = FQuat::Slerp(FQuat::Identity, FQuat::FindBetweenNormals(CamUp, AntUp), UpAlpha);
	CamUp = UpDelta.RotateVector(CamUp).GetSafeNormal();
	RefForward = ProjectOnPlane(UpDelta.RotateVector(RefForward), CamUp).GetSafeNormal();
	if (RefForward.IsNearlyZero()) RefForward = AnyPerpendicular(CamUp);

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
		const FVector AntFwd = ProjectOnPlane(M->GetBodyForward(), CamUp).GetSafeNormal();
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
	double Want = Tuning.BoomLength;
	double HardMax = Tuning.BoomLength;
	if (Field && Field->IsValid())
	{
		double HitT;
		if (Field->March(Focus, Focus - View * Tuning.BoomLength, Tuning.ProbeRadius, HitT))
		{
			HardMax = HitT * Tuning.BoomLength;
			Want = FMath::Max(HardMax - 10.0, (double)Tuning.MinBoomLength);
		}
	}
	CurrentBoom = FMath::FInterpTo(CurrentBoom, (float)Want, (float)Dt, Want < CurrentBoom ? Tuning.BoomShrinkRate : Tuning.BoomGrowRate);
	CurrentBoom = FMath::Min(CurrentBoom, (float)FMath::Max(HardMax, 1.0));

	const FVector CamPos = Focus - View * CurrentBoom;
	SetActorLocationAndRotation(CamPos, FRotationMatrix::MakeFromXZ(View, CamUp).ToQuat());
}
