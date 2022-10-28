// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "BIMKernel/Presets/BIMPresetLayerPriority.h"
#include "BIMKernel/Presets/BIMPresetEditorForm.h"

bool operator==(const FBIMPresetLayerPriority& LHS, const FBIMPresetLayerPriority& RHS)
{
	return LHS.PriorityGroup == RHS.PriorityGroup && LHS.PriorityValue == RHS.PriorityValue;
}

bool operator!=(const FBIMPresetLayerPriority& LHS, const FBIMPresetLayerPriority& RHS)
{
	return !(LHS == RHS);
}

bool operator<(const FBIMPresetLayerPriority& LHS, const FBIMPresetLayerPriority& RHS)
{
	int64 lhsPG = static_cast<int64>(LHS.PriorityGroup);
	int64 rhsPG = static_cast<int64>(RHS.PriorityGroup);
	if (lhsPG < rhsPG)
	{
		return true;
	}
	if (lhsPG > rhsPG)
	{
		return false;
	}

	return LHS.PriorityValue < RHS.PriorityValue;
}

bool operator>(const FBIMPresetLayerPriority& LHS, const FBIMPresetLayerPriority& RHS)
{
	return !(LHS < RHS || LHS == RHS);
}

bool operator>=(const FBIMPresetLayerPriority& LHS, const FBIMPresetLayerPriority& RHS)
{
	return !(LHS < RHS);
}

bool operator<=(const FBIMPresetLayerPriority& LHS, const FBIMPresetLayerPriority& RHS)
{
	return !(LHS > RHS);
}
