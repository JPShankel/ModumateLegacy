// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BIMKernel/BIMProperties.h"
#include "BIMKernel/BIMLegacyPortals.h"
#include "DocumentManagement/ModumatePresetManager.h"
#include "Database/ModumateObjectAssembly.h"
#include "UnrealClasses/DynamicMeshActor.h"
#include "ModumateCraftingWidget_CPP.generated.h"

/**
 *
 */

class FPresetManager;

UCLASS()
class MODUMATE_API UModumateCraftingWidget_CPP : public UUserWidget
{
	GENERATED_BODY()

		// the widget must not make mutating calls on the preset manager, those must be routed through the command system
		const FPresetManager &GetDocumentPresetManager() const;

	// wrapper for ObjectTypeSupportsDDL2 that also checks debug flag to force all objects to ddl2 for data testing
	bool ShouldUseDDL2ForObjectType(EObjectType ObjectType) const;

public:

	/*
	DDL 2.0
	*/
	Modumate::BIM::FCraftingTreeNodeInstancePool CraftingTreeNodeInstances;

	UFUNCTION(BlueprintCallable, Category = "Crafting")
	ECraftingResult CreateNewNodeInstanceFromPreset(EToolMode ToolMode, int32 ParentID, const FName &PresetID, FCraftingNode &OutNode);

	UFUNCTION(BlueprintCallable, Category = "Crafting")
	ECraftingResult GetInstantiatedNodes(EToolMode ToolMode, TArray<FCraftingNode> &OutNodes);

	UFUNCTION(BlueprintCallable, Category = "Crafting")
	ECraftingResult GetPinAttachmentToParentForNode(EToolMode ToolMode, int32 InstanceID, int32 &OutPinGroupIndex, int32 &OutPinIndex);

	UFUNCTION(BlueprintCallable, Category = "Crafting")
	ECraftingResult GetFormItemsForCraftingNode(EToolMode ToolMode, int32 InstanceID, TArray<FCraftingNodeFormItem> &OutForm);

	UFUNCTION(BlueprintCallable, Category = "Crafting")
	ECraftingResult GetFormItemsForPresetID(EToolMode ToolMode, const FName &PresetID, TArray<FCraftingNodeFormItem> &OutForm);

	UFUNCTION(BlueprintCallable, Category = "Crafting")
	ECraftingResult SetValueForFormItem(EToolMode ToolMode, const FCraftingNodeFormItem &FormItem, const FString &Value);

	// TODO: Support changing preset value
	UFUNCTION(BlueprintCallable, Category = "Crafting")
	ECraftingResult SetValueForPreset(EToolMode ToolMode, const FName &PresetID, const FString &Value);

	UFUNCTION(BlueprintCallable, Category = "Crafting")
	ECraftingResult RemoveNodeInstance(EToolMode ToolMode, int32 ParentID, int32 InstanceID);

	UFUNCTION(BlueprintCallable, Category = "Crafting")
	ECraftingResult GetPinGroupsForNode(EToolMode ToolMode, int32 NodeID, TArray<FCraftingNodePinGroup> &OutPins);

	UFUNCTION(BlueprintCallable, Category = "Crafting")
	ECraftingResult DragDropPinForNode(EToolMode ToolMode, int32 NodeID, const FName &PinGroup, int32 From, int32 To);

	UFUNCTION(BlueprintCallable, Category = "Crafting")
	ECraftingResult SetNewPresetForNode(EToolMode ToolMode, int32 InstanceID, const FName &PresetID);

	UFUNCTION(BlueprintCallable, Category = "Crafting")
	ECraftingResult GetPropertiesForPresetID(EToolMode ToolMode, FName PresetID, TMap<FString, FString> &OutPropertyMap);

	UFUNCTION(BlueprintCallable, Category = "Crafting")
	ECraftingResult GetPropertyTipsForPresetByIconType(EConfiguratorNodeIconType IconType, const FName &PresetID, TArray<FString> &OutTips);

	UFUNCTION(BlueprintCallable, Category = "Crafting")
	ECraftingResult IsCraftingAssemblyValid(EToolMode ToolMode, bool& OutIsValid);

	UFUNCTION(BlueprintCallable, Category = "Crafting")
	ECraftingResult BeginCraftingWithToolModeAndAssembly(EToolMode ToolMode, const FName& DatabaseKey, TArray<FCraftingNode> &OutNodes);

	UFUNCTION(BlueprintCallable, Category = "Crafting")
	ECraftingResult SaveOrCreateCraftingPreset(EToolMode ToolMode, int32 InstanceID, const FName &PresetKey, FName &OutKey);

	UFUNCTION(BlueprintCallable, Category = "Crafting")
	ECraftingResult CancelCraftingPresetEdit(EToolMode ToolMode, int32 InstanceID);

	UFUNCTION(BlueprintCallable, Category = "Crafting")
	ECraftingResult GetEligiblePresetsForSave(EToolMode ToolMode, int32 InstanceID, TArray<FCraftingNode> &OutPresets);

	UFUNCTION(BlueprintCallable, Category = "Crafting")
	ECraftingResult GetEligiblePresetsForSwap(EToolMode ToolMode, int32 InstanceID, TArray<FCraftingNode> &OutPresets);

	UFUNCTION(BlueprintCallable, Category = "Crafting")
	ECraftingResult UpdateNodePreviewAssemblyWithPreset(EToolMode ToolMode, int32 InstanceID, const FName &PresetKey);

	// Assuming current configuration is layered assembly, return the layer ID the current node is in
	UFUNCTION(BlueprintCallable, Category = "Crafting")
	ECraftingResult GetLayerIDFromNodeInstanceID(int32 InstanceID, int32 &LayerID, int32 &NumberOfLayers);

	ECraftingResult TryMakeNodeAssembly(EToolMode ToolMode, const Modumate::BIM::FCraftingTreeNodeInstancePool &InNodes, FModumateObjectAssembly &OutAssembly);
	ECraftingResult GetAvailableStarterNodes(EToolMode ToolMode, TArray<FCraftingNode> &OutNode) const;

	FModumateObjectAssembly CraftingAssembly, PreviewAssembly;
	void UpdateCraftingAssembly(EToolMode toolMode);

	UFUNCTION(BlueprintCallable, Category = "Preview Model")
	bool ApplyMaterialsToCraftingAssemblyMeshes(
		const TArray<UProceduralMeshComponent*> &ProceduralSubLayers,
		EToolMode toolMode,
		const TArray<UMaterialInterface*> &TilingMaterials,
		UMaterialInterface *MasterPBRMaterial,
		const FString &AssemblyKey);

	UFUNCTION(BlueprintCallable, Category = "Preview Model")
	TArray<float> GetCraftingAssemblyLayerThickness();
};
