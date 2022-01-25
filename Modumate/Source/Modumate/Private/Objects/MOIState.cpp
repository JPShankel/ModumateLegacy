// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Objects/MOIState.h"


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
		(CustomData == Other.CustomData);
}

bool FMOIStateData::operator!=(const FMOIStateData& Other) const
{
	return !(*this == Other);
}
