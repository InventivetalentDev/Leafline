#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LeaflineShots.generated.h"

class AAntCamera;
class AAntPawn;
class ATreeActor;

/** Console-only recording helper. Spawned on demand, with no save or economy access. */
UCLASS(NotBlueprintable, Transient)
class ANTTREEGAME_API ALeaflineShots : public AActor
{
	GENERATED_BODY()

public:
	ALeaflineShots();
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	static ALeaflineShots* Get(UWorld* World);
	void SetCinematic(bool bOn);
	bool Play(const FString& Name, float Seconds);
	bool PlayList(const TArray<FString>& Names);
	static bool IsShotName(const FString& Name);
	void SetSlomo(float Factor);
	void Stop();

private:
	AAntCamera* FindCamera() const;
	void ApplyCinematic();
	bool StartShot(const FString& Name, float Seconds);
	void PrepareReveal();
	void TickReveal();
	bool ClearPosition(FVector& Position) const;
	bool SweepPosition(FVector& Position) const;
	void BeginReturn();
	void TickReturn(double T);
	void Evaluate(double T, FVector& Position, FVector& LookAt, FVector& Up, float& FOV) const;

	bool bCinematic = false;
	bool bSavedScreenMessages = true;
	TWeakObjectPtr<AAntCamera> ActiveCamera;
	TWeakObjectPtr<AAntPawn> ShotAnt;
	TWeakObjectPtr<ATreeActor> ShotTree;
	FString ShotName;
	TArray<FString> Queue;
	float Duration = 8.f;
	double Elapsed = 0.0;
	double LastRealTime = 0.0;
	FTransform ReturnView;
	float ReturnFOV = 75.f;
	FTransform EntryView;
	float EntryFOV = 75.f;
	bool bReturning = false;
	FTransform ReturnStartView;
	float ReturnStartFOV = 75.f;
	TArray<FVector> History;
	TArray<FVector> ReturnPath;
	TArray<double> ReturnDistances;
	FVector Anchor = FVector::ZeroVector;
	FVector Normal = FVector::UpVector;
	FVector Forward = FVector::ForwardVector;
	FVector Right = FVector::RightVector;
	FVector Crown = FVector::ZeroVector;
	FBox TreeBounds = FBox(ForceInit);
	FVector Nest = FVector::ZeroVector;
	FVector NestOut = FVector::ForwardVector;
	FVector RevealEnd = FVector::ZeroVector;
	FVector RevealFocus = FVector::ZeroVector;
	float RevealFOV = 110.f;
	float RevealMaxRoll = 0.f;
	int32 RevealNextSample = 0;
	int32 DollyBranch = INDEX_NONE;
	double DollyFrom = 0.0;
	double DollyTo = 0.0;
	FVector DollyRadial = FVector::UpVector;
	FVector ChasePosition = FVector::ZeroVector;
	FVector ChaseFocus = FVector::ZeroVector;
	FVector ChaseUp = FVector::UpVector;
	float Boom = 170.f;
	FVector LastSafePosition = FVector::ZeroVector;
	double MinClearance = TNumericLimits<double>::Max();
	int32 HeldFrames = 0;
	int32 ClippedFrames = 0;
	bool bSlomoSet = false;
	float SavedDilation = 1.f;
};
