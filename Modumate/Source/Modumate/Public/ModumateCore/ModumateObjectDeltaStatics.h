// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

class FModumateDocument;

// Helper functions for delta preview operations
class FModumateObjectDeltaStatics
{
public:
	static bool PreviewMovement(const TMap<int32, TArray<FVector>>& ObjectMovements, FModumateDocument *doc, UWorld *World);
};
