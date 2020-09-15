// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DynamicIconGenerator.generated.h"

class FBIMAssemblySpec;

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
	class USpringArmComponent* SpringArm;

	UPROPERTY(EditAnywhere)
	class USceneCaptureComponent2D* SceneCaptureComp;

	UPROPERTY(EditAnywhere)
	class URectLightComponent* RectLight;

	UPROPERTY(EditAnywhere)
	class URectLightComponent* RectLight1;

	// Reusable meshes
	UPROPERTY(EditAnywhere)
	class UStaticMeshComponent* IconStaticMesh;

	UPROPERTY(EditAnywhere)
	class UStaticMeshComponent* IconSphereMesh;

	UPROPERTY()
	class ADynamicMeshActor* IconDynamicMeshActor;

	UPROPERTY()
	class ACompoundMeshActor* IconCompoundMeshActor;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Icon Dimension")
	float WallLength = 91.44f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Icon Dimension")
	float WallHeight = 60.96f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Icon Dimension")
	float FloorLength = 121.92f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Icon Dimension")
	float FloorDepth = 91.44f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Icon Dimension")
	FVector CabinetDimension = FVector(30.0f, 24.0f, 34.5f);

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Icon Dimension")
	float TrimLength = 32.f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Icon Size")
	float WallIconScaleFactor = 0.35f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Icon Size")
	float FloorIconScaleFactor = 0.25f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Icon Size")
	float PortalIconScaleFactor = 50.f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Icon Size")
	float CabinetIconScaleFactor = 23.f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Icon Size")
	float TrimIconScaleFactor = 24.f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Icon Size")
	float FFEIconScaleFactor = 22.f;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "Dynamic Icon Generator")
	bool SetIconMeshForAssemblyByToolMode(bool UseAssemblyFromBIMDesigner, const FName &AsmKey, EToolMode mode, UTextureRenderTarget2D* RenderTarget);
	bool SetIconMeshForBIMDesigner(const FName &PresetID, UTextureRenderTarget2D* RenderTarget);

	bool SetIconMeshForWallAssembly(const FBIMAssemblySpec &Assembly, EToolMode mode, UTextureRenderTarget2D* RenderTarget);
	bool SetIconMeshForFloorAssembly(const FBIMAssemblySpec &Assembly, EToolMode mode, UTextureRenderTarget2D* RenderTarget);
	bool SetIconMeshForPortalAssembly(const FBIMAssemblySpec &Assembly, EToolMode mode, UTextureRenderTarget2D* RenderTarget);
	bool SetIconMeshForCabinetAssembly(const FBIMAssemblySpec &Assembly, UTextureRenderTarget2D* RenderTarget);
	bool SetIconMeshForTrimAssembly(const FBIMAssemblySpec &Assembly, EToolMode mode, UTextureRenderTarget2D* RenderTarget);
	bool SetIconMeshForFFEAssembly(const FBIMAssemblySpec &Assembly, UTextureRenderTarget2D* RenderTarget);

	bool SetIconMeshForMaterial(const FName &MaterialKey, UTextureRenderTarget2D* RenderTarget);

	void GetWallSliceLocationNormal(int32 CurrentLayer, int32 NumberOfLayers, const FVector& Cp1, const FVector& Cp2, float Height, FVector& OutLocation, FVector& OutNormal);
	void GetFloorSliceLocationNormal(int32 CurrentLayer, int32 NumberOfLayers, const FVector& StartPt, const FVector& EndPt, float Height, FVector& OutLocation, FVector& OutNormal, bool& OutSliced);
	bool SetComponentForIconCapture(UPrimitiveComponent* Comp, bool CanCapture);
	void SetIconDynamicMeshLayersForCapture(bool Visible);
	void SetIconCompoundMeshActorForCapture(bool Visible);
};
