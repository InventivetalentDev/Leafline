#include "Ant/AntMovementComponent.h"
#include "AntTreeGame.h"
#include "Tree/TreeActor.h"
#include "Tree/TreeSkeleton.h"
#include "Debug/LeafDebug.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

namespace
{
	FVector ProjectOnPlane(const FVector& V, const FVector& N)
	{
		return V - N * FVector::DotProduct(V, N);
	}

	FVector SlerpNormal(const FVector& From, const FVector& To, double Alpha)
	{
		const FQuat Q = FQuat::FindBetweenNormals(From, To);
		return FQuat::Slerp(FQuat::Identity, Q, Alpha).RotateVector(From).GetSafeNormal();
	}

	FVector MoveTowards(const FVector& Current, const FVector& Target, double MaxDelta)
	{
		const FVector Delta = Target - Current;
		const double Len = Delta.Size();
		return Len <= MaxDelta || Len < UE_SMALL_NUMBER ? Target : Current + Delta * (MaxDelta / Len);
	}

	FVector AnyPerpendicular(const FVector& N)
	{
		const FVector Ref = FMath::Abs(N.Z) < 0.9 ? FVector::UpVector : FVector::ForwardVector;
		return ProjectOnPlane(Ref, N).GetSafeNormal();
	}

	/** Window start points for net-displacement telemetry (file-local so Live Coding never adds members). */
	struct FTelemetryScratch
	{
		FVector WindowStart = FVector::ZeroVector;
		FVector StallStart = FVector::ZeroVector;
		bool bInit = false;
	};
	TMap<const UAntMovementComponent*, FTelemetryScratch> GTelemetry;

	/** Control trace state per ant (file-local, like GTelemetry). */
	struct FControlTrace
	{
		int32 LastBranch = INDEX_NONE;
		float LastRadius = 0.f;
		FString LastRule;
		FVector2D LastFwdScreen = FVector2D::ZeroVector;
		FVector LastCamForward = FVector::ForwardVector;
		double OffSince = -1.0;
		double LastOffLog = -10.0;
		double LastJunctionLog = -10.0;
		int32 JunctionLogsThisSecond = 0;
	};
	TMap<const UAntMovementComponent*, FControlTrace> GControlTrace;

	/** Leaf.FollowDiag 1: followers and walkers home that make no headway log what each step does (fillet-walk diagnosis). */
	int32 GFollowDiagOn = 0;
	FAutoConsoleVariableRef CVarFollowDiag(TEXT("Leaf.FollowDiag"), GFollowDiagOn,
		TEXT("Leaf.FollowDiag 0|1: followers (recruits, riders) and ants walking home that make no headway log each step's details."));
	struct FFollowDiag
	{
		FVector ProgressPos = FVector::ZeroVector;
		double Clock = 0.0;
		double ProgressTime = 0.0;
		double LastLog = -10.0;
		int32 Logged = 0;
	};
	TMap<const UAntMovementComponent*, FFollowDiag> GFollowDiag;

	/** True when this ant should log now (no 20 cm of headway for 4 s; then every 0.25 s, at most 60 lines a stall). */
	bool FollowDiagDue(const UAntMovementComponent* M, const FVector& Pos, float Dt)
	{
		if (!GFollowDiagOn) return false;
		FFollowDiag& D = GFollowDiag.FindOrAdd(M);
		D.Clock += Dt;
		if (FVector::DistSquared(Pos, D.ProgressPos) > 20.0 * 20.0)
		{
			if (D.Logged > 0) UE_LOG(LogLeafline, Log, TEXT("FollowDiag %s: moving again after %d lines"), *M->GetOwner()->GetName(), D.Logged);
			D.ProgressPos = Pos;
			D.ProgressTime = D.Clock;
			D.Logged = 0;
			return false;
		}
		if (D.Clock - D.ProgressTime < 4.0 || D.Logged >= 60) return false;
		if (D.Logged >= 12 && D.Clock - D.LastLog < 0.25) return false;
		D.LastLog = D.Clock;
		++D.Logged;
		return true;
	}
}

UAntMovementComponent::UAntMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

const FTreeField* UAntMovementComponent::GetField()
{
	if (!Tree.IsValid()) Tree = ATreeActor::Get(this);
	return Tree.IsValid() ? &Tree->GetField() : nullptr;
}

void UAntMovementComponent::BeginPlay()
{
	Super::BeginPlay();
	Pos = GetOwner()->GetActorLocation();
	Forward = GetOwner()->GetActorForwardVector();
	SnapToSurface();
}

void UAntMovementComponent::SnapToSurface()
{
	const FTreeField* F = GetField();
	if (!F || !F->IsValid()) return;
	FVector N;
	double Err = 0.0;
	if (!F->Project(Pos, N, Tuning.RideHeight, &Err))
	{
		// A symmetric spot can have no gradient: nudge off it and try again.
		Pos += FVector(0.7, 0.3, 1.1);
		if (!F->Project(Pos, N, Tuning.RideHeight, &Err))
		{
			UE_LOG(LogLeafline, Warning, TEXT("%s: could not place the ant on the surface at %s (error %.2f cm)"),
				*GetOwner()->GetName(), *Pos.ToString(), Err);
		}
	}
	SurfaceNormal = N;
	Up = N;
	Forward = ProjectOnPlane(Forward, N).GetSafeNormal();
	if (Forward.IsNearlyZero()) Forward = AnyPerpendicular(N);
	Velocity = FVector::ZeroVector;
	bPlaced = true;
	ApplyTransform();
}

void UAntMovementComponent::SetMoveInput(const FVector2D& Input, const FVector& InCamForward, const FVector& InCamUp, const FVector& InCamLocation)
{
	MoveInput = Input.GetClampedToMaxSize(1.0);
	CamForward = InCamForward;
	CamUp = InCamUp;
	CamPos = InCamLocation;
	bHasCamPos = !InCamLocation.IsZero();
}

void UAntMovementComponent::SetState(EAntState NewState)
{
	if (NewState == State) return;
	UE_LOG(LogLeafline, Log, TEXT("%s: state %s -> %s"), *GetOwner()->GetName(),
		*UEnum::GetValueAsString(State), *UEnum::GetValueAsString(NewState));
	State = NewState;
}

void UAntMovementComponent::TeleportTo(const FVector& Location, const FVector& Facing)
{
	Pos = Location;
	Forward = Facing.GetSafeNormal();
	bPlaced = false;
	SnapToSurface();
}

bool UAntMovementComponent::PlaceOnBranch(int32 Branch, float T)
{
	GetField();
	if (!Tree.IsValid()) return false;
	const FTreeSkeleton& S = Tree->GetSkeleton();
	if (!S.Branches.IsValidIndex(Branch)) return false;
	float R;
	FVector Tan;
	const FVector C = TreeSkeleton::SampleAt(S.Branches[Branch], T, &R, &Tan);
	FVector Side = ProjectOnPlane(FVector::UpVector, Tan);
	if (Side.SizeSquared() < 1e-4) Side = ProjectOnPlane(FVector::ForwardVector, Tan);
	Side.Normalize();
	TeleportTo(C + Side * (R + 30.0), Tan);
	UE_LOG(LogLeafline, Log, TEXT("Teleported to branch %d (level %d) at t=%.2f, radius %.0f cm"), Branch, S.Branches[Branch].Level, T, R);
	return true;
}

void UAntMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (bExternalStep) return;
	Step(DeltaTime, true);
}

void UAntMovementComponent::Step(float DeltaTime, bool bApplyTransform)
{
	const FTreeField* F = GetField();
	if (!F || !F->IsValid()) return;
	if (!bPlaced) SnapToSurface();

	// Substeps keep long frames safe, so only clamp hitches (below 10 fps) instead of slowing the game below 20.
	const float Dt = FMath::Min(DeltaTime, 0.1f);
	switch (State)
	{
	case EAntState::Walking:
	case EAntState::WalkingHome:
		TickWalking(Dt);
		break;
	case EAntState::Falling:
		TickFalling(Dt);
		break;
	case EAntState::Riding:
	case EAntState::Returning:
		TickFollow(Dt);
		break;
	default:
		TickRooted(Dt);
		break;
	}
	if (bApplyTransform) ApplyTransform();

	if (LeafDebug::bDebugField)
	{
		const UWorld* World = GetWorld();
		DrawDebugLine(World, Pos, Pos + SurfaceNormal * 40.0, FColor::Green, false, -1.f, 0, 0.6f);
		DrawDebugLine(World, Pos, Pos + Up * 30.0, FColor::Blue, false, -1.f, 0, 0.6f);
		DrawDebugLine(World, Pos, Pos + Forward * 30.0, FColor::Red, false, -1.f, 0, 0.6f);
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage((uint64)GetUniqueID(), 0.f, FColor::White,
				FString::Printf(TEXT("proj err %.3f cm  branch %d  speed %.0f  stamina %.2f"), LastProjError, CurrentBranch, Velocity.Size(), Stamina));
		}
	}
}

