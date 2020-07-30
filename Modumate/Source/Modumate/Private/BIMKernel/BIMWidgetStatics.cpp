// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "BIMKernel/BIMWidgetStatics.h"
#include "Database/ModumateArchitecturalMaterial.h"
#include "Database/ModumateObjectDatabase.h"
#include "Algo/Compare.h"
#include "Algo/Accumulate.h"
#include "Algo/Transform.h"
using namespace Modumate;

ECraftingResult UModumateCraftingNodeWidgetStatics::CreateNewNodeInstanceFromPreset(
	Modumate::BIM::FCraftingTreeNodeInstancePool &NodeInstances,
	const Modumate::BIM::FCraftingPresetCollection &PresetCollection,
	int32 ParentID,
	int32 ParentPinIndex,
	int32 ParentPinPosition,
	const FName &PresetID,
	FCraftingNode &OutNode)
{
	BIM::FCraftingTreeNodeInstanceSharedPtr inst = NodeInstances.CreateNodeInstanceFromPreset(PresetCollection, ParentID, PresetID, ParentPinIndex, ParentPinPosition);

	if (ensureAlways(inst.IsValid()))
	{
		return CraftingNodeFromInstance(NodeInstances, inst->GetInstanceID(), PresetCollection, OutNode);
	}
	return ECraftingResult::Error;
}

ECraftingResult UModumateCraftingNodeWidgetStatics::CraftingNodeFromInstance(
	const Modumate::BIM::FCraftingTreeNodeInstancePool &NodeInstances,
	int32 InstanceID,
	const Modumate::BIM::FCraftingPresetCollection &PresetCollection,
	FCraftingNode &OutNode)
{
	const BIM::FCraftingTreeNodeInstanceSharedPtr instance = NodeInstances.InstanceFromID(InstanceID);
	if (!instance.IsValid())
	{
		return ECraftingResult::Error;
	}
	OutNode.CanSwapPreset = instance->ParentInstance.IsValid();
	OutNode.InstanceID = instance->GetInstanceID();
	OutNode.ParentInstanceID = instance->ParentInstance.IsValid() ? instance->ParentInstance.Pin()->GetInstanceID() : 0;
	OutNode.PresetID = instance->PresetID;
	OutNode.CanAddToProject = false;
	OutNode.PresetStatus = instance->GetPresetStatus(PresetCollection);
	OutNode.ParentPinLabel = FText::FromString(OutNode.Label.ToString() + TEXT("ON A PIN!"));

	if (instance->AttachedChildren.Num() == 0)
	{
		OutNode.IsEmbeddedInParent = true;
	}

	const BIM::FCraftingTreeNodePreset *preset = PresetCollection.Presets.Find(instance->PresetID);
	if (ensureAlways(preset != nullptr))
	{
		const Modumate::BIM::FCraftingTreeNodeType *nodeType = PresetCollection.NodeDescriptors.Find(preset->NodeType);
		if (ensureAlways(nodeType != nullptr))
		{
			OutNode.NodeIconType = preset->IconType;
			OutNode.CanSwitchOrientation = preset->CanFlipOrientation;
		}

		OutNode.Label = FText::FromString(preset->GetDisplayName());
		BIM::FCraftingTreeNodeInstanceSharedPtr instIter = instance;
		if (preset != nullptr)
		{
			OutNode.ParentPinLabel = FText::FromString(preset->GetDisplayName());
		}
		else
		{
			OutNode.ParentPinLabel = OutNode.Label;
		}
	}

	OutNode.NodeIconOrientation = instance->CurrentOrientation;

	if (instance->ParentInstance.IsValid())
	{
		Modumate::BIM::FCraftingTreeNodeInstanceSharedPtr parent = instance->ParentInstance.Pin();
		parent->FindChildOrder(instance->GetInstanceID(), OutNode.ChildOrder);
	}
	else
	{
		OutNode.ChildOrder = 0;
	}

	return ECraftingResult::Success;
}

