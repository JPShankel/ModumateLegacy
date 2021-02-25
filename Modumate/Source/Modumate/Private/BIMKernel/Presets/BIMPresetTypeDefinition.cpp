// Copyright 2020 Modumate, Inc.All Rights Reserved.

#include "BIMKernel/Presets/BIMPresetTypeDefinition.h"

FBIMPresetNodePinSet::FBIMPresetNodePinSet(FName InSetName, int32 InMinCount, int32 InMaxCount) :
	SetName(InSetName), MinCount(InMinCount), MaxCount(InMaxCount)
{}

FBIMPresetNodePinSet::FBIMPresetNodePinSet() : 
	MinCount(0), MaxCount(0)
{
}

bool FBIMPresetNodePinSet::Matches(const FBIMPresetNodePinSet& OtherPinSet) const
{
	if (!SetName.IsEqual(OtherPinSet.SetName))
	{
		return false;
	}

	if (MinCount != OtherPinSet.MinCount)
	{
		return false;
	}

	if (MaxCount != OtherPinSet.MaxCount)
	{
		return false;
	}

	if (PinTarget != OtherPinSet.PinTarget)
	{
		return false;
	}

	return true;
}

bool FBIMPresetTypeDefinition::Matches(const FBIMPresetTypeDefinition& OtherDefinition) const
{
	if (!TypeName.IsEqual(OtherDefinition.TypeName))
	{
		return false;
	}

	if (Scope != OtherDefinition.Scope)
	{
		return false;
	}

	if (!FormTemplate.Matches(OtherDefinition.FormTemplate))
	{
		return false;
	}

	if (PinSets.Num() != OtherDefinition.PinSets.Num())
	{
		return false;
	}

	for (int32 i = 0; i < PinSets.Num(); ++i)
	{
		if (!PinSets[i].Matches(OtherDefinition.PinSets[i]))
		{
			return false;
		}
	}

	if (!Properties.Matches(OtherDefinition.Properties))
	{
		return false;
	}

	return true;
}
