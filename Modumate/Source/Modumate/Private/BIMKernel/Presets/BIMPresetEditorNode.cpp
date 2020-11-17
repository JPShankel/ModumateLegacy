// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "BIMKernel/Presets/BIMPresetEditorNode.h"
#include "BIMKernel/Presets/BIMPresetCollection.h"
#include "Algo/Transform.h"
#include "Algo/Accumulate.h"

#if WITH_EDITOR // for debugging new/delete balance
int32 FBIMPresetEditorNode::InstanceCount = 0;
#endif

FBIMPresetEditorNode::FBIMPresetEditorNode(int32 instanceID, int32 InPinSetIndex, int32 InPinSetPosition,const FBIMPresetInstance& InPreset) :
	InstanceID(instanceID),
	OriginalPresetCopy(InPreset),
	WorkingPresetCopy(InPreset),
	MyParentPinSetIndex(InPinSetIndex),
	MyParentPinSetPosition(InPinSetPosition)
{
	CategoryTitle = InPreset.CategoryTitle;

#if WITH_EDITOR 
	++InstanceCount;
#endif
}

int32 FBIMPresetEditorNode::GetInstanceID() const
{
	return InstanceID;
}

EBIMResult FBIMPresetEditorNode::GetPartSlots(TArray<FBIMPresetPartSlot>& OutPartSlots) const
{
	OutPartSlots = WorkingPresetCopy.PartSlots;
	return EBIMResult::Success;
}

EBIMResult FBIMPresetEditorNode::ClearPartSlot(const FBIMKey& SlotPreset)
{
	for (auto& partSlot : WorkingPresetCopy.PartSlots)
	{
		if (partSlot.SlotPreset == SlotPreset)
		{
			partSlot.PartPreset = FBIMKey();
			return EBIMResult::Success;
		}
	}
	return EBIMResult::Error;
}

EBIMResult FBIMPresetEditorNode::SetPartSlotPreset(const FBIMKey& SlotPreset, const FBIMKey& PartPreset)
{
	for (auto& partSlot : WorkingPresetCopy.PartSlots)
	{
		if (partSlot.SlotPreset == SlotPreset)
		{
			partSlot.PartPreset = PartPreset;
			return EBIMResult::Success;
		}
	}
	return EBIMResult::Error;
}

EBIMResult FBIMPresetEditorNode::NodeIamEmbeddedIn(int32& OutNodeId) const
{
	OutNodeId = INDEX_NONE;
	
	if (WorkingPresetCopy.ChildPresets.Num() == 0 && ParentInstance.IsValid())
	{
		OutNodeId = ParentInstance.Pin()->GetInstanceID();
		return EBIMResult::Success;
	}
	return EBIMResult::Error;
}

EBIMResult FBIMPresetEditorNode::NodesEmbeddedInMe(TArray<int32>& OutNodeIds) const
{
	OutNodeIds.Empty();
	for (const auto& child : ChildNodes)
	{
		int32 embeddedInId;
		FBIMPresetEditorNodeSharedPtr childNodePinned = child.Pin();
		if (childNodePinned->NodeIamEmbeddedIn(embeddedInId) == EBIMResult::Success && embeddedInId == InstanceID)
		{
			OutNodeIds.Add(childNodePinned->GetInstanceID());
		}
	}
	return OutNodeIds.Num() > 0 ? EBIMResult::Success : EBIMResult::Error;
}

EBIMResult FBIMPresetEditorNode::GatherAllChildNodes(TArray<FBIMPresetEditorNodeSharedPtr>& OutChildren)
{
	TArray<FBIMPresetEditorNodeSharedPtr> nodeStack;
	nodeStack.Push(AsShared());
	do
	{
		FBIMPresetEditorNodeSharedPtr currentNode = nodeStack.Pop();
		for (auto& childNode : currentNode->ChildNodes)
		{
			FBIMPresetEditorNodeSharedPtr childNodePinned = childNode.Pin();
			if (childNodePinned != nullptr)
			{
				OutChildren.Add(childNodePinned);
				nodeStack.Push(childNodePinned);
			}
		}
	} while (nodeStack.Num() > 0);

	return EBIMResult::Success;
}

