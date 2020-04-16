// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DynamicMeshActor.h"
#include "ModumateIconMeshStatics.generated.h"


/**
*
*/

class UModumateCraftingWidget_CPP;

UCLASS()
class MODUMATE_API UModumateIconMeshStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, Category = "Modumate IconMesh")
		static bool GetWallIconParamsFromShopItem(
			AActor* IconGenerator,
			UModumateCraftingWidget_CPP* CraftingWidget,
			const FName &AssemblyKey,
			FVector RefCP1,
			FVector RefCP2,
			TArray<FProceduralMeshParams>& ProceduralMeshParam,
			TArray<UMaterialInterface*>& Materials,
			float& WallThickness,
			float& LayerHeight,
			FVector& CP1, FVector& CP2,
			EToolMode mode,
			bool ManualLayerCPs = false,
			float UVScale = 1.f,
			bool bMarketplaceAsm = false);

	UFUNCTION(BlueprintCallable, Category = "Modumate IconMesh")
		static bool GetFloorIconParamsFromShopItem(
			AActor* IconGenerator,
			EToolMode FromToolMode,
			UModumateCraftingWidget_CPP* CraftingWidget,
			const FName &AssemblyKey,
			FVector Offset,
			float FloorStartHeight,
			TArray<FProceduralMeshParams>& ProceduralMeshParam,
			TArray<UMaterialInterface*>& Materials,
			float& FloorThickness,
			float& LayerDepth,
			FVector& CP1, FVector& CP2, FVector& CP3, FVector& CP4,
			bool ManualLayerCPs = false,
			bool bMarketplaceAsm = false);

	UFUNCTION(BlueprintCallable, Category = "Modumate IconMesh")
		static bool MakeCompoundMeshFromShoppingItem(
			AActor* IconGenerator,
			const FName &AssemblyKey,
			EToolMode FromToolMode,
			float& TotalWidth,
			UModumateCraftingWidget_CPP* CraftingWidget,
			bool bMarketplaceAsm = false);

	UFUNCTION(BlueprintCallable, Category = "Modumate IconMesh")
		static bool MakeCabinetFromShoppingItem(
			AActor* IconGenerator,
			const FName &AsmKey,
			TArray<FVector>& CabinetPointRefs,
			UModumateCraftingWidget_CPP* CraftingWidget,
			bool bMarketplaceAsm = false);

	UFUNCTION(BlueprintCallable, Category = "Modumate IconMesh")
		static bool MakeExtrudedPolyFromShoppingItem(
			AActor* iconGenerator,
			EToolMode fromToolMode,
			const FName &asmKey,
			const FVector &rootLoation,
			UModumateCraftingWidget_CPP* CraftingWidget,
			float length = 25.f,
			bool bMarketplaceAsm = false);

	UFUNCTION(BlueprintCallable, Category = "Modumate IconMesh")
		static bool GetModuleGapIconParamsFromCrafting(
			AActor* IconGenerator,
			UModumateCraftingWidget_CPP* CraftingWidget,
			EToolMode FromToolMode,
			const FString &AssemblyKey,
			FVector &ModuleExtent,
			FVector &GapExtent,
			UMaterialInterface* &ModuleMaterial,
			FCustomColor &ModuleColor,
			FCustomColor &GapColor,
			bool bMarketplaceAsm = false);

	UFUNCTION(BlueprintCallable, Category = "Modumate IconMesh")
		static bool GetMeshesFromShoppingItem(AEditModelPlayerController_CPP *Controller, const FName &AsmKey, EToolMode FromToolMode, TArray<UStaticMesh*>& TargetComps, bool bMarketplaceAsm = false);

	UFUNCTION(BlueprintCallable, Category = "Modumate IconMesh")
		static bool GetEngineMaterialByKey(AEditModelPlayerController_CPP *Controller, const FName &Key, UMaterialInterface* &ModuleMaterial);
	
	UFUNCTION(BlueprintCallable, Category = "Modumate IconMesh")
		static bool GetEngineCustomColorByKey(AEditModelPlayerController_CPP *Controller, const FName &Key, FCustomColor &ModuleColor);

	UFUNCTION(BlueprintCallable, Category = "Modumate IconMesh")
		static bool MakeIconMeshFromPofileKey(
			AEditModelPlayerController_CPP *Controller,
			ADynamicMeshActor *DynamicMeshActor,
			EToolMode FromToolMode, 
			const FName &ProfileKey,
			const FVector &RootLoation,
			float Length = 25.f);

	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject"), Category = "Modumate IconMesh")
		static bool GetModuleIconParamsFromPreset(
			UObject* WorldContextObject,
			UModumateCraftingWidget_CPP* CraftingWidget,
			EToolMode FromToolMode,
			const FName &PresetKey,
			FVector &ModuleExtent,
			UMaterialInterface* &ModuleMaterial,
			FCustomColor &ModuleColor);
};
