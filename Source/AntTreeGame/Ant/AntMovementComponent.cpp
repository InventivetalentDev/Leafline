#include "Ant/AntMovementComponent.h"
#include "AntTreeGame.h"
#include "Tree/TreeActor.h"
#include "Tree/TreeSkeleton.h"
#include "Debug/LeafDebug.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"

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

void UAntMovementComponent::SetMoveInput(const FVector2D& Input, const FVector& InCamForward, const FVector& InCamUp)
{
	MoveInput = Input.GetClampedToMaxSize(1.0);
	CamForward = InCamForward;
	CamUp = InCamUp;
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
	ApplyTransform();

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

void UAntMovementComponent::TickWalking(float Dt)
{
	const FTreeField* F = GetField();
	const bool bWantsMove = MoveInput.SizeSquared() > 0.01;
	UpdateStamina(Dt, bWantsMove);

	const float CarryFactor = FMath::Clamp(1.f - Tuning.SpeedLossPerMass * CarriedMass, Tuning.MinCarrySpeedFactor, 1.f);
	const float Speed = Tuning.WalkSpeed * CarryFactor * (bSprinting ? Tuning.SprintMultiplier : 1.f);
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
		}
	}
	if (ThinBlend <= 0.f) SpareInt = 0;

	for (int32 s = 0; s < Steps; ++s)
	{
		FVector Wish = State == EAntState::WalkingHome ? HomeWish(N) : ComputeWish(N);
		if (ThinBlend > 0.f && bWantsMove && State == EAntState::Walking)
		{
			// At a branch tip the axis turns into the surface normal; fade the push there so the ant comes to rest at
			// the tip instead of overshooting the apex back and forth.
			const FVector AP = ProjectOnPlane(Axis, N);
			const double APLen = AP.Size();
			const FVector A = APLen > 1e-3 ? AP / APLen : FVector::ZeroVector;
			const double TipFade = FMath::Clamp(APLen / 0.6, 0.0, 1.0);
			if (!A.IsNearlyZero())
			{
				const double InputMag = FMath::Min(MoveInput.Size(), 1.0);
				const double Along = FVector::DotProduct(Wish, A);
				if (FMath::Abs(Along) > 0.25 * InputMag)
				{
					SpareInt = Along >= 0.0 ? 1 : -1;
				}
				else if (SpareInt == 0)
				{
					SpareInt = FVector::DotProduct(Forward, A) >= 0.0 ? 1 : -1;
				}
				Wish = FMath::Lerp(Wish, A * (SpareInt * InputMag * TipFade), (double)ThinBlend);
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

		const FVector Target = Wish * Speed;
		const float Rate = Wish.IsNearlyZero() ? Tuning.Deceleration : Tuning.Acceleration;
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
		// Carry the velocity over the curve into the new tangent plane without losing speed.
		Velocity = ProjectOnPlane(FQuat::FindBetweenNormals(N, NewN).RotateVector(Velocity), NewN);
		N = NewN;
	}

	SurfaceNormal = N;
	LastProjError = MaxErr;
	F->Distance(Pos, &CurrentBranch);
	UpdateOrientation(Dt);
	UpdateTelemetry(Dt, FVector::Dist(Start, Pos), bWantsMove);
}

void UAntMovementComponent::StartFollow(const TArray<FTrailPoint>& Points, int32 StartIndex, int32 Direction, EAntState FollowState)
{
	if (Points.Num() < 2) return;
	FollowPoints = Points;
	FollowIndex = FMath::Clamp(StartIndex, 0, Points.Num() - 1);
	FollowFrac = 0.0;
	FollowDir = Direction >= 0 ? 1 : -1;
	bFollowFinished = false;
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
	const float Speed = State == EAntState::Riding ? Tuning.RideSpeed : Tuning.WalkSpeed * CarryFactor;
	double Remaining = Speed * Dt;
	while (Remaining > 0.0 && !bFollowFinished)
	{
		const int32 Next = FollowIndex + FollowDir;
		if (!FollowPoints.IsValidIndex(Next))
		{
			bFollowFinished = true;
			break;
		}
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
	FVector N;
	double Err = 0.0;
	if (!F->Project(Pos, N, Tuning.RideHeight, &Err))
	{
		++ProjFailCount;
		Pos = Start;
		N = SurfaceNormal;
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
	FallVelocity = WindDirection.GetSafeNormal2D() * 520.0 + SurfaceNormal * 160.0 + FVector::UpVector * 120.0;
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
			bSprinting = false;
			UE_LOG(LogLeafline, Log, TEXT("%s: stamina empty"), *GetOwner()->GetName());
		}
	}
	else
	{
		Stamina = FMath::Min(1.f, Stamina + Tuning.StaminaRegen * Dt);
		if (bSprintLocked && Stamina >= Tuning.StaminaResumeThreshold) bSprintLocked = false;
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
		if (LeafDebug::AutoMove != 0.f || LeafDebug::bDebugField)
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
	if (AActor* Owner = GetOwner())
	{
		Owner->SetActorLocationAndRotation(Pos, FRotationMatrix::MakeFromXZ(Forward, Up).ToQuat(), false, nullptr, ETeleportType::TeleportPhysics);
	}
}
