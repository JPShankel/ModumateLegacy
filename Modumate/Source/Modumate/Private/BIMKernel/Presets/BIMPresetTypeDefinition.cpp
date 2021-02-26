// Copyright 2020 Modumate, Inc.All Rights Reserved.

#include "BIMKernel/Presets/BIMPresetTypeDefinition.h"

FBIMPresetNodePinSet::FBIMPresetNodePinSet(FName InSetName, int32 InMinCount, int32 InMaxCount) :
	SetName(InSetName), MinCount(InMinCount), MaxCount(InMaxCount)
{}

FBIMPresetNodePinSet::FBIMPresetNodePinSet() : 
	MinCount(0), MaxCount(0)
{
}

bool FBIMPresetNodePinSet::operator==(const FBIMPresetNodePinSet& RHS) const
{
	if (!SetName.IsEqual(RHS.SetName))
	{
		return false;
	}

	if (MinCount != RHS.MinCount)
	{
		return false;
	}

	if (MaxCount != RHS.MaxCount)
	{
		return false;
	}

	if (PinTarget != RHS.PinTarget)
	{
		return false;
	}

	return true;
}

bool FBIMPresetNodePinSet::operator!=(const FBIMPresetNodePinSet& RHS) const
{
	return !(*this == RHS);
}

bool FBIMPresetTypeDefinition::operator==(const FBIMPresetTypeDefinition& RHS) const
{
	if (!TypeName.IsEqual(RHS.TypeName))
	{
		return false;
	}

	if (Scope != RHS.Scope)
	{
		return false;
	}

	if (FormTemplate != RHS.FormTemplate)
	{
		return false;
	}

	if (PinSets.Num() != RHS.PinSets.Num())
	{
		return false;
	}

	for (int32 i = 0; i < PinSets.Num(); ++i)
	{
		if (PinSets[i] != RHS.PinSets[i])
		{
			return false;
		}
	}

	if (Properties != RHS.Properties)
	{
		return false;
	}

	return true;
}

bool FBIMPresetTypeDefinition::operator!=(const FBIMPresetTypeDefinition& RHS) const
{
	return !(*this == RHS);
}
