#include "BIMKernel/Presets/CustomData/BIMIESProfile.h"

bool FBIMIESProfile::operator==(const FBIMIESProfile& Other) const
{
	return Other.AssetPath == AssetPath && Other.CraftingIconAssetFilePath == CraftingIconAssetFilePath;
}

UStruct* FBIMIESProfile::VirtualizedStaticStruct()
{
	return FBIMIESProfile::StaticStruct();
}