FVector UAntMovementComponent::ComputeWish(const FVector& N) const
{
	// Screen-forward on the surface: away from the camera plus screen-up, both projected onto the tangent plane.
	// Where that projection vanishes (one spot on every thin branch), normalising it would flip its direction from
	// step to step and the ant would vibrate in place. Fade the forward input out there instead.
	// Screen-up only helps when the camera looks at the surface from its front side; seen from behind the surface
	// plane (camera below a branch looking up) it points back toward the camera and would reverse the controls.
	const double Facing = FMath::Clamp(-FVector::DotProduct(CamForward, N) * 2.0, 0.0, 1.0);
	const FVector RawFwd = ProjectOnPlane(CamForward, N) + ProjectOnPlane(CamUp, N) * Facing;
	const double FwdLen = RawFwd.Size();
	const FVector CamRight = FVector::CrossProduct(CamUp, CamForward);
	FVector Fwd, Right;
	if (FwdLen > 0.1)
	{
		Fwd = RawFwd / FwdLen;
		Right = FVector::CrossProduct(N, Fwd);
	}
	else
	{
		Right = ProjectOnPlane(CamRight, N).GetSafeNormal();
		if (Right.IsNearlyZero()) Right = FVector::CrossProduct(N, ProjectOnPlane(Forward, N).GetSafeNormal());
		Fwd = FVector::CrossProduct(Right, N);
	}
	// D must always move toward screen right.
	if (FVector::DotProduct(Right, CamRight) < 0.0) Right = -Right;
	const double FwdGain = FMath::Clamp(FwdLen / 0.35, 0.0, 1.0);
	return (Fwd * (MoveInput.Y * FwdGain) + Right * MoveInput.X).GetClampedToMaxSize(1.0);
}

float UAntMovementComponent::MakeExactScreenBasis(const FVector& N, const FVector& Axis, FVector& OutFwd, FVector& OutRight) const
{
	OutFwd = OutRight = FVector::ZeroVector;
	const FVector CamRightV = FVector::CrossProduct(CamUp, CamForward);
	// Two unit tangent directions at the ant: along the branch (or screen-up on the bark) and across it.
	FVector T1 = Axis.IsNearlyZero() ? FVector::ZeroVector : ProjectOnPlane(Axis, N);
	if (T1.SizeSquared() < 0.04) T1 = ProjectOnPlane(CamUp, N);
	if (T1.SizeSquared() < 0.04) T1 = ProjectOnPlane(CamRightV, N);
	if (T1.SizeSquared() < 1e-6) return 0.f;
	T1.Normalize();
	const FVector T2 = FVector::CrossProduct(N, T1).GetSafeNormal();
	// A direction's motion on screen at the ant (x right, y up), with perspective: the derivative of the projection,
	// up to the common 1/depth factor. Without a camera position, orthographic.
	double SX = 0.0, SY = 0.0;
	if (bHasCamPos)
	{
		const FVector Rel = Pos - CamPos;
		const double Z = FMath::Max(FVector::DotProduct(Rel, CamForward), 10.0);
		SX = FVector::DotProduct(Rel, CamRightV) / Z;
		SY = FVector::DotProduct(Rel, CamUp) / Z;
	}
	auto OnScreen = [&](const FVector& V)
	{
		const double VF = FVector::DotProduct(V, CamForward);
		return FVector2D(FVector::DotProduct(V, CamRightV) - SX * VF, FVector::DotProduct(V, CamUp) - SY * VF);
	};
	// J = [img(T1) img(T2)]; the tangent direction c1 T1 + c2 T2 shows on screen as J c, so "up" is J^-1 (0, 1) and
	// "right" J^-1 (1, 0): up = (-b T1 + a T2) / det, right = (d T1 - c T2) / det for J = [[a, b], [c, d]].
	const FVector2D I1 = OnScreen(T1), I2 = OnScreen(T2);
	const double A = I1.X, B = I2.X, C = I1.Y, D = I2.Y;
	const double Det = A * D - B * C;
	const double Longer = FMath::Max(I1.SizeSquared(), I2.SizeSquared());
	if (Longer < 1e-8) return 0.f;
	const double Fit = FMath::Abs(Det) / Longer;
	if (Fit < 1e-4) return 0.f;
	OutFwd = ((-B * T1 + A * T2) / Det).GetSafeNormal();
	OutRight = ((D * T1 - C * T2) / Det).GetSafeNormal();
	if (OutFwd.IsNearlyZero() || OutRight.IsNearlyZero()) return 0.f;
	return (float)Fit;
}

void UAntMovementComponent::MakeScreenBasis(const FVector& N, const FVector& Axis, FVector& OutFwd, FVector& OutRight, int32 Mode) const
{
	if (Mode == 1 || (Mode < 0 && Tuning.bExactScreenDir))
	{
		// Exact mapping where the bark is seen well enough; back to the old rule where it is seen edge-on.
		FVector EF, ER, LF, LR;
		const float Fit = MakeExactScreenBasis(N, Axis, EF, ER);
		const float W = Fit <= 0.f ? 0.f : FMath::SmoothStep(Tuning.ExactMinFit, FMath::Max(Tuning.ExactFullFit, Tuning.ExactMinFit + 0.001f), Fit);
		if (W >= 1.f)
		{
			OutFwd = EF;
			OutRight = ER;
			return;
		}
		MakeScreenBasis(N, Axis, LF, LR, 0);
		if (W <= 0.f)
		{
			OutFwd = LF;
			OutRight = LR;
			return;
		}
		OutFwd = FMath::Lerp(LF, EF, (double)W).GetSafeNormal();
		OutRight = FMath::Lerp(LR, ER, (double)W).GetSafeNormal();
		if (OutFwd.IsNearlyZero()) OutFwd = W >= 0.5f ? EF : LF;
		if (OutRight.IsNearlyZero()) OutRight = W >= 0.5f ? ER : LR;
		return;
	}
	const FVector CamRightV = FVector::CrossProduct(CamUp, CamForward);
	const FVector AP = Axis.IsNearlyZero() ? FVector::ZeroVector : ProjectOnPlane(Axis, N);
	if (AP.SizeSquared() > 0.04)
	{
		// On a branch: read the stick against how the whole branch lies on screen, not the patch of bark under the ant
		// (on the side of a branch running away from you, screen-up there is over the top). Stick along the branch's screen
		// direction walks along it; across it turns the branch's near side that way; diagonals go diagonally. Away from
		// the camera counts as up, so a branch running into the screen reads as running up and W walks along it.
		// A direction's motion on screen at the ant, with perspective: going away from the camera moves a point toward the
		// screen centre (so up for an ant below the centre, left for one right of it).
		double SX = 0.0, SY = 0.0;
		if (bHasCamPos)
		{
			const FVector Rel = Pos - CamPos;
			const double Z = FMath::Max(FVector::DotProduct(Rel, CamForward), 10.0);
			SX = FVector::DotProduct(Rel, CamRightV) / Z;
			SY = FVector::DotProduct(Rel, CamUp) / Z;
		}
		auto OnScreen = [&](const FVector& V)
		{
			const double VF = FVector::DotProduct(V, CamForward);
			return FVector2D(FVector::DotProduct(V, CamRightV) - SX * VF, FVector::DotProduct(V, CamUp) - SY * VF + Tuning.AxisDepthWeight * VF);
		};
		FVector2D Ax2 = OnScreen(Axis);
		FVector AxisW = Axis;
		if (Ax2.SizeSquared() < 1e-4)
		{
			Ax2 = FVector2D(0.0, 1.0);
		}
		Ax2.Normalize();
		const FVector2D Perp2(Ax2.Y, -Ax2.X); // screen-right of the branch's screen direction
		const FVector A = AP.GetSafeNormal();
		// Which way round is "right": the side of the branch facing the camera moves toward Perp2 on screen.
		FVector NFront = -ProjectOnPlane(CamForward, AxisW.GetSafeNormal());
		if (NFront.SizeSquared() < 1e-4) NFront = CamUp;
		NFront.Normalize();
		const FVector AroundFront = FVector::CrossProduct(NFront, AxisW).GetSafeNormal();
		const double Sense = FVector2D::DotProduct(OnScreen(AroundFront), Perp2) >= 0.0 ? 1.0 : -1.0;
		const FVector Around = (FVector::CrossProduct(N, AxisW).GetSafeNormal()) * Sense;
		// The along direction on the bark that matches the axis' sign.
		const FVector AlongW = FVector::DotProduct(A, AxisW) >= 0.0 ? A : -A;
		OutFwd = (AlongW * Ax2.Y + Around * Perp2.Y).GetSafeNormal();
		OutRight = (AlongW * Ax2.X + Around * Perp2.X).GetSafeNormal();
		if (!OutFwd.IsNearlyZero() && !OutRight.IsNearlyZero()) return;
	}
	// Up on screen: screen-up projected onto the surface, plus a little "away from the camera" (the third-person habit
	// that W walks away), so a surface seen edge-on still has an up. Right on screen: screen-right projected.
	const FVector CamRight = FVector::CrossProduct(CamUp, CamForward);
	const FVector UpGoal = CamUp + CamForward * Tuning.ScreenDepthWeight;
	FVector F = ProjectOnPlane(UpGoal, N);
	FVector R = ProjectOnPlane(CamRight, N);
	if (F.SizeSquared() < 0.01 && R.SizeSquared() < 0.01)
	{
		F = ProjectOnPlane(Forward, N);
		R = FVector::CrossProduct(N, F.GetSafeNormal());
	}
	else if (R.SizeSquared() < 0.01)
	{
		// The screen's right points into the wood (left or right edge of a branch): right is round the branch, toward the
		// far side on the right edge and toward the near side on the left edge.
		R = FVector::CrossProduct(N, F.GetSafeNormal());
		const double Side = FVector::DotProduct(N, CamRight) >= 0.0 ? 1.0 : -1.0;
		if (FVector::DotProduct(R, CamRight) * 10.0 + Side * FVector::DotProduct(R, CamForward) < 0.0) R = -R;
	}
	else if (F.SizeSquared() < 0.01)
	{
		F = FVector::CrossProduct(R.GetSafeNormal(), N);
		if (FVector::DotProduct(F, UpGoal) < 0.0) F = -F;
	}
	OutFwd = F.GetSafeNormal();
	OutRight = R.GetSafeNormal();
}