// The preset on a crafting node is 'dirty' if the node's properties or input pins is inconsistent with the values in its preset
EBIMPresetEditorNodeStatus FBIMPresetEditorNode::GetPresetStatus() const
{
	if (OriginalPresetCopy.Matches(WorkingPresetCopy))
	{
		return EBIMPresetEditorNodeStatus::UpToDate;
	}
	return EBIMPresetEditorNodeStatus::Dirty;
}

int32 FBIMPresetEditorNode::GetNumChildrenOnPin(int32 InPin) const
{
	return Algo::Accumulate(WorkingPresetCopy.ChildPresets, 0, [InPin](int32 Sum, const FBIMPresetPinAttachment& Pin)
	{
		if (Pin.ParentPinSetIndex == InPin)
		{
			return Sum + 1;
		}
		return Sum;
	});
}

bool FBIMPresetEditorNode::CanRemoveChild(const FBIMPresetEditorNodeSharedPtrConst& Child) const
{
	if (ensureAlways(MyParentPinSetIndex != INDEX_NONE && MyParentPinSetIndex < WorkingPresetCopy.TypeDefinition.PinSets.Num()))
	{
		return GetNumChildrenOnPin(MyParentPinSetIndex) > WorkingPresetCopy.TypeDefinition.PinSets[MyParentPinSetIndex].MinCount;
	}

	return false;
}

bool FBIMPresetEditorNode::CanReorderChild(const FBIMPresetEditorNodeSharedPtrConst& Child) const
{
	if (ensureAlways(Child->MyParentPinSetIndex != INDEX_NONE))
	{
		return GetNumChildrenOnPin(Child->MyParentPinSetIndex) > 1;
	}

	return false;
}

EBIMResult FBIMPresetEditorNode::FindChild(int32 ChildID, int32& OutPinSetIndex, int32& OutPinSetPosition) const
{
	FBIMPresetEditorNodeSharedPtr foundChildNode;

	for (const auto& childNode : ChildNodes)
	{
		if (childNode.Pin()->GetInstanceID() == ChildID)
		{
			foundChildNode = childNode.Pin();
		}
	}

	if (!foundChildNode.IsValid())
	{
		return EBIMResult::Error;
	}

	for (auto& checkChild : WorkingPresetCopy.ChildPresets)
	{
		if (checkChild.PresetID == foundChildNode->WorkingPresetCopy.PresetID)
		{
			OutPinSetIndex = checkChild.ParentPinSetIndex;
			OutPinSetPosition = checkChild.ParentPinSetPosition;
			return EBIMResult::Success;
		}
	}

	return EBIMResult::Error;
}

EBIMResult FBIMPresetEditorNode::FindOtherChildrenOnPin(TArray<int32>& OutChildIDs) const
{
	if (ParentInstance != nullptr)
	{
		auto parentInstance = ParentInstance.Pin();
		int32 myPinSet, myPinPosition;
		if (parentInstance->FindChild(InstanceID, myPinSet, myPinPosition) != EBIMResult::Success)
		{
			return EBIMResult::Error;
		}
		for (auto& child : parentInstance->ChildNodes)
		{
			int32 childPinSet, childPinPosition;
			int32 childID = child.Pin()->InstanceID;
			if (parentInstance->FindChild(childID, childPinSet, childPinPosition) != EBIMResult::Error)
			{
				if (childPinSet == myPinSet)
				{
					OutChildIDs.Add(childID);
				}
			}
		}
		return EBIMResult::Success;
	}

	return EBIMResult::Error;
}

EBIMResult FBIMPresetEditorNode::GatherChildrenInOrder(TArray<int32>& OutChildIDs) const
{
	Algo::Transform(ChildNodes, OutChildIDs, [](const FBIMPresetEditorNodeWeakPtr& Child) {return Child.Pin()->GetInstanceID(); });
	return EBIMResult::Success;
}

