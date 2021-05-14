// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "ModumateCore/EnumHelpers.h"
#include "UObject/Class.h"
#include "UObject/UObjectGlobals.h"

bool GetEnumDisplayNamesAndValues(const UEnum* EnumClass, TArray<TPair<FText, int64>>& OutDisplayNames)
{
	if (EnumClass == nullptr)
	{
		return false;
	}

	int32 numEnums = EnumClass->NumEnums();
	if (EnumClass->ContainsExistingMax())
	{
		numEnums--;
	}
	for (int32 offsetTypeIdx = 0; offsetTypeIdx < numEnums; ++offsetTypeIdx)
	{
		OutDisplayNames.Add(TPair<FText, int64>(EnumClass->GetDisplayNameTextByIndex(offsetTypeIdx), EnumClass->GetValueByIndex(offsetTypeIdx)));
	}
	return true;
}

bool GetEnumDisplayNamesAndValues(const FString& EnumClassName, TArray<TPair<FText, int64>>& OutDisplayNames)
{
	return GetEnumDisplayNamesAndValues(FindObject<UEnum>((UObject*)ANY_PACKAGE, *EnumClassName, true),OutDisplayNames);
}
