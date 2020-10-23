// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "BIMKernel/BIMLayerSpec.h"
#include "ModumateCore/ModumateDimensionStatics.h"
#include "Database/ModumateObjectDatabase.h"

ECraftingResult FBIMLayerSpec::BuildFromProperties(const FModumateDatabase& InDB)
{
	if (ModuleProperties.Num() == 0)
	{
		ECraftingResult res = BuildUnpatternedLayer(InDB);
		if (res != ECraftingResult::Success)
		{
			return res;
		}
	}
	else
	{
		ECraftingResult res = BuildPatternedLayer(InDB);
		if (res != ECraftingResult::Success)
		{
			return res;
		}
	}

	return ensureAlways(Thickness.AsWorldCentimeters() > 0.0f) ? ECraftingResult::Success : ECraftingResult::Error;
}

/*
TODO: an "unpatterened" layer (ie abstract, cast in place concrete, etc), just has a material and thickness.
As we refactor patterns, we would like "unpatterened" layers to consist of a single module with a single dimension (thickness)
*/
ECraftingResult FBIMLayerSpec::BuildUnpatternedLayer(const FModumateDatabase& InDB)
{
	FString materialKey;
	if (ensureAlways(LayerProperties.TryGetProperty(EBIMValueScope::Material, BIMPropertyNames::AssetID, materialKey)))
	{
		const FArchitecturalMaterial* mat = InDB.GetArchitecturalMaterialByKey(FBIMKey(materialKey));
		if (ensureAlways(mat != nullptr))
		{
			Material_DEPRECATED = *mat;
			ensureAlways(Material_DEPRECATED.EngineMaterial != nullptr);
		}
	}

	FString colorKey;
	if (ensureAlways(LayerProperties.TryGetProperty(EBIMValueScope::Color, BIMPropertyNames::AssetID, colorKey)))
	{
		const FCustomColor* customColor = InDB.GetCustomColorByKey(FBIMKey(colorKey));
		if (customColor != nullptr)
		{
			Material_DEPRECATED.DefaultBaseColor = *customColor;
		}
	}

	FString thickness;

	if (!LayerProperties.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Thickness, thickness))
	{
		if (!LayerProperties.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Depth, thickness))
		{
			LayerProperties.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Width, thickness);
		}
	}

	if (ensureAlways(!thickness.IsEmpty()))
	{
		FModumateFormattedDimension dim = UModumateDimensionStatics::StringToFormattedDimension(thickness);
		if (ensureAlways(dim.Format != EDimensionFormat::Error))
		{
			Thickness = Modumate::Units::FUnitValue::WorldCentimeters(dim.Centimeters);
		}
	}
	else
	{
		return ECraftingResult::Error;
	}

	return ECraftingResult::Success;
}

