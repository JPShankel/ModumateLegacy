// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "BIMKernel/BIMNodeEditor.h"

#include "Database/ModumateObjectDatabase.h"
#include "Database/ModumateArchitecturalMaterial.h"
#include "ModumateCore/ModumateScriptProcessor.h"
#include "DocumentManagement/ModumateCommands.h"
#include "Algo/Accumulate.h"
#include "Algo/Transform.h"
#include "Algo/Compare.h"

#if WITH_EDITOR // for debugging new/delete balance
int32 FBIMCraftingTreeNode::InstanceCount = 0;
#endif

FBIMCraftingTreeNode::FBIMCraftingTreeNode(int32 instanceID) : InstanceID(instanceID)
{
#if WITH_EDITOR 
	++InstanceCount;
#endif
}

int32 FBIMCraftingTreeNode::GetInstanceID() const
{
	return InstanceID;
}

EBIMResult FBIMCraftingTreeNode::NodeIamEmbeddedIn(int32& OutNodeId) const
{
	OutNodeId = INDEX_NONE;
	if (AttachedChildren.Num() == 0 && ParentInstance.IsValid())
	{
		OutNodeId = ParentInstance.Pin()->GetInstanceID();
		return EBIMResult::Success;
	}
	return EBIMResult::Error;
}

EBIMResult FBIMCraftingTreeNode::NodesEmbeddedInMe(TArray<int32>& OutNodeIds) const
{
	OutNodeIds.Empty();
	for (const auto& pin : AttachedChildren)
	{
		for (const auto& childPin : pin.Children)
		{
			int32 embeddedInId;
			if (childPin.Pin()->NodeIamEmbeddedIn(embeddedInId) == EBIMResult::Success && embeddedInId == InstanceID)
			{
				OutNodeIds.Add(childPin.Pin()->GetInstanceID());
			}
		}
	}
	return OutNodeIds.Num() > 0 ? EBIMResult::Success : EBIMResult::Error;
}

EBIMResult FBIMCraftingTreeNode::GatherAllChildNodes(TArray<FBIMCraftingTreeNodeSharedPtr> &OutChildren)
{
	TArray<FBIMCraftingTreeNodeSharedPtr> nodeStack;
	nodeStack.Push(AsShared());
	do
	{
		FBIMCraftingTreeNodeSharedPtr currentNode = nodeStack.Pop();
		for (auto &pin : currentNode->AttachedChildren)
		{
			for (auto &ao : pin.Children)
			{
				if (ao != nullptr)
				{
					OutChildren.Add(ao.Pin());
					nodeStack.Push(ao.Pin());
				}
			}
		}
	} while (nodeStack.Num() > 0);

	return EBIMResult::Success;
}

// The preset on a crafting node is 'dirty' if the node's properties or input pins is inconsistent with the values in its preset
EBIMPresetEditorNodeStatus FBIMCraftingTreeNode::GetPresetStatus(const FBIMPresetCollection &PresetCollection) const
{
	const FBIMPresetInstance* preset = PresetCollection.Presets.Find(PresetID);
	if (!ensureAlways(preset != nullptr))
	{
		return EBIMPresetEditorNodeStatus::None;
	}

	FBIMPresetInstance instanceAsPreset;
	ToPreset(PresetCollection, instanceAsPreset);

	if (preset->Matches(instanceAsPreset))
	{
		return EBIMPresetEditorNodeStatus::UpToDate;
	}

	return EBIMPresetEditorNodeStatus::Dirty;
}

EBIMResult FBIMCraftingTreeNodePool::ResetInstances()
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

FBIMCraftingTreeNodeSharedPtr FBIMCraftingTreeNodePool::FindInstanceByPredicate(const std::function<bool(const FBIMCraftingTreeNodeSharedPtr &Instance)> &Predicate)
{
	FBIMCraftingTreeNodeSharedPtr *ret = InstancePool.FindByPredicate(Predicate);
	if (ret != nullptr)
	{
		return *ret;
	}
	return nullptr;
}

