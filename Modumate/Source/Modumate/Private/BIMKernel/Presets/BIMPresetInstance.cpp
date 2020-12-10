// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "BIMKernel/Presets/BIMPresetInstance.h"
#include "BIMKernel/Presets/BIMPresetEditor.h"
#include "BIMKernel/Presets/BIMPresetCollection.h"
#include "DocumentManagement/ModumateCommands.h"

bool FBIMPresetPinAttachment::operator==(const FBIMPresetPinAttachment &OtherAttachment) const
{
	return ParentPinSetIndex == OtherAttachment.ParentPinSetIndex &&
		ParentPinSetPosition == OtherAttachment.ParentPinSetPosition &&
		PresetID == OtherAttachment.PresetID;
}

bool FBIMPresetInstance::Matches(const FBIMPresetInstance &OtherPreset) const
{
	if (NodeType != OtherPreset.NodeType)
	{
		return false;
	}

	if (PresetID != OtherPreset.PresetID)
	{
		return false;
	}

	if (PartSlots.Num() != OtherPreset.PartSlots.Num())
	{
		return false;
	}

	if (ChildPresets.Num() != OtherPreset.ChildPresets.Num())
	{
		return false;
	}

	if (ParentTagPaths.Num() != OtherPreset.ParentTagPaths.Num())
	{
		return false;
	}

	if (SlotConfigPresetID != OtherPreset.SlotConfigPresetID)
	{
		return false;
	}

	if (MyTagPath != OtherPreset.MyTagPath)
	{
		return false;
	}

	if (!Properties.Matches(OtherPreset.Properties))
	{
		return false;
	}

	for (int32 i=0;i<PartSlots.Num();++i)
	{
		if (OtherPreset.PartSlots[i] != PartSlots[i])
		{
			return false;
		}
	}

	for (auto& cp : ChildPresets)
	{
		if (!OtherPreset.ChildPresets.Contains(cp))
		{
			return false;
		}
	}

	for (auto &ptp : ParentTagPaths)
	{
		if (!OtherPreset.ParentTagPaths.Contains(ptp))
		{
			return false;
		}
	}

	return true;
}

EBIMResult FBIMPresetInstance::SortChildPresets()
{
	ChildPresets.Sort([](const FBIMPresetPinAttachment& LHS, const FBIMPresetPinAttachment& RHS)
	{
		if (LHS.ParentPinSetIndex < RHS.ParentPinSetIndex)
		{
			return true;
		}
		if (LHS.ParentPinSetIndex > RHS.ParentPinSetIndex)
		{
			return false;
		}
		return LHS.ParentPinSetPosition < RHS.ParentPinSetPosition;
	});
	return EBIMResult::Success;
}

bool FBIMPresetInstance::HasProperty(const FBIMNameType& Name) const
{
	return Properties.HasProperty<float>(NodeScope, Name) || Properties.HasProperty<FString>(NodeScope, Name);
}

void FBIMPresetInstance::SetProperties(const FBIMPropertySheet& InProperties)
{
	Properties = InProperties;
}

bool FBIMPresetInstance::HasPin(int32 PinSetIndex, int32 PinSetPosition) const
{
	for (auto& child : ChildPresets)
	{
		if (child.ParentPinSetIndex == PinSetIndex && child.ParentPinSetPosition == PinSetPosition)
		{
			return true;
		}
	}
	return false;
}

EBIMResult FBIMPresetInstance::AddChildPreset(const FBIMKey& ChildPresetID, int32 PinSetIndex, int32 PinSetPosition)
{
	EBIMPinTarget target = EBIMPinTarget::Default;
	for (auto& child : ChildPresets)
	{
		if (child.ParentPinSetIndex == PinSetIndex && child.ParentPinSetPosition >= PinSetPosition)
		{
			++child.ParentPinSetPosition;
			target = child.Target;
		}
	}

	FBIMPresetPinAttachment& newAttachment = ChildPresets.AddDefaulted_GetRef();
	newAttachment.ParentPinSetIndex = PinSetIndex;
	newAttachment.ParentPinSetPosition = PinSetPosition;
	newAttachment.PresetID = ChildPresetID;
	newAttachment.Target = target;

	SortChildPresets();

	return EBIMResult::Success;
}

EBIMResult FBIMPresetInstance::RemoveChildPreset(int32 PinSetIndex, int32 PinSetPosition)
{
	int32 indexToRemove = INDEX_NONE;
	for (int32 i = 0; i < ChildPresets.Num(); ++i)
	{
		if (ChildPresets[i].ParentPinSetIndex == PinSetIndex)
		{
			if (ChildPresets[i].ParentPinSetPosition == PinSetPosition)
			{
				indexToRemove = i;
			}
			else if (ChildPresets[i].ParentPinSetPosition > PinSetPosition)
			{
				--ChildPresets[i].ParentPinSetPosition;
			}
		}
	}
	if (ensureAlways(indexToRemove != INDEX_NONE))
	{
		ChildPresets.RemoveAt(indexToRemove);
	}
	return EBIMResult::Success;
}

EBIMResult FBIMPresetInstance::SetPartPreset(const FBIMKey& SlotPreset, const FBIMKey& PartPreset)
{
	for (auto& partSlot : PartSlots)
	{
		if (partSlot.SlotPreset == SlotPreset)
		{
			partSlot.PartPreset = PartPreset;
			return EBIMResult::Success;
		}
	}
	return EBIMResult::Error;
}


bool FBIMPresetInstance::ValidatePreset() const
{
	if (ChildPresets.Num() > 0)
	{
		if (ChildPresets[0].ParentPinSetPosition != 0)
		{
			return false;
		}
		for (int32 i = 1; i < ChildPresets.Num(); ++i)
		{
			const FBIMPresetPinAttachment& previous = ChildPresets[i-1];
			const FBIMPresetPinAttachment& current = ChildPresets[i];
			bool isSibling = (current.ParentPinSetIndex == previous.ParentPinSetIndex && current.ParentPinSetPosition == previous.ParentPinSetPosition + 1);
			bool isCousin = (current.ParentPinSetIndex > previous.ParentPinSetIndex && current.ParentPinSetPosition == 0);
			if (!isSibling && !isCousin)
			{
				return false;
			}
		}
	}
	return true;
}
