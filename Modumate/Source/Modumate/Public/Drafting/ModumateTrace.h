// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "ModumateTrace.generated.h"

USTRUCT()
struct FModumateTraceVertex
{
	GENERATED_BODY();

	UPROPERTY()
	float x { NAN };
	UPROPERTY()
	float y { NAN };
};

USTRUCT()
struct FModumateTraceSplineEntry
{
	GENERATED_BODY();

	UPROPERTY()
	FModumateTraceVertex open;
	UPROPERTY()
	FModumateTraceVertex closed;
	UPROPERTY()
	FModumateTraceVertex linear;
	UPROPERTY()
	TArray<FModumateTraceVertex> cubic;
	UPROPERTY()
	float depth { NAN };
};

USTRUCT()
struct MODUMATE_API FModumateTraceObject
{
	GENERATED_BODY();

	UPROPERTY()
	TArray<FModumateTraceSplineEntry> lines;
};
