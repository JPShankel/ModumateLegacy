// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "BIMKernel/BIMKey.h"
#include "Hash/CityHash.h"

FBIMKey::FBIMKey() 
{
	HashValue = 0;
}

FBIMKey::FBIMKey(const FString& InStr) : StringValue(InStr)
{
	// From UnrealHeaderTool/StringUtils.cpp
	uint64 hash64 = CityHash64(reinterpret_cast<const char *>(*StringValue), StringValue.Len() * sizeof(TCHAR));
	HashValue = static_cast<uint32>(hash64) + static_cast<uint32>(hash64 >> 32);
}

bool FBIMKey::operator==(const FBIMKey& RHS) const
{
	if (RHS.HashValue != HashValue)
	{
		return false;
	}
	return RHS.StringValue.Equals(StringValue);
}

bool FBIMKey::operator!=(const FBIMKey& RHS) const
{
	return !(*this == RHS);
}
