// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "BIMKernel/BIMLegacyPortals.h"

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

	bool FPortalAssemblyConfigurationOption::IsValid() const
	{
		return (PortalFunction != EPortalFunction::None) && !Key.IsNone() && !DisplayName.IsEmpty() &&
			(ReferencePlanes[0].Num() >= 2) && (ReferencePlanes[2].Num() >= 2) && (Slots.Num() > 0);
	}
}