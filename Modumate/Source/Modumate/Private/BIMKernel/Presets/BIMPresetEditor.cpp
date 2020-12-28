// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "BIMKernel/Presets/BIMPresetEditor.h"

#include "Database/ModumateObjectDatabase.h"
#include "Database/ModumateArchitecturalMaterial.h"
#include "ModumateCore/ModumateScriptProcessor.h"
#include "DocumentManagement/ModumateCommands.h"
#include "Algo/Accumulate.h"
#include "Algo/Transform.h"
#include "Algo/Compare.h"

EBIMResult FBIMPresetEditor::ResetInstances()
{
	InstancePool.Empty();
	InstanceMap.Empty();
	NextInstanceID = 1;
	if (!ValidatePool())
	{
		return EBIMResult::Error;
	}
	return EBIMResult::Success;
}

const FBIMPresetEditorNodeSharedPtr FBIMPresetEditor::InstanceFromID(const FBIMEditorNodeIDType& InstanceID) const
{
	const FBIMPresetEditorNodeWeakPtr *instancePtr = InstanceMap.Find(InstanceID);
	if (instancePtr != nullptr && instancePtr->IsValid())
	{
		return instancePtr->Pin();
	}
	return FBIMPresetEditorNodeSharedPtr();
}

FBIMPresetEditorNodeSharedPtr FBIMPresetEditor::InstanceFromID(const FBIMEditorNodeIDType& InstanceID)
{
	const FBIMPresetEditorNodeWeakPtr *instancePtr = InstanceMap.Find(InstanceID);
	if (instancePtr != nullptr && instancePtr->IsValid())
	{
		return instancePtr->Pin();
	}
	return FBIMPresetEditorNodeSharedPtr();
}

const FBIMPresetEditorNodeSharedPtr FBIMPresetEditor::GetRootInstance() const
{
	for (const auto& inst: InstancePool)
	{
		if (!inst->ParentInstance.IsValid())
		{
			return inst;
		}
	}
	return nullptr;
}

FBIMPresetEditorNodeSharedPtr FBIMPresetEditor::GetRootInstance()
{
	for (const auto& inst : InstancePool)
	{
		if (!inst->ParentInstance.IsValid())
		{
			return inst;
		}
	}
	return nullptr;
}

FBIMPresetEditorNodeSharedPtr FBIMPresetEditor::CreateNodeInstanceFromPreset(const FBIMPresetCollection& PresetCollection, const FBIMEditorNodeIDType& ParentID, const FGuid& PresetID, int32 ParentSetIndex, int32 ParentSetPosition)
{
	return CreateNodeInstanceFromPreset(PresetCollection, ParentID, PresetID, ParentSetIndex, ParentSetPosition, FGuid());
}

