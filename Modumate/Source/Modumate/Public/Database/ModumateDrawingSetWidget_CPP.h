// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BIMKernel/BIMNodeEditor.h"
#include "BIMKernel/BIMWidgetStatics.h"
#include "DocumentManagement/ModumatePresetManager.h"
#include "ModumateDrawingSetWidget_CPP.generated.h"

/**
 *
 */

UCLASS()
class MODUMATE_API UModumateDrawingSetWidget_CPP : public UUserWidget
{
	GENERATED_BODY()

	const FPresetManager &GetDocumentPresetManager() const;
	Modumate::BIM::FCraftingTreeNodeInstancePool DraftingNodeInstances;

public:

	UFUNCTION(BlueprintCallable, Category = "DrawingSet")
	ECraftingResult CreateNewNodeInstanceFromPreset(int32 ParentID, const FName &PresetID, FCraftingNode &OutNode);

	UFUNCTION(BlueprintCallable, Category = "DrawingSet")
	ECraftingResult GetInstantiatedNodes(TArray<FCraftingNode> &OutNodes);

	UFUNCTION(BlueprintCallable, Category = "DrawingSet")
	ECraftingResult GetPinAttachmentToParentForNode(int32 InstanceID, int32 &OutPinGroupIndex, int32 &OutPinIndex);

	UFUNCTION(BlueprintCallable, Category = "DrawingSet")
	ECraftingResult GetFormItemsForDrawingSetNode(int32 InstanceID, TArray<FCraftingNodeFormItem> &OutForm);

	UFUNCTION(BlueprintCallable, Category = "DrawingSet")
	ECraftingResult SetValueForFormItem(const FCraftingNodeFormItem &FormItem, const FString &Value);

	UFUNCTION(BlueprintCallable, Category = "DrawingSet")
	ECraftingResult RemoveNodeInstance(int32 ParentID, int32 InstanceID);

	UFUNCTION(BlueprintCallable, Category = "DrawingSet")
	ECraftingResult GetPinGroupsForNode(int32 NodeID, TArray<FCraftingNodePinGroup> &OutPins);

	UFUNCTION(BlueprintCallable, Category = "DrawingSet")
	ECraftingResult SaveOrCreateDrawingSetPreset(int32 InstanceID, const FName &PresetKey, FName &OutKey);

};
