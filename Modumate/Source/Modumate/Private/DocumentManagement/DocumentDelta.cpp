// Copyright 2020 Modumate, Inc,  All Rights Reserved.

#include "DocumentManagement/DocumentDelta.h"


FStructDataWrapper FDocumentDelta::SerializeStruct()
{
	// The base delta class should never be serialized!
	check(false);
	return FStructDataWrapper();
}

FDeltasRecord::FDeltasRecord()
{
}

FDeltasRecord::FDeltasRecord(const TArray<FDeltaPtr>& InDeltas, int32 InID, const FString& InOriginUserID)
	: ID(InID)
	, OriginUserID(InOriginUserID)
	, TimeStamp(FDateTime::Now())
{
	for (auto& deltaPtr : InDeltas)
	{
		DeltaStructWrappers.Add(deltaPtr->SerializeStruct());
	}
}

bool FDeltasRecord::IsEmpty() const
{
	return (DeltaStructWrappers.Num() == 0);
}

bool FDeltasRecord::operator==(const FDeltasRecord& Other) const
{
	return DeltaStructWrappers == Other.DeltaStructWrappers;
}

bool FDeltasRecord::operator!=(const FDeltasRecord& Other) const
{
	return !(*this == Other);
}
