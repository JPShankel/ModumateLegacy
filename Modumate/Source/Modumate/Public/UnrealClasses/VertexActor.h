// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include <functional>
#include "Database/ModumateArchitecturalMaterial.h"
#include "Engine/StaticMeshActor.h"
#include "VertexActor.generated.h"

/**
 * 
 */
class AEditModelPlayerController;

UCLASS()
class MODUMATE_API AVertexActor : public AStaticMeshActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AVertexActor();

	UPROPERTY()
	UStaticMeshComponent* MeshComp;

	FArchitecturalMaterial Material;

	UPROPERTY()
	AEditModelPlayerController *Controller;

	float HandleScreenSize = 0.0f;
	FVector MoiLocation = FVector::ZeroVector;
	FVector CameraLocation = FVector::ZeroVector;

	virtual void Tick(float DeltaTime) override;
	void SetActorMesh(UStaticMesh *mesh);
	void SetActorMaterial(FArchitecturalMaterial& MaterialData);
	void SetHandleScaleScreenSize(float NewSize);
	void SetMOILocation(const FVector &NewMOILocation);

protected:
	//~ Begin AActor Interface
	virtual void BeginPlay() override;
	//~ End AActor Interface

	void UpdateVisuals();
};
