// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Database/ModumateShoppingItem.h"
#include "DynamicIconGenerator.generated.h"


class USpringArmComponent;
class USceneCaptureComponent2D;
class UStaticMeshComponent;
class URectLightComponent;
class UProceduralMeshComponent;
class UTextureRenderTarget2D;

UCLASS()
class MODUMATE_API ADynamicIconGenerator : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ADynamicIconGenerator();

	UPROPERTY(EditAnywhere)
	USceneComponent* Root;

	UPROPERTY(EditAnywhere)
	USpringArmComponent* SpringArm;

	UPROPERTY(EditAnywhere)
	USceneCaptureComponent2D* SceneCaptureComp;

	UPROPERTY(EditAnywhere)
	UStaticMeshComponent* GroundPlane;

	UPROPERTY(EditAnywhere)
	USceneComponent* CpRef1;

	UPROPERTY(EditAnywhere)
	USceneComponent* CpRef2;

	UPROPERTY(EditAnywhere)
	URectLightComponent* RectLight;

	UPROPERTY(EditAnywhere)
	URectLightComponent* RectLight1;

	UPROPERTY(EditAnywhere)
	TArray<UProceduralMeshComponent*> Procedurallayers;

	UPROPERTY(EditAnywhere)
	TArray<UStaticMeshComponent*> StaticMeshes;

	UPROPERTY(EditAnywhere)
	TArray<UTextureRenderTarget2D*> RenderTargetPool;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "Icon Generation")
	void GetWallSliceParam(FVector& Location, FVector& Normal, int32 CurrentLayer, int32 NumberOfLayers, FVector Cp1, FVector Cp2, float Height);

	UFUNCTION(BlueprintCallable, Category = "Icon Generation")
	TArray<FColor> GetFColorFromRenderTarget(UTextureRenderTarget2D* RenderTarget);

	UFUNCTION(BlueprintCallable, Category = "Icon Generation")
	TArray<FColor> DownsampleTextureData(TArray<FColor>& Data, int32 Width, int32 Kernel);
};
