// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "BIMKernel/BIMLegacyPattern.h"
#include "Algo/Transform.h"

using namespace Modumate::Units;

namespace Modumate
{
	namespace CraftingParameters
	{
		const FString ThicknessValue = TEXT("Thickness.Value");
		const FString ThicknessUnits = TEXT("Thickness.Units");

		const FString MaterialColorMaterial = TEXT("MaterialColor.Material");
		const FString MaterialColorColor = TEXT("MaterialColor.Color");

		const FString DimensionLength = TEXT("Dimension.Length");
		const FString DimensionWidth = TEXT("Dimension.Width");
		const FString DimensionHeight = TEXT("Dimension.Height");
		const FString DimensionDepth = TEXT("Dimension.Depth");
		const FString DimensionThickness = TEXT("Dimension.Thickness");
		const FString DimensionBevelWidth = TEXT("Dimension.BevelWidth");

		const FString PatternModuleCount = TEXT("Pattern.ModuleCount");
		const FString PatternExtents = TEXT("Pattern.Extents");
		const FString PatternThickness = TEXT("Pattern.Thickness");
		const FString PatternGap = TEXT("Pattern.Gap");
		const FString PatternName = TEXT("Pattern.Name");
		const FString PatternModuleDimensions = TEXT("Pattern.ModuleDimensions");

		const FString TrimProfileNativeSizeX = TEXT("TrimProfile.NativeSizeX");
		const FString TrimProfileNativeSizeY = TEXT("TrimProfile.NativeSizeY");
	}

}

void FLayerPatternModule::InitFromCraftingParameters(const Modumate::FModumateFunctionParameterSet &params)
{
	ModuleExtents.X = params.GetValue(Modumate::CraftingParameters::DimensionLength);
	ModuleExtents.Z = params.GetValue(Modumate::CraftingParameters::DimensionHeight);

	if (params.HasValue(Modumate::CraftingParameters::DimensionWidth))
	{
		ModuleExtents.Y = params.GetValue(Modumate::CraftingParameters::DimensionWidth);
	}
	else if (params.HasValue(Modumate::CraftingParameters::DimensionDepth))
	{
		ModuleExtents.Y = params.GetValue(Modumate::CraftingParameters::DimensionDepth);
	}
	else if (params.HasValue(Modumate::CraftingParameters::DimensionThickness))
	{
		ModuleExtents.Y = params.GetValue(Modumate::CraftingParameters::DimensionThickness);
	}

	BevelWidth = FUnitValue::WorldCentimeters(params.GetValue(Modumate::CraftingParameters::DimensionBevelWidth));
}

void FLayerPatternGap::InitFromCraftingParameters(const Modumate::FModumateFunctionParameterSet &params)
{
	GapExtents = FVector2D(
		params.GetValue(Modumate::CraftingParameters::DimensionWidth),
		params.GetValue(Modumate::CraftingParameters::DimensionDepth));
}

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

void FLayerPattern::InitFromCraftingParameters(const Modumate::FModumateFunctionParameterSet &params)
{
	ModuleCount = params.GetValue(Modumate::CraftingParameters::PatternModuleCount);
	ParameterizedExtents = params.GetValue(Modumate::CraftingParameters::PatternExtents);
	ParameterizedThickness = params.GetValue(Modumate::CraftingParameters::PatternThickness);

	Algo::Transform(
		params.GetValue(Modumate::CraftingParameters::PatternModuleDimensions).AsStringArray(), 
		ParameterizedModuleDimensions,
		[](const FString &modDim)
		{
			return FPatternModuleTemplate(modDim);
		}
	);
}

