// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "BIMKernel/Core/BIMTagPath.h"

static constexpr TCHAR BIMTagPathSeparator[] = TEXT("_");

EBIMResult FBIMTagPath::FromString(const FString &InString)
{
	Tags.Reset();
	InString.ParseIntoArray(Tags, BIMTagPathSeparator);
	return EBIMResult::Success;
}

EBIMResult FBIMTagPath::ToString(FString &OutString) const
{
	if (Tags.Num() > 0)
	{
		OutString = Tags[0];

		for (int32 i = 1; i < Tags.Num(); ++i)
		{
			OutString = FString::Printf(TEXT("%s%s%s"), *OutString, BIMTagPathSeparator, *Tags[i]);
		}
	}

	return EBIMResult::Success;
}

EBIMResult FBIMTagPath::GetPartialPath(int32 Count, FBIMTagPath& PartialPath) const
{
	if (ensureAlways(Count > 0 && Tags.Num() >= Count))
	{
		PartialPath = *this;
		PartialPath.Tags.SetNum(Count);
		return EBIMResult::Success;
	}
	return EBIMResult::Error;
}

bool FBIMTagPath::MatchesExact(const FBIMTagPath &OtherPath) const
{
	if (Tags.Num() != OtherPath.Tags.Num())
	{
		return false;
	}

	for (int32 i = 0; i < Tags.Num(); ++i)
	{
		if (!OtherPath.Tags[i].Equals(Tags[i]))
		{
			return false;
		}
	}

	return true;
}

bool FBIMTagPath::MatchesPartial(const FBIMTagPath& OtherPath) const
{
	if (Tags.Num() == 0 || OtherPath.Tags.Num() == 0)
	{
		return false;
	}
	for (int32 i = 0; i < FMath::Min(Tags.Num(),OtherPath.Tags.Num()); ++i)
	{
		if (!OtherPath.Tags[i].Equals(Tags[i]))
		{
			return false;
		}
	}

	return true;
}

bool FBIMTagPath::Contains(const FString& Tag) const
{
	return Tags.Contains(Tag);
}

bool FBIMTagPath::operator==(const FBIMTagPath& RHS) const
{
	return RHS.Tags == Tags;
}

bool FBIMTagPath::operator!=(const FBIMTagPath& RHS) const
{
	return RHS.Tags != Tags;
}

FBIMTagPath::FBIMTagPath(const FString& InStr)
{
	FromString(InStr);
}

bool FBIMTagPath::ExportTextItem(FString& ValueStr, FBIMTagPath const& DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const
{
	FString myString;
	if (ToString(myString) != EBIMResult::Error)
	{
		if (0 != (PortFlags & EPropertyPortFlags::PPF_ExportCpp))
		{
			ValueStr += FString::Printf(TEXT("FBIMTagPath(TEXT(\"%s\"))"), *myString);
			return true;
		}
		ValueStr += myString;
		return true;
	}
	return false;
}

bool FBIMTagPath::ImportTextItem(const TCHAR*& Buffer, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText)
{
	if (FromString(Buffer) != EBIMResult::Error)
	{
		return true;
	}
	return false;
}
