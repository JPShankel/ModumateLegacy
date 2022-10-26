// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "BIMKernel/Presets/BIMPresetEditorNode.h"
#include "BIMKernel/Presets/BIMPresetCollection.h"
#include "Algo/Transform.h"
#include "Algo/Accumulate.h"

#if WITH_EDITOR // for debugging new/delete balance
int32 FBIMPresetEditorNode::InstanceCount = 0;
#endif

FBIMPresetEditorNode::FBIMPresetEditorNode(const FBIMEditorNodeIDType& InInstanceID, int32 InPinSetIndex, int32 InPinSetPosition,
	const FBIMPresetInstance& InPreset,
	const FVDPTable* VdpTable) :
	InstanceID(InInstanceID),
	VDPTable(VdpTable),
	Preset(InPreset),
	MyParentPinSetIndex(InPinSetIndex),
	MyParentPinSetPosition(InPinSetPosition)
{
	CategoryTitle = InPreset.CategoryTitle;

#if WITH_EDITOR
	++InstanceCount;
#endif
}

FBIMEditorNodeIDType FBIMPresetEditorNode::GetInstanceID() const
{
	return InstanceID;
}

EBIMResult FBIMPresetEditorNode::GetPartSlots(TArray<FBIMPresetPartSlot>& OutPartSlots) const
{
	OutPartSlots = Preset.PartSlots;
	return EBIMResult::Success;
}

