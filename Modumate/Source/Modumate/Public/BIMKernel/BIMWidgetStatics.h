// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "BIMKernel/BIMNodeEditor.h"
#include "Database/ModumateObjectEnums.h"
#include "BIMWidgetStatics.generated.h"

namespace Modumate
{
	class MODUMATE_API ModumateObjectDatabase;
}

/*
TODO: Rename these blueprint structs to reflect intent that they are used for UI crafting
*/

USTRUCT(BlueprintType)
struct MODUMATE_API FCraftingNodeFormItem
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY(BlueprintReadWrite, Category = "Crafting")
	FText DisplayLabel;

	UPROPERTY(BlueprintReadWrite, Category = "Crafting")
	FString ItemValue;

	UPROPERTY(BlueprintReadWrite, Category = "Crafting")
	TArray<FString> ListItems;

	int32 NodeInstance;
	FString PropertyBindingQN;
};


USTRUCT(BlueprintType)
struct MODUMATE_API FCraftingNode
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY(BlueprintReadWrite, Category = "Crafting")
	FText Label;

	UPROPERTY(BlueprintReadWrite, Category = "Crafting")
	FText ParentPinLabel;

	UPROPERTY(BlueprintReadWrite, Category = "Crafting")
	int32 InstanceID;

	UPROPERTY(BlueprintReadWrite, Category = "Crafting")
	int32 ParentInstanceID;

	UPROPERTY(BlueprintReadWrite, Category = "Crafting")
	FName PresetID;

	UPROPERTY(BlueprintReadWrite, Category = "Crafting")
	ECraftingNodePresetStatus PresetStatus;

	UPROPERTY(BlueprintReadWrite, Category = "Crafting")
	bool CanSwapPreset;

	UPROPERTY(BlueprintReadWrite, Category = "Crafting")
	bool CanAddToProject;

	UPROPERTY(BlueprintReadWrite, Category = "Crafting")
	bool IsEmbeddedInParent = false;

	UPROPERTY(BlueprintReadWrite, Category = "Crafting")
	EConfiguratorNodeIconType NodeIconType;

	UPROPERTY(BlueprintReadWrite, Category = "Crafting")
	EConfiguratorNodeIconOrientation NodeIconOrientation;

	UPROPERTY(BlueprintReadWrite, Category = "Crafting")
	bool CanSwitchOrientation;

	UPROPERTY(BlueprintReadWrite, Category = "Crafting")
	TArray<int32> EmbeddedInstanceIDs;

	UPROPERTY(BlueprintReadWrite, Category = "Crafting")
	int32 ChildOrder;

	static FCraftingNode FromPreset(const Modumate::BIM::FCraftingTreeNodePreset &Preset);
};

USTRUCT(BlueprintType)
struct MODUMATE_API FCraftingNodePinGroup
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY(BlueprintReadWrite, Category = "Crafting")
	int32 MinPins;

	UPROPERTY(BlueprintReadWrite, Category = "Crafting")
	int32 MaxPins;

	UPROPERTY(BlueprintReadWrite, Category = "Crafting")
	TArray<int32> AttachedChildren;

	UPROPERTY(BlueprintReadWrite, Category = "Crafting")
	TArray<FName> SupportedTypePins;

	UPROPERTY(BlueprintReadWrite, Category = "Crafting")
	FName GroupName;

	UPROPERTY(BlueprintReadWrite, Category = "Crafting")
	FName NodeType;

	UPROPERTY(BlueprintReadWrite, Category = "Crafting")
	TArray<FString> Tags;

	UPROPERTY(BlueprintReadWrite, Category = "Crafting")
	int32 OwnerInstance;
};

/*
Static support functions for common node widget UI operations
Custom behaviors are in the widget classes themsleves (ModumateCraftingWidget, ModumateDrawingSetWidget)
*/
class MODUMATE_API UModumateCraftingNodeWidgetStatics : public UBlueprintFunctionLibrary
{
private:
	static ECraftingResult CraftingNodeFromInstance(const Modumate::BIM::FCraftingTreeNodeInstancePool &NodeInstances, int32 InstanceID, const Modumate::BIM::FCraftingPresetCollection &PresetCollection, FCraftingNode &OutNode);

public:

	static ECraftingResult CreateNewNodeInstanceFromPreset(Modumate::BIM::FCraftingTreeNodeInstancePool &NodeInstances, const Modumate::BIM::FCraftingPresetCollection &PresetCollection, int32 ParentID, int32 ParentPinIndex,int32 ParentPinPosition,const FName &PresetID, FCraftingNode &OutNode);
	static ECraftingResult GetInstantiatedNodes(Modumate::BIM::FCraftingTreeNodeInstancePool &NodeInstances, const Modumate::BIM::FCraftingPresetCollection &PresetCollection, TArray<FCraftingNode> &OutNodes);
	static ECraftingResult GetPinAttachmentToParentForNode(Modumate::BIM::FCraftingTreeNodeInstancePool &NodeInstances, int32 InstanceID, int32 &OutPinGroupIndex, int32 &OutPinIndex);
	static ECraftingResult GetFormItemsForCraftingNode(Modumate::BIM::FCraftingTreeNodeInstancePool &NodeInstances, const Modumate::BIM::FCraftingPresetCollection &PresetCollection, int32 InstanceID, TArray<FCraftingNodeFormItem> &OutForm);
	static ECraftingResult GetFormItemsForPreset(const Modumate::BIM::FCraftingPresetCollection &PresetCollection, const FName &PresetID, TArray<FCraftingNodeFormItem> &OutForm);
	static ECraftingResult SetValueForFormItem(Modumate::BIM::FCraftingTreeNodeInstancePool &NodeInstances, const FCraftingNodeFormItem &FormItem, const FString &Value);
	static ECraftingResult SetValueForPreset(const FName &PresetID, const FString &Value);
	static ECraftingResult RemoveNodeInstance(Modumate::BIM::FCraftingTreeNodeInstancePool &NodeInstances, int32 ParentID, int32 InstanceID);
	static ECraftingResult GetPinGroupsForNode(Modumate::BIM::FCraftingTreeNodeInstancePool &NodeInstances, const Modumate::BIM::FCraftingPresetCollection &PresetCollection, int32 NodeID, TArray<FCraftingNodePinGroup> &OutPins);
	static ECraftingResult DragMovePinChild(Modumate::BIM::FCraftingTreeNodeInstancePool &NodeInstances, int32 InstanceID, const FName &PinGroup, int32 From, int32 To);
	static ECraftingResult GetLayerIDFromNodeInstanceID(const Modumate::BIM::FCraftingTreeNodeInstancePool &NodeInstances, const Modumate::BIM::FCraftingPresetCollection &PresetCollection, int32 InstanceID, int32 &OutLayerID, int32 &NumberOfLayers);
	static ECraftingResult GetPropertyTipsByIconType(const Modumate::ModumateObjectDatabase &InDB, EConfiguratorNodeIconType IconType, const Modumate::BIM::FModumateAssemblyPropertySpec &PresetProperties, TArray<FString> &OutTips);
	static ECraftingResult GetEligiblePresetsForSwap(Modumate::BIM::FCraftingTreeNodeInstancePool &NodeInstances, const Modumate::BIM::FCraftingPresetCollection &PresetCollection, int32 InstanceID, TArray<FCraftingNode> &OutPresets);
};