ECraftingResult UModumateCraftingNodeWidgetStatics::GetInstantiatedNodes(
	Modumate::BIM::FCraftingTreeNodeInstancePool &NodeInstances,
	const Modumate::BIM::FCraftingPresetCollection &PresetCollection,
	TArray<FCraftingNode> &OutNodes)
{
	/*
	The widget depends on nodes being in depth-first pin/child order,
	which they naturally are when first established, but when pins are
	reordered the contract breaks, so we explicitly sort here
	*/
	TArray<Modumate::BIM::FCraftingTreeNodeInstanceSharedPtr> nodeStack;
	for (auto nodeInstance : NodeInstances.GetInstancePool())
	{
		if (!nodeInstance->ParentInstance.IsValid())
		{
			nodeStack.Push(nodeInstance);
			break;
		}
	}

	if (!ensureAlways(nodeStack.Num() == 1))
	{
		return ECraftingResult::Error;
	}

	TArray<Modumate::BIM::FCraftingTreeNodeInstanceSharedPtr> nodeArray;
	while (nodeStack.Num() > 0)
	{
		Modumate::BIM::FCraftingTreeNodeInstanceSharedPtr nodeInstance = nodeStack.Pop();
		nodeArray.Push(nodeInstance);
		for (auto &ip : nodeInstance->AttachedChildren)
		{
			for (int32 i = 0; i < ip.Children.Num(); ++i)
			{
				nodeStack.Push(ip.Children[ip.Children.Num() - 1 - i].Pin());
			}
		}
	}

	if (ensureAlways(nodeArray.Num() > 0))
	{
		Algo::Transform(nodeArray, OutNodes, [&PresetCollection, &NodeInstances](const Modumate::BIM::FCraftingTreeNodeInstanceSharedPtr &Node)
		{
			FCraftingNode outNode;
			CraftingNodeFromInstance(NodeInstances, Node->GetInstanceID(), PresetCollection, outNode);
			return outNode;
		});
		return ECraftingResult::Success;
	}
	return ECraftingResult::Error;
}

ECraftingResult UModumateCraftingNodeWidgetStatics::GetPinAttachmentToParentForNode(
	Modumate::BIM::FCraftingTreeNodeInstancePool &NodeInstances,
	int32 InstanceID,
	int32 &OutPinGroupIndex,
	int32 &OutPinIndex)
{
	BIM::FCraftingTreeNodeInstanceSharedPtr inst = NodeInstances.InstanceFromID(InstanceID);

	if (!inst.IsValid() || !inst->ParentInstance.IsValid())
	{
		return ECraftingResult::Error;
	}

	for (int32 p = 0; p < inst->ParentInstance.Pin()->AttachedChildren.Num(); ++p)
	{
		auto &pg = inst->ParentInstance.Pin()->AttachedChildren[p];
		for (int32 i = 0; i < pg.Children.Num(); ++i)
		{
			if (pg.Children[i].HasSameObject(inst.Get()))
			{
				OutPinGroupIndex = p;
				OutPinIndex = i;
				return ECraftingResult::Success;
			}
		}
	}
	return ECraftingResult::Error;
}

ECraftingResult UModumateCraftingNodeWidgetStatics::GetFormItemsForCraftingNode(
	Modumate::BIM::FCraftingTreeNodeInstancePool &NodeInstances,
	const Modumate::BIM::FCraftingPresetCollection &PresetCollection,
	int32 InstanceID, TArray<FCraftingNodeFormItem> &OutForm)
{
	BIM::FCraftingTreeNodeInstanceSharedPtr node = NodeInstances.InstanceFromID(InstanceID);
	if (!node.IsValid())
	{
		return ECraftingResult::Error;
	}

	const BIM::FCraftingTreeNodePreset *preset = PresetCollection.Presets.Find(node->PresetID);
	const BIM::FCraftingTreeNodeType *descriptor = preset != nullptr ? PresetCollection.NodeDescriptors.Find(preset->NodeType) : nullptr;

	if (!ensureAlways(descriptor != nullptr))
	{
		return ECraftingResult::Error;
	}

	for (auto &kvp : descriptor->FormItemToProperty)
	{
		BIM::FValueSpec vs(*kvp.Value.ToString());
		FString outVal;
		if (ensureAlways(node->InstanceProperties.TryGetProperty(vs.Scope, vs.Name, outVal)))
		{
			FCraftingNodeFormItem &temp = OutForm.AddDefaulted_GetRef();
			temp.ItemValue = outVal;
			temp.DisplayLabel = FText::FromString(kvp.Key);
			temp.NodeInstance = InstanceID;
			temp.PropertyBindingQN = vs.QN().ToString();
		}
	}
	return ECraftingResult::Success;
}