FBIMPresetEditorNodeSharedPtr FBIMPresetEditor::CreateNodeInstanceFromPreset(const FBIMPresetCollection& PresetCollection, const FBIMEditorNodeIDType& ParentID, const FGuid& PresetGUID, int32 ParentSetIndex, int32 ParentSetPosition, const FGuid& SlotAssignment)
{
	if (!ensureAlways(PresetGUID.IsValid()))
	{
		return nullptr;
	}

	struct FPresetTreeIterator
	{
		FBIMEditorNodeIDType ParentNode;
		FGuid PresetID;
		FGuid SlotAssignment;
		int32 ParentSetIndex, ParentSetPosition;

		FPresetTreeIterator() {};
		FPresetTreeIterator(const FBIMEditorNodeIDType& InParentNode, const FGuid& InPresetID, const FGuid& InSlot) :
			ParentNode(InParentNode), PresetID(InPresetID), SlotAssignment(InSlot)
		{
			ensureAlways(InPresetID.IsValid());
			ensureAlways(InSlot.IsValid());
			ParentSetIndex = INDEX_NONE;
			ParentSetPosition = INDEX_NONE;
		}

		FPresetTreeIterator(const FBIMEditorNodeIDType& InParentNode, const FGuid& InPresetID, int32 InParentSetIndex, int32 InParentSetPosition) :
			ParentNode(InParentNode), PresetID(InPresetID), ParentSetIndex(InParentSetIndex), ParentSetPosition(InParentSetPosition)
		{
			ensureAlways(InPresetID.IsValid());
		}
	};

	TArray<FPresetTreeIterator> iteratorStack;
	if (!SlotAssignment.IsValid())
	{
		iteratorStack.Push(FPresetTreeIterator(ParentID, PresetGUID, ParentSetIndex, ParentSetPosition));
	}
	else
	{
		iteratorStack.Push(FPresetTreeIterator(ParentID, PresetGUID, SlotAssignment));
	}

	FBIMPresetEditorNodeSharedPtr returnVal = nullptr;
	TArray<FBIMPresetEditorNodeSharedPtr> createdInstances;
	FBIMPresetEditorNodeSharedPtr parent = InstanceFromID(ParentID);

	if (parent.IsValid())
	{
		// Presets are assigned either to a part slot or a pin, but not both
		if (!SlotAssignment.IsValid())
		{
			if (!parent->WorkingPresetCopy.HasPin(ParentSetIndex, ParentSetPosition))
			{
				parent->WorkingPresetCopy.AddChildPreset(PresetGUID, ParentSetIndex, ParentSetPosition);
			}
		}
		else
		{
			parent->WorkingPresetCopy.SetPartPreset(SlotAssignment, PresetGUID);
		}
	}

	while (iteratorStack.Num() > 0)
	{
		FPresetTreeIterator iterator = iteratorStack.Pop();

		const FBIMPresetInstance* preset = PresetCollection.PresetFromGUID(iterator.PresetID);
		if (!ensureAlways(preset != nullptr))
		{
			return nullptr;
		}

		FBIMEditorNodeIDType nodeID = *FString::Printf(TEXT("NODE-%02d"), NextInstanceID);
		FBIMPresetEditorNodeSharedPtr instance = InstancePool.Add_GetRef(MakeShareable(new FBIMPresetEditorNode(nodeID,iterator.ParentSetIndex,iterator.ParentSetPosition,*preset)));
		createdInstances.Add(instance);
		InstanceMap.Add(nodeID, instance);
		++NextInstanceID;

		parent = InstanceFromID(iterator.ParentNode);
		if (parent.IsValid())
		{
			instance->ParentInstance = parent;

			if (!iterator.SlotAssignment.IsValid())
			{
				instance->MyParentPinSetIndex = iterator.ParentSetIndex;
				instance->MyParentPinSetPosition = iterator.ParentSetPosition;
				parent->ChildNodes.Add(instance);
			}
			else
			{
				instance->MyParentPartSlot = iterator.SlotAssignment;
				instance->MyParentPinSetIndex = INDEX_NONE;
				instance->MyParentPinSetPosition = INDEX_NONE;
				parent->PartNodes.Add(instance);
			}
		}

		if (returnVal == nullptr)
		{
			returnVal = instance;
		}

		for (int32 i= preset->ChildPresets.Num()-1;i>=0;--i)
		{
			auto& childPreset = preset->ChildPresets[i];
			if (ensureAlways(childPreset.PresetGUID.IsValid()))
			{
				iteratorStack.Push(FPresetTreeIterator(instance->GetInstanceID(), childPreset.PresetGUID, childPreset.ParentPinSetIndex, childPreset.ParentPinSetPosition));
			}
		}

		for (int32 i = preset->PartSlots.Num() - 1; i >= 0; --i)
		{
			auto& partPreset = preset->PartSlots[i];
			if (partPreset.PartPresetGUID.IsValid())
			{
				iteratorStack.Push(FPresetTreeIterator(instance->GetInstanceID(), partPreset.PartPresetGUID, partPreset.SlotPresetGUID));
			}
		}
	}

	for (auto& createdInstance : createdInstances)
	{
		createdInstance->SortChildren();
	}

	return returnVal;
}

EBIMResult FBIMPresetEditor::SortAndValidate() const
{
	for (auto inst : InstancePool)
	{
		inst->SortChildren();
	}
	ValidatePool();
	return EBIMResult::Success;
}

