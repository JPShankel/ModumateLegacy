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

	return ensureAlways(ThicknessCentimeters > 0.0f) ? EBIMResult::Success : EBIMResult::Error;
}

/*
TODO: an "unpatterened" layer (ie abstract, cast in place concrete, etc), just has a material and thickness.
As we refactor patterns, we would like "unpatterened" layers to consist of a single module with a single dimension (thickness)
*/
EBIMResult FBIMLayerSpec::BuildUnpatternedLayer(const FModumateDatabase& InDB)
{
	FModumateUnitValue dimension = FModumateUnitValue::WorldCentimeters(0.0f);
	if (!LayerProperties.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Thickness, dimension))
	{
		if (!LayerProperties.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Depth, dimension))
		{
			LayerProperties.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Width, dimension);
		}
	}
	ThicknessCentimeters = dimension.AsWorldCentimeters();

	if (ModuleMaterialBindingSet.MaterialBindings.Num() > 0)
	{
		const auto& binding = ModuleMaterialBindingSet.MaterialBindings[0];
		FLayerPatternModule& module = Modules.AddDefaulted_GetRef();
		return binding.GetEngineMaterial(InDB, module.Material);
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
	if (!ensureAlways(ModuleMaterialBindingSet.MaterialBindings.Num() == ModuleProperties.Num()))
	{
		return EBIMResult::Error;
	}

	/*
	ModuleProperties contain the properties for each uniquely materialed module in the pattern
	Patterns with more than one module shape may have only one module definition but multiple sets of module parameters (ie Stack Bond)
	*/
	for (auto& modBinding : ModuleMaterialBindingSet.MaterialBindings)
	{
		FLayerPatternModule& module = Modules.AddDefaulted_GetRef();
		ensureAlways(modBinding.GetEngineMaterial(InDB, module.Material) == EBIMResult::Success);
	}

	/*
	TODO: Module dimension sets come in multiple formats
	We need to reconcile those against the actual targets for X, Y & Z
	For now "everyone has a depth or a length and a thickness and a width" is a reasonable approximation
	*/		
	for (int32 i=0;i<Modules.Num();++i)
	{
		FLayerPatternModule& module = Modules[i];
		const FBIMPropertySheet& modProps = ModuleProperties[i];

		FModumateUnitValue bevelWidth;
		if (modProps.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::BevelWidth, bevelWidth))
		{
			module.BevelWidthCentimeters = bevelWidth.AsWorldCentimeters();
		}
		else
		{
			module.BevelWidthCentimeters = 0;
		}

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

	if (GapMaterialBindingSet.MaterialBindings.Num() > 0)
	{
		ensureAlways(GapMaterialBindingSet.MaterialBindings[0].GetEngineMaterial(InDB, Gap.Material) == EBIMResult::Success);
		ensureAlways(GapProperties.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Width, Gap.GapExtents.X));
		ensureAlways(GapProperties.TryGetProperty(EBIMValueScope::Dimension, BIMPropertyNames::Recess, Gap.GapExtents.Y));
	}

	// TODO: handle multiple modules, find overall thickness for 2.5 dimensional patterns 
	if (Modules[0].ModuleExtents.Y > 0)
	{
		ThicknessCentimeters = Modules[0].ModuleExtents.Y;
	}
	else if (Modules[0].ModuleExtents.X > 0)
	{
		ThicknessCentimeters = Modules[0].ModuleExtents.X;
	}
	else if (Modules[0].ModuleExtents.Z > 0)
	{
		ThicknessCentimeters = Modules[0].ModuleExtents.Z;
	}

	return EBIMResult::Success;
}

