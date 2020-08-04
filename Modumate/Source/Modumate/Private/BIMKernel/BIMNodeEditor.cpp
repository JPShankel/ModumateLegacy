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

ECraftingResult FBIMCraftingTreeNode::GatherAllChildNodes(TArray<FBIMCraftingTreeNodeSharedPtr> &OutChildren)
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

	return ECraftingResult::Success;
}

// The preset on a crafting node is 'dirty' if the node's properties or input pins is inconsistent with the values in its preset
ECraftingNodePresetStatus FBIMCraftingTreeNode::GetPresetStatus(const FBIMPresetCollection &PresetCollection) const
{
	const FBIMPreset *preset = PresetCollection.Presets.Find(PresetID);
	if (preset == nullptr)
	{
		return ECraftingNodePresetStatus::None;
	}

	FBIMPreset instanceAsPreset;
	ToPreset(PresetCollection, instanceAsPreset);

	if (preset->Matches(instanceAsPreset))
	{
		return ECraftingNodePresetStatus::UpToDate;
	}
	return ECraftingNodePresetStatus::Dirty;
}

ECraftingResult FBIMCraftingTreeNodePool::ResetInstances()
{
	InstancePool.Empty();
	InstanceMap.Empty();
	NextInstanceID = 1;
	if (!ValidatePool())
	{
		return ECraftingResult::Error;
	}
	return ECraftingResult::Success;
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

ECraftingResult FBIMCraftingTreeNodePool::PresetToSpec(const FName &PresetID, const FBIMPresetCollection &PresetCollection, FBIMAssemblySpec &OutAssemblySpec) const
{
	const FBIMCraftingTreeNodeSharedPtr *presetNode = GetInstancePool().FindByPredicate(
		[PresetID](const FBIMCraftingTreeNodeSharedPtr &Instance)
	{
		return Instance->PresetID == PresetID;
	}
	);

	if (!ensureAlways(presetNode != nullptr))
	{
		return ECraftingResult::Error;
	}

	OutAssemblySpec.RootPreset = PresetID;
	FBIMPropertySheet *currentSheet = &OutAssemblySpec.RootProperties;

	TArray<FBIMCraftingTreeNodeSharedPtr> nodeStack;
	nodeStack.Push(*presetNode);

	TArray<EBIMValueScope> scopeStack;
	scopeStack.Push(EBIMValueScope::Assembly);

	while (!nodeStack.Num() == 0)
	{
		FBIMCraftingTreeNodeSharedPtr inst = nodeStack.Pop();
		EBIMValueScope pinScope = scopeStack.Pop();

		const FBIMPreset *preset = PresetCollection.Presets.Find(inst->PresetID);
		if (ensureAlways(preset != nullptr))
		{
			const FCraftingTreeNodeType *nodeType = PresetCollection.NodeDescriptors.Find(preset->NodeType);
			if (ensureAlways(nodeType != nullptr))
			{
				if (nodeType->Scope == EBIMValueScope::Layer)
				{
					OutAssemblySpec.LayerProperties.AddDefaulted();
					currentSheet = &OutAssemblySpec.LayerProperties.Last();
				}
				if (nodeType->Scope != EBIMValueScope::None)
				{
					pinScope = nodeType->Scope;
				}
			}
		}

		EObjectType objectType = PresetCollection.GetPresetObjectType(inst->PresetID);
		if (objectType != EObjectType::OTNone && ensureAlways(OutAssemblySpec.ObjectType == EObjectType::OTNone))
		{
			OutAssemblySpec.ObjectType = objectType;
		}

		inst->InstanceProperties.ForEachProperty([&OutAssemblySpec, &currentSheet, &inst, pinScope, PresetID](const FString &Name, const Modumate::FModumateCommandParameter &Param)
		{
			FBIMPropertyValue vs(*Name);

			// 'Node' scope values inherit their scope from their parents, specified on the pin
			EBIMValueScope nodeScope = vs.Scope == EBIMValueScope::Node ? pinScope : vs.Scope;

			// Preset scope properties only apply to the root node in the tree
			if (nodeScope != EBIMValueScope::Preset || inst->PresetID == PresetID)
			{
				currentSheet->SetProperty(nodeScope, vs.Name, Param);
			}
		});

		for (auto &inputPin : inst->AttachedChildren)
		{
			for (auto &ao : inputPin.Children)
			{
				nodeStack.Push(ao.Pin());
				scopeStack.Push(pinScope);
			}
		}
	}

	// All assembly specs must bind to an object type
	if (OutAssemblySpec.ObjectType == EObjectType::OTNone)
	{
		return ECraftingResult::Error;
	}

	return ECraftingResult::Success;
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

ECraftingResult FBIMCraftingTreeNode::FindChildOrder(int32 ChildID, int32 &Order)
{
	int32 pinSetIndex = -1, pinSetPosition = -1;
	ECraftingResult result = FindChild(ChildID, pinSetIndex, pinSetPosition);
	if (result == ECraftingResult::Success)
	{
		Order = 0;
		for (int32 i = 0; i < pinSetIndex - 1; ++i)
		{
			Order += AttachedChildren[i].Children.Num();
		}
		Order += pinSetPosition;
	}
	return result;
}

ECraftingResult FBIMCraftingTreeNode::FindChild(int32 ChildID, int32 &OutPinSetIndex, int32 &OutPinSetPosition)
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
				return ECraftingResult::Success;
			}
		}
	}
	return ECraftingResult::Error;
}

