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

bool FBIMPresetInstance::HasProperty(const FBIMNameType& Name) const
{
	return Properties.HasProperty<float>(NodeScope, Name) || Properties.HasProperty<FString>(NodeScope, Name);
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
