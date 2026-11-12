#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Colony/ColonyTypes.h"
#include "AntPawn.generated.h"

class UAntMovementComponent;
class UStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UProceduralMeshComponent;
class ATreeActor;
class ATrailNetwork;
class ADayDirector;

/** A worker ant. The same class is used for the controlled ant and unattended ones. */
UCLASS()
class ANTTREEGAME_API AAntPawn : public APawn
{
	GENERATED_BODY()

public:
	AAntPawn();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void UnPossessed() override;

	UAntMovementComponent* GetAntMovement() const { return Movement; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ant")
	FLinearColor BodyColor = FLinearColor(0.32f, 0.10f, 0.04f);

	/** Final mesh from the Blender script. While empty, placeholder spheres are shown. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ant")
	TObjectPtr<UStaticMesh> AntMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ant")
	TObjectPtr<UMaterialInterface> AntMaterial;

	/** Where the carried fragment attaches, relative to the actor (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ant")
	FVector MandibleOffset = FVector(11.f, 0.f, 4.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ant")
	FCutTuning Cut;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ant")
	FLinearColor FragmentColor = FLinearColor(0.12f, 0.36f, 0.06f);

	/** Multiplies the cut rate (the Mandibles upgrade). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ant")
	float CutRateMultiplier = 1.f;

	// Worker identity, set by the day director.
	void SetWorker(int32 Index, int32 Path);
	int32 GetWorkerIndex() const { return WorkerIndex; }
	int32 GetPathIndex() const { return PathIndex; }

	/** Held buttons, fed by the controller every frame while possessed. */
	void SetActionInput(bool bInCutHeld, bool bInFollowHeld);

	// Cutting and carrying.
	bool IsCutting() const { return CuttingLeaf != INDEX_NONE; }
	float GetCutSize() const { return CutSize; }
	float GetCutCap() const { return CutCap; }
	int32 GetLeafInReach() const { return LeafInReach; }
	bool IsTrailInReach() const { return bTrailInReach; }
	bool IsCarrying() const { return CarriedSize > 0.f; }
	float GetCarriedSize() const { return CarriedSize; }
	float GetCarriedMass() const { return CarriedMass; }
	float MassForSize(float Size01) const { return FMath::Lerp(Cut.MassMin, Cut.MassMax, FMath::Clamp(Size01, 0.f, 1.f)); }
	float DiameterForSize(float Size01) const { return FMath::Lerp(Cut.DiameterMin, Cut.DiameterMax, FMath::Clamp(Size01, 0.f, 1.f)); }
	void ClearFragment();
	/** Hands the ant a fragment without cutting (debug). */
	void GiveFragment(float Size01) { SetFragment(Size01); }

	/** Replay the recorded path back to the nest, unattended. */
	void StartReturn();

protected:
	UPROPERTY(VisibleAnywhere, Category = "Ant")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Ant")
	TObjectPtr<UAntMovementComponent> Movement;

	UPROPERTY(VisibleAnywhere, Category = "Ant")
	TObjectPtr<USceneComponent> VisualRoot;

	UPROPERTY(VisibleAnywhere, Category = "Ant")
	TObjectPtr<UStaticMeshComponent> Head;

	UPROPERTY(VisibleAnywhere, Category = "Ant")
	TObjectPtr<UStaticMeshComponent> Thorax;

	UPROPERTY(VisibleAnywhere, Category = "Ant")
	TObjectPtr<UStaticMeshComponent> Gaster;

	UPROPERTY(VisibleAnywhere, Category = "Ant")
	TObjectPtr<UStaticMeshComponent> Body;

	/** Holds the carried fragment. */
	UPROPERTY(VisibleAnywhere, Category = "Ant")
	TObjectPtr<USceneComponent> CarrySocket;

	UPROPERTY(VisibleAnywhere, Category = "Ant")
	TObjectPtr<UProceduralMeshComponent> FragmentMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FragmentMID;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BasicMaterial;

private:
	void UpdateCutting(float Dt);
	void UpdateRiding(float Dt);
	void UpdateRecording();
	void SetFragment(float Size01);
	void BuildFragmentMesh(float Diameter);
	ATreeActor* GetTree();
	ATrailNetwork* GetTrails();
	ADayDirector* GetDirector();

	TWeakObjectPtr<ATreeActor> TreeRef;
	TWeakObjectPtr<ADayDirector> DirectorRef;

	int32 WorkerIndex = INDEX_NONE;
	int32 PathIndex = INDEX_NONE;
	FVector LastRecordPos = FVector::ZeroVector;
	bool bCutHeld = false;
	bool bPrevCutHeld = false;
	bool bFollowHeld = false;
	int32 CuttingLeaf = INDEX_NONE;
	float CutSize = 0.f;
	float CutCap = 0.f;
	float CarriedSize = 0.f;
	float CarriedMass = 0.f;
	int32 LeafInReach = INDEX_NONE;
	bool bTrailInReach = false;
	float TrailCheckTimer = 0.f;
	float SpareFloat = 0.f;
	int32 SpareInt = 0;
};
