// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ModumateBIMSchema.h"
#include "ModumateCrafting.h"
#include "ModumatePresetManager.h"
#include "ModumateObjectAssembly.h"
#include "DynamicMeshActor.h"
#include "ModumateCraftingWidget_CPP.generated.h"

/**
 *
 */

namespace Modumate
{
	class MODUMATE_API FPresetManager;
}

UCLASS()
class MODUMATE_API UModumateCraftingWidget_CPP : public UUserWidget
{
	GENERATED_BODY()

	// the widget must not make mutating calls on the preset manager, those must be routed through the command system
	const Modumate::FPresetManager &GetDocumentPresetManager() const;

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
	ECraftingResult SetValueForFormItem(EToolMode ToolMode, const FCraftingNodeFormItem &FormItem,const FString &Value);

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


	/*
	DDL 1.0, to be deprecated
	*/
	Modumate::FCraftingDecisionTree CurrentCraftingTree;

	void UpdateCraftingAssembly(EToolMode toolMode);
	void UpdateAssemblyWithCraftingDecision(EToolMode mode, Modumate::FCraftingDecisionTree &craftingTree, const FName &nodeGUID, const FName &decisionGUID, FModumateObjectAssembly &outAssembly, bool usePresets);
	void UpdateAssemblyWithPreset(EToolMode mode, Modumate::FCraftingDecisionTree &craftingTree, const FName &formGUID, const FName &presetKey, FModumateObjectAssembly &outAssembly, bool usePresets, const int32 showOnlyLayerID = -1);

	void InitDecisionTrees();
	


	UPROPERTY(BlueprintReadOnly, Category = "Crafting")
	TArray<FCraftingItem> CraftingDecisions;


	UFUNCTION(BlueprintCallable, Category = "Crafting")
	TArray<FCraftingItem> GetElementsOfDynamicList(EToolMode mode, const FCraftingItem &dynamicList);

	UFUNCTION(BlueprintCallable, Category = "Crafting")
	TArray<FCraftingItem> GetDecisionsForDynamicListElement(EToolMode mode, const FName &GUID, int32 i);

	UFUNCTION(BlueprintCallable, Category = "Crafting")
	void UpdateCraftingDecisions(EToolMode mode);

	UFUNCTION(BlueprintCallable, Category = "Crafting")
	void SetValueForCraftingDecision(EToolMode mode, const FName &nodeGUID, const FName &decisionGUID);

	UFUNCTION(BlueprintCallable, Category = "Crafting")
	void UpdatePreviewAssemblyWithCraftingDecision(EToolMode mode, const FName &nodeGUID, const FName &decisionGUID);

	UFUNCTION(BlueprintCallable, Category = "Crafting")
	bool CommitCraftingAssembly(EToolMode toolMode);

	UFUNCTION(BlueprintCallable, Category = "Crafting")
	bool OverwriteExistingAssembly(EToolMode toolMode, const FName& databaseKey);

	// Get all params and their values inside this assembly. Specify layer id for spec.LayerProperties. Return false if assembly not found.
	UFUNCTION(BlueprintCallable, Category = "Crafting")
	bool GetAllPropertiesFromAssembly(EToolMode mode, const FName& databaseKey, TArray<FString> &properties, TArray<FString> &values, int32 specificLayer = -1);

	// Add/Remove/Reorder dynamic list elements
	UFUNCTION(BlueprintCallable, Category = "Crafting")
	FCraftingItem AddElementToDynamicListAt(EToolMode mode, const FCraftingItem &dynamicList, int32 i);

	UFUNCTION(BlueprintCallable, Category = "Crafting")
	TArray<FCraftingItem> RemoveElementFromDynamicListAt(EToolMode mode, const FCraftingItem &dynamicList, int32 i);

	UFUNCTION(BlueprintCallable, Category = "Crafting")
	TArray<FCraftingItem> MoveElementInDynamicList(EToolMode mode, const FCraftingItem &dynamicList, int32 from, int32 to);

	UFUNCTION(BlueprintCallable, Category = "Preview Model")
	bool ApplyMaterialsToCraftingAssemblyMeshes(
		const TArray<UProceduralMeshComponent*> &ProceduralSubLayers,
		EToolMode toolMode,
		const TArray<UMaterialInterface*> &TilingMaterials,
		UMaterialInterface *MasterPBRMaterial,
		const FString &AssemblyKey);

	UFUNCTION(BlueprintCallable, Category = "Preview Model")
	TArray<float> GetCraftingAssemblyLayerThickness();

	UFUNCTION(BlueprintCallable, Category = "Crafting")
	bool GetPresetsForForm(EToolMode mode, const FName &formGUID, TArray<FCraftingItem> &outPresets);

	UFUNCTION(BlueprintCallable, Category = "Crafting")
	bool GetBuiltinsForForm(EToolMode mode, const FName &formGUID, TArray<FCraftingItem> &outPresets);

	UFUNCTION(BlueprintCallable, Category = "Crafting")
	void OnPresetSelected(EToolMode mode, const FName &formGUID, const FName &presetKey);

	// Return true if a assemblies are affected
	UFUNCTION(BlueprintCallable, Category = "Crafting")
	bool CheckSaveFormPreset(EToolMode mode, const FName &formGUID, TArray<FName> &affectedAssemblies);

	UFUNCTION(BlueprintCallable, Category = "Crafting")
	void DoSaveFormPreset(EToolMode mode, const FName &formGUID);

	UFUNCTION(BlueprintCallable, Category = "Crafting")
	void CancelFormPresetEdit(EToolMode mode, const FName &formGUID);

	UFUNCTION(BlueprintCallable, Category = "Crafting")
	void UpdatePreviewAssemblyWithPreset(EToolMode mode, const FName &formGUID, const FName &presetKey, const int32 showOnlyLayerID = -1);

	UFUNCTION(BlueprintCallable, Category = "Crafting")
	bool CheckRemoveFormPreset(EToolMode mode, const FName &formGUID, const FName &presetKey, TArray<FName> &affectedAssemblies);

	UFUNCTION(BlueprintCallable, Category = "Crafting")
	bool DoReplaceFormPreset(EToolMode mode, const FName &formGUID, const FName &presetKey, const TArray<FName> &replaceAssemblies, const FName &replacementKey);

	UFUNCTION(BlueprintCallable, Category = "Crafting")
	void DoRemoveFormPreset(EToolMode mode, const FName &formGUID, const FName &presetKey);

	UFUNCTION(BlueprintCallable, Category = "Crafting")
	void AddPreset(EToolMode mode, const FName &formGUID);

	UFUNCTION(BlueprintCallable, Category = "Crafting")
	void OverwritePreset(EToolMode mode, const FName &formGUID, const FName &presetKey);

	UFUNCTION(BlueprintCallable, Category = "Crafting")
	FName GetSelectedPresetForForm(EToolMode mode, const FName &formGUID);
};
