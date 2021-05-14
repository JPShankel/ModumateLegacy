// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// UEnum <-> FName/String helpers that are not already statically defined in UEnum in Class.h

template<typename TEnum>
static FORCEINLINE FString GetEnumValueString(TEnum EnumValue)
{
	static_assert(TIsEnum<TEnum>::Value, "Should only call this with enum types");
	UEnum* enumClass = StaticEnum<TEnum>();
	return enumClass ? enumClass->GetNameStringByValue((int64)EnumValue) : FString();
}

template<typename TEnum>
static FORCEINLINE FName GetEnumValueShortName(TEnum EnumValue)
{
	return FName(*GetEnumValueString<TEnum>(EnumValue));
}

template<typename TEnum>
static FORCEINLINE bool FindEnumValueByName(FName NameValue, TEnum& OutValue)
{
	static_assert(TIsEnum<TEnum>::Value, "Should only call this with enum types");
	UEnum* enumClass = StaticEnum<TEnum>();
	int64 enumIntValue = enumClass ? enumClass->GetValueByName(NameValue) : INDEX_NONE;
	if (enumIntValue != INDEX_NONE)
	{
		OutValue = static_cast<TEnum>(enumIntValue);
		return true;
	}
	return false;
}

template<typename TEnum>
static FORCEINLINE bool FindEnumValueByString(const FString& StringValue, TEnum& OutValue)
{
	return FindEnumValueByName<TEnum>(FName(*StringValue), OutValue);
}

template<typename TEnum>
static FORCEINLINE TEnum GetEnumValueByName(FName NameValue)
{
	TEnum enumValue;
	if (FindEnumValueByName<TEnum>(NameValue, enumValue))
	{
		return enumValue;
	}
	else
	{
		return static_cast<TEnum>(0);
	}
}

template<typename TEnum>
static FORCEINLINE TEnum GetEnumValueByString(const FString& StringValue)
{
	return GetEnumValueByName<TEnum>(FName(*StringValue));
}

bool GetEnumDisplayNamesAndValues(const UEnum* EnumClass, TArray<TPair<FText, int64>>& OutDisplayNames);
bool GetEnumDisplayNamesAndValues(const FString& EnumClassName, TArray<TPair<FText, int64>>& OutDisplayNames);

template<typename TEnum>
static FORCEINLINE bool GetEnumDisplayNamesAndValues(TArray<TPair<FText, int64>>& OutDisplayNames)
{
	return GetEnumDisplayNames(StaticEnum<TEnum>(), OutDisplayNames);
}