EBIMResult FBIMPresetEditorNode::DetachSelfFromParent()
{
	if (ParentInstance != nullptr)
	{
		FBIMPresetEditorNodeWeakPtr childToRemove;
		for (auto& child : ParentInstance.Pin()->ChildNodes)
		{
			if (child.HasSameObject(this))
			{
				childToRemove = child;
			}

			auto pinnedChild = child.Pin();
			if (pinnedChild->MyParentPinSetIndex == MyParentPinSetIndex && pinnedChild->MyParentPinSetPosition > MyParentPinSetPosition)
			{
				--pinnedChild->MyParentPinSetPosition;
			}
		}

		if (childToRemove.IsValid())
		{
			ParentInstance.Pin()->ChildNodes.Remove(childToRemove);
			ParentInstance.Pin()->WorkingPresetCopy.RemoveChildPreset(MyParentPinSetIndex, MyParentPinSetPosition);
		}

		ParentInstance = nullptr;

		return EBIMResult::Success;
	}
	return EBIMResult::Error;
}

EBIMResult FBIMPresetEditorNode::AddChildPreset(const FBIMPresetEditorNodeSharedPtr& Child, int32 PinSetIndex, int32 PinSetPosition)
{
	ChildNodes.Add(Child);
	ChildNodes.Last().Pin()->MyParentPinSetIndex = PinSetIndex;
	ChildNodes.Last().Pin()->MyParentPinSetPosition = PinSetPosition;
	ChildNodes.Last().Pin()->ParentInstance = AsShared();
	WorkingPresetCopy.AddChildPreset(Child->WorkingPresetCopy.PresetID, PinSetIndex, PinSetPosition);
	return SortChildren();
}

EBIMResult FBIMPresetEditorNode::SortChildren()
{
	WorkingPresetCopy.SortChildPresets();

	ChildNodes.Sort([](const FBIMPresetEditorNodeWeakPtr& LHS, const FBIMPresetEditorNodeWeakPtr& RHS)
	{
		auto lhsPin = LHS.Pin();
		auto rhsPin = RHS.Pin();

		if (lhsPin->MyParentPinSetIndex < rhsPin->MyParentPinSetIndex)
		{
			return true;
		}
		if (lhsPin->MyParentPinSetIndex > rhsPin->MyParentPinSetIndex)
		{
			return false;
		}
		return lhsPin->MyParentPinSetPosition < rhsPin->MyParentPinSetPosition;
	});

	UpdateAddButtons();

#if WITH_EDITOR
	ensureAlways(ValidateNode());
#endif

	return ensureAlways(ChildNodes.Num() == WorkingPresetCopy.ChildPresets.Num()) ? EBIMResult::Success : EBIMResult::Error;
}

EBIMResult FBIMPresetEditorNode::UpdateAddButtons()
{
	if (ChildNodes.Num() > 0)
	{
		for (int32 i = 0; i < ChildNodes.Num() - 1; ++i)
		{
			ChildNodes[i].Pin()->bWantAddButton = false;
		}
		ChildNodes.Last().Pin()->bWantAddButton = true;
	}
	return EBIMResult::Success;
}

bool FBIMPresetEditorNode::ValidateNode() const
{
	if (WorkingPresetCopy.ChildPresets.Num() != ChildNodes.Num())
	{
		return false;
	}

	if (MyParentPinSetIndex == INDEX_NONE || MyParentPinSetPosition == INDEX_NONE)
	{
		return false;
	}

	FBIMPresetEditorNodeSharedPtr previousPtr;
	for (int32 i = 0; i < ChildNodes.Num(); ++i)
	{
		// Child nodes should mirror child presets in the working copy
		FBIMPresetEditorNodeSharedPtr childPtr = ChildNodes[i].Pin();
		if (childPtr->MyParentPinSetIndex != WorkingPresetCopy.ChildPresets[i].ParentPinSetIndex ||
			childPtr->MyParentPinSetPosition != WorkingPresetCopy.ChildPresets[i].ParentPinSetPosition)
		{
			return false;
		}

		// Nodes should be sorted so positions increment by 1 and wrap back to zero when index increments by 1
		if (previousPtr.IsValid())
		{
			bool isSibling = (childPtr->MyParentPinSetIndex == previousPtr->MyParentPinSetIndex &&
				childPtr->MyParentPinSetPosition == previousPtr->MyParentPinSetPosition + 1);

			bool isCousin = (childPtr->MyParentPinSetIndex == previousPtr->MyParentPinSetIndex+1 && childPtr->MyParentPinSetPosition == 0);

			if (!isSibling && !isCousin)
			{
				return false;
			}
		}

		previousPtr = childPtr;
	}
	return WorkingPresetCopy.ValidatePreset();
}