ECraftingResult UModumateCraftingNodeWidgetStatics::GetFormItemsForPreset(const Modumate::BIM::FCraftingPresetCollection &PresetCollection, const FName &PresetID, TArray<FCraftingNodeFormItem> &OutForm)
{
	const BIM::FCraftingTreeNodePreset *preset = PresetCollection.Presets.Find(PresetID);
	const BIM::FCraftingTreeNodeType *descriptor = preset != nullptr ? PresetCollection.NodeDescriptors.Find(preset->NodeType) : nullptr;

	if (!ensureAlways(descriptor != nullptr))
	{
		return ECraftingResult::Error;
	}

	for (auto &kvp : descriptor->FormItemToProperty)
	{
		BIM::FValueSpec vs(*kvp.Value.ToString());
		FString outVal;
		if (ensureAlways(preset->Properties.TryGetProperty(vs.Scope, vs.Name, outVal)))
		{
			FCraftingNodeFormItem &temp = OutForm.AddDefaulted_GetRef();
			temp.ItemValue = outVal;
			temp.DisplayLabel = FText::FromString(kvp.Key);
			temp.PropertyBindingQN = vs.QN().ToString();
		}
	}
	return ECraftingResult::Success;
}

ECraftingResult UModumateCraftingNodeWidgetStatics::SetValueForFormItem(
	Modumate::BIM::FCraftingTreeNodeInstancePool &NodeInstances,
	const FCraftingNodeFormItem &FormItem,
	const FString &Value)
{
	BIM::FCraftingTreeNodeInstanceSharedPtr node = NodeInstances.InstanceFromID(FormItem.NodeInstance);
	if (node.IsValid())
	{
		BIM::FValueSpec vs(*FormItem.PropertyBindingQN);
		node->InstanceProperties.SetProperty(vs.Scope, vs.Name, Value);
		return ECraftingResult::Success;
	}
	return ECraftingResult::Error;
}

ECraftingResult UModumateCraftingNodeWidgetStatics::SetValueForPreset(const FName &PresetID, const FString &Value)
{
	return ECraftingResult::Error;
}

ECraftingResult UModumateCraftingNodeWidgetStatics::RemoveNodeInstance(
	Modumate::BIM::FCraftingTreeNodeInstancePool &NodeInstances,
	int32 ParentID,
	int32 InstanceID)
{
	if (InstanceID == 0)
	{
		return ECraftingResult::Error;
	}

	BIM::FCraftingTreeNodeInstanceSharedPtr parentInst = NodeInstances.InstanceFromID(ParentID);
	BIM::FCraftingTreeNodeInstanceSharedPtr childInst = NodeInstances.InstanceFromID(InstanceID);

	if (parentInst.IsValid() && !parentInst->CanRemoveChild(childInst))
	{
		return ECraftingResult::Error;
	}
	TArray<int32> destroyedChildren;
	NodeInstances.DestroyNodeInstance(InstanceID, destroyedChildren);
	return ECraftingResult::Success;
}

ECraftingResult UModumateCraftingNodeWidgetStatics::GetPinGroupsForNode(
	Modumate::BIM::FCraftingTreeNodeInstancePool &NodeInstances,
	const Modumate::BIM::FCraftingPresetCollection &PresetCollection,
	int32 NodeID,
	TArray<FCraftingNodePinGroup> &OutPins)
{

	BIM::FCraftingTreeNodeInstanceSharedPtr node = NodeInstances.InstanceFromID(NodeID);
	if (!node.IsValid())
	{
		return ECraftingResult::Error;
	}
	for (auto &pin : node->AttachedChildren)
	{
		FCraftingNodePinGroup &group = OutPins.AddDefaulted_GetRef();
		group.MaxPins = pin.SetType.MaxCount;
		group.MinPins = pin.SetType.MinCount;
		group.OwnerInstance = NodeID;

		for (auto &ao : pin.Children)
		{
			group.AttachedChildren.Add(ao.Pin()->GetInstanceID());
		}

		// TODO: GroupName is used to pass SetName for now. Will determine which approach is more appropriate in the future 
		if (group.GroupName.IsNone())
		{
			group.GroupName = pin.SetType.SetName;
		}
	}
	return ECraftingResult::Success;
}

