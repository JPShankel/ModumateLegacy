// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BIMKernel/Core/BIMKey.h"
#include "BIMKernel/Presets/BIMPresetEditorNode.h"
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Icon Material")
	int32 RenderTargetSize = 512;

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

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Icon Dimension")
	FVector StairDimension = FVector(250.f, -150.f, 300.f);

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Icon Size")
	float WallIconScaleFactor = 1.f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Icon Size")
	float FloorIconScaleFactor = 1.f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Icon Size")
	float PortalIconScaleFactor = 1.f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Icon Size")
	float CabinetScaleFactor = 1.f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Icon Size")
	float TrimIconScaleFactor = 1.f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Icon Size")
	float FFEIconScaleFactor = 22.f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Icon Size")
	float ProfileIconScaleFactor = 1.f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Icon Size")
	float StairIconScaleFactor = 1.f;

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

private:
	int32 StairLayerTreadAssemblyPartIndex = 1;
	int32 StairLayerRiserAssemblyPartIndex = 2;

public:

	UPROPERTY()
	UTextureRenderTarget2D* IconRenderTarget;

	// TODO: Temporary for storing render target until BIMKey can guarantee unique appearance
	UPROPERTY()
	TArray<UTextureRenderTarget2D*> BIMKeyToRenderTarget;

	// Called every frame
	virtual void Tick(float DeltaTime) override;

	bool SetIconMeshForAssembly(const FGuid& AsmKey, UMaterialInterface*& OutMaterial);
	bool SetIconMeshForBIMDesigner(bool UseDependentPreset, const FGuid& PresetID, UMaterialInterface*& OutMaterial, UTexture*& OutTexture, const FBIMEditorNodeIDType& NodeID);
	bool GetSavedIconFromPreset(const FGuid& PresetID, UTexture*& OutTexture);
	UMaterialInterface* CreateMaterialForIconTexture(const FGuid& PresetID, UTexture* InTexture);
	bool SetIconMeshForAssemblyType(const FBIMAssemblySpec &Assembly, UTextureRenderTarget2D* InRenderTarget, int32 PartIndex, bool bFromRootNode);

	bool SetIconMeshForWallAssembly(const FBIMAssemblySpec &Assembly, UTextureRenderTarget2D* InRenderTarget);
	bool SetIconMeshForFloorAssembly(const FBIMAssemblySpec &Assembly, UTextureRenderTarget2D* InRenderTarget);
	bool SetIconMeshForPortalAssembly(const FBIMAssemblySpec &Assembly, UTextureRenderTarget2D* InRenderTarget, int32 PartIndex);
	bool SetIconMeshForCabinetAssembly(const FBIMAssemblySpec &Assembly, UTextureRenderTarget2D* InRenderTarget, int32 PartIndex, bool bIncludeCabinetBoxMesh);
	bool SetIconMeshForTrimAssembly(const FBIMAssemblySpec &Assembly, UTextureRenderTarget2D* InRenderTarget);
	bool SetIconMeshForFFEAssembly(const FBIMAssemblySpec &Assembly, UTextureRenderTarget2D* InRenderTarget);
	bool SetIconMeshForStairAssembly(const FBIMAssemblySpec &Assembly, UTextureRenderTarget2D* InRenderTarget, bool bMakeTread, bool bMakeRiser);

	bool SetIconFromTextureAsset(const FGuid& PresetID, UMaterialInterface*& OutMaterial);
	bool SetIconMeshForRawMaterial(const FGuid& MaterialKey, UTextureRenderTarget2D* InRenderTarget);
	bool SetIconMeshForProfile(const FGuid& ProfileKey, UTextureRenderTarget2D* InRenderTarget);
	bool SetIconMeshForMesh(const FGuid& MeshKey, UTextureRenderTarget2D* InRenderTarget);
	bool SetIconMeshForPart(bool UseDependentPreset, const FGuid& PresetID, const FBIMEditorNodeIDType& NodeID, UTextureRenderTarget2D* InRenderTarget);
	bool SetIconMeshForMaterial(bool UseDependentPreset, const FGuid& PresetID, const FBIMEditorNodeIDType& NodeID, UTextureRenderTarget2D* InRenderTarget);
	bool SetIconMeshForModule(bool UseDependentPreset, const FGuid& PresetID, const FBIMEditorNodeIDType& NodeID, UTextureRenderTarget2D* InRenderTarget);
	bool SetIconMeshForLayerNodeID(const FBIMEditorNodeIDType& NodeID, UTextureRenderTarget2D* InRenderTarget);
	bool SetIconMeshForLayerPreset(const FGuid& PresetID, UTextureRenderTarget2D* InRenderTarget);
	bool SetIconFromColor(const FBIMKey& ColorHex, UMaterialInterface*& OutMaterial);

	void GetWallSliceLocationNormal(int32 CurrentLayer, int32 NumberOfLayers, const FVector& Cp1, const FVector& Cp2, float Height, FVector& OutLocation, FVector& OutNormal);
	void GetFloorSliceLocationNormal(int32 CurrentLayer, int32 NumberOfLayers, const FVector& StartPt, const FVector& EndPt, float Height, FVector& OutLocation, FVector& OutNormal, bool& OutSliced);
	bool SetComponentForIconCapture(UPrimitiveComponent* Comp, bool CanCapture);
	void SetIconDynamicMeshLayersForCapture(bool Visible);
	void SetIconDynamicMeshMeshCompForCapture(bool Visible);
	void SetIconCompoundMeshActorForCapture(bool Visible);

	void SetCaptureCompTransformForCapture(AActor* ActorToCapture, float SizeScale, bool OnlyCollidingComponents);

	// TODO: This is a temp function for releasing saved render targets related to BIM presets	
	// When BIMKey can guarantee unique appearance, then BIMKeyToRenderTarget array and this function can be removed	
	void ReleaseSavedRenderTarget();
};
