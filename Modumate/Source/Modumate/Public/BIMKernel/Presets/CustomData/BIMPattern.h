#pragma once

#include "BIMPattern.generated.h"

// also known as 2019 patterns in the legacy csvs
USTRUCT()
struct FBIMPatternConfig
{
	GENERATED_BODY()
	
	UPROPERTY()
	FString CraftingIconAssetFilePath;

	UPROPERTY()
	float ModuleCount;

	UPROPERTY()
	FString Extents;
	
	UPROPERTY()
    FString Thickness;

	UPROPERTY()
	TArray<FString> ModuleDimensions;
};