ECraftingResult UModumateCraftingNodeWidgetStatics::GetLayerIDFromNodeInstanceID(
	const Modumate::BIM::FCraftingTreeNodeInstancePool &NodeInstances,
	const Modumate::BIM::FCraftingPresetCollection &PresetCollection,
	int32 InstanceID,
	int32 &OutLayerID,
	int32 &NumberOfLayers)
{
	BIM::FCraftingTreeNodeInstanceSharedPtr thisNode = NodeInstances.InstanceFromID(InstanceID);
	BIM::FCraftingTreeNodeInstanceSharedPtr rootNode = thisNode;

	int32 childPinset = -1, childPinPosition = -1;

	while (rootNode.IsValid())
	{
		const Modumate::BIM::FCraftingTreeNodePreset *preset = PresetCollection.Presets.Find(rootNode->PresetID);
		if (preset != nullptr)
		{
			if (preset->ObjectType != EObjectType::OTNone)
			{
				break;
			}
		}

		int32 childID = rootNode->GetInstanceID();

		rootNode = rootNode->ParentInstance.Pin();
		if (rootNode.IsValid())
		{
			rootNode->FindChild(childID, childPinset, childPinPosition);
			NumberOfLayers = rootNode->AttachedChildren[childPinset].Children.Num();
		}
	}

	OutLayerID = childPinPosition;

	return rootNode != nullptr && childPinPosition != -1 ? ECraftingResult::Success : ECraftingResult::Error;
}

ECraftingResult UModumateCraftingNodeWidgetStatics::GetPropertyTipsByIconType(const FModumateDatabase &InDB, EConfiguratorNodeIconType IconType, const FBIMAssemblySpec &PresetProperties, TArray<FString> &OutTips)
{
	if (IconType == EConfiguratorNodeIconType::Module || IconType == EConfiguratorNodeIconType::Gap2D)
	{
		BIM::EScope scope = IconType == EConfiguratorNodeIconType::Module ? BIM::EScope::Module : BIM::EScope::Gap;

		FString xExtentName, yExtentName, zExtentName, dimension;
		PresetProperties.RootProperties.TryGetProperty(scope, BIM::Parameters::XExtents, xExtentName);
		PresetProperties.RootProperties.TryGetProperty(scope, BIM::Parameters::YExtents, yExtentName);
		PresetProperties.RootProperties.TryGetProperty(scope, BIM::Parameters::ZExtents, zExtentName);
		if (IconType == EConfiguratorNodeIconType::Module)
		{
			dimension = xExtentName + ", " + yExtentName + ", " + zExtentName;
		}
		else
		{
			dimension = xExtentName;
		}

		FString colorName, materialName, color, material;

		if (PresetProperties.RootProperties.TryGetProperty(scope, BIM::Parameters::Color, colorName))
		{
			const FCustomColor *customColor = InDB.GetCustomColorByKey(FName(*colorName));
			if (ensureAlways(customColor != nullptr))
			{
				color = customColor->DisplayName.ToString();
			}
		}

		if (PresetProperties.RootProperties.TryGetProperty(scope, BIM::Parameters::MaterialKey, materialName))
		{
			const FArchitecturalMaterial *mat = InDB.GetArchitecturalMaterialByKey(FName(*materialName));
			if (ensureAlways(mat != nullptr))
			{
				material = mat->DisplayName.ToString();
			}
		}

		OutTips = TArray<FString>{ dimension, material, color };
		return ECraftingResult::Success;
	}
	if (IconType == EConfiguratorNodeIconType::Layer)
	{
		FString layerFunction, thickness, materialName, material;
		for (auto &curLayerProperties : PresetProperties.LayerProperties)
		{
			curLayerProperties.TryGetProperty(BIM::EScope::Layer, BIM::Parameters::Function, layerFunction);
			curLayerProperties.TryGetProperty(BIM::EScope::Layer, BIM::Parameters::Thickness, thickness);
			curLayerProperties.TryGetProperty(BIM::EScope::Module, BIM::Parameters::MaterialKey, materialName);
			if (materialName.IsEmpty())
			{
				curLayerProperties.TryGetProperty(BIM::EScope::Layer, BIM::Parameters::MaterialKey, materialName);
			}
			if (materialName.IsEmpty())
			{
				curLayerProperties.TryGetProperty(BIM::EScope::Gap, BIM::Parameters::MaterialKey, materialName);
			}
			const FArchitecturalMaterial *mat = InDB.GetArchitecturalMaterialByKey(FName(*materialName));
			if (ensureAlways(mat != nullptr))
			{
				material = mat->DisplayName.ToString();
			}
		}
		OutTips = TArray<FString>{ layerFunction, thickness, material };
		return ECraftingResult::Success;
	}
	return ECraftingResult::Error;
}

