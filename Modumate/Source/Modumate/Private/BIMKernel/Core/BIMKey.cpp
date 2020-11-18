// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "BIMKernel/Core/BIMKey.h"
#include "Hash/CityHash.h"

FBIMKey::FBIMKey() : StringValue()
{
	UpdateHash();
}

FBIMKey::FBIMKey(FString InStr) : StringValue(MoveTemp(InStr))
{
	UpdateHash();
}

bool FBIMKey::ExportTextItem(FString& ValueStr, FBIMKey const& DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const
{
	if (0 != (PortFlags & EPropertyPortFlags::PPF_ExportCpp))
	{
		ValueStr += FString::Printf(TEXT("FBIMKey(TEXT(\"%s\"))"), *StringValue);
		return true;
	}

	ValueStr += StringValue;
	return true;
}

bool FBIMKey::ImportTextItem(const TCHAR*& Buffer, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText)
{
	StringValue = Buffer;
	UpdateHash();
	return true;
}

void FBIMKey::UpdateHash()
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

FBIMKey operator+(const FBIMKey& LHS, const FBIMKey& RHS)
{
	return FBIMKey(LHS.StringValue + RHS.StringValue);
}
