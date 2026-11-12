#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NestActor.generated.h"

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nest")
	FLinearColor MoundColor = FLinearColor(0.07f, 0.055f, 0.035f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nest")
	FLinearColor HoleColor = FLinearColor(0.005f, 0.004f, 0.003f);

protected:
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

	float Pulse = 0.f;
	FVector MoundScale = FVector(2.4f, 2.4f, 0.14f);
};
