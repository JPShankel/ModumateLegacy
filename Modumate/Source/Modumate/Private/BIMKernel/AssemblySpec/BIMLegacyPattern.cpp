// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "BIMKernel/AssemblySpec/BIMLegacyPattern.h"
#include "BIMKernel/Presets/BIMPresetInstance.h"
#include "Algo/Transform.h"
#include "BIMKernel/Presets/CustomData/BIMPattern.h"


FPatternModuleTemplate::FPatternModuleTemplate(const FString &DimensionStringsCombined)
	: OriginalString(DimensionStringsCombined)
{
	static const FString commaStr(TEXT(","));
	FString splitTempStr = OriginalString;
	FString moduleDefIndexStr;

	splitTempStr.RemoveFromStart(TEXT("("));
	splitTempStr.Split(commaStr, &ModuleXExpr, &splitTempStr);
	splitTempStr.Split(commaStr, &ModuleYExpr, &splitTempStr);
	ModuleYExpr.RemoveFromEnd(TEXT(")"));
	splitTempStr.Split(commaStr, &moduleDefIndexStr, &splitTempStr);
	splitTempStr.Split(commaStr, &ModuleWidthExpr, &ModuleHeightExpr);

	moduleDefIndexStr.RemoveFromStart(TEXT("Module"));
	ModuleDefIndex = FCString::Atoi(*moduleDefIndexStr) - 1;

	bIsValid = !ModuleXExpr.IsEmpty() && !ModuleYExpr.IsEmpty() &&
		!ModuleWidthExpr.IsEmpty() && !ModuleHeightExpr.IsEmpty() &&
		(ModuleDefIndex >= 0);
}

void FLayerPattern::InitFromCraftingPreset(const FBIMPresetInstance& Preset)
{
	Key = Preset.GUID;
	DisplayName = Preset.DisplayName;

	FBIMPatternConfig patternConfig;
	if (ensureAlways(Preset.TryGetCustomData(patternConfig)))
	{
		ModuleCount = patternConfig.ModuleCount;
		ParameterizedExtents = patternConfig.Extents;
		ThicknessDimensionPropertyName = FName(patternConfig.Thickness);

		for (auto moduleDimension : patternConfig.ModuleDimensions)
		{
			if (!moduleDimension.IsEmpty())
			{
				ParameterizedModuleDimensions.Add(FPatternModuleTemplate(moduleDimension));	
			}
		}
	}
}

UStruct* FLightConfiguration::VirtualizedStaticStruct()
{
	return FLightConfiguration::StaticStruct();
}


