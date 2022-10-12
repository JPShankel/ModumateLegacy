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