bool FBIMPresetEditor::ValidatePool() const
{
	bool ret = true;

#if WITH_EDITOR
	auto myChildrenPointToMe = [](const FBIMPresetEditorNodeSharedPtr &me)
	{
		bool ret = true;
		for (auto &op : me->ChildNodes)
		{
			ret = ensureAlways(op.Pin()->ParentInstance.HasSameObject(me.Get())) && ret;
		}
		return ret;
	};

	auto myParentPointsToMe = [](const FBIMPresetEditorNodeSharedPtr& me)
	{
		if (me->ParentInstance == nullptr)
		{
			return true;
		}
		if (!me->MyParentPartSlot.IsValid())
		{
			for (auto& op : me->ParentInstance.Pin()->ChildNodes)
			{
				if (op.HasSameObject(me.Get()))
				{
					return true;
				}
			}
		}
		else
		{
			for (auto& op : me->ParentInstance.Pin()->PartNodes)
			{
				if (op.HasSameObject(me.Get()))
				{
					return true;
				}
			}
		}
		return false;
	};

	int32 parentlessNodes = 0;

	for (auto inst : InstancePool)
	{
		ret = ensureAlways(myParentPointsToMe(inst)) && ret;
		ret = ensureAlways(myChildrenPointToMe(inst)) && ret;
		if (!inst->ParentInstance.IsValid())
		{
			++parentlessNodes;
		}
	}

	ret = ensureAlways(parentlessNodes < 2) && ret;

	ret = ensureAlways(InstancePool.Num() == InstanceMap.Num()) && ret;

	for (auto &kvp : InstanceMap)
	{
		ret = ensureAlways(InstancePool.Contains(kvp.Value.Pin())) && ret;
		ret = ensureAlways(kvp.Value.Pin()->ValidateNode());
	}

	ensureAlways(ret);
#endif

	return ret;
}

EBIMResult FBIMPresetEditor::CreateAssemblyFromNodes(const FBIMPresetCollection& PresetCollection, const FModumateDatabase& InDB, FBIMAssemblySpec& OutAssemblySpec)
{
	EBIMResult result = EBIMResult::Error;
	// Get root node
	FBIMPresetEditorNodeSharedPtr rootNode;
	for (const auto &inst : InstancePool)
	{
		if (inst->ParentInstance == nullptr)
		{
			rootNode = inst;
			break;
		}
	}
	// Make assembly from root node
	if (rootNode.IsValid())
	{
		// Build the preset collection must reflect the dirty state of the node
		// Clean presets should never override dirty presets regardless of node order
		// Therefore dirty presets are added unconditional, and clean presets are added only if they don't already exist

		FBIMPresetCollection previewCollection;
		TArray<FGuid> dependentPresets;
		for (auto& instance : InstancePool)
		{
			previewCollection.AddPreset(instance->WorkingPresetCopy);
			PresetCollection.GetDependentPresets(instance->WorkingPresetCopy.GUID, dependentPresets);
		}

		for (auto& depPres : dependentPresets)
		{
			const FBIMPresetInstance* depPreset = PresetCollection.PresetFromGUID(depPres);
			if (ensureAlways(depPreset != nullptr))
			{
				previewCollection.AddPreset(*depPreset);
			}
		}

		result = OutAssemblySpec.FromPreset(InDB, previewCollection, rootNode->WorkingPresetCopy.GUID);
	}
	return result;
}

/*
Build a layered assembly based on a single layer, used by the icon generator to produce layer previews
*/
EBIMResult FBIMPresetEditor::CreateAssemblyFromLayerNode(const FBIMPresetCollection& PresetCollection, const FModumateDatabase& InDB, const FBIMEditorNodeIDType& LayerNodeID, FBIMAssemblySpec& OutAssemblySpec)
{
	// Because FBIMAssemblySpec::FromPreset must have an assembly scope, we're 'borrowing' the preset from root node, then presets from layer node 
	FBIMPresetCollection previewCollection;

	// Add layer node preset
	FBIMPresetEditorNodeSharedPtr layerNode = InstanceFromID(LayerNodeID);
	if (!layerNode.IsValid())
	{
		return EBIMResult::Error;
	}

	// Build a temporary top-level assembly node to host the single layer
	FBIMPresetInstance assemblyPreset;
	assemblyPreset.PresetID = FBIMKey(TEXT("TempIconPreset"));
	assemblyPreset.GUID= FGuid::NewGuid();
	assemblyPreset.NodeScope = EBIMValueScope::Assembly;

	// Give the temporary assembly a single layer child
	FBIMPresetPinAttachment& attachment = assemblyPreset.ChildPresets.AddDefaulted_GetRef();
	attachment.ParentPinSetIndex = 0;
	attachment.ParentPinSetPosition = 0;
	attachment.PresetID = layerNode->WorkingPresetCopy.PresetID;
	attachment.PresetGUID = layerNode->WorkingPresetCopy.GUID;

	// Fetch the root node to get node and object type information for the temp assembly
	for (const auto &inst : InstancePool)
	{
		if (inst->ParentInstance == nullptr)
		{
			assemblyPreset.NodeType = inst->WorkingPresetCopy.NodeType;
			assemblyPreset.ObjectType = inst->WorkingPresetCopy.ObjectType;
			break;
		}
	}

	// Add the temp assembly and layer presets
	previewCollection.AddPreset(assemblyPreset);
	previewCollection.AddPreset(layerNode->WorkingPresetCopy);

	// Add presets from children of the layer node 
	TArray<FBIMPresetEditorNodeSharedPtr> childrenNodes;
	layerNode->GatherAllChildNodes(childrenNodes);
	for (auto& child : childrenNodes)
	{
		previewCollection.AddPreset(child->WorkingPresetCopy);
	}
	
	return OutAssemblySpec.FromPreset(InDB, previewCollection, assemblyPreset.GUID);
}