ECraftingResult UModumateCraftingNodeWidgetStatics::DragMovePinChild(
	Modumate::BIM::FCraftingTreeNodeInstancePool &NodeInstances,
	int32 InstanceID,
	const FName &PinGroup,
	int32 From,
	int32 To)
{
	BIM::FCraftingTreeNodeInstanceSharedPtr node = NodeInstances.InstanceFromID(InstanceID);
	if (!node.IsValid())
	{
		return ECraftingResult::Error;
	}

	Modumate::BIM::FCraftingTreeNodeAttachedChildren *pinSet = nullptr;
	for (auto &pin : node->AttachedChildren)
	{
		if (pin.SetType.SetName == PinGroup)
		{
			pinSet = &pin;
			break;
		}
	}
	if (pinSet == nullptr)
	{
		return ECraftingResult::Error;
	}

	// The widget reckons children in reverse order, so invert the inputs
	int32 reorderFrom = pinSet->Children.Num() - From - 1;
	int32 reorderTo = pinSet->Children.Num() - To - 1;
	if (pinSet->Children.IsValidIndex(reorderFrom) && pinSet->Children.IsValidIndex(reorderTo))
	{
		Modumate::BIM::FCraftingTreeNodeInstanceWeakPtr nodeInstPtr = pinSet->Children[reorderFrom];
		pinSet->Children.RemoveAt(reorderFrom);
		pinSet->Children.Insert(nodeInstPtr, reorderTo);
	}
	return ECraftingResult::Success;
}

ECraftingResult UModumateCraftingNodeWidgetStatics::GetEligiblePresetsForSwap(
	Modumate::BIM::FCraftingTreeNodeInstancePool &NodeInstances,
	const Modumate::BIM::FCraftingPresetCollection &PresetCollection,
	int32 InstanceID,
	TArray<FCraftingNode> &OutPresets)
{
	BIM::FCraftingTreeNodeInstanceSharedPtr instance = NodeInstances.InstanceFromID(InstanceID);
	if (!ensureAlways(instance.IsValid()))
	{
		return ECraftingResult::Error;
	}

	const BIM::FCraftingTreeNodePreset *currentPreset = PresetCollection.Presets.Find(instance->PresetID);

	if (!ensureAlways(currentPreset != nullptr))
	{
		return ECraftingResult::Error;
	}
	TArray<FName> eligiblePresets;
	TArray<Modumate::BIM::FTag> tags;

	// If we have a parent, check its pin for search terms, otherwise use our own tags
	if (instance->ParentInstance.IsValid())
	{
		int32 pinGroup, pinIndex;
		ECraftingResult result = UModumateCraftingNodeWidgetStatics::GetPinAttachmentToParentForNode(NodeInstances, InstanceID, pinGroup, pinIndex);
		if (!ensureAlways(result == ECraftingResult::Success))
		{
			return result;
		}
	}
	return ECraftingResult::Success;
}