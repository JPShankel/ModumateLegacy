// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

class UModumateDocument;

struct FMOIDocumentRecordV4;
using FMOIDocumentRecord = FMOIDocumentRecordV4;

// Helper functions for delta preview operations
class FModumateObjectDeltaStatics
{
public:
	static void GetTransformableIDs(const TArray<int32>& InObjectIDs, UModumateDocument *doc, TSet<int32>& OutTransformableIDs);
	// Get deltas for movement of an object
	static bool MoveTransformableIDs(const TMap<int32, FTransform>& ObjectMovements, UModumateDocument *doc, UWorld *World, bool bIsPreview);

	static void SaveSelection(const TArray<int32>& InObjectIDs, UModumateDocument* doc, FMOIDocumentRecord* OutRecord);

	static bool PasteObjects(const FMOIDocumentRecord* InRecord, const FVector& InOffset, UModumateDocument* doc, UWorld* World, bool bIsPreview);
};
