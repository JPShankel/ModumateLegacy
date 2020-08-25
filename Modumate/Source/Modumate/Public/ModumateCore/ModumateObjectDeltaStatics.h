// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

class FModumateDocument;

// Helper functions for delta preview operations
class FModumateObjectDeltaStatics
{
public:
	static void GetVertexIDs(const TArray<int32>& InObjectIDs, FModumateDocument *doc, TSet<int32>& OutVertexIDs);
	static bool PreviewMovement(const TMap<int32, FVector>& ObjectMovements, FModumateDocument *doc, UWorld *World);
};
