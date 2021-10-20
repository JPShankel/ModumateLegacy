// Copyright 2021 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

#include "DrawingDesignerView.generated.h"

USTRUCT()
struct MODUMATE_API FDrawingDesignerViewSnap
{
	GENERATED_BODY()

	UPROPERTY()
	int32 id = INDEX_NONE;

	// Normalized in range 0-1
	UPROPERTY()
	float x = 0.0f;

	UPROPERTY()
	float y = 0.0f;

	bool operator==(const FDrawingDesignerViewSnap& RHS) const;
	bool operator!=(const FDrawingDesignerViewSnap& RHS) const;
};

USTRUCT()
struct MODUMATE_API FDrawingDesignerView 
{
	GENERATED_BODY()

	FDrawingDesignerView();

	UPROPERTY()
	int32 id = INDEX_NONE;
	
	UPROPERTY()
	FString image;

	UPROPERTY() // FString key encoded from int32 id
	TMap<FString, FDrawingDesignerViewSnap> snaps;

	bool operator==(const FDrawingDesignerView& RHS) const;
	bool operator!=(const FDrawingDesignerView& RHS) const;
};