ECraftingResult FBIMCraftingTreeNode::DetachSelfFromParent()
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
	return ECraftingResult::Success;
}

ECraftingResult FBIMCraftingTreeNode::AttachChildAt(const FBIMPresetCollection &PresetCollection, const FBIMCraftingTreeNodeSharedPtr &Child, int32 PinSetIndex, int32 PinSetPosition)
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
	return ECraftingResult::Success;
}

ECraftingResult FBIMCraftingTreeNode::AttachChild(const FBIMPresetCollection &PresetCollection, const FBIMCraftingTreeNodeSharedPtr &Child)
{
	if (AttachedChildren.Num() == 0)
	{
		AttachedChildren.AddDefaulted();
	}

	AttachedChildren[0].Children.Add(Child);

	return ECraftingResult::Success;
}

ECraftingResult FBIMCraftingTreeNode::ToDataRecord(FCustomAssemblyCraftingNodeRecord &OutRecord) const
{
	OutRecord.InstanceID = InstanceID;
	OutRecord.PresetID = PresetID;

	if (ParentInstance.IsValid())
	{
		FBIMCraftingTreeNodeSharedPtr sharedParent = ParentInstance.Pin();
		OutRecord.ParentID = sharedParent->GetInstanceID();
		ensureAlways(sharedParent->FindChild(InstanceID, OutRecord.PinSetIndex, OutRecord.PinSetPosition) != ECraftingResult::Error);
	}
	else
	{
		OutRecord.PinSetIndex = 0;
		OutRecord.PinSetPosition = 0;
		OutRecord.ParentID = 0;
	}

	InstanceProperties.ToDataRecord(OutRecord.PropertyRecord);

	return ECraftingResult::Success;
}

ECraftingResult FBIMCraftingTreeNode::FromDataRecord(
	FBIMCraftingTreeNodePool &InstancePool,
	const FBIMPresetCollection &PresetCollection,
	const FCustomAssemblyCraftingNodeRecord &DataRecord,
	bool RecursePresets)
{
	const FBIMPreset *preset = PresetCollection.Presets.Find(DataRecord.PresetID);
	if (preset == nullptr)
	{
		return ECraftingResult::Error;
	}

	const FCraftingTreeNodeType *descriptor = PresetCollection.NodeDescriptors.Find(preset->NodeType);
	if (descriptor == nullptr)
	{
		return ECraftingResult::Error;
	}

	// DestroyNodeInstance removes grand children, so we don't need to recurse
	for (auto &pin : AttachedChildren)
	{
		while (pin.Children.Num() > 0)
		{
			TArray<int32> destroyedChildren;
			InstancePool.DestroyNodeInstance(pin.Children[0].Pin()->GetInstanceID(), destroyedChildren);
		}
	}

	PresetID = DataRecord.PresetID;
	InstanceProperties.FromDataRecord(DataRecord.PropertyRecord);

	for (auto &ip : preset->ChildPresets)
	{
		if (ensureAlways(ip.ParentPinSetIndex < AttachedChildren.Num()))
		{
			// The first designated list in a sequence is assigned to the top of the pinset, not the last
			// This ensures that categories will be chosen from a list of categories, not the user preset at the end of the sequence
			FAttachedChildGroup &pinSet = AttachedChildren[ip.ParentPinSetIndex];
		}
	}

	if (!RecursePresets)
	{
		FBIMCraftingTreeNodeSharedPtr parent = InstancePool.InstanceFromID(DataRecord.ParentID);
		if (parent.IsValid())
		{
			parent->AttachChildAt(PresetCollection, AsShared(), DataRecord.PinSetIndex, DataRecord.PinSetPosition);
		}
	}
	else
	{
		for (auto &ip : preset->ChildPresets)
		{
			if (ensureAlways(ip.ParentPinSetIndex < AttachedChildren.Num()))
			{
				FAttachedChildGroup &pinSet = AttachedChildren[ip.ParentPinSetIndex];
				if (ip.ParentPinSetPosition == pinSet.Children.Num())
				{
					InstancePool.CreateNodeInstanceFromPreset(PresetCollection, DataRecord.InstanceID, ip.PresetID, ip.ParentPinSetIndex,ip.ParentPinSetPosition);
				}
				else
				{
					InstancePool.SetNewPresetForNode(PresetCollection, pinSet.Children[ip.ParentPinSetPosition].Pin()->GetInstanceID(), ip.PresetID);
				}
			}
		}
	}
	return ECraftingResult::Success;
}

