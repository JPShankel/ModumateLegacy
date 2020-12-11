// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

class UModumateDocument;

// Helper functions for delta preview operations
class FModumateObjectDeltaStatics
{
public:
	static void GetTransformableIDs(const TArray<int32>& InObjectIDs, UModumateDocument *doc, TSet<int32>& OutTransformableIDs);
	// Get deltas for movement of an object
	static bool MoveTransformableIDs(const TMap<int32, FTransform>& ObjectMovements, UModumateDocument *doc, UWorld *World, bool bIsPreview);
};