EBIMResult FBIMPresetEditor::ReorderChildNode(const FBIMEditorNodeIDType& ChildNode, int32 FromPosition, int32 ToPosition)
{
	FBIMPresetEditorNodeSharedPtr childPtr = InstanceFromID(ChildNode);

	if (!childPtr.IsValid())
	{
		return EBIMResult::Error;
	}

	if (!childPtr->ParentInstance.IsValid())
	{
		return EBIMResult::Error;
	}

	auto pinnedParent = childPtr->ParentInstance.Pin();

	childPtr->MyParentPinSetPosition = ToPosition;
	for (auto& child : pinnedParent->ChildNodes)
	{
		auto pinnedChild = child.Pin();
		if (pinnedChild == childPtr)
		{
			continue;
		}
		if (pinnedChild->MyParentPinSetIndex == childPtr->MyParentPinSetIndex)
		{
			if (pinnedChild->MyParentPinSetPosition >= ToPosition && pinnedChild->MyParentPinSetPosition < FromPosition)
			{
				++pinnedChild->MyParentPinSetPosition;
			}
			if (pinnedChild->MyParentPinSetPosition <= ToPosition && pinnedChild->MyParentPinSetPosition > FromPosition)
			{
				--pinnedChild->MyParentPinSetPosition;
			}
		}
	}

	for (auto& child : pinnedParent->WorkingPresetCopy.ChildPresets)
	{
		if (child.ParentPinSetIndex == childPtr->MyParentPinSetIndex)
		{
			if (child.ParentPinSetPosition == FromPosition)
			{
				child.ParentPinSetPosition = ToPosition;
			}
			else
			if (child.ParentPinSetPosition >= ToPosition && child.ParentPinSetPosition < FromPosition)
			{
				++child.ParentPinSetPosition;
			}
			else
			if (child.ParentPinSetPosition <= ToPosition && child.ParentPinSetPosition > FromPosition)
			{
				--child.ParentPinSetPosition;
			}
		}
	}
	pinnedParent->SortChildren();
	return EBIMResult::Success;
}

EBIMResult FBIMPresetEditor::FindNodeParentLineage(const FBIMEditorNodeIDType& NodeID, TArray<FBIMEditorNodeIDType>& OutLineage)
{
	OutLineage.Empty();
	FBIMPresetEditorNodeSharedPtr instPtr = InstanceFromID(NodeID);
	if (!instPtr.IsValid())
	{
		return EBIMResult::Error;
	}

	FBIMPresetEditorNodeSharedPtr parentPtr = instPtr->ParentInstance.Pin();
	while (parentPtr.IsValid())
	{
		OutLineage.Add(parentPtr->InstanceID);
		if (parentPtr->ParentInstance.IsValid())
		{
			parentPtr = parentPtr->ParentInstance.Pin();
		}
		else
		{
			parentPtr = nullptr;
		}
	}
	return EBIMResult::Success;
}

