// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BIMKernel/BIMKey.h"
#include "Database/ModumateObjectEnums.h"
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

	UPROPERTY(EditAnywhere)
	class UStaticMeshComponent* IconCubeMesh;

	UPROPERTY()
	class ADynamicMeshActor* IconDynamicMeshActor;

	UPROPERTY()
	class ACompoundMeshActor* IconCompoundMeshActor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Icon Material")
	UMaterialInterface* CustomMaterialBase;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Icon Material")
	FName MaterialColorParamName = TEXT("ColorMultiplier");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Icon Material")
	UMaterialInterface* IconMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Icon Material")
	UMaterialInterface* IconColorMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Icon Material")
	UMaterialInterface* IconDimensionMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Icon Material")
	FName MaterialIconTextureParamName = TEXT("Texture");

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

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Icon Size")
	float ProfileIconScaleFactor = 24.f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Icon Size")
	float StairIconScaleFactor = 24.f;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY()
	class AEditModelGameMode_CPP* Gamemode;

	UPROPERTY()
	class AEditModelGameState_CPP* GameState;

	UPROPERTY()
	class AEditModelPlayerController_CPP* Controller;

	UPROPERTY()
	class UMaterialInstanceDynamic* DynCustomMaterial;

public:

	UPROPERTY()
	UTextureRenderTarget2D* IconRenderTarget;

	// TODO: Temporary for storing render target until BIMKey can guarantee unique appearance
	UPROPERTY()
	TArray<UTextureRenderTarget2D*> BIMKeyToRenderTarget;

	// Called every frame
	virtual void Tick(float DeltaTime) override;

	bool SetIconMeshForAssembly(const FBIMKey& AsmKey, UMaterialInterface*& OutMaterial);
	bool SetIconMeshForBIMDesigner(bool UseDependentPreset, const FBIMKey& PresetID, UMaterialInterface*& OutMaterial, UTexture*& OutTexture, int32 NodeID);
	bool GetSavedIconFromPreset(const FBIMKey& PresetID, UTexture*& OutTexture);
	UMaterialInterface* CreateMaterialForIconTexture(const FBIMKey& PresetID, UTexture* InTexture);
	bool SetIconMeshForAssemblyType(const FBIMAssemblySpec &Assembly, UTextureRenderTarget2D* InRenderTarget);

	bool SetIconMeshForWallAssembly(const FBIMAssemblySpec &Assembly, UTextureRenderTarget2D* InRenderTarget);
	bool SetIconMeshForFloorAssembly(const FBIMAssemblySpec &Assembly, UTextureRenderTarget2D* InRenderTarget);
	bool SetIconMeshForPortalAssembly(const FBIMAssemblySpec &Assembly, UTextureRenderTarget2D* InRenderTarget);
	bool SetIconMeshForCabinetAssembly(const FBIMAssemblySpec &Assembly, UTextureRenderTarget2D* InRenderTarget);
	bool SetIconMeshForTrimAssembly(const FBIMAssemblySpec &Assembly, UTextureRenderTarget2D* InRenderTarget);
	bool SetIconMeshForFFEAssembly(const FBIMAssemblySpec &Assembly, UTextureRenderTarget2D* InRenderTarget);
	bool SetIconMeshForStairAssembly(const FBIMAssemblySpec &Assembly, UTextureRenderTarget2D* InRenderTarget);

	bool SetIconMeshForRawMaterial(const FBIMKey& MaterialKey, UTextureRenderTarget2D* InRenderTarget);
	bool SetIconMeshForColor(const FBIMKey& ColorKey, UMaterialInterface*& OutMaterial);
	bool SetIconMeshForProfile(const FBIMKey& ProfileKey, UTextureRenderTarget2D* InRenderTarget);
	bool SetIconMeshForMesh(const FBIMKey& MeshKey, UTextureRenderTarget2D* InRenderTarget);
	bool SetIconMeshForPart(bool UseDependentPreset, const FBIMKey& PresetID, int32 NodeID, UTextureRenderTarget2D* InRenderTarget);
	bool SetIconMeshForMaterial(bool UseDependentPreset, const FBIMKey& PresetID, int32 NodeID, UTextureRenderTarget2D* InRenderTarget);
	bool SetIconMeshForModule(bool UseDependentPreset, const FBIMKey& PresetID, int32 NodeID, UTextureRenderTarget2D* InRenderTarget);
	bool SetIconMeshForLayerNodeID(int32 NodeID, UTextureRenderTarget2D* InRenderTarget);
	bool SetIconMeshForLayerPreset(const FBIMKey& PresetID, UTextureRenderTarget2D* InRenderTarget);

	void GetWallSliceLocationNormal(int32 CurrentLayer, int32 NumberOfLayers, const FVector& Cp1, const FVector& Cp2, float Height, FVector& OutLocation, FVector& OutNormal);
	void GetFloorSliceLocationNormal(int32 CurrentLayer, int32 NumberOfLayers, const FVector& StartPt, const FVector& EndPt, float Height, FVector& OutLocation, FVector& OutNormal, bool& OutSliced);
	bool SetComponentForIconCapture(UPrimitiveComponent* Comp, bool CanCapture);
	void SetIconDynamicMeshLayersForCapture(bool Visible);
	void SetIconCompoundMeshActorForCapture(bool Visible);

	// TODO: This is a temp function for releasing saved render targets related to BIM presets	
	// When BIMKey can guarantee unique appearance, then BIMKeyToRenderTarget array and this function can be removed	
	void ReleaseSavedRenderTarget();
};