void UAntMovementComponent::UpdateHeldInput(float Dt, const FVector& N, const FVector& Axis)
{
	if (State != EAntState::Walking)
	{
		HeldFwd = HeldRight = FVector::ZeroVector;
		SnapChoice = -1;
		HeldIdle = 0.f;
		return;
	}
	if (MoveInput.SizeSquared() <= 0.01)
	{
		// Released: a short dip (a flick through the centre) keeps the snapshot; a pause drops it.
		HeldIdle += Dt;
		if (HeldIdle >= Tuning.PauseSeconds)
		{
			HeldFwd = HeldRight = FVector::ZeroVector;
			SnapChoice = -1;
		}
		return;
	}
	HeldIdle = 0.f;
	const FVector2D Dir = MoveInput.GetSafeNormal();
	// A sudden jump of the stick (W to D, a reversal) is a new input: read the camera again. Gradual turns steer
	// within the snapshot.
	const bool bNew = HeldFwd.IsNearlyZero() ||
		FVector2D::DotProduct(Dir, HeldStickDir) < FMath::Cos(FMath::DegreesToRadians(Tuning.NewInputAngleDeg));
	if (bNew)
	{
		MakeScreenBasis(N, Axis, HeldFwd, HeldRight);
		SnapChoice = -1;
	}
	else if (Tuning.bHeldFollowsLook && bPlayerLooking)
	{
		// The player is turning the camera: the held direction follows their look (not the camera's own roll).
		MakeScreenBasis(N, Axis, HeldFwd, HeldRight);
	}
	HeldStickDir = Dir;
}

int32 UAntMovementComponent::ChooseSnap(const FVector& Dir, const FVector& A, const FVector& Around, int32 Current) const
{
	const FVector Cands[4] = { A, -A, Around, -Around };
	int32 Best = 0;
	double BestScore = -2.0;
	for (int32 k = 0; k < 4; ++k)
	{
		const double S = FVector::DotProduct(Dir, Cands[k]);
		if (S > BestScore)
		{
			BestScore = S;
			Best = k;
		}
	}
	if (Current >= 0 && Current < 4 && BestScore <= FVector::DotProduct(Dir, Cands[Current]) + Tuning.SnapStickiness) return Current;
	return Best;
}

FVector UAntMovementComponent::PreviewWish(const FVector2D& Input, const FVector& N, const FVector& Axis, float BranchBlend, float RadiusHere, float DistToEnd) const
{
	const double InputMag = FMath::Min(Input.Size(), 1.0);
	if (InputMag < 0.05) return FVector::ZeroVector;
	FVector AP = Axis.IsNearlyZero() ? FVector::ZeroVector : ProjectOnPlane(Axis, N);
	const double APLen = AP.Size();
	const FVector A = APLen > 1e-3 ? AP / APLen : FVector::ZeroVector;
	const double TipFade = FMath::Clamp(APLen / 0.6, 0.0, 1.0);
	const double AroundRate = FMath::Clamp(RadiusHere / 30.0, 0.2, 1.0);
	if (Tuning.ControlStyle == 0)
	{
		FVector F = HeldFwd, R = HeldRight;
		if (F.IsNearlyZero()) MakeScreenBasis(N, Axis, F, R);
		const FVector Raw = (F * Input.Y + R * Input.X).GetSafeNormal() * InputMag;
		if (BranchBlend <= 0.f || A.IsNearlyZero() || !Tuning.bSnapOnBranches) return Raw;
		const FVector Around = FVector::CrossProduct(N, A).GetSafeNormal();
		// The live snap applies to the live stick; a pretend input picks its own.
		const int32 Choice = ChooseSnap(Raw.GetSafeNormal(), A, Around, Input.Equals(MoveInput, 0.05) ? SnapChoice : -1);
		FVector Target;
		if (Choice <= 1) Target = A * ((Choice == 0 ? 1.0 : -1.0) * InputMag * TipFade * (Choice == 0 ? FMath::Clamp(DistToEnd / 50.0, 0.15, 1.0) : 1.0));
		else Target = Around * ((Choice == 2 ? 1.0 : -1.0) * InputMag * AroundRate * TipFade);
		return FMath::Lerp(Raw, Target, (double)BranchBlend);
	}
	// Steering wheel (ControlStyle 1): camera-relative, with W along the branch end picked at press and A/D round it.
	FVector2D Saved = Input;
	const double Facing = FMath::Clamp(-FVector::DotProduct(CamForward, N) * 2.0, 0.0, 1.0);
	const FVector RawFwd = ProjectOnPlane(CamForward, N) + ProjectOnPlane(CamUp, N) * Facing;
	const FVector CamRight = FVector::CrossProduct(CamUp, CamForward);
	FVector Fwd = RawFwd.GetSafeNormal(), Right = FVector::CrossProduct(N, Fwd);
	if (FVector::DotProduct(Right, CamRight) < 0.0) Right = -Right;
	const FVector Cam = (Fwd * (Saved.Y * FMath::Clamp(RawFwd.Size() / 0.35, 0.0, 1.0)) + Right * Saved.X).GetClampedToMaxSize(1.0);
	if (BranchBlend <= 0.f || A.IsNearlyZero()) return Cam;
	FVector Away = WheelAway;
	if (Away.IsNearlyZero())
	{
		double S = FVector::DotProduct(Axis, CamForward) + 0.7 * FVector::DotProduct(Axis, CamUp);
		if (FMath::Abs(S) < 0.15) S = FVector::DotProduct(Axis, Forward);
		Away = S >= 0.0 ? Axis : -Axis;
	}
	double X = Saved.X, Y = Saved.Y;
	if (FMath::Abs(X) < 0.35 * FMath::Abs(Y)) X = 0.0;
	if (FMath::Abs(Y) < 0.35 * FMath::Abs(Saved.X)) Y = 0.0;
	const int32 Sense = (Saved.Y >= 0.0 ? 1 : -1) * (FVector::DotProduct(Away, Axis) >= 0.0 ? 1 : -1);
	const FVector Clockwise = FVector::CrossProduct(N, Away).GetSafeNormal();
	return FMath::Lerp(Cam, A * (Sense * FMath::Abs(Y) * TipFade) + Clockwise * (X * AroundRate * TipFade), (double)BranchBlend);
}