ECraftingResult FBIMCraftingTreeNode::ToPreset(const FBIMPresetCollection& PresetCollection, FBIMPreset& OutPreset) const
{
	const FBIMPreset* basePreset = PresetCollection.Presets.Find(PresetID);
	if (basePreset == nullptr)
	{
		return ECraftingResult::Error;
	}

	OutPreset.NodeType = basePreset->NodeType;
	OutPreset.Properties = InstanceProperties;
	OutPreset.PresetID = basePreset->PresetID;
	OutPreset.MyTagPath = basePreset->MyTagPath;
	OutPreset.ParentTagPaths = basePreset->ParentTagPaths;
	OutPreset.ObjectType = basePreset->ObjectType;
	OutPreset.IconType = basePreset->IconType;
	OutPreset.Orientation = basePreset->Orientation;
	OutPreset.CanFlipOrientation = basePreset->CanFlipOrientation;

	for (int32 pinSetIndex = 0; pinSetIndex < AttachedChildren.Num(); ++pinSetIndex)
	{
		const FBIMCraftingTreeNode::FAttachedChildGroup& pinSet = AttachedChildren[pinSetIndex];
		for (int32 pinSetPosition = 0; pinSetPosition < pinSet.Children.Num(); ++pinSetPosition)
		{
			FBIMPreset::FChildAttachment& pinSpec = OutPreset.ChildPresets.AddDefaulted_GetRef();
			pinSpec.ParentPinSetIndex = pinSetIndex;
			pinSpec.ParentPinSetPosition = pinSetPosition;

			FBIMCraftingTreeNodeSharedPtr attachedOb = pinSet.Children[pinSetPosition].Pin();
			pinSpec.PresetID = attachedOb->PresetID;
			const FBIMPreset* attachedPreset = PresetCollection.Presets.Find(attachedOb->PresetID);
		}
	}
	OutPreset.SortChildNodes();
	return ECraftingResult::Success;
}