const FBIMCraftingTreeNodeSharedPtr FBIMCraftingTreeNodePool::InstanceFromID(int32 InstanceID) const
{
	const FBIMCraftingTreeNodeWeakPtr *instancePtr = InstanceMap.Find(InstanceID);
	if (instancePtr != nullptr && instancePtr->IsValid())
	{
		return instancePtr->Pin();
	}
	return FBIMCraftingTreeNodeSharedPtr();
}

FBIMCraftingTreeNodeSharedPtr FBIMCraftingTreeNodePool::InstanceFromID(int32 InstanceID)
{
	const FBIMCraftingTreeNodeWeakPtr *instancePtr = InstanceMap.Find(InstanceID);
	if (instancePtr != nullptr && instancePtr->IsValid())
	{
		return instancePtr->Pin();
	}
	return FBIMCraftingTreeNodeSharedPtr();
}

bool FBIMCraftingTreeNode::CanRemoveChild(const FBIMCraftingTreeNodeSharedPtrConst &Child) const
{
	for (auto &pin : AttachedChildren)
	{
		if (pin.Children.Contains(Child))
		{
			if (pin.Children.Num() <= pin.SetType.MinCount)
			{
				return false;
			}
			return true;
		}
	}
	return false;
}

bool FBIMCraftingTreeNode::CanReorderChild(const FBIMCraftingTreeNodeSharedPtrConst &Child) const
{
	for (auto& pin : AttachedChildren)
	{
		if (pin.Children.Contains(Child))
		{
			return pin.Children.Num() > 1;
		}
	}
	return false;
}

EBIMResult FBIMCraftingTreeNode::FindChild(int32 ChildID, int32 &OutPinSetIndex, int32 &OutPinSetPosition)
{
	for (int32 pinIndex = 0; pinIndex < AttachedChildren.Num(); ++pinIndex)
	{
		FAttachedChildGroup &pinSet = AttachedChildren[pinIndex];
		for (int32 pinPosition = 0; pinPosition < pinSet.Children.Num(); ++pinPosition)
		{
			if (pinSet.Children[pinPosition].Pin()->GetInstanceID() == ChildID)
			{
				OutPinSetIndex = pinIndex;
				OutPinSetPosition = pinPosition;
				return EBIMResult::Success;
			}
		}
	}
	return EBIMResult::Error;
}

EBIMResult FBIMCraftingTreeNode::FindOtherChildrenOnPin(TArray<int32> &OutChildIDs)
{
	if (ParentInstance != nullptr)
	{
		for (int32 pinIndex = 0; pinIndex < ParentInstance.Pin()->AttachedChildren.Num(); ++pinIndex)
		{
			FAttachedChildGroup &pinSet = ParentInstance.Pin()->AttachedChildren[pinIndex];
			for (int32 pinPosition = 0; pinPosition < pinSet.Children.Num(); ++pinPosition)
			{
				if (pinSet.Children[pinPosition].Pin()->GetInstanceID() == InstanceID)
				{
					for (const auto& curChild : pinSet.Children)
					{
						OutChildIDs.Add(curChild.Pin()->GetInstanceID());
					}
					return EBIMResult::Success;
				}
			}
		}
	}
	return EBIMResult::Error;
}

EBIMResult FBIMCraftingTreeNode::GatherChildrenInOrder(TArray<int32> &OutChildIDs)
{
	for (auto& pin : AttachedChildren)
	{
		for (auto& child : pin.Children)
		{
			OutChildIDs.Add(child.Pin()->GetInstanceID());
		}
	}
	return EBIMResult::Success;
}

EBIMResult FBIMCraftingTreeNode::DetachSelfFromParent()
{
	if (ParentInstance != nullptr)
	{
		for (auto &pin : ParentInstance.Pin()->AttachedChildren)
		{
			for (auto ao : pin.Children)
			{
				if (ao.HasSameObject(this))
				{
					pin.Children.Remove(ao);
					break;
				}
			}
		}
	}
	ParentInstance = nullptr;
	return EBIMResult::Success;
}