void UAntMovementComponent::UpdateInputViz(float Dt, const FVector& Axis, float RadiusHere, float ThinBlend, float BranchBlend, bool bWheel, bool bWantsMove,
	const FVector& LastWish, const FVector& LastIntent, float DistToEnd)
{
	const APawn* PawnOwner = Cast<APawn>(GetOwner());
	if (!PawnOwner || !PawnOwner->IsPlayerControlled()) return;
	APlayerController* PC = Cast<APlayerController>(PawnOwner->GetController());
	UWorld* World = GetWorld();
	if (!World || !PC) return;
	FControlTrace& T = GControlTrace.FindOrAdd(this);
	const double Now = World->GetTimeSeconds();
	const FVector N = SurfaceNormal;
	const bool bWalking = State == EAntState::Walking;
	const bool bScreen = Tuning.ControlStyle == 0;
	int32 Level = -9;
	if (Tree.IsValid() && Tree->GetSkeleton().Branches.IsValidIndex(CurrentBranch)) Level = Tree->GetSkeleton().Branches[CurrentBranch].Level;

	// The rule in charge, in words and colour.
	FString Rule;
	FColor RuleColor = FColor::Cyan;
	if (bScreen)
	{
		const TCHAR* Snap = (SnapChoice == 0 || SnapChoice == 1) ? TEXT("along") : (SnapChoice >= 2 ? TEXT("around") : TEXT("free"));
		if (BranchBlend >= 0.99f) { Rule = FString::Printf(TEXT("screen direction, branch: %s"), Snap); RuleColor = FColor::Orange; }
		else if (BranchBlend > 0.01f) { Rule = FString::Printf(TEXT("screen direction, %d%% branch (%s)"), FMath::RoundToInt(BranchBlend * 100.f), Snap); RuleColor = FColor::Yellow; }
		else { Rule = TEXT("screen direction, free"); }
		if (Tuning.bExactScreenDir) Rule += TEXT(", exact (trial)");
	}
	else if (bWantsMove && !bWheel && ThinBlend > 0.01f) { Rule = TEXT("thin-branch rules (gentle stick)"); RuleColor = FColor::Magenta; }
	else if (BranchBlend >= 0.99f) { Rule = TEXT("steering wheel"); RuleColor = FColor::Orange; }
	else if (BranchBlend > 0.01f) { Rule = FString::Printf(TEXT("mix: wheel %d%%, camera %d%%"), FMath::RoundToInt(BranchBlend * 100.f), 100 - FMath::RoundToInt(BranchBlend * 100.f)); RuleColor = FColor::Yellow; }
	else { Rule = TEXT("camera-relative"); }
	if (!bWalking) { Rule = FString::Printf(TEXT("%s (no stick control)"), *UEnum::GetValueAsString(State)); RuleColor = FColor::Silver; }

	auto ScreenDir = [&](const FVector& Dir) -> FVector2D
	{
		FVector2D S0, S1;
		if (Dir.IsNearlyZero() || !PC->ProjectWorldLocationToScreen(Pos, S0) || !PC->ProjectWorldLocationToScreen(Pos + Dir.GetSafeNormal() * 30.0, S1)) return FVector2D::ZeroVector;
		return (S1 - S0).GetSafeNormal();
	};
	auto Angle2D = [](const FVector2D& P, const FVector2D& Q) -> double
	{
		if (P.IsNearlyZero() || Q.IsNearlyZero()) return 0.0;
		return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector2D::DotProduct(P, Q), -1.0, 1.0)));
	};
	const FVector FwdPreview = bWalking ? PreviewWish(FVector2D(0.0, 1.0), N, Axis, BranchBlend, RadiusHere, DistToEnd) : FVector::ZeroVector;
	const FVector2D FwdScreen = ScreenDir(FwdPreview);
	const double CamTurn = Dt > 0.f ? FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(CamForward, T.LastCamForward), -1.0, 1.0))) / Dt : 0.0;
	T.LastCamForward = CamForward;

	// Control trace 1: every junction crossing while the stick is held (at most three lines a second).
	if (CurrentBranch != T.LastBranch)
	{
		if (bWantsMove && bWalking && T.LastBranch != INDEX_NONE && CurrentBranch != INDEX_NONE)
		{
			if (Now - T.LastJunctionLog >= 1.0)
			{
				T.LastJunctionLog = Now;
				T.JunctionLogsThisSecond = 0;
			}
			if (++T.JunctionLogsThisSecond <= 3)
			{
				UE_LOG(LogLeafline, Log, TEXT("Control trace: onto branch %d (level %d, r %.0f cm, %s) from branch %d (r %.0f cm, %s); forward moved %.0f deg on screen; stick (%.2f, %.2f)"),
					CurrentBranch, Level, RadiusHere, *Rule, T.LastBranch, T.LastRadius, *T.LastRule, Angle2D(T.LastFwdScreen, FwdScreen), MoveInput.X, MoveInput.Y);
			}
		}
		T.LastBranch = CurrentBranch;
	}
	T.LastRadius = RadiusHere;
	T.LastRule = Rule;
	T.LastFwdScreen = FwdScreen;

	// Control trace 2: the ant has moved more than 60 degrees away from the stick's direction on screen for a third of
	// a second (at most a line a second).
	const FVector2D StickScreen = FVector2D(MoveInput.X, -MoveInput.Y).GetSafeNormal();
	const FVector2D MoveScreen = ScreenDir(Velocity.SizeSquared() > 100.0 ? Velocity : FVector::ZeroVector);
	const double Off = Angle2D(StickScreen, MoveScreen);
	if (bWantsMove && bWalking && !MoveScreen.IsNearlyZero() && Off > 60.0)
	{
		if (T.OffSince < 0.0) T.OffSince = Now;
		if (Now - T.OffSince > 0.33 && Now - T.LastOffLog > 1.0)
		{
			T.LastOffLog = Now;
			UE_LOG(LogLeafline, Log, TEXT("Control trace: moving %.0f deg off the stick's screen direction for %.1f s: %s, branch %d (level %d, r %.0f cm), stick (%.2f, %.2f), snapshot %s, camera turning %.0f deg/s"),
				Off, Now - T.OffSince, *Rule, CurrentBranch, Level, RadiusHere, MoveInput.X, MoveInput.Y, HeldFwd.IsNearlyZero() ? TEXT("none") : TEXT("held"), CamTurn);
		}
	}
	else
	{
		T.OffSince = -1.0;
	}

	// Leaf.ScreenDirLog: where W/A/S/D point on screen under both mappings (the raw held basis; the snap is off by default).
	if (LeafDebug::ScreenDirLogs > 0 && bWalking && bScreen)
	{
		--LeafDebug::ScreenDirLogs;
		auto Deg = [&](const FVector& Dir) -> double
		{
			const FVector2D S = ScreenDir(Dir);
			if (S.IsNearlyZero()) return -1.0;
			const double A = FMath::RadiansToDegrees(FMath::Atan2(-S.Y, S.X)); // screen y grows downward
			return A < 0.0 ? A + 360.0 : A;
		};
		auto Err = [](double Got, double Want) { return Got < 0.0 ? -1.0 : FMath::Abs(FMath::Fmod(Got - Want + 540.0, 360.0) - 180.0); };
		FString Parts;
		for (int32 Mode = 0; Mode <= 1; ++Mode)
		{
			FVector F, R;
			MakeScreenBasis(N, Axis, F, R, Mode);
			const double W = Deg(F), A = Deg(-R), S = Deg(-F), Dd = Deg(R);
			const double Worst = FMath::Max(FMath::Max(Err(W, 90.0), Err(A, 180.0)), FMath::Max(Err(S, 270.0), Err(Dd, 0.0)));
			Parts += FString::Printf(TEXT("%s W %.0f A %.0f S %.0f D %.0f (worst %.0f off)%s"), Mode == 0 ? TEXT("current:") : TEXT("exact:"),
				W, A, S, Dd, Worst, Mode == 0 ? TEXT("   ") : TEXT(""));
		}
		FVector EF, ER;
		const float Fit = MakeExactScreenBasis(N, Axis, EF, ER);
		const FVector Rel = (Pos - CamPos).GetSafeNormal();
		UE_LOG(LogLeafline, Log, TEXT("Screen dirs: %s; fit %.2f, view %.0f deg to the bark, branch %d (level %d, r %.0f cm), axis on screen %.0f deg, switch %s"),
			*Parts, Fit, FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(-FVector::DotProduct(Rel, N), -1.0, 1.0))), CurrentBranch, Level, RadiusHere,
			Deg(Axis), Tuning.bExactScreenDir ? TEXT("on") : TEXT("off"));
	}

	LeafDebug::InputVizLines.Reset();
	LeafDebug::InputVizLabels.Reset();
	if (!LeafDebug::bInputViz) return;
	// The view: where W / A / S / D would go from here (short paths over the bark), the live stick (white), the held
	// direction before the branch rules (thin cyan) and the actual movement (green).
	const FTreeField* Field = Tree.IsValid() ? &Tree->GetField() : nullptr;
	auto DrawPath = [&](const FVector& Dir, const FColor& Col, float Radius, const TCHAR* Label)
	{
		if (Dir.IsNearlyZero() || !Field) return;
		FVector P = Pos, Nn = N, D = Dir.GetSafeNormal();
		const double StepLen = 10.0 * FMath::Clamp(Dir.Size(), 0.2, 1.0);
		for (int32 i = 0; i < 6; ++i)
		{
			FVector Q = P + D * StepLen, Qn;
			double Err = 0.0;
			if (!Field->Project(Q, Qn, Tuning.RideHeight, &Err)) break;
			// With depth: a path that goes over an edge and round the back disappears there instead of seeming to cross the
			// front (the end marker stays on top).
			DrawDebugLine(World, P + Nn * 2.0, Q + Qn * 2.0, Col, false, -1.f, SDPG_World, 1.0f);
			D = ProjectOnPlane(FQuat::FindBetweenNormals(Nn, Qn).RotateVector(D), Qn).GetSafeNormal();
			P = Q;
			Nn = Qn;
		}
		DrawDebugSphere(World, P + Nn * 1.5, Radius, 8, Col, false, -1.f, SDPG_Foreground, 0.6f);
		if (Label) LeafDebug::InputVizLabels.Add({ P + Nn * (Radius + 4.0), Label, Col });
	};
	if (bWalking)
	{
		const FColor Dim(RuleColor.R / 2 + 40, RuleColor.G / 2 + 40, RuleColor.B / 2 + 40);
		DrawPath(FwdPreview, RuleColor, 4.f, TEXT("W"));
		DrawPath(PreviewWish(FVector2D(0.0, -1.0), N, Axis, BranchBlend, RadiusHere, DistToEnd), Dim, 2.5f, TEXT("S"));
		DrawPath(PreviewWish(FVector2D(-1.0, 0.0), N, Axis, BranchBlend, RadiusHere, DistToEnd), Dim, 2.5f, TEXT("A"));
		DrawPath(PreviewWish(FVector2D(1.0, 0.0), N, Axis, BranchBlend, RadiusHere, DistToEnd), Dim, 2.5f, TEXT("D"));
		if (bWantsMove)
		{
			DrawPath(LastWish, FColor::White, 5.f, nullptr);
			DrawDebugLine(World, Pos + N * 2.0, Pos + N * 2.0 + LastIntent * 35.0, FColor::Cyan, false, -1.f, SDPG_Foreground, 0.4f);
		}
	}
	DrawDebugLine(World, Pos + N * 2.5, Pos + N * 2.5 + Velocity * 0.25, FColor::Green, false, -1.f, SDPG_Foreground, 1.2f);
	LeafDebug::InputVizColor = RuleColor;
	LeafDebug::InputVizLines.Add(FString::Printf(TEXT("Input view (F3): %s"), *Rule));
	LeafDebug::InputVizLines.Add(FString::Printf(TEXT("branch %d (level %d), radius %.0f cm   branch rules %d%%   snapshot %s"),
		CurrentBranch, Level, RadiusHere, FMath::RoundToInt(BranchBlend * 100.f), bScreen ? (HeldFwd.IsNearlyZero() ? TEXT("none (live camera)") : TEXT("held")) : TEXT("n/a")));
	LeafDebug::InputVizLines.Add(FString::Printf(TEXT("stick (%.2f, %.2f)   camera turning %.0f deg/s   ControlStyle %d, snap on branches %s"),
		MoveInput.X, MoveInput.Y, CamTurn, Tuning.ControlStyle, Tuning.bSnapOnBranches ? TEXT("on") : TEXT("off")));
	LeafDebug::InputVizLines.Add(TEXT("white: your stick   cyan: held direction   green: actual movement   W/A/S/D: where each would go"));
}

