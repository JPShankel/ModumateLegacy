// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealClasses/ModumateGameModeBase.h"
#include "EditModelGameMode_CPP.generated.h"

class MODUMATE_API FModumateDatabase;

class UDataTable;

UCLASS()
class MODUMATE_API AEditModelGameMode_CPP : public AModumateGameModeBase
{
	GENERATED_BODY()

public:

	AEditModelGameMode_CPP();

	//~ Begin AActor Interface
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End AActor Interface

	//~ Begin AGameModeBase Interface
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void InitGameState() override;
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
	UStaticMesh *LineMesh;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Core Content")
	UMaterialInterface *VertexMaterial;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Core Content")
	UStaticMesh *MetaPlaneVertexIconMesh;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Core Content")
	UMaterialInterface *BillboardTextureMaterial;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Core Content")
	UTexture2D *ButtonEditGreenTexture;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Core Content")
	UTexture2D *ButtonEditRedTexture;

	FModumateDatabase *ObjectDatabase;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Shopping")
	UDataTable* RoomConfigurationTable;

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