EBIMResult FBIMCraftingTreeNode::AttachPartChild(const FBIMPresetCollection &PresetCollection, const FBIMCraftingTreeNodeSharedPtr &Child, const FName& PartName)
{
	FAttachedChildGroup& childGroup = AttachedChildren.AddDefaulted_GetRef();
	childGroup.Children.Add(Child);
	childGroup.SetType.MaxCount = 1;
	childGroup.SetType.MinCount = 0;
	childGroup.SetType.SetName = PartName;
	childGroup.PartSlot.PartPreset = Child->PresetID;
	return EBIMResult::Success;
}

EBIMResult FBIMCraftingTreeNode::AttachChildAt(const FBIMPresetCollection &PresetCollection, const FBIMCraftingTreeNodeSharedPtr &Child, int32 PinSetIndex, int32 PinSetPosition)
{
	if (AttachedChildren.Num() <= PinSetIndex)
	{
		AttachedChildren.SetNum(PinSetIndex+1);
	}
	if (AttachedChildren[PinSetIndex].Children.Num() <= PinSetPosition)
	{
		AttachedChildren[PinSetIndex].Children.SetNum(PinSetPosition + 1);
	}
	AttachedChildren[PinSetIndex].Children[PinSetPosition] = Child;
	return EBIMResult::Success;
}

EBIMResult FBIMCraftingTreeNode::AttachChild(const FBIMPresetCollection &PresetCollection, const FBIMCraftingTreeNodeSharedPtr &Child)
{
	if (AttachedChildren.Num() == 0)
	{
		AttachedChildren.AddDefaulted();
	}

	AttachedChildren[0].Children.Add(Child);

	return EBIMResult::Success;
}

EBIMResult FBIMCraftingTreeNode::ToPreset(const FBIMPresetCollection& PresetCollection, FBIMPresetInstance& OutPreset) const
{
	const FBIMPresetInstance* basePreset = PresetCollection.Presets.Find(PresetID);
	if (basePreset == nullptr)
	{
		return EBIMResult::Error;
	}

	OutPreset.NodeScope = basePreset->NodeScope;
	OutPreset.NodeType = basePreset->NodeType;
	OutPreset.SetProperties(InstanceProperties);
	OutPreset.PresetID = basePreset->PresetID;
	OutPreset.MyTagPath = basePreset->MyTagPath;
	OutPreset.ParentTagPaths = basePreset->ParentTagPaths;
	OutPreset.ObjectType = basePreset->ObjectType;
	OutPreset.IconType = basePreset->IconType;
	OutPreset.Orientation = basePreset->Orientation;

	for (int32 pinSetIndex = 0; pinSetIndex < AttachedChildren.Num(); ++pinSetIndex)
	{
		const FBIMCraftingTreeNode::FAttachedChildGroup& pinSet = AttachedChildren[pinSetIndex];
		if (pinSet.IsPart())
		{
			FBIMPresetPartSlot& partSlot = OutPreset.PartSlots.AddDefaulted_GetRef();
			partSlot.PartPreset = pinSet.PartSlot.PartPreset;
			partSlot.SlotName = FBIMKey(pinSet.SetType.SetName.ToString());
		}
		else
		{
			for (int32 pinSetPosition = 0; pinSetPosition < pinSet.Children.Num(); ++pinSetPosition)
			{
				FBIMPresetPinAttachment& pinSpec = OutPreset.ChildPresets.AddDefaulted_GetRef();
				pinSpec.ParentPinSetIndex = pinSetIndex;
				pinSpec.ParentPinSetPosition = pinSetPosition;

				FBIMCraftingTreeNodeSharedPtr attachedOb = pinSet.Children[pinSetPosition].Pin();
				pinSpec.PresetID = attachedOb->PresetID;
			}
		}
	}
	OutPreset.SortChildNodes();
	return EBIMResult::Success;
}