void UAntMovementComponent::TickWalking(float Dt)
{
	const FTreeField* F = GetField();
	const bool bWantsMove = MoveInput.SizeSquared() > 0.01;
	UpdateStamina(Dt, bWantsMove);

	const float CarryFactor = FMath::Clamp(1.f - Tuning.SpeedLossPerMass * CarriedMass, Tuning.MinCarrySpeedFactor, 1.f);
	TrailBoost = FMath::FInterpTo(TrailBoost, bOnTrail ? Tuning.TrailSpeedMultiplier : 1.f, Dt, 4.f);
	const float Speed = Tuning.WalkSpeed * CarryFactor * TrailBoost * (bSprinting ? Tuning.SprintMultiplier : 1.f);
	const double Expected = FMath::Max(Velocity.Size(), (double)Speed) * Dt;
	const int32 Steps = FMath::Clamp(FMath::CeilToInt(Expected / FMath::Max(Tuning.MaxSubstep, 1.f)), 1, 24);
	const float H = Dt / Steps;
	const FVector Start = Pos;
	FVector N = SurfaceNormal;
	double MaxErr = 0.0;

	// Thin branches: walk along the axis like a tightrope instead of spiralling round it. The input picks the
	// direction; when it points across the branch the last direction is kept (SpareInt holds the sign).
	FVector Axis = FVector::ZeroVector;
	float ThinBlend = 0.f;
	float RadiusHere = 1.0e6f;
	float DistToEnd = 1.0e6f;
	if (Tree.IsValid())
	{
		const TArray<FTreeBranch>& Branches = Tree->GetSkeleton().Branches;
		if (Branches.IsValidIndex(CurrentBranch))
		{
			const FTreeBranch& B = Branches[CurrentBranch];
			int32 Nearest = 0;
			double Best = TNumericLimits<double>::Max();
			for (int32 i = 0; i < B.Samples.Num(); ++i)
			{
				const double D2 = FVector::DistSquared(Pos, B.Samples[i]);
				if (D2 < Best)
				{
					Best = D2;
					Nearest = i;
				}
			}
			const int32 Lo = FMath::Max(Nearest - 1, 0), Hi = FMath::Min(Nearest + 1, B.Samples.Num() - 1);
			Axis = (B.Samples[Hi] - B.Samples[Lo]).GetSafeNormal();
			ThinBlend = 1.f - FMath::SmoothStep(15.f, 70.f, B.SampleRadius[Nearest]);
			RadiusHere = B.SampleRadius[Nearest];
			DistToEnd = B.Length() - B.SampleDist[Nearest];
		}
	}
	// The steering wheel reaches further than the old thin-branch rules: on mid-size limbs (up to about 2 m radius)
	// camera-relative "right" still pointed into the wood at the silhouette and the ant stalled at the tangent point.
	const float WheelBlend = Tuning.bSteeringWheel ? FMath::Max(ThinBlend, 1.f - FMath::SmoothStep(Tuning.WheelFullRadius, FMath::Max(Tuning.WheelFadeRadius, Tuning.WheelFullRadius + 1.f), RadiusHere)) : 0.f;
	if (ThinBlend <= 0.f && WheelBlend <= 0.f)
	{
		SpareInt = 0;
		AroundSign = 0;
	}
	// Steering wheel: fix which way along the branch W means when the stick is first held on a thin branch and keep it
	// until the stick is let go. Onto the next branch it carries over by continuity.
	// Screen-direction controls (ControlStyle 0): branches resolve the held screen direction into along or around over
	// the same radii as the wheel; the snapshot of the camera is taken or kept here, once a frame.
	const bool bScreen = Tuning.ControlStyle == 0;
	const float BranchBlend = bScreen
		? 1.f - FMath::SmoothStep(Tuning.SnapFullRadius, FMath::Max(Tuning.SnapFadeRadius, Tuning.SnapFullRadius + 1.f), RadiusHere)
		: WheelBlend;
	if (bScreen)
	{
		UpdateHeldInput(Dt, N, Axis);
	}
	else
	{
		HeldFwd = HeldRight = FVector::ZeroVector;
		SnapChoice = -1;
	}
	if (!bScreen && Tuning.bSteeringWheel && WheelBlend > 0.f && MoveInput.SizeSquared() > 0.04 && !Axis.IsNearlyZero())
	{
		if (WheelAway.IsNearlyZero())
		{
			double S = FVector::DotProduct(Axis, CamForward) + 0.7 * FVector::DotProduct(Axis, CamUp);
			if (FMath::Abs(S) < 0.15) S = FVector::DotProduct(Axis, Forward); // branch across the screen: the way the ant faces
			WheelAway = S >= 0.0 ? Axis : -Axis;
		}
		else
		{
			WheelAway = FVector::DotProduct(Axis, WheelAway) >= 0.0 ? Axis : -Axis;
		}
	}
	else
	{
		WheelAway = FVector::ZeroVector;
	}
	const bool bWheel = !WheelAway.IsNearlyZero();
	FVector LastWish = FVector::ZeroVector, LastIntent = FVector::ZeroVector;

	for (int32 s = 0; s < Steps; ++s)
	{
		FVector Wish;
		if (State == EAntState::WalkingHome)
		{
			Wish = HomeWish(N);
		}
		else if (bScreen && !HeldFwd.IsNearlyZero())
		{
			// The held screen direction, carried along the bark since the stick was pushed.
			const FVector Raw = HeldFwd * MoveInput.Y + HeldRight * MoveInput.X;
			Wish = Raw.SizeSquared() > 1e-4 ? Raw.GetSafeNormal() * FMath::Min(MoveInput.Size(), 1.0) : ComputeWish(N);
		}
		else
		{
			Wish = ComputeWish(N);
		}
		LastIntent = Wish;
		const bool bBranchRules = bScreen ? (BranchBlend > 0.f && !Axis.IsNearlyZero()) : (ThinBlend > 0.f || bWheel);
		if (bBranchRules && bWantsMove && State == EAntState::Walking)
		{
			// At a branch tip the axis turns into the surface normal; fade the push there so the ant comes to rest at
			// the tip instead of overshooting the apex back and forth.
			const FVector AP = ProjectOnPlane(Axis, N);
			const double APLen = AP.Size();
			// Exactly on the apex the axis has no direction along the surface; use the body's facing, which points the
			// way the ant came up, so reversing still leads back down.
			const FVector A = APLen > 1e-3 ? AP / APLen : ProjectOnPlane(Forward, N).GetSafeNormal();
			const double TipFade = FMath::Clamp(APLen / 0.6, 0.0, 1.0);
			if (!A.IsNearlyZero())
			{
				const double InputMag = FMath::Min(MoveInput.Size(), 1.0);
				// Tip latch, in spare members: SpareVector = (stick direction when the ant settled at the apex, 1) or
				// zero; SpareFloat = 1 while leaving the apex. Both clear once the ant is off the rounded tip.
				const FVector2D Stick = MoveInput.GetSafeNormal();
				if (TipFade >= 1.0)
				{
					SpareVector = FVector::ZeroVector;
					SpareFloat = 0.f;
				}
				if (SpareVector.Z > 0.5)
				{
					// Settled at the apex: hold until the stick turns round, then leave at full push. (There the faded
					// push is near zero in both directions, so without this the ant could never get off again.)
					if (FVector2D::DotProduct(Stick, FVector2D(SpareVector.X, SpareVector.Y)) < 0.0)
					{
						SpareVector = FVector::ZeroVector;
						SpareFloat = 1.f;
					}
					SpareInt = SpareFloat > 0.f ? -1 : 1;
				}
				else if (SpareFloat > 0.f)
				{
					SpareInt = -1;
				}
				else if (bScreen)
				{
					// Screen direction on a branch: along or around, whichever is closer to the held direction.
					const FVector Ar = FVector::CrossProduct(N, A).GetSafeNormal();
					if (Tuning.bSnapOnBranches && TipFade >= 1.0)
					{
						SnapChoice = ChooseSnap(Wish.GetSafeNormal(), A, Ar, SnapChoice);
						// Walking along: the held direction bends with the twig, so a curving twig stays "along" and the next
						// twig is joined by continuity (it is not re-judged against a direction the twig has turned away from).
						if (SnapChoice <= 1 && !HeldFwd.IsNearlyZero())
						{
							const FVector Cand = SnapChoice == 0 ? A : -A;
							const FQuat Align = FQuat::Slerp(FQuat::Identity, FQuat::FindBetweenNormals(Wish.GetSafeNormal(), Cand), (double)BranchBlend);
							HeldFwd = ProjectOnPlane(Align.RotateVector(HeldFwd), N).GetSafeNormal();
							HeldRight = ProjectOnPlane(Align.RotateVector(HeldRight), N).GetSafeNormal();
						}
					}
					if (SnapChoice == 0 || SnapChoice == 1)
					{
						SpareInt = SnapChoice == 0 ? 1 : -1;
					}
					else if (SpareInt == 0)
					{
						SpareInt = FVector::DotProduct(Wish, A) >= 0.0 ? 1 : -1;
					}
					if (SpareInt > 0 && TipFade < 0.3 && SnapChoice <= 1) SpareVector = FVector(Stick.X, Stick.Y, 1.0);
				}
				else if (bWheel)
				{
					// W heads the way WheelAway points, S the other way; pure A/D keeps the last sense.
					if (FMath::Abs(MoveInput.Y) > 0.35 * FMath::Abs(MoveInput.X))
					{
						const int32 AwaySign = FVector::DotProduct(WheelAway, Axis) >= 0.0 ? 1 : -1;
						SpareInt = (MoveInput.Y >= 0.0 ? 1 : -1) * AwaySign;
					}
					else if (SpareInt == 0)
					{
						SpareInt = FVector::DotProduct(Forward, A) >= 0.0 ? 1 : -1;
					}
					if (SpareInt > 0 && TipFade < 0.3) SpareVector = FVector(Stick.X, Stick.Y, 1.0);
				}
				else
				{
					const double Along = FVector::DotProduct(Wish, A);
					if (FMath::Abs(Along) > 0.25 * InputMag)
					{
						SpareInt = Along >= 0.0 ? 1 : -1;
					}
					else if (SpareInt == 0)
					{
						SpareInt = FVector::DotProduct(Forward, A) >= 0.0 ? 1 : -1;
					}
					if (SpareInt > 0 && TipFade < 0.3) SpareVector = FVector(Stick.X, Stick.Y, 1.0);
				}
				// Heading for the free end of a thin branch: slow down over the last half metre so the rounded tip
				// (only a few cm of curve) is reached at a crawl, and fade the push over the cap so the ant comes to rest
				// at the apex instead of overshooting it back and forth.
				const double TipSlow = SpareInt > 0 ? FMath::Clamp(DistToEnd / 50.0, 0.15, 1.0) : 1.0;
				const double Fade = SpareFloat > 0.f ? 1.0 : TipFade;
				// Sideways input that is clearly meant (more than about 27 degrees off the branch) walks the ant round the
				// branch; small sideways parts are dropped, so walking along a thin branch never spirals. The sense of
				// rotation and the share of sideways input are taken when the stick settles on a direction and kept while it
				// holds it, so the ant goes all the way round, behind the branch too: there the screen-relative wish would
				// fade out at the silhouette and then point the other way. Not on the rounded tip.
				const FVector Around = FVector::CrossProduct(N, A).GetSafeNormal();
				const double AlongPart = FVector::DotProduct(Wish, A);
				const double AroundPart = FVector::DotProduct(Wish, Around);
				const double WishLen = Wish.Size();
				const FVector2D StickDir = MoveInput.GetSafeNormal();
				if (TipFade < 1.0 || StickDir.IsNearlyZero() || FVector2D::DotProduct(StickDir, AroundStick) < 0.7)
				{
					AroundSign = 0;
				}
				if (AroundSign == 0 && TipFade >= 1.0 && WishLen > 1e-3)
				{
					const double Frac = FMath::Abs(AroundPart) / WishLen;
					if (Frac > 0.45)
					{
						AroundSign = AroundPart >= 0.0 ? 1 : -1;
						AroundStick = StickDir;
						AroundKeep = (float)FMath::SmoothStep(0.45, 0.8, Frac);
					}
				}
				const double Keep = AroundSign != 0 ? AroundKeep : 0.0;
				// Round a thin twig at most about one lap a second: at full walking speed a 6 cm twig spun the ant (and the
				// view) round five times a second.
				const double AroundRate = FMath::Clamp(RadiusHere / 30.0, 0.2, 1.0);
				const double AlongMag = InputMag * (1.0 - Keep) + FMath::Abs(AlongPart) * Keep;
				if (bScreen)
				{
					// At the rounded tip the snap holds (the axis turns into the normal there); the latch decides the sense.
					const bool bLatch = SpareVector.Z > 0.5 || SpareFloat > 0.f;
					FVector Target;
					if (bLatch || SnapChoice == 0 || SnapChoice == 1) Target = A * (SpareInt * InputMag * Fade * TipSlow);
					else if (SnapChoice >= 2) Target = Around * ((SnapChoice == 2 ? 1.0 : -1.0) * InputMag * AroundRate * TipFade);
					else Target = Wish.GetSafeNormal() * InputMag; // no snapping: the held direction as it is
					Wish = FMath::Lerp(Wish, Target, (double)BranchBlend);
				}
				else if (bWheel)
				{
					// Steering wheel: Y along, X round (clockwise looking along WheelAway, i.e. as seen from behind). Each
					// axis is dropped when it is a small part of the input, so W alone never spirals and D alone never drifts.
					double X = MoveInput.X, Y = MoveInput.Y;
					if (FMath::Abs(X) < 0.35 * FMath::Abs(Y)) X = 0.0;
					if (FMath::Abs(Y) < 0.35 * FMath::Abs(MoveInput.X)) Y = 0.0;
					// UE is left-handed (X forward, Y right, Z up): up x forward = right, so on top of the branch D goes right.
					const FVector Clockwise = FVector::CrossProduct(N, WheelAway).GetSafeNormal();
					Wish = FMath::Lerp(Wish, A * (SpareInt * FMath::Abs(Y) * Fade * TipSlow) + Clockwise * (X * AroundRate * TipFade), (double)WheelBlend);
				}
				else
				{
					Wish = FMath::Lerp(Wish, A * (SpareInt * AlongMag * Fade * TipSlow) + Around * (AroundSign * InputMag * Keep * AroundRate), (double)ThinBlend);
				}
			}
		}

		// Screen direction on thin wood: going round is limited to MaxLapsPerSecond (walking along is not), so a diagonal on
		// a twig is a gentle spiral, not five laps a second.
		if (bScreen && State == EAntState::Walking && Tuning.MaxLapsPerSecond > 0.f && !Axis.IsNearlyZero() && RadiusHere < 1.0e5f && Speed > 1.f)
		{
			const FVector AlongDir = ProjectOnPlane(Axis, N).GetSafeNormal();
			if (!AlongDir.IsNearlyZero())
			{
				const FVector RoundDir = FVector::CrossProduct(N, AlongDir).GetSafeNormal();
				const double RoundPart = FVector::DotProduct(Wish, RoundDir);
				const double MaxRound = Tuning.MaxLapsPerSecond * 2.0 * PI * RadiusHere / Speed;
				if (FMath::Abs(RoundPart) > MaxRound) Wish -= RoundDir * (RoundPart - FMath::Sign(RoundPart) * MaxRound);
			}
		}

		// Soft boundary on the ground: turn the ant back toward the trunk.
		if (Tree.IsValid() && Pos.Z < 600.0)
		{
			FVector Out = Pos - Tree->GetSkeleton().TrunkBase;
			Out.Z = 0.0;
			const double R = Out.Size();
			if (R > Tuning.BoundaryRadius && R > UE_SMALL_NUMBER)
			{
				const FVector OutDir = Out / R;
				const double Push = FMath::Clamp((R - Tuning.BoundaryRadius) / FMath::Max(Tuning.BoundaryWidth, 1.f), 0.0, 1.0);
				const double Outward = FVector::DotProduct(Wish, OutDir);
				if (Outward > 0.0) Wish -= OutDir * Outward * Push;
				Wish += ProjectOnPlane(-OutDir, N).GetSafeNormal() * Push;
				Wish = Wish.GetClampedToMaxSize(1.0);
			}
		}

		// A faded-out wish (at a tip, or where the camera direction vanishes) means "stay": stop cleanly.
		if (Wish.SizeSquared() < 0.01) Wish = FVector::ZeroVector;
		LastWish = Wish;
		const FVector Target = Wish * Speed;
		float Rate = Wish.IsNearlyZero() ? Tuning.Deceleration : Tuning.Acceleration;
		// Brake hard when the wish turns against the motion, so the ant never swings back and forth over a point.
		if (FVector::DotProduct(Target, Velocity) < 0.0) Rate = Tuning.Deceleration * 2.5f;
		Velocity = ProjectOnPlane(MoveTowards(Velocity, Target, Rate * H), N);
		const FVector Prev = Pos;
		Pos += Velocity * H;

		FVector NewN;
		double Err = 0.0;
		if (!F->Project(Pos, NewN, Tuning.RideHeight, &Err))
		{
			// Never commit a failed projection: stay on the last good pose and lose speed.
			++ProjFailCount;
			Pos = Prev;
			Velocity *= 0.5;
			continue;
		}
		MaxErr = FMath::Max(MaxErr, Err);
		// Carry the velocity over the curve into the new tangent plane without losing speed; the held screen directions
		// ride along the same way, so a held stick keeps going straight over the bark.
		const FQuat Turn = FQuat::FindBetweenNormals(N, NewN);
		Velocity = ProjectOnPlane(Turn.RotateVector(Velocity), NewN);
		if (!HeldFwd.IsNearlyZero())
		{
			HeldFwd = ProjectOnPlane(Turn.RotateVector(HeldFwd), NewN).GetSafeNormal();
			HeldRight = ProjectOnPlane(Turn.RotateVector(HeldRight), NewN).GetSafeNormal();
		}
		N = NewN;
	}

	SurfaceNormal = N;
	LastProjError = MaxErr;
	F->Distance(Pos, &CurrentBranch);
	UpdateOrientation(Dt);
	if (State == EAntState::WalkingHome && FollowDiagDue(this, Pos, Dt))
	{
		const FVector GP = F->RawGradient(Pos);
		UE_LOG(LogLeafline, Log, TEXT("FollowDiag %s WalkingHome: pos %s (d %.2f, |g| %.2f, br %d) n %s; wish %s vel %s; steps %d maxerr %.2f fails %d"),
			*GetOwner()->GetName(), *Pos.ToCompactString(), F->Distance(Pos), GP.Size(), CurrentBranch, *N.ToCompactString(),
			*LastWish.ToCompactString(), *Velocity.ToCompactString(), Steps, MaxErr, ProjFailCount);
	}
	UpdateTelemetry(Dt, FVector::Dist(Start, Pos), bWantsMove);
	UpdateInputViz(Dt, Axis, RadiusHere, ThinBlend, BranchBlend, bWheel, bWantsMove, LastWish, LastIntent, DistToEnd);
}