EBIMResult FBIMPresetEditorNode::GatherAllChildNodes(TArray<FBIMPresetEditorNodeSharedPtr>& OutChildren)
{
	// TODO: Split this into separate function to get PartNodes
	TArray<FBIMPresetEditorNodeSharedPtr> nodeStack;
	nodeStack.Push(AsShared());
	do
	{
		FBIMPresetEditorNodeSharedPtr currentNode = nodeStack.Pop();

		TArray<FBIMPresetEditorNodeWeakPtr> currentNodeToCheck = currentNode->PartNodes.Num() == 0 ? currentNode->ChildNodes : currentNode->PartNodes;
		
		for (auto& childNode : currentNodeToCheck)
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

int32 FBIMPresetEditorNode::GetNumChildrenOnPin(int32 InPin) const
{
	return Algo::Accumulate(Preset.ChildPresets, 0, [InPin](int32 Sum, const FBIMPresetPinAttachment& Pin)
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
	if (Child->MyParentPartSlot.IsValid())
	{
		return false;
	}

	if (ensureAlways(Child->MyParentPinSetIndex != INDEX_NONE && Child->MyParentPinSetIndex < Preset.PinSets.Num()))
	{
		return GetNumChildrenOnPin(Child->MyParentPinSetIndex) > Preset.PinSets[Child->MyParentPinSetIndex].MinCount;
	}

	return false;
}

bool FBIMPresetEditorNode::CanReorderChild(const FBIMPresetEditorNodeSharedPtrConst& Child) const
{
	if (Child->MyParentPartSlot.IsValid())
	{
		return false;
	}

	if (ensureAlways(Child->MyParentPinSetIndex != INDEX_NONE))
	{
		return GetNumChildrenOnPin(Child->MyParentPinSetIndex) > 1;
	}

	return false;
}

EBIMResult FBIMPresetEditorNode::FindChild(const FBIMEditorNodeIDType& ChildID, int32& OutPinSetIndex, int32& OutPinSetPosition) const
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

	for (auto& checkChild : Preset.ChildPresets)
	{
		//KLUDGE: This is going to break in the future. We need access to the VDP table to make sure this is
		// valid. Invented presets

		if(ensure(VDPTable != nullptr))
		{
			if(VDPTable->TranslateToDerived(checkChild.PresetGUID) == foundChildNode->Preset.GUID)
			{
				OutPinSetIndex = checkChild.ParentPinSetIndex;
				OutPinSetPosition = checkChild.ParentPinSetPosition;
				return EBIMResult::Success;
			}
		}
	}

	return EBIMResult::Error;
}

EBIMResult FBIMPresetEditorNode::FindNodeIDConnectedToSlot(const FGuid& SlotPreset, FBIMEditorNodeIDType& OutChildID) const
{
	for (auto& curNode : PartNodes)
	{
		if (curNode.Pin()->MyParentPartSlot == SlotPreset)
		{
			OutChildID = curNode.Pin()->GetInstanceID();
			return EBIMResult::Success;
		}
	}
	return EBIMResult::Error;
}

EBIMResult FBIMPresetEditorNode::FindOtherChildrenOnPin(TArray<FBIMEditorNodeIDType>& OutChildIDs) const
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
			FBIMEditorNodeIDType childID = child.Pin()->InstanceID;
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

EBIMResult FBIMPresetEditorNode::GatherChildrenInOrder(TArray<FBIMEditorNodeIDType>& OutChildIDs) const
{
	// TODO: Split this into separate function for PartNodes
	if (PartNodes.Num() == 0)
	{
		Algo::Transform(ChildNodes, OutChildIDs, [](const FBIMPresetEditorNodeWeakPtr& Child) {return Child.Pin()->GetInstanceID(); });
	}
	else
	{
		Algo::Transform(PartNodes, OutChildIDs, [](const FBIMPresetEditorNodeWeakPtr& Child) {return Child.Pin()->GetInstanceID(); });
	}
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

		auto pinnedParent = ParentInstance.Pin();
		if (childToRemove.IsValid())
		{
			pinnedParent->ChildNodes.Remove(childToRemove);
			pinnedParent->Preset.RemoveChildPreset(MyParentPinSetIndex, MyParentPinSetPosition);
		}
		else
		{
			for (auto& partNode : ParentInstance.Pin()->PartNodes)
			{
				if (partNode.HasSameObject(this))
				{
					childToRemove = partNode;
					break;
				}
			}
			if (childToRemove.IsValid())
			{
				pinnedParent->PartNodes.Remove(childToRemove);
				pinnedParent->Preset.SetPartPreset(childToRemove.Pin()->MyParentPartSlot, FGuid());
			}
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
	Preset.AddChildPreset(Child->Preset.GUID, PinSetIndex, PinSetPosition);
	return SortChildren();
}

EBIMResult FBIMPresetEditorNode::SortChildren()
{
	Preset.SortChildPresets();

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


	// When parts are added and removed, the PartNodes array gets out of order
	TMap<FGuid, FBIMPresetEditorNodeWeakPtr> slotMap;
	for (auto& partNode : PartNodes)
	{
		slotMap.Add(partNode.Pin()->MyParentPartSlot, partNode);
	}

	PartNodes.Empty();

	for (auto& presetPart : Preset.PartSlots)
	{
		auto* slotNode = slotMap.Find(presetPart.SlotPresetGUID);
		
		// Will be null for empty slots
		if (slotNode != nullptr)
		{
			PartNodes.Add(*slotNode);
		}
	}

	UpdateAddButtons();

#if WITH_EDITOR
	if (!ensureAlways(ValidateNode()))
	{
		// Breakpoint here to analyze why it failed
		ValidateNode();
	}
#endif

	return ensureAlways(ChildNodes.Num() == Preset.ChildPresets.Num()) ? EBIMResult::Success : EBIMResult::Error;
}

EBIMResult FBIMPresetEditorNode::UpdateAddButtons()
{
	if (ChildNodes.Num() > 0)
	{
		for (int32 i = 0; i < ChildNodes.Num() - 1; ++i)
		{
			ChildNodes[i].Pin()->bWantAddButton = false;
		}
		ChildNodes.Last().Pin()->bWantAddButton = Preset.HasOpenPin();
	}
	return EBIMResult::Success;
}

bool FBIMPresetEditorNode::ValidateNode() const
{
	if (Preset.ChildPresets.Num() != ChildNodes.Num())
	{
		return false;
	}

	if (!MyParentPartSlot.IsValid() && (MyParentPinSetIndex == INDEX_NONE || MyParentPinSetPosition == INDEX_NONE))
	{
		return false;
	}

	FBIMPresetEditorNodeSharedPtr previousPtr;
	for (int32 i = 0; i < ChildNodes.Num(); ++i)
	{
		// Child nodes should mirror child presets in the working copy
		FBIMPresetEditorNodeSharedPtr childPtr = ChildNodes[i].Pin();
		if (childPtr->MyParentPinSetIndex != Preset.ChildPresets[i].ParentPinSetIndex ||
			childPtr->MyParentPinSetPosition != Preset.ChildPresets[i].ParentPinSetPosition)
		{
			return false;
		}

		// Nodes should be sorted so positions increment by 1 and wrap back to zero when index increments by 1
		if (previousPtr.IsValid())
		{
			bool isSibling = (childPtr->MyParentPinSetIndex == previousPtr->MyParentPinSetIndex &&
				childPtr->MyParentPinSetPosition == previousPtr->MyParentPinSetPosition + 1);

			bool isCousin = (childPtr->MyParentPinSetIndex > previousPtr->MyParentPinSetIndex && childPtr->MyParentPinSetPosition == 0);

			if (!isSibling && !isCousin)
			{
				return false;
			}
		}

		previousPtr = childPtr;
	}
	return Preset.CheckChildrenForErrors();
}

/*
* The property form binds a display name (ie "Color") to a qualified property (ie. "Color.HexValue")
* FormIntToProperty on the preset provides the fixed set of bindings that all presets of this type always have
* VisibleNamedDimensions is calculated for rigged assemblies using the layout system and provides properties
* which may or may not be visible depending on whether other parts need to reference them (ie HandleHeight)
*/
EBIMResult FBIMPresetEditorNode::GetPresetForm(const UModumateDocument* InDocument,FBIMPresetForm& OutForm) const
{
	if (Preset.GetForm(InDocument,OutForm) == EBIMResult::Success)
	{
		// VisibleNamedDimensions determined by layout walk
		for (auto& namedDim : VisibleNamedDimensions)
		{
			FPartNamedDimension* partDim = FBIMPartSlotSpec::NamedDimensionMap.Find(namedDim);
			if (partDim != nullptr && partDim->UIType != EPartSlotDimensionUIType::Hidden)
			{
				FBIMPropertyKey propKey(EBIMValueScope::Dimension, *namedDim);
				OutForm.AddPropertyElement(partDim->DisplayName, propKey.QN(), EBIMPresetEditorField::DimensionProperty);
			}
		}

		return Preset.UpdateFormElements(InDocument,OutForm);
	}

	return EBIMResult::Error;
}

