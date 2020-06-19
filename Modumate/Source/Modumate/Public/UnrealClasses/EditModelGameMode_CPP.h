// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealClasses/ModumateGameModeBase.h"
#include "Database/ModumateShoppingItem.h"
#include "EditModelGameMode_CPP.generated.h"

namespace Modumate {
	class ModumateObjectDatabase;
}

class UDataTable;

UCLASS()
class MODUMATE_API AEditModelGameMode_CPP : public AModumateGameModeBase
{
	GENERATED_BODY()

public:

	AEditModelGameMode_CPP();
	~AEditModelGameMode_CPP();

	//~ Begin AGameModeBase Interface
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	//~ End AGameModeBase Interface

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Core Content")
	UClass *PortalFrameActorClass;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Core Content")
	UClass *MOIGroupActorClass;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Core Content")
	UMaterialInterface *GreenMaterial;
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Core Content")
	UMaterialInterface *HideMaterial;
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Core Content")
	UStaticMesh *AnchorMesh;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Core Content")
	UMaterialInterface *VolumeHoverMaterial;
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Core Content")
	UMaterialInterface *RoomHandleMaterial;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Core Content")
	UMaterialInterface *RoomHandleMaterialNoDepth;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Core Content")
	UStaticMesh *SphereMesh;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Core Content")
	UStaticMesh *PlaneMesh;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Core Content")
	UMaterialInterface *DefaultObjectMaterial;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Core Content")
	UMaterialInterface *MetaPlaneMaterial;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Core Content")
	UMaterialInterface *CutPlaneMaterial;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Core Content")
	UMaterialInterface *ScopeBoxMaterial;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Core Content")
	UMaterialInterface *LineMaterial;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Core Content")
	UStaticMesh *FlipHandleMesh;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Core Content")
	UStaticMesh *JustificationHandleMesh;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Core Content")
	UStaticMesh *JustificationHandleChildMesh;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Core Content")
	UStaticMesh *FaceAdjusterMesh;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Core Content")
	UStaticMesh *PointAdjusterMesh;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Core Content")
	UStaticMesh *MetaPlaneVertexIconMesh;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Core Content")
	UStaticMesh *RotateHandleMesh;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Core Content")
	UStaticMesh *InvertHandleMesh;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Core Content")
	UMaterialInterface *BillboardTextureMaterial;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Core Content")
	UTexture2D *ButtonEditGreenTexture;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Core Content")
	UTexture2D *ButtonEditRedTexture;


	Modumate::ModumateObjectDatabase *ObjectDatabase;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Shopping")
	UObjectLibrary* BIMTableLibrary;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Shopping")
	UDataTable* RoomConfigurationTable;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Shopping")
	UDataTable* PortalPartsTable;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Shopping")
	UDataTable* FFEPartTable;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Shopping")
	UDataTable* MeshTable;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Shopping")
	UDataTable* PortalConfigurationTable;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Shopping")
	UDataTable* FFEAssemblyTable;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Shopping")
	UDataTable* ColorTable;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Shopping")
	UDataTable* MaterialTable;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Shopping")
	UDataTable* LightConfigTable;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Crafting")
	UDataTable* CraftingSubcategoryTable;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Crafting")
	UDataTable* DecisionTable;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Crafting")
	UDataTable* LayerThicknessOptionSetTable;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Crafting")
	UDataTable* DimensionalOptionSetDataTable;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Crafting")
	UDataTable* MaterialsAndColorsOptionSetDataTable;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Crafting")
	UDataTable* PatternOptionSetDataTable;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Crafting")
	UDataTable* PortalPartOptionSetDataTable;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Crafting")
	UDataTable* ProfileMeshDataTable;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Editing")
	TSubclassOf<class ADynamicMeshActor> DynamicMeshActorClass;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Editing")
	TSubclassOf<class ACutPlaneCaptureActor> CutPlaneCaptureActorClass;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Loading")
	FString PendingProjectPath;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Actors")
	AActor *Axes;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Pawn")
	TSubclassOf<class AEditModelToggleGravityPawn_CPP> ToggleGravityPawnClass;
};