void UAntMovementComponent::StartFollow(const TArray<FTrailPoint>& Points, int32 StartIndex, int32 Direction, EAntState FollowState)
{
	if (Points.Num() < 2) return;
	FollowPoints = Points;
	FollowIndex = FMath::Clamp(StartIndex, 0, Points.Num() - 1);
	FollowFrac = 0.0;
	FollowDir = Direction >= 0 ? 1 : -1;
	bFollowFinished = false;
	FollowWait = 0.f;
	FollowLag = 0.0;
	SetState(FollowState);
}

void UAntMovementComponent::StopFollow()
{
	if (State == EAntState::Riding || State == EAntState::Returning) SetState(EAntState::Walking);
	FollowPoints.Reset();
}

void UAntMovementComponent::TickFollow(float Dt)
{
	const FTreeField* F = GetField();
	// Riding ends on any movement input; the input then drives the walk this frame.
	if (State == EAntState::Riding && MoveInput.SizeSquared() > 0.04)
	{
		StopFollow();
		TickWalking(Dt);
		return;
	}
	UpdateStamina(Dt, false);

	const float CarryFactor = FMath::Clamp(1.f - Tuning.SpeedLossPerMass * CarriedMass, Tuning.MinCarrySpeedFactor, 1.f);
	const float Speed = State == EAntState::Riding ? (FollowRideOverride > 0.f ? FollowRideOverride : Tuning.RideSpeed)
	                                               : Tuning.WalkSpeed * CarryFactor * FollowReturnScale;
	// The point being followed waits for an ant that falls behind (a concave corner where the ground meets the trunk,
	// a sharp turn), so the straight step toward it never cuts through wood and pins the ant there. The wait is
	// bounded so a stuck ant still gets on, and a follow only finishes once the ant has reached the end.
	{
		const int32 NextIdx = FollowIndex + FollowDir;
		const FVector Cur = FollowPoints.IsValidIndex(NextIdx)
			? FMath::Lerp(FollowPoints[FollowIndex].Pos, FollowPoints[NextIdx].Pos, FollowFrac)
			: FollowPoints[FollowIndex].Pos;
		FollowLag = FVector::Dist(Pos, Cur);
		// The point followed can lie under the walking surface: a long step between two route points cuts through the wood
		// (test walks leave steps of metres across a limb), and a fillet covers a route laid on the field without fillets.
		// The ant then rides on the bark above it, so count only how far it trails that, not the depth. Otherwise the lag
		// stays over 30 cm, the point waits, and each one-second snap moves it on by one tick (about 1 cm/s: laden recruits
		// were rescued after 420 s at junctions). Only with fillets, so the field without them walks exactly as before
		// (where the same creep is shorter).
		if (F->HasFillets())
		{
			const double Buried = Tuning.RideHeight - F->Distance(Cur);
			if (Buried > 0.0) FollowLag = FMath::Max(FollowLag - Buried, 0.0);
		}
		// Pinned for a second (wood between the ant and the point it follows): step onto the route point itself, which
		// is on the bark. A small jump, rare, and it keeps every follower on its route.
		if (FollowLag > 30.0 && FollowWait > 1.f)
		{
			Pos = Cur;
			FollowLag = 0.0;
			FollowWait = 0.f;
			++FollowSnaps;
		}
		const bool bAtEnd = !FollowPoints.IsValidIndex(NextIdx);
		FollowWait = (FollowLag > 30.0 || bAtEnd) ? FollowWait + Dt : 0.f;
		if (bAtEnd && (FollowLag < 20.0 || FollowWait >= 2.f)) bFollowFinished = true;
	}
	double Remaining = (FollowLag > 30.0 && FollowWait < 2.f) ? 0.0 : Speed * Dt;
	const double Remaining0 = Remaining;
	while (Remaining > 0.0 && !bFollowFinished)
	{
		const int32 Next = FollowIndex + FollowDir;
		if (!FollowPoints.IsValidIndex(Next)) break; // at the end: finishes once the ant is there (above)
		const double SegLen = FMath::Max(FVector::Dist(FollowPoints[FollowIndex].Pos, FollowPoints[Next].Pos), 0.01);
		const double Left = SegLen * (1.0 - FollowFrac);
		if (Remaining < Left)
		{
			FollowFrac += Remaining / SegLen;
			Remaining = 0.0;
		}
		else
		{
			Remaining -= Left;
			FollowIndex = Next;
			FollowFrac = 0.0;
		}
	}
	const int32 Next = FollowIndex + FollowDir;
	const FVector Target = FollowPoints.IsValidIndex(Next)
		? FMath::Lerp(FollowPoints[FollowIndex].Pos, FollowPoints[Next].Pos, FollowFrac)
		: FollowPoints[FollowIndex].Pos;

	// Ease onto the path (riding starts up to RideReach away), then track it.
	const FVector Start = Pos;
	Pos = MoveTowards(Pos, Target, Speed * 2.0 * Dt + 1.0);
	const FVector Tried = Pos;
	FVector N;
	double Err = 0.0;
	const bool bProjOk = F->Project(Pos, N, Tuning.RideHeight, &Err);
	if (!bProjOk)
	{
		++ProjFailCount;
		Pos = Start;
		N = SurfaceNormal;
	}
	if (FollowDiagDue(this, Pos, Dt))
	{
		int32 Br = INDEX_NONE;
		const double DPos = F->Distance(Pos, &Br);
		double DTarget = 0.0;
		const FVector GT = F->RawGradient(Target, &DTarget);
		const FVector GP = F->RawGradient(Pos);
		FString Pts;
		for (int32 k = -2; k <= 2; ++k)
		{
			const int32 I = FollowIndex + k * FollowDir;
			if (!FollowPoints.IsValidIndex(I)) continue;
			Pts += FString::Printf(TEXT(" [%d %s d%.1f]"), I, *FollowPoints[I].Pos.ToCompactString(), F->Distance(FollowPoints[I].Pos));
		}
		UE_LOG(LogLeafline, Log, TEXT("FollowDiag %s %s: pos %s (d %.2f, |g| %.2f, br %d) n %s; target %s (d %.2f, |g| %.2f) idx %d/%d frac %.2f dir %d; lag %.1f wait %.2f rem %.1f; tried %s proj %s err %.2f; snaps %d fails %d;%s"),
			*GetOwner()->GetName(), *UEnum::GetValueAsString(State), *Pos.ToCompactString(), DPos, GP.Size(), Br, *N.ToCompactString(),
			*Target.ToCompactString(), DTarget, GT.Size(), FollowIndex, FollowPoints.Num(), FollowFrac, FollowDir, FollowLag, FollowWait, Remaining0,
			*Tried.ToCompactString(), bProjOk ? TEXT("ok") : TEXT("FAIL"), Err, FollowSnaps, ProjFailCount, *Pts);
	}
	SurfaceNormal = N;
	LastProjError = Err;
	Velocity = Dt > 0.f ? ProjectOnPlane((Pos - Start) / Dt, N) : FVector::ZeroVector;
	F->Distance(Pos, &CurrentBranch);
	UpdateOrientation(Dt);

	if (bFollowFinished && State == EAntState::Riding)
	{
		StopFollow();
		bFollowFinished = false;
	}
}

