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
class ULeaflineAudioLoop;
class ALineSystem;

/** Where a worker's last piece came from: a line is built from here, not from where send-home was pressed. */
struct FAntCutRecord
{
	int32 Leaf = INDEX_NONE;
	/** Branch the ant stood on while cutting. */
	int32 Branch = INDEX_NONE;
	FVector Pos = FVector::ZeroVector;
	FVector Normal = FVector::UpVector;
	float Size = 0.f;
	float Richness = 1.f;
	bool IsValid() const { return Leaf != INDEX_NONE; }
};

/** What knocked an ant off the bark: the log line and the day's stats tell them apart. */
enum class EBlowOffCause : uint8
{
	Wind,
	Bird // the bird's wing gust (World/BirdSystem.*)
};

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
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

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
	FVector MandibleOffset = FVector(9.1f, 0.f, 4.f);

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
	void SetActionInput(bool bInCutHeld, bool bInFollowHeld, bool bInGripHeld);

	// Cutting and carrying.
	bool IsCutting() const { return CuttingLeaf != INDEX_NONE; }
	float GetCutSize() const { return CutSize; }
	float GetCutCap() const { return CutCap; }
	int32 GetLeafInReach() const { return LeafInReach; }
	/** The leaf being cut, or INDEX_NONE. */
	int32 GetCuttingLeaf() const { return CuttingLeaf; }
	bool IsTrailInReach() const { return bTrailInReach; }
	bool IsCarrying() const { return CarriedSize > 0.f; }
	float GetCarriedSize() const { return CarriedSize; }
	/** Size-based mass: what slows the ant and tires it. */
	float GetCarriedMass() const { return CarriedMass; }
	/** What the colony is credited on delivery: the mass times the leaf's richness. */
	float GetCarriedValue() const { return IsCarrying() ? CarriedMass * CarriedRichness : 0.f; }
	float GetCarriedRichness() const { return CarriedRichness; }
	/** The last piece this ant cut (invalid until it cuts one). */
	const FAntCutRecord& GetLastCut() const { return LastCut; }

	// Recruits: workers the colony sends by itself along a line. Never controlled; the line system steps them.
	void MakeRecruit(ALineSystem* Owner);
	bool IsRecruit() const { return bRecruit; }
	/** Cut this leaf now, the piece growing to Size at the ant's cut rate, then onto the back (recruits). */
	void StartAutoCut(int32 Leaf, float Size);
	/** Movement and logic advance only through SimStep (recruits, and returning scouts during the harvest). */
	void SetSimDriven(bool bDriven);
	bool IsSimDriven() const { return bSimDriven; }
	/** One fixed sim step. bVisuals: also update the transform, legs and other looks (the last step of a frame). */
	void SimStep(float Dt, bool bVisuals);
	/** After a frame's sim steps: place the actor and update the legs and sounds once. */
	void PushVisuals();
	float MassForSize(float Size01) const { return FMath::Lerp(Cut.MassMin, Cut.MassMax, FMath::Clamp(Size01, 0.f, 1.f)); }
	float DiameterForSize(float Size01) const { return FMath::Lerp(Cut.DiameterMin, Cut.DiameterMax, FMath::Clamp(Size01, 0.f, 1.f)); }
	void ClearFragment();
	/** Hands the ant a fragment without cutting (debug). */
	void GiveFragment(float Size01) { SetFragment(Size01); }
	/** Cuts the leaf in reach at once with the given size (debug). Returns false if no leaf is in reach. */
	bool DebugCutNow(float Size01);
	/** Cuts this leaf at once, wherever the ant is (automated tests). */
	bool DebugCutLeaf(int32 Leaf, float Size01);

	/** Replay the recorded path back to the nest, unattended. */
	void StartReturn();

	/** Home: walk into the nest entrance and sink out of sight (the piece, if any, drops in first), then destroy itself. */
	void DepositAndVanish(const FVector& Entrance);
	bool IsVanishing() const { return VanishTime >= 0.f; }

	/** Wind (or the bird's wing gust) knocked the ant off: it drops its fragment, falls and walks home. */
	void BlowOff(float Force, float Hold, float Exposure, EBlowOffCause Cause = EBlowOffCause::Wind);
	/** Bait (the bird, tier 2): the piece is put down where the ant stands and the ant is as before its cut (no line). */
	void DropCarried();
	bool IsGripping() const;

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

	/** Body material instance carrying the gait phase for the shader-driven legs. */
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> BodyMID;

	UPROPERTY(Transient) TObjectPtr<ULeaflineAudioLoop> CutLoop;
	UPROPERTY(Transient) TObjectPtr<ULeaflineAudioLoop> GripLoop;

private:
	void StepLogic(float Dt, bool bVisuals);
	void UpdateCutting(float Dt);
	/** The cut ends: bite the leaf and take the piece if it is big enough, else cancel. */
	void FinishCut();
	void UpdateRiding(float Dt);
	void UpdateRecording();
	void UpdateGrip();
	void UpdateWind(float Dt);
	float GetGripMultiplier() const;
	void SetFragment(float Size01);
	/** The cut piece tears out of the leaf at WorldCenter (in the leaf's plane) and swings up onto the ant's back. */
	void StartFragmentSwing(const FVector& WorldCenter, const FVector& LeafDir, const FVector& LeafNormal);
	void UpdateCutVisuals(float Dt);
	void UpdateVanish(float Dt);
	/** The dropped piece flutters away downwind, spinning and shrinking, then goes back on the socket hidden. */
	void UpdateFlutter(float Dt);
	void ReattachFragment();
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
	bool bGripHeld = false;
	float WindCheckTimer = 0.f;
	float GustMaxRatio = 0.f;
	float GustMaxExposure = 0.f;
	bool bGustSeen = false;
	bool bGustGripped = false;
	float StepDistance = 0.f;
	FVector LastStepPos = FVector::ZeroVector;
	bool bWasSprintLocked = false;
	float GaitPhase = 0.f;
	float GaitAmount = 0.f;
	void UpdateSounds();
	int32 CuttingLeaf = INDEX_NONE;
	float CutSize = 0.f;
	float CutCap = 0.f;
	float CarriedSize = 0.f;
	float CarriedMass = 0.f;
	float CarriedRichness = 1.f;
	FAntCutRecord LastCut;
	bool bRecruit = false;
	bool bSimDriven = false;
	bool bAutoCut = false;
	TWeakObjectPtr<ALineSystem> LineOwner;
	int32 LeafInReach = INDEX_NONE;
	bool bTrailInReach = false;
	float TrailCheckTimer = 0.f;
	float SpareFloat = 0.f;
	int32 SpareInt = 0;
	// Cut sequence: sawing motion while cutting, then the piece's swing from the leaf to the back.
	float SawTime = 0.f;
	float SwingTime = -1.f; // < 0: no swing running
	FRotator FragmentRestRotation = FRotator(12.f, 35.f, 0.f);
	float VanishTime = -1.f;
	FVector VanishFrom = FVector::ZeroVector;
	FVector VanishTo = FVector::ZeroVector;
	FTransform VanishPieceFrom;
	bool bVanishPiece = false;
	float FlutterTime = -1.f;
	float VisTime = 0.f;
	float FeltTimer = 0.f;
	float FeltExposure = 0.f;
	float WindFeel = 0.f;
	FVector FlutterVelocity = FVector::ZeroVector;
	FVector FlutterSpinAxis = FVector::UpVector;
	FTransform SwingFrom;
};
