// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "BIMKernel/Presets/BIMPresetLayerPriority.h"
#include "BIMKernel/Presets/BIMPresetEditorForm.h"
#include "BIMKernel/Presets/BIMWebPreset.h"

void FBIMPresetLayerPriority::ConvertToWebPreset(FBIMWebPreset& OutPreset)
{
	const FString PropertyKey = GetEnumValueString(EPresetPropertyMatrixNames::MiterPriority);
	const FString GroupKey = PropertyKey + TEXT(".") + TEXT("PriorityGroup");
	const FString PriorityKey = PropertyKey + TEXT(".") + TEXT("PriorityValue");
	
	FString group = GetEnumValueString(PriorityGroup);
	FString priority = FString::FromInt(PriorityValue);
	if(!OutPreset.properties.Contains(GroupKey))
	{
		OutPreset.properties.Add(GroupKey, {});
	}
	if(!OutPreset.properties.Contains(PriorityKey))
	{
		OutPreset.properties.Add(PriorityKey, {});
	}
	OutPreset.properties[GroupKey].value.Add(group);
	OutPreset.properties[PriorityKey].value.Add(priority);
}

void FBIMPresetLayerPriority::ConvertFromWebPreset(const FBIMWebPreset& InPreset)
{
	const FString PropertyKey = GetEnumValueString(EPresetPropertyMatrixNames::MiterPriority);
	const FString GroupKey = PropertyKey + TEXT(".") + TEXT("PriorityGroup");
	const FString PriorityKey = PropertyKey + TEXT(".") + TEXT("PriorityValue");
	
	FString group = InPreset.properties[GroupKey].value[0];
	FString priority = InPreset.properties[PriorityKey].value[0];

	ensure(FindEnumValueByString(group, PriorityGroup) && LexTryParseString(PriorityValue, *priority));
}

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
