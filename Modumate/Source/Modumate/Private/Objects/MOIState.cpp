// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Objects/MOIState.h"
#include "ModumateCore/PrettyJSONWriter.h"

FMOIStateData::FMOIStateData()
{
}

FMOIStateData::FMOIStateData(int32 InID, EObjectType InObjectType, int32 InParentID)
	: ID(InID)
	, ObjectType(InObjectType)
	, ParentID(InParentID)
{
}

bool FMOIStateData::operator==(const FMOIStateData& Other) const
{
	return (ID == Other.ID) &&
		(ObjectType == Other.ObjectType) &&
		(ParentID == Other.ParentID) &&
		(AssemblyGUID == Other.AssemblyGUID) &&
		(DisplayName == Other.DisplayName) &&
		(CustomData == Other.CustomData) &&
		(Alignment == Other.Alignment);
}

bool FMOIStateData::operator!=(const FMOIStateData& Other) const
{
	return !(*this == Other);
}

bool FMOIPresetZonePlane::operator==(const FMOIPresetZonePlane& Other) const
{
	return (moiId == Other.moiId) &&
		(zoneId == Other.zoneId) &&
		(origin == Other.origin) &&
		(FMath::IsNearlyEqual(displacement, Other.displacement));
}

bool FMOIPresetZonePlane::operator!=(const FMOIPresetZonePlane& Other) const
{
	return !(*this == Other);
}

bool FMOIAlignment::operator==(const FMOIAlignment& Other) const
{
	return (subjectPZP == Other.subjectPZP) &&
		(targetPZP == Other.targetPZP);
}

bool FMOIAlignment::operator!=(const FMOIAlignment& Other) const
{
	return !(*this == Other);
}

FString FMOIAlignment::ToString() const
{
	FString outputString;
	WriteJsonGeneric(outputString, this);
	return outputString;
}

void FMOIAlignment::FromString(FString& str)
{
	ReadJsonGeneric(str, this);
}
