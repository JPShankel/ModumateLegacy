// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "BIMKernel/BIMPresetInstance.h"
#include "BIMKernel/BIMNodeEditor.h"
#include "BIMKernel/BIMPresetCollection.h"
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

	if (ChildPresets.Num() != OtherPreset.ChildPresets.Num())
	{
		return false;
	}

	for (auto &cp : ChildPresets)
	{
		if (!OtherPreset.ChildPresets.Contains(cp))
		{
			return false;
		}
	}

	if (ParentTagPaths.Num() != OtherPreset.ParentTagPaths.Num())
	{
		return false;
	}

	for (auto &ptp : ParentTagPaths)
	{
		if (!OtherPreset.ParentTagPaths.Contains(ptp))
		{
			return false;
		}
	}

	if (MyTagPath != OtherPreset.MyTagPath)
	{
		return false;
	}

	if (!Properties.Matches(OtherPreset.Properties))
	{
		return false;
	}

	return true;
}

EBIMResult FBIMPresetInstance::ToDataRecord(FCraftingPresetRecord& OutRecord) const
{
	OutRecord.DisplayName = GetDisplayName();
	OutRecord.NodeType = NodeType;
	OutRecord.PresetID = PresetID;
	OutRecord.SlotConfigPresetID = SlotConfigPresetID;
	OutRecord.CategoryTitle = CategoryTitle.ToString();
	OutRecord.ObjectType = ObjectType;
	MyTagPath.ToString(OutRecord.MyTagPath);

	for (auto& ptp : ParentTagPaths)
	{
		ptp.ToString(OutRecord.ParentTagPaths.AddDefaulted_GetRef());
	}

	for (auto& childPreset : ChildPresets)
	{
		OutRecord.ChildSetIndices.Add(childPreset.ParentPinSetIndex);
		OutRecord.ChildSetPositions.Add(childPreset.ParentPinSetPosition);
		OutRecord.ChildPresets.Add(childPreset.PresetID);
	}

	for (auto& partSlot : PartSlots)
	{
		OutRecord.PartPresets.Add(partSlot.PartPreset);
		OutRecord.PartIDs.Add(partSlot.ID);
		OutRecord.PartParentIDs.Add(partSlot.ParentID);
		OutRecord.PartSlotNames.Add(partSlot.SlotName);
	}

	Properties.ToDataRecord(OutRecord.PropertyRecord);

	return EBIMResult::Success;
}

EBIMResult FBIMPresetInstance::SortChildNodes()
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

EBIMResult FBIMPresetInstance::FromDataRecord(const FBIMPresetCollection &PresetCollection, const FCraftingPresetRecord &Record)
{
	NodeType = Record.NodeType;
	CategoryTitle = FText::FromString(Record.CategoryTitle);

	const FBIMPresetTypeDefinition *nodeType = PresetCollection.NodeDescriptors.Find(NodeType);
	// TODO: this ensure will fire if expected presets have become obsolete, resave to fix
	if (!ensureAlways(nodeType != nullptr))
	{
		return EBIMResult::Error;
	}

	PresetID = Record.PresetID;
	SlotConfigPresetID = Record.SlotConfigPresetID;
	ObjectType = Record.ObjectType;
	NodeScope = nodeType->Scope;

	Properties.Empty();
	ChildPresets.Empty();

	Properties.FromDataRecord(Record.PropertyRecord);

	if (ensureAlways(Record.ChildSetIndices.Num() == Record.ChildPresets.Num() &&
		Record.ChildSetPositions.Num() == Record.ChildPresets.Num()))
	{
		for (int32 i = 0; i < Record.ChildPresets.Num(); ++i)
		{
			FBIMPresetPinAttachment &attachment = ChildPresets.AddDefaulted_GetRef();
			attachment.ParentPinSetIndex = Record.ChildSetIndices[i];
			attachment.ParentPinSetPosition = Record.ChildSetPositions[i];
			attachment.PresetID = Record.ChildPresets[i];
		}
	}

	if (ensureAlways(Record.PartIDs.Num() == Record.PartParentIDs.Num() &&
		Record.PartIDs.Num() == Record.PartPresets.Num() &&
		Record.PartIDs.Num() == Record.PartSlotNames.Num()))
	{
		for (int32 i = 0; i < Record.PartIDs.Num();++i)
		{
			FBIMPresetPartSlot &partSlot = PartSlots.AddDefaulted_GetRef();
			partSlot.ID = Record.PartIDs[i];
			partSlot.ParentID = Record.PartParentIDs[i];
			partSlot.PartPreset = Record.PartPresets[i];
			partSlot.SlotName = Record.PartSlotNames[i];
		}
	}

	MyTagPath.FromString(Record.MyTagPath);

	for (auto &ptp : Record.ParentTagPaths)
	{
		ParentTagPaths.AddDefaulted_GetRef().FromString(ptp);
	}

	FString displayName;
	if (TryGetProperty(BIMPropertyNames::Name, displayName))
	{
		DisplayName = FText::FromString(displayName);
	}

	return EBIMResult::Success;
}


FString FBIMPresetInstance::GetDisplayName() const
{
	return Properties.GetProperty(EBIMValueScope::Preset, BIMPropertyNames::Name);
}

bool FBIMPresetInstance::HasProperty(const FBIMNameType& Name) const
{
	return Properties.HasProperty(NodeScope, Name);
}

Modumate::FModumateCommandParameter FBIMPresetInstance::GetProperty(const FBIMNameType& Name) const
{
	return Properties.GetProperty(NodeScope, Name);
}

Modumate::FModumateCommandParameter FBIMPresetInstance::GetScopedProperty(const EBIMValueScope& Scope, const FBIMNameType& Name) const
{
	return Properties.GetProperty(Scope, Name);
}

void FBIMPresetInstance::SetScopedProperty(const EBIMValueScope& Scope, const FBIMNameType& Name, const Modumate::FModumateCommandParameter& V)
{
	Properties.SetProperty(Scope, Name, V);
}

void FBIMPresetInstance::SetProperties(const FBIMPropertySheet& InProperties)
{
	Properties = InProperties;
}

bool FBIMPresetInstance::SupportsChild(const FBIMPresetInstance& CandidateChild) const
{
	for (auto& childPath : CandidateChild.ParentTagPaths)
	{
		if (childPath.MatchesPartial(MyTagPath))
		{
			return true;
		}
	}
	return false;
}