bool FBIMPresetEditor::GetSortedNodeIDs(TArray<FBIMEditorNodeIDType> &OutNodeIDs)
{
	// Use the root node as starting point
	FBIMPresetEditorNodeSharedPtr startInst;
	for (const auto& inst : InstancePool)
	{
		if (inst->ParentInstance == nullptr)
		{
			startInst = inst;
			break;
		}
	}

	while (OutNodeIDs.Num() != InstancePool.Num())
	{
		// Add current node to array
		OutNodeIDs.AddUnique(startInst->GetInstanceID());
		if (OutNodeIDs.Num() == InstancePool.Num())
		{
			return true;
		}

		TArray<FBIMEditorNodeIDType> childrenIDs;
		startInst->GatherChildrenInOrder(childrenIDs);

		// Go through its children to find the next appropriate node
		bool canUseNextChild = false;
		if (childrenIDs.Num() > 0)
		{
			for (const auto& curChildID : childrenIDs)
			{
				if (!OutNodeIDs.Contains(curChildID))
				{
					startInst = InstanceFromID(curChildID);
					canUseNextChild = true;
					break;
				}
			}
		}

		// No available child, this can happen by:
		// 1. This node is in the end of the tree
		// 2. All of its children have been used 
		if (!canUseNextChild)
		{
			// Keep searching from previous parents that has an available child
			FBIMPresetEditorNodeSharedPtr parentInst = startInst->ParentInstance.Pin();
			if (!parentInst.IsValid())
			{
				return false;
			}
			bool validParent = false;
			while (!validParent)
			{
				TArray<FBIMEditorNodeIDType> otherChildrenPin;
				parentInst->GatherChildrenInOrder(otherChildrenPin);
				for (const auto& curOtherChildID : otherChildrenPin)
				{
					if (!OutNodeIDs.Contains(curOtherChildID))
					{
						validParent = true;
						startInst = parentInst;
					}
				}
				// If the current parent doesn't have an open child, then search its grandparent
				if (!validParent)
				{
					parentInst = parentInst->ParentInstance.Pin();
				}
			}
		}
	}

	return false;
}

EBIMResult FBIMPresetEditor::DestroyNodeInstance(const FBIMPresetEditorNodeSharedPtr& Instance, TArray<FBIMEditorNodeIDType>& OutDestroyed)
{
	FBIMPresetEditorNodeSharedPtr parent;
	if (Instance->ParentInstance.IsValid())
	{
		parent = InstanceFromID(Instance->ParentInstance.Pin()->GetInstanceID());
	}

	if (!ensureAlways(Instance != nullptr))
	{
		return EBIMResult::Error;
	}
	TArray<FBIMPresetEditorNodeSharedPtr> childNodes;
	Instance->GatherAllChildNodes(childNodes);
	OutDestroyed.Add(Instance->GetInstanceID());
	Instance->DetachSelfFromParent();
	InstanceMap.Remove(Instance->GetInstanceID());

	InstancePool.Remove(Instance);
	for (auto& inst : childNodes)
	{
		OutDestroyed.Add(inst->GetInstanceID());
		InstanceMap.Remove(inst->GetInstanceID());
		InstancePool.Remove(inst);
	}

	if (parent.IsValid())
	{
		parent->SortChildren();
	}
	ValidatePool();

	return EBIMResult::Success;
}

EBIMResult FBIMPresetEditor::DestroyNodeInstance(const FBIMEditorNodeIDType& InstanceID, TArray<FBIMEditorNodeIDType>& OutDestroyed)
{
	return DestroyNodeInstance(InstanceFromID(InstanceID), OutDestroyed);
}

EBIMResult FBIMPresetEditor::InitFromPreset(const FBIMPresetCollection& PresetCollection, const FGuid& PresetGUID, FBIMPresetEditorNodeSharedPtr& OutRootNode)
{
	ResetInstances();
	OutRootNode = CreateNodeInstanceFromPreset(PresetCollection, BIM_ID_NONE, PresetGUID, 0, 0);
	SortAndValidate();
	return OutRootNode.IsValid() ? EBIMResult::Success : EBIMResult::Error;
}