void UAntMovementComponent::StartFall(const FVector& WindDirection)
{
	FollowPoints.Reset();
	// Never into the bark: most blow-offs happen on the windward face, where the wind points into the wood, and the ant
	// used to fly straight through the trunk (the camera then showed only bark). Keep the along-surface part of the
	// wind and peel the ant off the surface.
	FVector W = WindDirection.GetSafeNormal2D();
	const double Into = FVector::DotProduct(W, SurfaceNormal);
	if (Into < 0.0) W -= SurfaceNormal * Into;
	FallVelocity = W * 520.0 + SurfaceNormal * 260.0 + FVector::UpVector * 120.0;
	FallSpinAxis = FVector::CrossProduct(FVector::UpVector, WindDirection).GetSafeNormal();
	if (FallSpinAxis.IsNearlyZero()) FallSpinAxis = FVector::RightVector;
	SetState(EAntState::Falling);
}

void UAntMovementComponent::TickFalling(float Dt)
{
	// A simple ballistic drop that ignores branches, with air drag and a tumble, until the ground.
	const FTreeField* F = GetField();
	FallVelocity.Z -= 980.0 * Dt;
	FallVelocity *= FMath::Max(0.0, 1.0 - 0.35 * Dt);
	Pos += FallVelocity * Dt;
	// Glance off wood on the way down instead of passing through it.
	if (Pos.Z > F->GroundZ + 50.0 && F->Distance(Pos) < 0.0)
	{
		FVector P = Pos, N;
		if (F->Project(P, N, 2.0))
		{
			Pos = P;
			const double VN = FVector::DotProduct(FallVelocity, N);
			if (VN < 0.0) FallVelocity -= N * (1.4 * VN);
		}
	}
	Velocity = FallVelocity;
	const FQuat Spin(FallSpinAxis, 7.0 * Dt);
	Up = Spin.RotateVector(Up).GetSafeNormal();
	Forward = Spin.RotateVector(Forward).GetSafeNormal();
	if (Pos.Z <= F->GroundZ + Tuning.RideHeight + 1.0 && FallVelocity.Z < 0.0)
	{
		Pos.Z = F->GroundZ + Tuning.RideHeight + 20.0;
		// Fell through the root flare: step out of the trunk before settling on the ground.
		FVector FromAxis = Pos - HomeTrunkBase;
		FromAxis.Z = 0.0;
		if (F->Distance(Pos) < 0.0 && FromAxis.SizeSquared() > 1.0)
		{
			Pos = FVector(HomeTrunkBase.X, HomeTrunkBase.Y, Pos.Z) + FromAxis.GetSafeNormal() * (HomeTrunkRadius + 350.0);
		}
		FVector N;
		F->Project(Pos, N, Tuning.RideHeight);
		SurfaceNormal = N;
		Velocity = FVector::ZeroVector;
		F->Distance(Pos, &CurrentBranch);
		SetState(EAntState::WalkingHome);
		UE_LOG(LogLeafline, Log, TEXT("%s: landed at %s, walking home"), *GetOwner()->GetName(), *Pos.ToString());
	}
	UpdateStamina(Dt, false);
}

