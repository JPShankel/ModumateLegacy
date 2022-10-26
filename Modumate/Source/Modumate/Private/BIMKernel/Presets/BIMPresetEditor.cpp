// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "BIMKernel/Presets/BIMPresetEditor.h"


#include "Database/ModumateArchitecturalMaterial.h"
#include "ModumateCore/ModumateScriptProcessor.h"
#include "DocumentManagement/ModumateCommands.h"
#include "BIMKernel/AssemblySpec/BIMPartLayout.h"
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

FBIMPresetEditorNodeSharedPtr FBIMPresetEditor::CreateNodeInstanceFromPreset(const FBIMEditorNodeIDType& ParentID, const FGuid& PresetID, int32 ParentSetIndex, int32 ParentSetPosition)
{
	return CreateNodeInstanceFromPreset(ParentID, PresetID, ParentSetIndex, ParentSetPosition, FGuid());
}

FBIMPresetEditorNodeSharedPtr FBIMPresetEditor::CreateNodeInstanceFromPreset(const FBIMEditorNodeIDType& ParentID, const FGuid& PresetGUID, int32 ParentSetIndex, int32 ParentSetPosition, const FGuid& SlotAssignment)
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

	FBIMPresetEditorNodeSharedPtr parent = InstanceFromID(ParentID);

	if (ParentSetPosition == INDEX_NONE)
	{
		ParentSetPosition = 0;
		for (auto& child : parent->Preset.ChildPresets)
		{
			if (child.ParentPinSetIndex == ParentSetIndex)
			{
				ParentSetPosition = FMath::Max(ParentSetPosition,child.ParentPinSetPosition+1);
			}
		}
	}

	if (parent.IsValid())
	{
		// Presets are assigned either to a part slot or a pin, but not both
		if (!SlotAssignment.IsValid())
		{
			if (!parent->Preset.HasPin(ParentSetIndex, ParentSetPosition))
			{
				parent->Preset.AddChildPreset(PresetGUID, ParentSetIndex, ParentSetPosition);
			}
		}
		else
		{
			parent->Preset.SetPartPreset(SlotAssignment, PresetGUID);
		}
	}

	FBIMPresetEditorNodeSharedPtr returnVal = nullptr;
	TArray<FBIMPresetEditorNodeSharedPtr> createdInstances;

	// "Child" presets are layers, "Part" presets are parts of rigged assemblies
	// Iterate over children depth first, so all details of a child layer are processed before moving on
	// Iterate over parts breadth first, so all parts associated with a preset are processed in the order they appear in the assembly spec
	TArray<FPresetTreeIterator> childStack;
	TQueue<FPresetTreeIterator> partQueue;

	if (!SlotAssignment.IsValid())
	{
		childStack.Push(FPresetTreeIterator(ParentID, PresetGUID, ParentSetIndex, ParentSetPosition));
	}
	else
	{
		partQueue.Enqueue(FPresetTreeIterator(ParentID, PresetGUID, SlotAssignment));
	}

	while (childStack.Num() > 0 || !partQueue.IsEmpty())
	{
		FPresetTreeIterator iterator;
		
		// Process all parts before moving on to next child (which may or may not have parts)
		if (!partQueue.IsEmpty())
		{
			partQueue.Dequeue(iterator);
		}
		else
		{
			iterator = childStack.Pop();
		}

		const FBIMPresetInstance* preset = PresetCollectionProxy.PresetFromGUID(iterator.PresetID);
		if (!ensureAlways(preset != nullptr))
		{
			return nullptr;
		}

		FBIMEditorNodeIDType nodeID = *FString::Printf(TEXT("NODE-%02d"), NextInstanceID);
		auto* fben = new FBIMPresetEditorNode(nodeID,iterator.ParentSetIndex,iterator.ParentSetPosition,*preset, PresetCollectionProxy.GetVDPTable());
		FBIMPresetEditorNodeSharedPtr instance = InstancePool.Add_GetRef(MakeShareable(fben));
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
				childStack.Push(FPresetTreeIterator(instance->GetInstanceID(), childPreset.PresetGUID, childPreset.ParentPinSetIndex, childPreset.ParentPinSetPosition));
			}
		}

		for (const auto& partPreset: preset->PartSlots)
		{
			if (partPreset.PartPresetGUID.IsValid())
			{
				partQueue.Enqueue(FPresetTreeIterator(instance->GetInstanceID(), partPreset.PartPresetGUID, partPreset.SlotPresetGUID));
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

EBIMResult FBIMPresetEditor::CreateAssemblyFromNodes(FBIMAssemblySpec& OutAssemblySpec)
{
	FBIMPresetEditorNodeSharedPtr rootNode;
	for (const auto &inst : InstancePool)
	{
		PresetCollectionProxy.OverridePreset(inst->Preset);
		if (inst->ParentInstance == nullptr)
		{
			rootNode = inst;
		}
	}

	if (rootNode.IsValid())
	{
		return OutAssemblySpec.FromPreset(PresetCollectionProxy, rootNode->Preset.GUID);
	}
	return EBIMResult::Error;
}

/*
Build a layered assembly based on a single layer, used by the icon generator to produce layer previews
*/
EBIMResult FBIMPresetEditor::CreateAssemblyFromLayerNode(const FBIMEditorNodeIDType& LayerNodeID, FBIMAssemblySpec& OutAssemblySpec)
{
	// Add layer node preset
	FBIMPresetEditorNodeSharedPtr layerNode = InstanceFromID(LayerNodeID);
	if (!layerNode.IsValid())
	{
		return EBIMResult::Error;
	}

	// Build a temporary top-level assembly node to host the single layer
	FBIMPresetInstance assemblyPreset;
	assemblyPreset.GUID= FGuid::NewGuid();
	assemblyPreset.NodeScope = EBIMValueScope::Assembly;

	// Give the temporary assembly a single layer child
	FBIMPresetPinAttachment& attachment = assemblyPreset.ChildPresets.AddDefaulted_GetRef();
	attachment.ParentPinSetIndex = 0;
	attachment.ParentPinSetPosition = 0;
	attachment.PresetGUID = layerNode->Preset.GUID;

	// Fetch the root node to get node and object type information for the temp assembly
	for (const auto &inst : InstancePool)
	{
		if (inst->ParentInstance == nullptr)
		{
			assemblyPreset.ObjectType = inst->Preset.ObjectType;
			assemblyPreset.Properties.AddProperties(inst->Preset.Properties);
			break;
		}
	}
	PresetCollectionProxy.OverridePreset(assemblyPreset);
	
	return OutAssemblySpec.FromPreset(PresetCollectionProxy, assemblyPreset.GUID);
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

	for (auto& child : pinnedParent->Preset.ChildPresets)
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
	if (!ensureAlways(Instance.IsValid()))
	{
		return EBIMResult::Error;
	}

	FBIMPresetEditorNodeSharedPtr parent;
	if (Instance->ParentInstance.IsValid())
	{
		parent = InstanceFromID(Instance->ParentInstance.Pin()->GetInstanceID());
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

EBIMResult FBIMPresetEditor::InitFromPreset(const FGuid& PresetGUID, FBIMPresetEditorNodeSharedPtr& OutRootNode)
{
	ResetInstances();
	OutRootNode = CreateNodeInstanceFromPreset(BIM_ID_NONE, PresetGUID, 0, 0);

	if (!OutRootNode.IsValid())
	{
		return EBIMResult::Error;
	}

	SortAndValidate();

	// If we have part nodes, run a layout and collect the visible named dimensions for each part
	if (OutRootNode->PartNodes.Num() > 0)
	{
		FBIMAssemblySpec assemblySpec;
		if (CreateAssemblyFromNodes(assemblySpec) == EBIMResult::Success)
		{
			FBIMPartLayout layout;
			layout.FromAssembly(assemblySpec, FVector::OneVector);

			// Part slot instances, editor nodes and the assembly part list must corespond
			// This condition is checked in the automated test of the preset database
			if (ensureAlways(layout.PartSlotInstances.Num() == InstancePool.Num() && layout.PartSlotInstances.Num() == assemblySpec.Parts.Num()))
			{
				for (int32 i = 0; i < layout.PartSlotInstances.Num(); ++i)
				{
					const auto& part = assemblySpec.Parts[i];
					const auto& slot = layout.PartSlotInstances[i];
					if (ensureAlways(part.PresetGUID == slot.PresetGUID && part.SlotGUID == slot.SlotGUID))
					{
						auto inst = InstancePool[i];
						// Visible named dimensions are keys into the NamedDimensions map of the underlying preset
						inst->VisibleNamedDimensions = slot.VisibleNamedDimensions;
					}
					else
					{
						return EBIMResult::Error;
					}
				}
			}
			else
			{
				return EBIMResult::Error;
			}
		}
		else
		{
			return EBIMResult::Error;
		}
	}

	return EBIMResult::Success;
}

EBIMResult FBIMPresetEditor::SetNewPresetForNode(const FBIMEditorNodeIDType& InstanceID, const FGuid& PresetGUID)
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

	const FBIMPresetInstance* preset = PresetCollectionProxy.PresetFromGUID(PresetGUID);

	if (!ensureAlways(preset != nullptr))
	{
		return EBIMResult::Error;
	}

	if (inst->ParentInstance.IsValid())
	{
		if (!parentSlot.IsValid())
		{
			for (auto& sibling : inst->ParentInstance.Pin()->Preset.ChildPresets)
			{
				if (sibling.ParentPinSetIndex == inst->MyParentPinSetIndex && sibling.ParentPinSetPosition == inst->MyParentPinSetPosition)
				{
					sibling.PresetGUID = preset->GUID;
					break;
				}
			}
		}
		else
		{
			for (auto& part : inst->ParentInstance.Pin()->Preset.PartSlots)
			{
				if (part.SlotPresetGUID == parentSlot)
				{
					part.PartPresetGUID = preset->GUID;
					break;
				}
			}
		}
	}

	inst->Preset = *preset;

	for (auto& childPreset : preset->ChildPresets)
	{
		if (childPreset.PresetGUID.IsValid())
		{
			CreateNodeInstanceFromPreset(InstanceID, childPreset.PresetGUID, childPreset.ParentPinSetIndex, childPreset.ParentPinSetPosition);
		}
	}

	for (auto& partPreset : preset->PartSlots)
	{
		if (partPreset.PartPresetGUID.IsValid())
		{
			CreateNodeInstanceFromPreset(inst->GetInstanceID(), partPreset.PartPresetGUID, INDEX_NONE, INDEX_NONE, partPreset.SlotPresetGUID);
		}
	}

	SortAndValidate();

	return EBIMResult::Success;
}

EBIMResult FBIMPresetEditor::SetPartPreset(const FBIMEditorNodeIDType& ParentID, int32 SlotID, const FGuid& PartPreset)
{
	const FBIMPresetEditorNodeSharedPtr nodeParent = InstanceFromID(ParentID);

	if (!nodeParent.IsValid())
	{
		return EBIMResult::Error;
	}

	FGuid slotPreset = nodeParent->Preset.PartSlots[SlotID].SlotPresetGUID;

	// If we already have a part here, just swap the preset
	for (auto& partNode : nodeParent->PartNodes)
	{
		auto partNodePinned = partNode.Pin();
		if (partNodePinned->MyParentPartSlot == slotPreset)
		{
			SetNewPresetForNode(partNodePinned->GetInstanceID(), PartPreset);
			return EBIMResult::Success;
		}
	}

	// If we don't already have this part, create a new node
	if (CreateNodeInstanceFromPreset(ParentID, PartPreset, INDEX_NONE, INDEX_NONE, slotPreset).IsValid())
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

	FGuid slotPreset = nodeParent->Preset.PartSlots[SlotID].SlotPresetGUID;

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

/*
* Given a child node attached to its parent by a pin, determine which layer type the child is attached to (Default, Tread, Riser or Cabinet)
* Used to determine how parts or plane-hosted assemblies which can target multiple sections of a complex assembly are interpreted
* Used by the dynamic icon generator to isolate icons by category as presets do not inherently have a target
*/
EBIMResult FBIMPresetEditor::GetBIMPinTargetForChildNode(const FBIMEditorNodeIDType& ChildID, EBIMPinTarget& OutTarget) const
{
	const auto child = InstanceMap.Find(ChildID);
	if (child == nullptr)
	{
		return EBIMResult::Error;
	}

	// If we don't have a parent, we don't target anything
	auto childPtr = child->Pin();
	if (!childPtr->ParentInstance.IsValid())
	{
		OutTarget = EBIMPinTarget::Default;
		return EBIMResult::Success;
	}

	// Find myself in my parent's sibling list and get the target stored on that pin
	for (auto& sibling : childPtr->ParentInstance.Pin()->Preset.ChildPresets)
	{
		if (sibling.ParentPinSetIndex == childPtr->MyParentPinSetIndex && sibling.ParentPinSetPosition == childPtr->MyParentPinSetPosition)
		{
			OutTarget = sibling.Target;
			return EBIMResult::Success;
		}
	}

	return EBIMResult::Error;
}
