// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "ModumateDrawingSetWidget_CPP.h"
#include "EditModelGameState_CPP.h"

using namespace Modumate;

const Modumate::FPresetManager &UModumateDrawingSetWidget_CPP::GetDocumentPresetManager() const
{
	const AEditModelGameState_CPP *gameState = GetWorld()->GetGameState<AEditModelGameState_CPP>();
	return gameState->Document.PresetManager;
}

ECraftingResult UModumateDrawingSetWidget_CPP::CreateNewNodeInstanceFromPreset(int32 ParentID, const FName &PresetID, FCraftingNode &OutNode)
{
	return UModumateCraftingNodeWidgetStatics::CreateNewNodeInstanceFromPreset(DraftingNodeInstances, GetDocumentPresetManager().DraftingNodePresets,ParentID,PresetID,OutNode);
}

ECraftingResult UModumateDrawingSetWidget_CPP::GetInstantiatedNodes(TArray<FCraftingNode> &OutNodes)
{
	if (DraftingNodeInstances.GetInstancePool().Num() == 0)
	{
		const BIM::FCraftingPresetCollection &presetCollection = GetDocumentPresetManager().DraftingNodePresets;
		const FName rootNode = TEXT("DRAWINGSET_ROOT");
		DraftingNodeInstances.CreateNodeInstanceFromPreset(presetCollection, 0, rootNode, true);
	}

	return UModumateCraftingNodeWidgetStatics::GetInstantiatedNodes(DraftingNodeInstances, GetDocumentPresetManager().DraftingNodePresets, OutNodes);
}

ECraftingResult UModumateDrawingSetWidget_CPP::GetPinAttachmentToParentForNode(int32 InstanceID, int32 &OutPinGroupIndex, int32 &OutPinIndex)
{
	return UModumateCraftingNodeWidgetStatics::GetPinAttachmentToParentForNode(DraftingNodeInstances,InstanceID,OutPinGroupIndex,OutPinIndex);
}

ECraftingResult UModumateDrawingSetWidget_CPP::GetFormItemsForDrawingSetNode(int32 InstanceID, TArray<FCraftingNodeFormItem> &OutForm)
{
	return UModumateCraftingNodeWidgetStatics::GetFormItemsForCraftingNode(DraftingNodeInstances, GetDocumentPresetManager().DraftingNodePresets, InstanceID, OutForm);
}

ECraftingResult UModumateDrawingSetWidget_CPP::SetValueForFormItem(const FCraftingNodeFormItem &FormItem, const FString &Value)
{
	return UModumateCraftingNodeWidgetStatics::SetValueForFormItem(DraftingNodeInstances, FormItem, Value);
}

ECraftingResult UModumateDrawingSetWidget_CPP::RemoveNodeInstance(int32 ParentID, int32 InstanceID)
{
	return UModumateCraftingNodeWidgetStatics::RemoveNodeInstance(DraftingNodeInstances,ParentID,InstanceID);
}

ECraftingResult UModumateDrawingSetWidget_CPP::GetPinGroupsForNode(int32 NodeID, TArray<FCraftingNodePinGroup> &OutPins)
{
	return UModumateCraftingNodeWidgetStatics::GetPinGroupsForNode(DraftingNodeInstances, GetDocumentPresetManager().DraftingNodePresets,NodeID,OutPins);
}

ECraftingResult UModumateDrawingSetWidget_CPP::SaveOrCreateDrawingSetPreset(int32 InstanceID, const FName &PresetKey, FName &OutKey)
{
	return ECraftingResult::Error;
}