/*
TODO: a "patterened" layer reads properties corresponding to the DDL 1.0 table entries for patterns
The parameters established here are accessed in UModumateFunctionLibrary::ApplyTileMaterialToMeshFromLayer
We want to refactor this for new patterns
*/
ECraftingResult FBIMLayerSpec::BuildPatternedLayer(const FModumateDatabase& InDB)
{
	/*
	ModuleProperties contain the properties for each uniquely materialed module in the pattern
	Patterns with more than one module shape may have only one module definition but multiple sets of module parameters (ie Stack Bond)
	*/
	for (auto& modProps : ModuleProperties)
	{
		FLayerPatternModule& module = Modules.AddDefaulted_GetRef();

		// Get the material and color for this module
		FString matColKey;
		if (ensureAlways(modProps.TryGetProperty(EBIMValueScope::Material, BIMPropertyNames::AssetID, matColKey)))
		{
			const FArchitecturalMaterial* mat = InDB.GetArchitecturalMaterialByKey(matColKey);
			if (ensureAlways(mat != nullptr))
			{
				module.Material = *mat;
				if (modProps.TryGetProperty(EBIMValueScope::Color, BIMPropertyNames::AssetID, matColKey))
				{
					const FCustomColor* color = InDB.GetCustomColorByKey(matColKey);
					if (ensureAlways(color != nullptr))
					{
						module.Material.DefaultBaseColor = *color;
					}
				}
			}
		}

		/*
		TODO: Module dimension sets come in multiple formats
		We need to reconcile those against the actual targets for X, Y & Z
		For now "everyone has a depth or a length and a thickness and a width" is a reasonable approximation
		*/
		FString dimStr;
		if (!modProps.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Depth, dimStr))
		{
			modProps.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Length, dimStr);
			if (!dimStr.IsEmpty())
			{
				FModumateFormattedDimension dim = UModumateDimensionStatics::StringToFormattedDimension(dimStr);
				if (ensureAlways(dim.Format != EDimensionFormat::Error))
				{
					module.ModuleExtents.X = dim.Centimeters;
				}
			}
		}

		if (modProps.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Thickness, dimStr))
		{
			FModumateFormattedDimension dim = UModumateDimensionStatics::StringToFormattedDimension(dimStr);
			if (ensureAlways(dim.Format != EDimensionFormat::Error))
			{
				module.ModuleExtents.Y = dim.Centimeters;
			}
		}

		if (modProps.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Width, dimStr))
		{
			FModumateFormattedDimension dim = UModumateDimensionStatics::StringToFormattedDimension(dimStr);
			if (ensureAlways(dim.Format != EDimensionFormat::Error))
			{
				module.ModuleExtents.Z = dim.Centimeters;
			}
		}
	}

	/*
	TODO: patterns 2.0 to allow the definition of multiple gaps, for now a single gap exists at the layer level per DDL 1.0
	Should we combine modules and gaps into a single concept? 
	*/

	FString gapStr;
	if (GapProperties.TryGetProperty(EBIMValueScope::Material, BIMPropertyNames::AssetID, gapStr))
	{
		// Fetch material & color per module
		const FArchitecturalMaterial* mat = InDB.GetArchitecturalMaterialByKey(gapStr);
		if (ensureAlways(mat != nullptr))
		{
			Gap.Material = *mat;
			if (GapProperties.TryGetProperty(EBIMValueScope::Color, BIMPropertyNames::AssetID, gapStr))
			{
				const FCustomColor* color = InDB.GetCustomColorByKey(gapStr);
				if (ensureAlways(color != nullptr))
				{
					Gap.Material.DefaultBaseColor = *color;
				}
			}
		}
		/*
		Gap dimensions are reliably defined
		*/
		if (ensureAlways(GapProperties.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Width, gapStr)))
		{
			FModumateFormattedDimension dim = UModumateDimensionStatics::StringToFormattedDimension(gapStr);
			if (ensureAlways(dim.Format != EDimensionFormat::Error))
			{
				Gap.GapExtents.X = dim.Centimeters;
			}
		}

		if (ensureAlways(GapProperties.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Depth, gapStr)))
		{
			FModumateFormattedDimension dim = UModumateDimensionStatics::StringToFormattedDimension(gapStr);
			if (ensureAlways(dim.Format != EDimensionFormat::Error))
			{
				Gap.GapExtents.Y = dim.Centimeters;
			}
		}
	}

	// TODO: handle multiple modules, find overall thickness for 2.5 dimensional patterns 
	if (Modules[0].ModuleExtents.Y > 0)
	{
		Thickness = Modumate::Units::FUnitValue::WorldCentimeters(Modules[0].ModuleExtents.Y);
	}
	else if (Modules[0].ModuleExtents.X > 0)
	{
		Thickness = Modumate::Units::FUnitValue::WorldCentimeters(Modules[0].ModuleExtents.X);
	}
	else if (Modules[0].ModuleExtents.Z > 0)
	{
		Thickness = Modumate::Units::FUnitValue::WorldCentimeters(Modules[0].ModuleExtents.Z);
	}

	// TODO: 1-dimensional patterns still depend on deprecated layer material
	Material_DEPRECATED = Modules[0].Material;

	return ECraftingResult::Success;
}

