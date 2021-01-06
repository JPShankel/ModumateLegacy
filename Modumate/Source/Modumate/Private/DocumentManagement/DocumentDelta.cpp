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

FDeltasRecord::FDeltasRecord(const TArray<FDeltaPtr>& InDeltas)
{
	for (auto& deltaPtr : InDeltas)
	{
		DeltaStructWrappers.Add(deltaPtr->SerializeStruct());
	}
}
