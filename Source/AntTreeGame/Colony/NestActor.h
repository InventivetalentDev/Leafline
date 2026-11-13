#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NestActor.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInterface;

/** The nest entrance among the roots. Workers start here and deposit fragments here. */
UCLASS()
class ANTTREEGAME_API ANestActor : public AActor
{
	GENERATED_BODY()

public:
	ANestActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/** A fragment arrived: a small pulse so the deposit reads on screen. */
	void PlayDeposit(float Mass);

	/** Match the mound to the walking field's (cm). SM_Nest is scaled from its authored 120 x 7 cm; without it, a
	 *  flattened sphere half in the ground. */
	void SetMoundSize(float Radius, float Height);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nest")
	FLinearColor MoundColor = FLinearColor(0.07f, 0.055f, 0.035f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nest")
	FLinearColor HoleColor = FLinearColor(0.005f, 0.004f, 0.003f);

protected:
	/** Loads SM_Nest and its materials the first time it is called; false if the mesh is missing (sphere and disc stay). */
	bool EnsureNestMesh();

	UPROPERTY(VisibleAnywhere, Category = "Nest")
	TObjectPtr<UStaticMeshComponent> Mound;

	UPROPERTY(VisibleAnywhere, Category = "Nest")
	TObjectPtr<UStaticMeshComponent> Hole;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BasicMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> SoilMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> DarkMaterial;

	/** The generated nest (Tools/blender/make_nest.py), null if it is not in the project. */
	UPROPERTY()
	TObjectPtr<UStaticMesh> NestMesh;

	bool bNestMeshTried = false;
	float MoundRadius = 120.f;
	float MoundHeight = 7.f;
	float Pulse = 0.f;
	FVector MoundScale = FVector(2.4f, 2.4f, 0.14f);
};