ECraftingResult FBIMCraftingTreeNodePool::DestroyNodeInstance(const FBIMCraftingTreeNodeSharedPtr &Instance, TArray<int32> &OutDestroyed)
{
	if (!ensureAlways(Instance != nullptr))
	{
		return ECraftingResult::Error;
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
	return ECraftingResult::Success;
}

ECraftingResult FBIMCraftingTreeNodePool::DestroyNodeInstance(int32 InstanceID, TArray<int32> &OutDestroyed)
{
	return DestroyNodeInstance(InstanceFromID(InstanceID), OutDestroyed);
}

FBIMCraftingTreeNodeSharedPtr FBIMCraftingTreeNodePool::CreateNodeInstanceFromDataRecord(const FBIMPresetCollection &PresetCollection, const FCustomAssemblyCraftingNodeRecord &DataRecord, bool RecursePresets)
{
	FBIMCraftingTreeNodeSharedPtr instance = InstancePool.Add_GetRef(MakeShareable(new FBIMCraftingTreeNode(DataRecord.InstanceID)));

	InstanceMap.Add(DataRecord.InstanceID, instance);
	NextInstanceID = FMath::Max(NextInstanceID + 1, DataRecord.InstanceID + 1);

	instance->FromDataRecord(*this, PresetCollection, DataRecord, RecursePresets);

	return instance;
}

ECraftingResult FBIMCraftingTreeNodePool::InitFromPreset(const FBIMPresetCollection& PresetCollection, const FName& PresetID, FBIMCraftingTreeNodeSharedPtr& OutRootNode)
{
	ResetInstances();
	OutRootNode = CreateNodeInstanceFromPreset(PresetCollection, 0, PresetID, 0, 0);
	return OutRootNode.IsValid() ? ECraftingResult::Success : ECraftingResult::Error;
}

ECraftingResult FBIMCraftingTreeNodePool::SetNewPresetForNode(const FBIMPresetCollection &PresetCollection,int32 InstanceID,const FName &PresetID)
{
	FBIMCraftingTreeNodeSharedPtr inst = InstanceFromID(InstanceID);

	if (!ensureAlways(inst != nullptr))
	{
		return ECraftingResult::Error;
	}

	const FBIMPreset *preset = PresetCollection.Presets.Find(PresetID);

	if (!ensureAlways(preset != nullptr))
	{
		return ECraftingResult::Error;
	}

	inst->PresetID = PresetID;
	inst->InstanceProperties = preset->Properties;

	FCustomAssemblyCraftingNodeRecord dataRecord;

	ECraftingResult result = inst->ToDataRecord(dataRecord);

	if (result != ECraftingResult::Success)
	{
		return result;
	}

	return inst->FromDataRecord(*this, PresetCollection, dataRecord, true);
}

FBIMCraftingTreeNodeSharedPtr FBIMCraftingTreeNodePool::CreateNodeInstanceFromPreset(const FBIMPresetCollection& PresetCollection, int32 ParentID, const FName& PresetID, int32 ParentSetIndex, int32 ParentSetPosition)
{
	const FBIMPreset* preset = PresetCollection.Presets.Find(PresetID);
	if (preset == nullptr)
	{
		return nullptr;
	}

	const FCraftingTreeNodeType* descriptor = PresetCollection.NodeDescriptors.Find(preset->NodeType);
	if (descriptor == nullptr)
	{
		return nullptr;
	}

	FBIMCraftingTreeNodeSharedPtr instance = InstancePool.Add_GetRef(MakeShareable(new FBIMCraftingTreeNode(NextInstanceID)));
	InstanceMap.Add(NextInstanceID, instance);
	++NextInstanceID;

	instance->PresetID = preset->PresetID;
	instance->CurrentOrientation = preset->Orientation;
	instance->InstanceProperties = preset->Properties;

	FBIMCraftingTreeNodeSharedPtr parent = InstanceFromID(ParentID);
	if (parent != nullptr)
	{
		instance->ParentInstance = parent;
		ensureAlways(parent->AttachChildAt(PresetCollection, instance, ParentSetIndex, ParentSetPosition) != ECraftingResult::Error);
	}

	for (auto& childPreset : preset->ChildPresets)
	{
		CreateNodeInstanceFromPreset(PresetCollection, instance->GetInstanceID(), childPreset.PresetID, childPreset.ParentPinSetIndex, childPreset.ParentPinSetPosition);
	}

	ValidatePool();
	return instance;
}

bool FBIMCraftingTreeNodePool::FromDataRecord(const FBIMPresetCollection &PresetCollection, const TArray<FCustomAssemblyCraftingNodeRecord> &InDataRecords, bool RecursePresets)
{
	ResetInstances();

	for (auto &dr : InDataRecords)
	{
		// Parents recursively create their children who might also be in the data record array already, so make sure we don't double up
		if (InstanceFromID(dr.InstanceID) == nullptr)
		{
			FBIMCraftingTreeNodeSharedPtr inst = CreateNodeInstanceFromDataRecord(PresetCollection, dr, RecursePresets);
			NextInstanceID = FMath::Max(dr.InstanceID + 1, NextInstanceID);
		}
	}

	ValidatePool();
	return true;
}

bool FBIMCraftingTreeNodePool::ToDataRecord(TArray<FCustomAssemblyCraftingNodeRecord> &OutDataRecords) const
{
	for (auto &instance : InstancePool)
	{
		FCustomAssemblyCraftingNodeRecord &record = OutDataRecords.AddDefaulted_GetRef();
		instance->ToDataRecord(record);
	}
	return true;
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