EBIMResult FBIMCraftingTreeNodePool::DestroyNodeInstance(const FBIMCraftingTreeNodeSharedPtr &Instance, TArray<int32> &OutDestroyed)
{
	if (!ensureAlways(Instance != nullptr))
	{
		return EBIMResult::Error;
	}
	TArray<FBIMCraftingTreeNodeSharedPtr> childNodes;
	Instance->GatherAllChildNodes(childNodes);
	OutDestroyed.Add(Instance->GetInstanceID());
	Instance->DetachSelfFromParent();
	InstanceMap.Remove(Instance->GetInstanceID());

	InstancePool.Remove(Instance);
	for (auto &inst : childNodes)
	{
		OutDestroyed.Add(inst->GetInstanceID());
		InstanceMap.Remove(inst->GetInstanceID());
		InstancePool.Remove(inst);
	}
	ValidatePool();
	return EBIMResult::Success;
}

EBIMResult FBIMCraftingTreeNodePool::DestroyNodeInstance(int32 InstanceID, TArray<int32> &OutDestroyed)
{
	return DestroyNodeInstance(InstanceFromID(InstanceID), OutDestroyed);
}


EBIMResult FBIMCraftingTreeNodePool::InitFromPreset(const FBIMPresetCollection& PresetCollection, const FBIMKey& PresetID, FBIMCraftingTreeNodeSharedPtr& OutRootNode)
{
	ResetInstances();
	OutRootNode = CreateNodeInstanceFromPreset(PresetCollection, 0, PresetID, 0, 0);
	return OutRootNode.IsValid() ? EBIMResult::Success : EBIMResult::Error;
}

EBIMResult FBIMCraftingTreeNodePool::SetNewPresetForNode(const FBIMPresetCollection &PresetCollection,int32 InstanceID,const FBIMKey& PresetID)
{
	FBIMCraftingTreeNodeSharedPtr inst = InstanceFromID(InstanceID);

	if (!ensureAlways(inst != nullptr))
	{
		return EBIMResult::Error;
	}

	TArray<int32> toDestroy;
	for (auto& ac : inst->AttachedChildren)
	{
		for (auto& child : ac.Children)
		{
			toDestroy.Add(child.Pin()->GetInstanceID());
		}
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

	inst->FromPreset(PresetCollection, *preset);

	for (auto& childPreset : preset->ChildPresets)
	{
		CreateNodeInstanceFromPreset(PresetCollection, inst->GetInstanceID(), childPreset.PresetID, childPreset.ParentPinSetIndex, childPreset.ParentPinSetPosition);
	}

	for (auto& partPreset : preset->PartSlots)
	{
		CreateNodeInstanceFromPreset(PresetCollection, inst->GetInstanceID(), partPreset.PartPreset, -1, -1, partPreset.SlotName);
	}

	return EBIMResult::Success;
}

EBIMResult FBIMCraftingTreeNode::FromPreset(const FBIMPresetCollection& PresetCollection,const FBIMPresetInstance& InPreset)
{
	const FBIMPresetTypeDefinition* descriptor = PresetCollection.NodeDescriptors.Find(InPreset.NodeType);
	if (!ensureAlways(descriptor != nullptr))
	{
		return EBIMResult::Error;
	}

	AttachedChildren.SetNum(descriptor->PinSets.Num());

	for (int32 i = 0; i < AttachedChildren.Num(); ++i)
	{
		AttachedChildren[i].SetType.MinCount = descriptor->PinSets[i].MinCount;
		AttachedChildren[i].SetType.MaxCount = descriptor->PinSets[i].MaxCount;
	}

	CategoryTitle = InPreset.CategoryTitle;
	PresetID = InPreset.PresetID;
	CurrentOrientation = InPreset.Orientation;
	InstanceProperties = InPreset.Properties;	
	
	return EBIMResult::Success;
}

FBIMCraftingTreeNodeSharedPtr FBIMCraftingTreeNodePool::CreateNodeInstanceFromPreset(const FBIMPresetCollection& PresetCollection, int32 ParentID, const FBIMKey& PresetID, int32 ParentSetIndex, int32 ParentSetPosition, const FBIMKey& SlotAssignment)
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
			ParentSetIndex = -1;
			ParentSetPosition = -1;
		}

		FPresetTreeIterator(int32 InParentNode, const FBIMKey& InPresetID, int32 InParentSetIndex, int32 InParentSetPosition, const FBIMKey& InSlot) : 
			ParentNode(InParentNode), PresetID(InPresetID), SlotAssignment(InSlot), ParentSetIndex(InParentSetIndex), ParentSetPosition(InParentSetPosition)
		{}
	};

	TArray<FPresetTreeIterator> iteratorStack;

	iteratorStack.Push(FPresetTreeIterator(ParentID, PresetID, ParentSetIndex, ParentSetPosition, SlotAssignment));

	FBIMCraftingTreeNodeSharedPtr returnVal = nullptr;

	while (iteratorStack.Num() > 0)
	{
		FPresetTreeIterator iterator = iteratorStack.Pop();

		const FBIMPresetInstance* preset = PresetCollection.Presets.Find(iterator.PresetID);
		if (!ensureAlways(preset != nullptr))
		{
			return nullptr;
		}

		FBIMCraftingTreeNodeSharedPtr instance = InstancePool.Add_GetRef(MakeShareable(new FBIMCraftingTreeNode(NextInstanceID)));
		InstanceMap.Add(NextInstanceID, instance);
		++NextInstanceID;

		instance->FromPreset(PresetCollection,*preset);

		FBIMCraftingTreeNodeSharedPtr parent = InstanceFromID(iterator.ParentNode);
		if (parent != nullptr)
		{
			instance->ParentInstance = parent;

			if (iterator.SlotAssignment.IsNone())
			{
				ensureAlways(parent->AttachChildAt(PresetCollection, instance, iterator.ParentSetIndex, iterator.ParentSetPosition) != EBIMResult::Error);
			}
			else
			{
				ensureAlways(parent->AttachPartChild(PresetCollection, instance, *iterator.SlotAssignment.ToString()) != EBIMResult::Error);
			}
		}

		if (returnVal == nullptr)
		{
			returnVal = instance;
		}

		for (auto& childPreset : preset->ChildPresets)
		{
			iteratorStack.Push(FPresetTreeIterator(instance->GetInstanceID(), childPreset.PresetID, childPreset.ParentPinSetIndex, childPreset.ParentPinSetPosition, FBIMKey()));
		}

		for (auto& partPreset : preset->PartSlots)
		{
			iteratorStack.Push(FPresetTreeIterator(instance->GetInstanceID(), partPreset.PartPreset, partPreset.SlotName));
		}
	}

	ValidatePool();
	return returnVal;
}

