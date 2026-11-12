#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "AntPawn.generated.h"

class UAntMovementComponent;
class UStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;

/** A worker ant. The same class is used for the controlled ant and unattended ones. */
UCLASS()
class ANTTREEGAME_API AAntPawn : public APawn
{
	GENERATED_BODY()

public:
	AAntPawn();

	virtual void BeginPlay() override;

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

	UPROPERTY()
	TObjectPtr<UMaterialInterface> BasicMaterial;
};
