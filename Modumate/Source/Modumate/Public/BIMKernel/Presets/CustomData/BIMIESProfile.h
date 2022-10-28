#pragma once

#include "BIMIESProfile.generated.h"


USTRUCT()
struct FBIMIESProfile
{
	GENERATED_BODY()

	UPROPERTY()
	FString AssetPath;

	UPROPERTY()
	FString CraftingIconAssetFilePath;
};