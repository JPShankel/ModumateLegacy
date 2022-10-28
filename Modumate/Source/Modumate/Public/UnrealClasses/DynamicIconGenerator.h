// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BIMKernel/Presets/BIMPresetEditorNode.h"
#include "BIMKernel/Presets/BIMPresetCollection.h"
#include "Objects/ModumateObjectEnums.h"
#include "DynamicIconGenerator.generated.h"

struct FBIMAssemblySpec;

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
	UMaterialInterface* IconSwapWarningMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Icon Material")
	UMaterialInterface* IconRenderSampleMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Icon Material")
	FName MaterialIconTextureParamName = TEXT("Texture");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Icon Material")
	int32 RenderTargetSize = 512;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Icon Material")
	int32 RenderTargetSizeForWeb = 64;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Icon Material")
	float RenderTargetWebGammaScale = 1.5;

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
	class UModumateGameInstance* GameInstance;

	UPROPERTY()
	class AEditModelGameState* GameState;

	UPROPERTY()
	class AEditModelPlayerController* Controller;

	UPROPERTY()
	class UMaterialInstanceDynamic* DynCustomMaterial;

private:
	int32 StairLayerTreadAssemblyPartIndex = -1;
	int32 StairLayerRiserAssemblyPartIndex = -2;

	UMaterialInterface* CreateMaterialForIconTexture(UTexture2D* InTexture);

	void GetRawMaterialAndColorFromPreset(const FBIMPresetInstance& InPreset, FColor& OutColor, FGuid& OutRawMaterial);

	TMap<FGuid, FString> WebCache;

public:

	UPROPERTY()
	UTextureRenderTarget2D* IconRenderTarget;

	UPROPERTY()
	UTextureRenderTarget2D* IconRenderTargetForWeb;

	// Called every frame
	virtual void Tick(float DeltaTime) override;

	bool SetIconMeshForAssembly(const FBIMPresetCollectionProxy& PresetCollection, const FGuid& AsmKey, bool bAllowOverwrite = false);
	bool SetIconMeshForAssembly(const FBIMPresetCollectionProxy& PresetCollection, const FGuid& AsmKey, UMaterialInterface*& OutMaterial, bool bAllowOverwrite = false);
	bool SetIconMeshForBIMDesigner(const FBIMPresetCollectionProxy& PresetCollection,bool bUseDependentPreset, const FGuid& PresetID, UMaterialInterface*& OutMaterial, const FBIMEditorNodeIDType& NodeID, bool bSaveToCache = true);
	bool GetSavedIconFromPreset(const FGuid& PresetID, UTexture2D*& OutTexture);
	bool SetIconMeshForAssemblyType(const FBIMAssemblySpec &Assembly, UTextureRenderTarget2D* InRenderTarget, int32 PartIndex, bool bFromRootNode);
	
	bool GetIconMeshForAssemblyForWeb(const FGuid& AsmKey, FString& OutResponse, bool bExportToTempFolder = false);

	bool SetIconMeshForWallAssembly(const FBIMAssemblySpec &Assembly, UTextureRenderTarget2D* InRenderTarget);
	bool SetIconMeshForFloorAssembly(const FBIMAssemblySpec &Assembly, UTextureRenderTarget2D* InRenderTarget);
	bool SetIconMeshForPortalAssembly(const FBIMAssemblySpec &Assembly, UTextureRenderTarget2D* InRenderTarget, int32 PartIndex);
	bool SetIconMeshForCabinetAssembly(const FBIMAssemblySpec &Assembly, UTextureRenderTarget2D* InRenderTarget, int32 PartIndex, bool bIncludeCabinetBoxMesh);
	bool SetIconMeshForTrimAssembly(const FBIMAssemblySpec &Assembly, UTextureRenderTarget2D* InRenderTarget);
	bool SetIconMeshForStairAssembly(const FBIMAssemblySpec &Assembly, UTextureRenderTarget2D* InRenderTarget, bool bMakeTread, bool bMakeRiser);
	bool SetIconMeshForPointHostedAssembly(const FBIMAssemblySpec& Assembly, UTextureRenderTarget2D* InRenderTarget);

	bool SetIconFromTextureAsset(const FGuid& PresetID, UMaterialInterface*& OutMaterial);
	bool SetIconForIESProfile(const FBIMPresetInstance* Preset, UMaterialInterface*& OutMaterial);
	bool SetIconMeshForRawMaterial(const FGuid& MaterialKey, UTextureRenderTarget2D* InRenderTarget);
	bool SetIconMeshForProfile(const FGuid& ProfileKey, UTextureRenderTarget2D* InRenderTarget);
	bool SetIconMeshForMesh(const FGuid& MeshKey, UTextureRenderTarget2D* InRenderTarget);
	bool SetIconMeshForPart(const FBIMPresetCollectionProxy& PresetCollection,bool UseDependentPreset, const FGuid& PresetID, const FBIMEditorNodeIDType& NodeID, UTextureRenderTarget2D* InRenderTarget);
	bool SetIconMeshForMaterial(const FBIMPresetCollectionProxy& PresetCollection, bool UseDependentPreset, const FGuid& PresetID, const FBIMEditorNodeIDType& NodeID, UTextureRenderTarget2D* InRenderTarget);
	bool SetIconMeshForModule(const FBIMPresetCollectionProxy& PresetCollection, bool UseDependentPreset, const FGuid& PresetID, const FBIMEditorNodeIDType& NodeID, UTextureRenderTarget2D* InRenderTarget);
	bool SetIconMeshForLayerNodeID(const FBIMPresetCollectionProxy& PresetCollection, const FBIMEditorNodeIDType& NodeID, UTextureRenderTarget2D* InRenderTarget);
	bool SetIconMeshForLayerPreset(const FBIMPresetCollectionProxy& PresetCollection, const FGuid& PresetID, UTextureRenderTarget2D* InRenderTarget);
	
	bool SetIconMeshForSymbol(const FBIMPresetInstance* SymbolPreset, UTextureRenderTarget2D* InRenderTarget);
	bool SetIconFromColor(const FString& ColorHex, UMaterialInterface*& OutMaterial);

	void GetWallSliceLocationNormal(int32 CurrentLayer, int32 NumberOfLayers, const FVector& Cp1, const FVector& Cp2, float Height, FVector& OutLocation, FVector& OutNormal);
	void GetFloorSliceLocationNormal(int32 CurrentLayer, int32 NumberOfLayers, const FVector& StartPt, const FVector& EndPt, float Height, FVector& OutLocation, FVector& OutNormal, bool& OutSliced);
	bool SetComponentForIconCapture(UPrimitiveComponent* Comp, bool CanCapture);
	void SetIconDynamicMeshLayersForCapture(bool Visible);
	void SetIconDynamicMeshMeshCompForCapture(bool Visible);
	void SetIconCompoundMeshActorForCapture(bool Visible);

	void SetCaptureCompTransformForCapture(AActor* ActorToCapture, float SizeScale, bool OnlyCollidingComponents);

	void UpdateCachedAssemblies(const FBIMPresetCollectionProxy& PresetCollection, const TArray<FGuid>& AsmKeys);

	bool InvalidateWebCacheEntry(const FGuid& Key) { return WebCache.Remove(Key) != 0; }

	// TODO: This is a temp function for releasing saved render targets related to BIM presets	
	// When BIMKey can guarantee unique appearance, then BIMKeyToRenderTarget array and this function can be removed	
	void ReleaseSavedRenderTarget();

	void DrawTextureSampleToRenderTarget(UTexture2D* InTextureSample, UTextureRenderTarget2D* InRenderTarget);
};
