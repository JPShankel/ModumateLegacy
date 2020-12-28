// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "BIMKernel/AssemblySpec/BIMLayerSpec.h"
#include "ModumateCore/ModumateDimensionStatics.h"
#include "Database/ModumateObjectDatabase.h"

EBIMResult FBIMLayerSpec::BuildFromProperties(const FModumateDatabase& InDB)
{
	if (ModuleProperties.Num() == 0)
	{
		EBIMResult res = BuildUnpatternedLayer(InDB);
		if (res != EBIMResult::Success)
		{
			return res;
		}
	}
	else
	{
		EBIMResult res = BuildPatternedLayer(InDB);
		if (res != EBIMResult::Success)
		{
			return res;
		}
	}

	return ensureAlways(Thickness.AsWorldCentimeters() > 0.0f) ? EBIMResult::Success : EBIMResult::Error;
}

/*
TODO: an "unpatterened" layer (ie abstract, cast in place concrete, etc), just has a material and thickness.
As we refactor patterns, we would like "unpatterened" layers to consist of a single module with a single dimension (thickness)
*/
EBIMResult FBIMLayerSpec::BuildUnpatternedLayer(const FModumateDatabase& InDB)
{
	FGuid materialKey;
	if (ensureAlways(LayerProperties.TryGetProperty(EBIMValueScope::RawMaterial, BIMPropertyNames::AssetID, materialKey)))
	{
		const FArchitecturalMaterial* mat = InDB.GetArchitecturalMaterialByGUID(materialKey);
		if (ensureAlways(mat != nullptr && mat->EngineMaterial.IsValid()))
		{
			FLayerPatternModule& module = Modules.AddDefaulted_GetRef();
			module.Material = *mat;

			FString colorHexValue;

			if (LayerProperties.TryGetProperty(EBIMValueScope::Color, BIMPropertyNames::HexValue, colorHexValue) && !colorHexValue.IsEmpty())
			{
				module.Material.Color = FColor::FromHex(colorHexValue);
			}

			if (!LayerProperties.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Thickness, Thickness))
			{
				if (!LayerProperties.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Depth, Thickness))
				{
					LayerProperties.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Width, Thickness);
				}
			}
			return EBIMResult::Success;
		}
	}
	return EBIMResult::Error;
}

/*
TODO: a "patterened" layer reads properties corresponding to the DDL 1.0 table entries for patterns
The parameters established here are accessed in UModumateFunctionLibrary::ApplyTileMaterialToMeshFromLayer
We want to refactor this for new patterns
*/
EBIMResult FBIMLayerSpec::BuildPatternedLayer(const FModumateDatabase& InDB)
{
	/*
	ModuleProperties contain the properties for each uniquely materialed module in the pattern
	Patterns with more than one module shape may have only one module definition but multiple sets of module parameters (ie Stack Bond)
	*/
	for (auto& modProps : ModuleProperties)
	{
		FLayerPatternModule& module = Modules.AddDefaulted_GetRef();

		// Get the material and color for this module
		FGuid rawMaterialKey;
		if (ensureAlways(modProps.TryGetProperty(EBIMValueScope::RawMaterial, BIMPropertyNames::AssetID, rawMaterialKey)))
		{
			const FArchitecturalMaterial* mat = InDB.GetArchitecturalMaterialByGUID(rawMaterialKey);
			if (ensureAlways(mat != nullptr))
			{
				module.Material = *mat;
				FString colorHexValue;
				if (modProps.TryGetProperty(EBIMValueScope::Color, BIMPropertyNames::HexValue, colorHexValue) && !colorHexValue.IsEmpty())
				{
					module.Material.Color = FColor::FromHex(colorHexValue);
				}
			}
		}

		/*
		TODO: Module dimension sets come in multiple formats
		We need to reconcile those against the actual targets for X, Y & Z
		For now "everyone has a depth or a length and a thickness and a width" is a reasonable approximation
		*/
		modProps.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::BevelWidth, module.BevelWidth);
		if (!modProps.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Depth, module.ModuleExtents.X))
		{
			modProps.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Length, module.ModuleExtents.X);
		}

		// The layer's thickness is either the module's thickness or whichever parameter (Depth or Width) the pattern specifies
		if (!modProps.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Thickness, module.ModuleExtents.Y))
		{
			modProps.TryGetProperty(EBIMValueScope::Dimension, Pattern.ThicknessDimensionPropertyName, module.ModuleExtents.Y);
		}

		modProps.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Width, module.ModuleExtents.Z);
	}

	/*
	TODO: patterns 2.0 to allow the definition of multiple gaps, for now a single gap exists at the layer level per DDL 1.0
	Should we combine modules and gaps into a single concept? 
	*/

	FGuid gapStr;
	if (GapProperties.TryGetProperty(EBIMValueScope::RawMaterial, BIMPropertyNames::AssetID, gapStr))
	{
		// Fetch material & color per module
		const FArchitecturalMaterial* mat = InDB.GetArchitecturalMaterialByGUID(gapStr);
		if (ensureAlways(mat != nullptr))
		{
			Gap.Material = *mat;
			FString colorHexValue;
			if (GapProperties.TryGetProperty(EBIMValueScope::Color, BIMPropertyNames::HexValue, colorHexValue) && !colorHexValue.IsEmpty())
			{
				Gap.Material.Color = FColor::FromHex(colorHexValue);
			}
		}
		/*
		Gap dimensions are reliably defined
		*/
		ensureAlways(GapProperties.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Width, Gap.GapExtents.X));
		ensureAlways(GapProperties.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Recess, Gap.GapExtents.Y));
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

	return EBIMResult::Success;
}