EBIMResult FBIMPresetEditor::SetNewPresetForNode(const FBIMPresetCollection& PresetCollection, const FBIMEditorNodeIDType& InstanceID, const FGuid& PresetGUID)
{
	FBIMPresetEditorNodeSharedPtr inst = InstanceFromID(InstanceID);

	if (!ensureAlways(inst != nullptr))
	{
		return EBIMResult::Error;
	}

	FGuid parentSlot = inst->MyParentPartSlot;

	TArray<FBIMEditorNodeIDType> toDestroy;
	for (auto& child : inst->ChildNodes)
	{
		toDestroy.Add(child.Pin()->GetInstanceID());
	}

	for (auto& part : inst->PartNodes)
	{
		toDestroy.Add(part.Pin()->GetInstanceID());
	}

	for (auto& child : toDestroy)
	{
		TArray<FBIMEditorNodeIDType> destroyed;
		DestroyNodeInstance(child, destroyed);
	}

	const FBIMPresetInstance* preset = PresetCollection.PresetFromGUID(PresetGUID);

	if (!ensureAlways(preset != nullptr))
	{
		return EBIMResult::Error;
	}

	if (inst->ParentInstance.IsValid())
	{
		if (!parentSlot.IsValid())
		{
			for (auto& sibling : inst->ParentInstance.Pin()->WorkingPresetCopy.ChildPresets)
			{
				if (sibling.ParentPinSetIndex == inst->MyParentPinSetIndex && sibling.ParentPinSetPosition == inst->MyParentPinSetPosition)
				{
					sibling.PresetID = preset->PresetID;
					sibling.PresetGUID = preset->GUID;
					break;
				}
			}
		}
		else
		{
			for (auto& part : inst->ParentInstance.Pin()->WorkingPresetCopy.PartSlots)
			{
				if (part.SlotPresetGUID == parentSlot)
				{
					part.PartPreset = preset->PresetID;
					part.PartPresetGUID = preset->GUID;
					break;
				}
			}
		}
	}

	inst->WorkingPresetCopy = *preset;
	inst->OriginalPresetCopy = *preset;

	for (auto& childPreset : preset->ChildPresets)
	{
		if (childPreset.PresetGUID.IsValid())
		{
			CreateNodeInstanceFromPreset(PresetCollection, InstanceID, childPreset.PresetGUID, childPreset.ParentPinSetIndex, childPreset.ParentPinSetPosition);
		}
	}

	for (auto& partPreset : preset->PartSlots)
	{
		if (partPreset.PartPresetGUID.IsValid())
		{
			CreateNodeInstanceFromPreset(PresetCollection, inst->GetInstanceID(), partPreset.PartPresetGUID, INDEX_NONE, INDEX_NONE, partPreset.SlotPresetGUID);
		}
	}

	SortAndValidate();

	return EBIMResult::Success;
}

EBIMResult FBIMPresetEditor::SetPartPreset(const FBIMPresetCollection& PresetCollection, const FBIMEditorNodeIDType& ParentID, int32 SlotID, const FGuid& PartPreset)
{
	const FBIMPresetEditorNodeSharedPtr nodeParent = InstanceFromID(ParentID);

	if (!nodeParent.IsValid())
	{
		return EBIMResult::Error;
	}

	FGuid slotPreset = nodeParent->WorkingPresetCopy.PartSlots[SlotID].SlotPresetGUID;

	// If we already have a part here, just swap the preset
	for (auto& partNode : nodeParent->PartNodes)
	{
		auto partNodePinned = partNode.Pin();
		if (partNodePinned->MyParentPartSlot == slotPreset)
		{
			SetNewPresetForNode(PresetCollection, partNodePinned->GetInstanceID(), PartPreset);
			return EBIMResult::Success;
		}
	}

	// If we don't already have this part, create a new node
	if (CreateNodeInstanceFromPreset(PresetCollection, ParentID, PartPreset, INDEX_NONE, INDEX_NONE, slotPreset).IsValid())
	{
		nodeParent->SortChildren();
		return EBIMResult::Success;
	}
	return EBIMResult::Error;
}

EBIMResult FBIMPresetEditor::ClearPartPreset(const FBIMEditorNodeIDType& ParentID, int32 SlotID)
{
	const FBIMPresetEditorNodeSharedPtr nodeParent = InstanceFromID(ParentID);

	if (!nodeParent.IsValid())
	{
		return EBIMResult::Error;
	}

	FGuid slotPreset = nodeParent->WorkingPresetCopy.PartSlots[SlotID].SlotPresetGUID;

	for (auto& partNode : nodeParent->PartNodes)
	{
		auto partNodePinned = partNode.Pin();
		if (partNodePinned->MyParentPartSlot == slotPreset)
		{
			TArray<FBIMEditorNodeIDType> destroyed;
			// DestroyNodeInstance will update the preset
			DestroyNodeInstance(partNodePinned,destroyed);
			return EBIMResult::Success;
		}
	}
	return EBIMResult::Error;
}
