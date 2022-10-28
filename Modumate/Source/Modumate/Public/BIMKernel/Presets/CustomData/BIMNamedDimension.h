#pragma once

#include "BIMNamedDimension.generated.h"

USTRUCT()
struct FBIMNamedDimension
{
	GENERATED_BODY()

	UPROPERTY()
	FString DimensionKey;

	UPROPERTY()
	FString DisplayName;

	UPROPERTY()
	float DefaultValue = 0.0f;

	UPROPERTY()
	FString UIGroup;

	UPROPERTY()
	FString Description;
};
