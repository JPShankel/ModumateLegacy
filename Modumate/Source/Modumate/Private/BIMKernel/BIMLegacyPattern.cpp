// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "BIMKernel/BIMLegacyPattern.h"
#include "BIMKernel/BIMPresets.h"
#include "Algo/Transform.h"

using namespace Modumate::Units;

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

void FLayerPattern::InitFromCraftingPreset(const FBIMPreset& Preset)
{
	Key = Preset.PresetID;

	FString localName;
	if (Preset.TryGetProperty(BIMPropertyNames::Name, localName))
	{
		DisplayName = FText::FromString(localName);
	}

	ensureAlways(Preset.TryGetProperty(BIMPropertyNames::ModuleCount, ModuleCount));
	ensureAlways(Preset.TryGetProperty(BIMPropertyNames::Extents, ParameterizedExtents));
	ensureAlways(Preset.TryGetProperty(BIMPropertyNames::Thickness, ThicknessDimensionPropertyName));

	int32 moduleIndex = 1;
	FString modString = FString::Printf(TEXT("Module%dDimensions"), moduleIndex);
	FString dimParam;
	while (Preset.TryGetProperty(*modString, dimParam))
	{
		if (!dimParam.IsEmpty())
		{
			ParameterizedModuleDimensions.Add(FPatternModuleTemplate(dimParam));
		}
		++moduleIndex;
		modString = FString::Printf(TEXT("Module%dDimensions"), moduleIndex);
	}
}


