// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include <functional>
#include "Engine/StaticMeshActor.h"
#include "ModumateVertexActor_CPP.generated.h"

/**
 * 
 */
class AEditModelPlayerController_CPP;

UCLASS()
class MODUMATE_API AModumateVertexActor_CPP : public AStaticMeshActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AModumateVertexActor_CPP();

	UPROPERTY()
	UStaticMeshComponent* MeshComp;

	UPROPERTY()
	UMaterialInstanceDynamic* DynamicMaterial;

	UPROPERTY()
	AEditModelPlayerController_CPP *Controller;

	float HandleScreenSize = 0.0f;
	FVector MoiLocation = FVector::ZeroVector;
	FVector CameraLocation = FVector::ZeroVector;

	virtual void Tick(float DeltaTime) override;
	void SetActorMesh(UStaticMesh *mesh);
	void SetHandleScaleScreenSize(float NewSize);
	void SetMOILocation(const FVector &NewMOILocation);

protected:
	//~ Begin AActor Interface
	virtual void BeginPlay() override;
	//~ End AActor Interface

	void UpdateVisuals();
};