FVector UAntMovementComponent::HomeWish(const FVector& N) const
{
	// Head for the nest along the ground, walking round the trunk when it is in the way.
	FVector ToNest = HomeTarget - Pos;
	ToNest.Z = 0.0;
	FVector FromAxis = Pos - HomeTrunkBase;
	FromAxis.Z = 0.0;
	const double R = FromAxis.Size();
	FVector Wish = ToNest.GetSafeNormal();
	if (R > UE_SMALL_NUMBER && R < HomeTrunkRadius + 400.0)
	{
		const FVector Out = FromAxis / R;
		if (FVector::DotProduct(Wish, -Out) > 0.2)
		{
			FVector Tangent = FVector::CrossProduct(FVector::UpVector, Out);
			if (FVector::DotProduct(Tangent, ToNest) < 0.0) Tangent = -Tangent;
			Wish = (Tangent + Out * 0.4).GetSafeNormal();
		}
	}
	return ProjectOnPlane(Wish, N).GetSafeNormal();
}

void UAntMovementComponent::TickRooted(float Dt)
{
	const FTreeField* F = GetField();
	UpdateStamina(Dt, false);
	Velocity = MoveTowards(Velocity, FVector::ZeroVector, Tuning.Deceleration * Dt);
	Pos += Velocity * Dt;
	FVector N;
	double Err = 0.0;
	F->Project(Pos, N, Tuning.RideHeight, &Err);
	SurfaceNormal = N;
	LastProjError = Err;
	UpdateOrientation(Dt);
}

void UAntMovementComponent::UpdateOrientation(float Dt)
{
	Up = SlerpNormal(Up, SurfaceNormal, 1.0 - FMath::Exp(-Tuning.UpRate * Dt));
	Forward = ProjectOnPlane(Forward, Up).GetSafeNormal();
	if (Forward.IsNearlyZero()) Forward = AnyPerpendicular(Up);

	const FVector V = ProjectOnPlane(Velocity, Up);
	if (V.SizeSquared() > 25.0)
	{
		// Turn about the body's up so a reversal never tips the body out of its plane.
		const FVector Desired = V.GetSafeNormal();
		const double Angle = FMath::Atan2(FVector::DotProduct(FVector::CrossProduct(Forward, Desired), Up), FVector::DotProduct(Forward, Desired));
		const double Alpha = 1.0 - FMath::Exp(-Tuning.TurnRate * Dt);
		Forward = FQuat(Up, Angle * Alpha).RotateVector(Forward).GetSafeNormal();
	}
}

void UAntMovementComponent::UpdateStamina(float Dt, bool bWantsMove)
{
	bSprinting = bSprintInput && !bSprintLocked && bWantsMove && State == EAntState::Walking && Stamina > 0.f;
	if (State == EAntState::WalkingHome) bSprinting = false;
	if (bSprinting)
	{
		Stamina -= (Tuning.StaminaDrain + Tuning.StaminaDrainPerMass * CarriedMass) * Dt;
		if (Stamina <= 0.f)
		{
			Stamina = 0.f;
			bSprintLocked = true;
			bSprintReleasedSinceLock = false;
			bSprinting = false;
			UE_LOG(LogLeafline, Log, TEXT("%s: stamina empty"), *GetOwner()->GetName());
		}
	}
	else
	{
		Stamina = FMath::Min(1.f, Stamina + Tuning.StaminaRegen * Dt);
		// Holding sprint through the empty bar no longer restarts it in stutters as the bar refills: the button must be
		// let go and pressed again (owner note).
		if (bSprintLocked && !bSprintInput) bSprintReleasedSinceLock = true;
		if (bSprintLocked && bSprintReleasedSinceLock && Stamina >= Tuning.StaminaResumeThreshold) bSprintLocked = false;
	}
}

void UAntMovementComponent::UpdateTelemetry(float Dt, double Moved, bool bHadInput)
{
	MaxProjErrWindow = FMath::Max(MaxProjErrWindow, LastProjError);
	MovedWindow += Moved;
	if (Tree.IsValid())
	{
		const FTreeSkeleton& S = Tree->GetSkeleton();
		VisitedLevels.Add(S.Branches.IsValidIndex(CurrentBranch) ? S.Branches[CurrentBranch].Level : -2);
	}

	// Stalls are judged on net displacement, so circling or vibrating in place counts as a stall.
	FTelemetryScratch& Scratch = GTelemetry.FindOrAdd(this);
	if (!Scratch.bInit)
	{
		Scratch.WindowStart = Scratch.StallStart = Pos;
		Scratch.bInit = true;
	}
	if (bHadInput)
	{
		StallTimer += Dt;
		StallMoved += Moved;
		if (StallTimer >= 1.f)
		{
			const double Net = FVector::Dist(Scratch.StallStart, Pos);
			const double ExpectedMin = Tuning.WalkSpeed * Tuning.MinCarrySpeedFactor * 0.25 * StallTimer;
			if (Net < ExpectedMin)
			{
				++StallCount;
				UE_LOG(LogLeafline, Warning, TEXT("%s: stall, net %.1f cm (path %.1f cm) in %.1f s at %s (branch %d, normal %s)"),
					*GetOwner()->GetName(), Net, StallMoved, StallTimer, *Pos.ToString(), CurrentBranch, *SurfaceNormal.ToString());
			}
			StallTimer = 0.f;
			StallMoved = 0.0;
			Scratch.StallStart = Pos;
		}
	}
	else
	{
		StallTimer = 0.f;
		StallMoved = 0.0;
		Scratch.StallStart = Pos;
	}

	LogTimer += Dt;
	if (LogTimer >= 5.f)
	{
		if (LeafDebug::AutoMove != 0.f || LeafDebug::AutoStrafe != 0.f || LeafDebug::bDebugField)
		{
			TArray<int32> Levels = VisitedLevels.Array();
			Levels.Sort();
			FString LevelStr;
			for (int32 L : Levels) LevelStr += FString::Printf(TEXT("%s%d"), LevelStr.IsEmpty() ? TEXT("") : TEXT(","), L);
			UE_LOG(LogLeafline, Log, TEXT("AutoMove: path %.0f cm, net %.0f cm in %.1f s, max projection error %.3f cm, stalls %d, projection failures %d, branch %d, z %.0f, levels visited [%s]"),
				MovedWindow, FVector::Dist(Scratch.WindowStart, Pos), LogTimer, MaxProjErrWindow, StallCount, ProjFailCount, CurrentBranch, Pos.Z, *LevelStr);
		}
		Scratch.WindowStart = Pos;
		LogTimer = 0.f;
		MovedWindow = 0.0;
		MaxProjErrWindow = 0.0;
	}
}

void UAntMovementComponent::ApplyTransform()
{
	AActor* Owner = GetOwner();
	if (!Owner) return;
	const FQuat Q = FRotationMatrix::MakeFromXZ(Forward, Up).ToQuat();
	Owner->SetActorLocationAndRotation(Pos, Q, false, nullptr, ETeleportType::TeleportPhysics);

	// Display only: the actor stays at the walking position (everything in the sim reads that); the visuals are lifted
	// onto a junction's skirt where one covers the ant, and tilted from the walking normal to the skirt's.
	ShownPos = Pos;
	ShownUp = Up;
	FVector P = Pos, N = SurfaceNormal;
	const bool bLift = State != EAntState::Falling && Tree.IsValid() && Tree->LiftOntoSkirts(P, N, Tuning.RideHeight);
	USceneComponent* Shown = ShownComponent.Get();
	if (bLift)
	{
		const FQuat Tilt = FQuat::FindBetweenNormals(SurfaceNormal, N);
		ShownPos = P;
		ShownUp = Tilt.RotateVector(Up);
		if (Shown)
		{
			const FQuat Inv = Q.Inverse();
			Shown->SetRelativeLocationAndRotation(Inv.RotateVector(P - Pos), Inv * Tilt * Q);
		}
	}
	else if (bShownLifted && Shown)
	{
		Shown->SetRelativeLocationAndRotation(FVector::ZeroVector, FQuat::Identity);
	}
	bShownLifted = bLift;
}
