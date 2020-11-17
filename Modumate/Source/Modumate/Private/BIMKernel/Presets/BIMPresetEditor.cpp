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

const FBIMPresetEditorNodeSharedPtr FBIMPresetEditor::InstanceFromID(int32 InstanceID) const
{
	const FBIMPresetEditorNodeWeakPtr *instancePtr = InstanceMap.Find(InstanceID);
	if (instancePtr != nullptr && instancePtr->IsValid())
	{
		return instancePtr->Pin();
	}
	return FBIMPresetEditorNodeSharedPtr();
}

FBIMPresetEditorNodeSharedPtr FBIMPresetEditor::InstanceFromID(int32 InstanceID)
{
	const FBIMPresetEditorNodeWeakPtr *instancePtr = InstanceMap.Find(InstanceID);
	if (instancePtr != nullptr && instancePtr->IsValid())
	{
		return instancePtr->Pin();
	}
	return FBIMPresetEditorNodeSharedPtr();
}

FBIMPresetEditorNodeSharedPtr FBIMPresetEditor::CreateNodeInstanceFromPreset(const FBIMPresetCollection& PresetCollection, int32 ParentID, const FBIMKey& PresetID, int32 ParentSetIndex, int32 ParentSetPosition, const FBIMKey& SlotAssignment)
{
	struct FPresetTreeIterator
	{
		int32 ParentNode;
		FBIMKey PresetID;
		FBIMKey SlotAssignment;
		int32 ParentSetIndex, ParentSetPosition;

		FPresetTreeIterator() {};
		FPresetTreeIterator(int32 InParentNode, const FBIMKey& InPresetID, const FBIMKey& InSlot) :
			ParentNode(InParentNode), PresetID(InPresetID), SlotAssignment(InSlot)
		{
			ParentSetIndex = INDEX_NONE;
			ParentSetPosition = INDEX_NONE;
		}

		FPresetTreeIterator(int32 InParentNode, const FBIMKey& InPresetID, int32 InParentSetIndex, int32 InParentSetPosition, const FBIMKey& InSlot) : 
			ParentNode(InParentNode), PresetID(InPresetID), SlotAssignment(InSlot), ParentSetIndex(InParentSetIndex), ParentSetPosition(InParentSetPosition)
		{}
	};

	TArray<FPresetTreeIterator> iteratorStack;

	iteratorStack.Push(FPresetTreeIterator(ParentID, PresetID, ParentSetIndex, ParentSetPosition, SlotAssignment));

	FBIMPresetEditorNodeSharedPtr returnVal = nullptr;

	TArray<FBIMPresetEditorNodeSharedPtr> createdInstances;

	FBIMPresetEditorNodeSharedPtr parent = InstanceFromID(ParentID);
	if (parent.IsValid())
	{
		parent->WorkingPresetCopy.AddChildPreset(PresetID, ParentSetIndex, ParentSetPosition);
	}

	while (iteratorStack.Num() > 0)
	{
		FPresetTreeIterator iterator = iteratorStack.Pop();

		const FBIMPresetInstance* preset = PresetCollection.Presets.Find(iterator.PresetID);
		if (!ensureAlways(preset != nullptr))
		{
			return nullptr;
		}

		FBIMPresetEditorNodeSharedPtr instance = InstancePool.Add_GetRef(MakeShareable(new FBIMPresetEditorNode(NextInstanceID,iterator.ParentSetIndex,iterator.ParentSetPosition,*preset)));
		createdInstances.Add(instance);
		InstanceMap.Add(NextInstanceID, instance);
		++NextInstanceID;

		parent = InstanceFromID(iterator.ParentNode);
		if (parent.IsValid())
		{
			instance->ParentInstance = parent;

			if (iterator.SlotAssignment.IsNone())
			{
				instance->MyParentPinSetIndex = iterator.ParentSetIndex;
				instance->MyParentPinSetPosition = iterator.ParentSetPosition;
				parent->ChildNodes.Add(instance);
			}
#if 0 // TODO: access parts directly from preset
			else
			{
				ensureAlways(parent->AttachPartChild(instance, *iterator.SlotAssignment.ToString()) != EBIMResult::Error);
			}
#endif
		}

		if (returnVal == nullptr)
		{
			returnVal = instance;
		}

		for (int32 i= preset->ChildPresets.Num()-1;i>=0;--i)
		{
			auto& childPreset = preset->ChildPresets[i];
			iteratorStack.Push(FPresetTreeIterator(instance->GetInstanceID(), childPreset.PresetID, childPreset.ParentPinSetIndex, childPreset.ParentPinSetPosition, FBIMKey()));
		}

#if 0 // TODO: parts not yet supported
		for (int32 i=preset->PartSlots.Num()-1;i>=0;--i)
		{
			auto& partPreset = preset->PartSlots[i];
			iteratorStack.Push(FPresetTreeIterator(instance->GetInstanceID(), partPreset.PartPreset, partPreset.SlotName));
		}
#endif
	}

	for (auto& createdInstance : createdInstances)
	{
		createdInstance->SortChildren();
	}

	if (returnVal->ParentInstance.IsValid())
	{
		returnVal->ParentInstance.Pin()->SortChildren();
	}

	ValidatePool();
	return returnVal;
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

	auto myParentPointsToMe = [](const FBIMPresetEditorNodeSharedPtr &me)
	{
		if (me->ParentInstance == nullptr)
		{
			return true;
		}
		for (auto &op : me->ParentInstance.Pin()->ChildNodes)
		{
			if (op.HasSameObject(me.Get()))
			{
				return true;
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
		TArray<FBIMKey> dependentPresets;
		for (auto& instance : InstancePool)
		{
			if (instance->GetPresetStatus() == EBIMPresetEditorNodeStatus::Dirty)
			{
				previewCollection.Presets.Add(instance->WorkingPresetCopy.PresetID, instance->WorkingPresetCopy);
			}
			else
			{
				const FBIMPresetInstance* original = PresetCollection.Presets.Find(instance->WorkingPresetCopy.PresetID);
				if (ensureAlways(original != nullptr) && !previewCollection.Presets.Contains(original->PresetID))
				{
					previewCollection.Presets.Add(original->PresetID, *original);
				}
			}
			PresetCollection.GetDependentPresets(instance->WorkingPresetCopy.PresetID, dependentPresets);
		}

		for (auto& depPres : dependentPresets)
		{
			if (!previewCollection.Presets.Contains(depPres))
			{
				const FBIMPresetInstance* depPreset = PresetCollection.Presets.Find(depPres);
				if (ensureAlways(depPreset != nullptr))
				{
					previewCollection.Presets.Add(depPreset->PresetID, *depPreset);
				}
			}
		}

		result = OutAssemblySpec.FromPreset(InDB, previewCollection, rootNode->WorkingPresetCopy.PresetID);
	}
	return result;
}

/*
Build a layered assembly based on a single layer, used by the icon generator to produce layer previews
*/
EBIMResult FBIMPresetEditor::CreateAssemblyFromLayerNode(const FBIMPresetCollection& PresetCollection, const FModumateDatabase& InDB, int32 LayerNodeID, FBIMAssemblySpec& OutAssemblySpec)
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
	assemblyPreset.NodeScope = EBIMValueScope::Assembly;

	// Give the temporary assembly a single layer child
	FBIMPresetPinAttachment& attachment = assemblyPreset.ChildPresets.AddDefaulted_GetRef();
	attachment.ParentPinSetIndex = 0;
	attachment.ParentPinSetPosition = 0;
	attachment.PresetID = layerNode->WorkingPresetCopy.PresetID;

	// Fetch the root node to get node and object type information for the temp assembly
	for (const auto &inst : InstancePool)
	{
		if (inst->ParentInstance == nullptr)
		{
			const FBIMPresetInstance* rootPreset = PresetCollection.Presets.Find(inst->WorkingPresetCopy.PresetID);
			if (ensureAlways(rootPreset != nullptr))
			{
				assemblyPreset.NodeType = rootPreset->NodeType;
				assemblyPreset.ObjectType = rootPreset->ObjectType;
			}
			else
			{
				return EBIMResult::Error;
			}
			break;
		}
	}

	// Add the temp assembly and layer presets
	previewCollection.Presets.Add(assemblyPreset.PresetID, assemblyPreset);

	const FBIMPresetInstance* layerPreset = PresetCollection.Presets.Find(layerNode->WorkingPresetCopy.PresetID);
	if (ensureAlways(layerPreset != nullptr))
	{
		previewCollection.Presets.Add(layerPreset->PresetID, *layerPreset);
	}
	else
	{
		return EBIMResult::Error;
	}

	// Add presets from children of the layer node 
	TArray<FBIMPresetEditorNodeSharedPtr> childrenNodes;
	layerNode->GatherAllChildNodes(childrenNodes);
	for (auto& child : childrenNodes)
	{
		if (child->GetPresetStatus() == EBIMPresetEditorNodeStatus::Dirty)
		{
			previewCollection.Presets.Add(child->WorkingPresetCopy.PresetID, child->WorkingPresetCopy);
		}
		else
		{
			const FBIMPresetInstance* original = PresetCollection.Presets.Find(child->WorkingPresetCopy.PresetID);
			if (ensureAlways(original != nullptr))
			{
				previewCollection.Presets.Add(original->PresetID, *original);
			}
		}
	}
	
	return OutAssemblySpec.FromPreset(InDB, previewCollection, assemblyPreset.PresetID);
}

EBIMResult FBIMPresetEditor::ReorderChildNode(int32 ChildNode, int32 FromPosition, int32 ToPosition)
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

EBIMResult FBIMPresetEditor::FindNodeParentLineage(int32 NodeID, TArray<int32>& OutLineage)
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

bool FBIMPresetEditor::GetSortedNodeIDs(TArray<int32> &OutNodeIDs)
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

		TArray<int32> childrenIDs;
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
				TArray<int32> otherChildrenPin;
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

EBIMResult FBIMPresetEditor::DestroyNodeInstance(const FBIMPresetEditorNodeSharedPtr& Instance, TArray<int32>& OutDestroyed)
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

EBIMResult FBIMPresetEditor::DestroyNodeInstance(int32 InstanceID, TArray<int32>& OutDestroyed)
{
	return DestroyNodeInstance(InstanceFromID(InstanceID), OutDestroyed);
}

EBIMResult FBIMPresetEditor::InitFromPreset(const FBIMPresetCollection& PresetCollection, const FBIMKey& PresetID, FBIMPresetEditorNodeSharedPtr& OutRootNode)
{
	ResetInstances();
	OutRootNode = CreateNodeInstanceFromPreset(PresetCollection, 0, PresetID, 0, 0);
	return OutRootNode.IsValid() ? EBIMResult::Success : EBIMResult::Error;
}

EBIMResult FBIMPresetEditor::SetNewPresetForNode(const FBIMPresetCollection& PresetCollection, int32 InstanceID, const FBIMKey& PresetID)
{
	FBIMPresetEditorNodeSharedPtr inst = InstanceFromID(InstanceID);

	if (!ensureAlways(inst != nullptr))
	{
		return EBIMResult::Error;
	}

	TArray<int32> toDestroy;
	for (auto& child : inst->ChildNodes)
	{
		toDestroy.Add(child.Pin()->GetInstanceID());
	}

	for (auto& child : toDestroy)
	{
		TArray<int32> destroyed;
		DestroyNodeInstance(child, destroyed);
	}

	const FBIMPresetInstance* preset = PresetCollection.Presets.Find(PresetID);

	if (!ensureAlways(preset != nullptr))
	{
		return EBIMResult::Error;
	}

	for (auto& sibling : inst->ParentInstance.Pin()->WorkingPresetCopy.ChildPresets)
	{
		if (sibling.ParentPinSetIndex == inst->MyParentPinSetIndex && sibling.ParentPinSetPosition == inst->MyParentPinSetPosition)
		{
			sibling.PresetID = preset->PresetID;
		}
	}

	inst->WorkingPresetCopy = *preset;
	inst->OriginalPresetCopy = *preset;

	for (auto& childPreset : preset->ChildPresets)
	{
		auto newNode = CreateNodeInstanceFromPreset(PresetCollection, 0, childPreset.PresetID, INDEX_NONE, INDEX_NONE);
		inst->ChildNodes.Add(newNode);
		auto lastPinned = inst->ChildNodes.Last().Pin();
		lastPinned->MyParentPinSetIndex = childPreset.ParentPinSetIndex;
		lastPinned->MyParentPinSetPosition = childPreset.ParentPinSetPosition;
		lastPinned->ParentInstance = inst;
	}

	inst->SortChildren();

#if 0 // TODO: UI will need custom part processing, not just nodes
	for (auto& partPreset : preset->PartSlots)
	{
		CreateNodeInstanceFromPreset(PresetCollection, inst->GetInstanceID(), partPreset.PartPreset, -1, -1, partPreset.SlotName);
	}
#endif

	return EBIMResult::Success;
}