bool FBIMCraftingTreeNodePool::ValidatePool() const
{
	bool ret = true;

#if WITH_EDITOR
	auto myChildrenPointToMe = [](const FBIMCraftingTreeNodeSharedPtr &me)
	{
		bool ret = true;
		for (auto &op : me->AttachedChildren)
		{
			for (auto &oa : op.Children)
			{
				ret = ensureAlways(oa.Pin()->ParentInstance.HasSameObject(me.Get())) && ret;
			}
		}
		return ret;
	};

	auto myParentPointsToMe = [](const FBIMCraftingTreeNodeSharedPtr &me)
	{
		if (me->ParentInstance == nullptr)
		{
			return true;
		}
		for (auto &op : me->ParentInstance.Pin()->AttachedChildren)
		{
			for (auto &oa : op.Children)
			{
				if (oa.HasSameObject(me.Get()))
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
	}

	ensureAlways(ret);
#endif
	return ret;
}

EBIMResult FBIMCraftingTreeNodePool::CreateAssemblyFromNodes(const FBIMPresetCollection& PresetCollection, const FModumateDatabase& InDB, FBIMAssemblySpec& OutAssemblySpec)
{
	EBIMResult result = EBIMResult::Error;
	// Get root node
	FBIMCraftingTreeNodeSharedPtr rootNode;
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
			if (instance->GetPresetStatus(PresetCollection) == EBIMPresetEditorNodeStatus::Dirty)
			{
				FBIMPresetInstance dirtyPreset;
				if (ensureAlways(instance->ToPreset(PresetCollection, dirtyPreset) != EBIMResult::Error))
				{
					previewCollection.Presets.Add(dirtyPreset.PresetID, dirtyPreset);
				}
			}
			else
			{
				const FBIMPresetInstance* original = PresetCollection.Presets.Find(instance->PresetID);
				if (ensureAlways(original != nullptr) && !previewCollection.Presets.Contains(original->PresetID))
				{
					previewCollection.Presets.Add(original->PresetID, *original);
				}
			}
			PresetCollection.GetDependentPresets(instance->PresetID, dependentPresets);
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

		result = OutAssemblySpec.FromPreset(InDB, previewCollection, rootNode->PresetID);
	}
	return result;
}

/*
Build a layered assembly based on a single layer, used by the icon generator to produce layer previews
*/
EBIMResult FBIMCraftingTreeNodePool::CreateAssemblyFromLayerNode(const FBIMPresetCollection& PresetCollection, const FModumateDatabase& InDB, int32 LayerNodeID, FBIMAssemblySpec& OutAssemblySpec)
{
	// Because FBIMAssemblySpec::FromPreset must have an assembly scope, we're 'borrowing' the preset from root node, then presets from layer node 
	FBIMPresetCollection previewCollection;

	// Add layer node preset
	FBIMCraftingTreeNodeSharedPtr layerNode = InstanceFromID(LayerNodeID);
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
	attachment.PresetID = layerNode->PresetID;

	// Fetch the root node to get node and object type information for the temp assembly
	for (const auto &inst : InstancePool)
	{
		if (inst->ParentInstance == nullptr)
		{
			const FBIMPresetInstance* rootPreset = PresetCollection.Presets.Find(inst->PresetID);
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

	const FBIMPresetInstance* layerPreset = PresetCollection.Presets.Find(layerNode->PresetID);
	if (ensureAlways(layerPreset != nullptr))
	{
		previewCollection.Presets.Add(layerPreset->PresetID, *layerPreset);
	}
	else
	{
		return EBIMResult::Error;
	}

	// Add presets from children of the layer node 
	TArray<FBIMCraftingTreeNodeSharedPtr> childrenNodes;
	layerNode->GatherAllChildNodes(childrenNodes);
	for (auto& child : childrenNodes)
	{
		if (child->GetPresetStatus(PresetCollection) == EBIMPresetEditorNodeStatus::Dirty)
		{
			FBIMPresetInstance dirtyPreset;
			if (ensureAlways(child->ToPreset(PresetCollection, dirtyPreset) != EBIMResult::Error))
			{
				previewCollection.Presets.Add(dirtyPreset.PresetID, dirtyPreset);
			}
		}
		else
		{
			const FBIMPresetInstance* original = PresetCollection.Presets.Find(child->PresetID);
			if (ensureAlways(original != nullptr))
			{
				previewCollection.Presets.Add(original->PresetID, *original);
			}
		}
	}
	
	return OutAssemblySpec.FromPreset(InDB, previewCollection, assemblyPreset.PresetID);
}

EBIMResult FBIMCraftingTreeNodePool::ReorderChildNode(int32 ChildNode, int32 FromPosition, int32 ToPosition)
{
	FBIMCraftingTreeNodeSharedPtr childPtr = InstanceFromID(ChildNode);

	if (!childPtr.IsValid())
	{
		return EBIMResult::Error;
	}

	if (!childPtr->ParentInstance.IsValid())
	{
		return EBIMResult::Error;
	}

	TArray<FBIMCraftingTreeNode::FAttachedChildGroup>& attachedChildren = childPtr->ParentInstance.Pin()->AttachedChildren;
	for (auto& attachment : attachedChildren)
	{
		if (attachment.Children.Contains(childPtr))
		{
			attachment.Children.RemoveAt(FromPosition);
			attachment.Children.EmplaceAt(ToPosition, childPtr);
			return EBIMResult::Success;
		}
	}

	return EBIMResult::Error;
}

bool FBIMCraftingTreeNodePool::GetSortedNodeIDs(TArray<int32> &OutNodeIDs)
{
	// Use the root node as starting point
	FBIMCraftingTreeNodeSharedPtr startInst;
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
			FBIMCraftingTreeNodeSharedPtr parentInst = startInst->ParentInstance.Pin();
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