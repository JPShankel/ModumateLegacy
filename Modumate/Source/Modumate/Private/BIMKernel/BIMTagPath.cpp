// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "BIMKernel/BIMTagPath.h"


bool FBIMTagGroup::MatchesAny(const FBIMTagGroup &OtherGroup) const
{
	for (auto &tag : *this)
	{
		if (OtherGroup.Contains(tag))
		{
			return true;
		}
	}
	return false;
}

bool FBIMTagGroup::MatchesAll(const FBIMTagGroup &OtherGroup) const
{
	if (Num() != OtherGroup.Num())
	{
		return false;
	}
	for (int32 i = 0; i < Num(); ++i)
	{
		if (!Contains(OtherGroup[i]))
		{
			return false;
		}
	}
	return true;
}

ECraftingResult FBIMTagGroup::FromString(const FString &InString)
{
	TArray<FString> tags;
	InString.ParseIntoArray(tags, TEXT(","));
	Empty();
	for (auto &tag : tags)
	{
		Add(*tag);
	}
	return ECraftingResult::Success;
}

ECraftingResult FBIMTagGroup::ToString(FString &OutString) const
{
	if (Num() == 0)
	{
		return ECraftingResult::Success;
	}
	OutString = (*this)[0].ToString();
	for (int32 i = 1; i < Num(); ++i)
	{
		OutString = FString::Printf(TEXT("%s,%s"), *OutString, *(*this)[i].ToString());
	}
	return ECraftingResult::Success;
}

ECraftingResult FBIMTagPath::FromString(const FString &InString)
{
	Empty();
	TArray<FString> groups;
	InString.ParseIntoArray(groups, TEXT("-->"));
	for (auto &group : groups)
	{
		FBIMTagGroup &newGroup = AddDefaulted_GetRef();
		newGroup.FromString(*group);
	}
	return ECraftingResult::Success;
}

ECraftingResult FBIMTagPath::ToString(FString &OutString) const
{
	if (Num() == 0)
	{
		return ECraftingResult::Success;
	}

	OutString = (*this)[0][0].ToString();

	for (int32 i = 1; i < Num(); ++i)
	{
		FString groupString;
		ECraftingResult result = (*this)[i].ToString(groupString);
		if (result != ECraftingResult::Success)
		{
			return result;
		}

		OutString = FString::Printf(TEXT("%s-->%s"), *OutString, *groupString);
	}

	return ECraftingResult::Success;
}

bool FBIMTagPath::MatchesExact(const FBIMTagPath &OtherPath) const
{
	if (Num() != OtherPath.Num())
	{
		return false;
	}

	for (int32 i = 0; i < Num(); ++i)
	{
		if (!OtherPath[i].MatchesAny((*this)[i]))
		{
			return false;
		}
	}

	return true;
}

bool FBIMTagPath::MatchesPartial(const FBIMTagPath& OtherPath) const
{
	for (int32 i = 0; i < FMath::Min(Num(),OtherPath.Num()); ++i)
	{
		if (!OtherPath[i].MatchesAny((*this)[i]))
		{
			return false;
		}
	}

	return true;
}
