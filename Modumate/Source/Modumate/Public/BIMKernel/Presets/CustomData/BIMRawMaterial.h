#pragma once

#include "BIMRawMaterial.generated.h"

USTRUCT()
struct FBIMRawMaterial
{
	GENERATED_BODY()
	
	// Path to the Unreal material asset
	UPROPERTY()
	FString AssetPath;

	// Doesn't seem to be used currently.
	UPROPERTY()
	float TextureTilingSize;
};
