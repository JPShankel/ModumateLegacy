// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "BIMKernel/AssemblySpec/BIMLayerSpec.h"
#include "ModumateCore/ModumateDimensionStatics.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "Database/ModumateObjectDatabase.h"

EBIMResult FBIMLayerSpec::BuildFromProperties(const FModumateDatabase& InDB)
{
	if (Pattern.Key.IsValid())
	{
		return BuildPatternedLayer(InDB);
	}

	return ensureAlways(ThicknessCentimeters > 0.0f) ? EBIMResult::Success : EBIMResult::Error;
}

/*
TODO: a "patterned" layer reads properties corresponding to the DDL 1.0 table entries for patterns
The parameters established here are accessed in UModumateFunctionLibrary::ApplyTileMaterialToMeshFromLayer
We want to refactor this for new patterns
*/
EBIMResult FBIMLayerSpec::BuildPatternedLayer(const FModumateDatabase& InDB)
{
	EvaluateParameterizedPatternExtents();

	return EBIMResult::Success;
}

void FBIMLayerSpec::PopulatePatternModuleVariables(TMap<FString, float>& patternExprVars, const FVector& moduleDims, int32 moduleIdx)
{
	auto makeModuleDimensionKey = [](int32 idx, const TCHAR* dimensionPrefix) {
		return (idx == 0) ? FString(dimensionPrefix) : FString::Printf(TEXT("%s%d"), dimensionPrefix, idx + 1);
	};

	patternExprVars.Add(makeModuleDimensionKey(moduleIdx, TEXT("L")), moduleDims.X);
	patternExprVars.Add(makeModuleDimensionKey(moduleIdx, TEXT("W")), moduleDims.Y);
	patternExprVars.Add(makeModuleDimensionKey(moduleIdx, TEXT("H")), moduleDims.Z);
}

void FBIMLayerSpec::EvaluateParameterizedPatternExtents()
{
	CachedPatternExprVars.Add(FString(TEXT("G")), Gap.GapExtents.X);
	// Define the dimension parameters for each module definition
	for (int32 moduleIdx = 0; moduleIdx < Pattern.ModuleCount; ++moduleIdx)
	{
		auto& moduleData = Modules[moduleIdx];

		// Define L, W, and H since some 3D patterns can be applied to 2D modules,
		// so make sure all extents are defined for all modules.
		FVector moduleDims = moduleData.ModuleExtents;
		if (moduleDims.Z == 0.0f)
		{
			moduleDims.Z = moduleDims.Y;
		}

		PopulatePatternModuleVariables(CachedPatternExprVars, moduleDims, moduleIdx);
	}

	float fixedExtent;

	FString extentsExpressions = Pattern.ParameterizedExtents;
	if (LexTryParseString(fixedExtent, *extentsExpressions))
	{
		Pattern.CachedExtents.X = fixedExtent;
		Pattern.CachedExtents.Y = fixedExtent;
	}
	else
	{
		extentsExpressions.RemoveFromStart(TEXT("("));
		extentsExpressions.RemoveFromEnd(TEXT(")"));
		FString patternWidthExpr, patternHeightExpr;

		if (extentsExpressions.Split(TEXT(","), &patternWidthExpr, &patternHeightExpr))
		{
			Pattern.CachedExtents.X = Modumate::Expression::Evaluate(CachedPatternExprVars, patternWidthExpr);
			Pattern.CachedExtents.Y = Modumate::Expression::Evaluate(CachedPatternExprVars, patternHeightExpr);
		}
	}
}

/*
* Construction function called from FBIMAssemblySpec::FromPreset
* This function may be called multiple times with different component presets (module, pattern, gap)
*/

EBIMResult FBIMLayerSpec::UpdatePatternFromPreset(const FModumateDatabase& InDB,const FBIMPresetInstance& Preset)
{
	/*
	* If we have a pattern, grab it
	*/
	FString patternGuidString;
	FGuid patternGuid;
	if (Preset.Properties.TryGetProperty(EBIMValueScope::Pattern, BIMPropertyNames::AssetID, patternGuidString) && FGuid::Parse(patternGuidString, patternGuid))
	{
		const FLayerPattern* pattern = InDB.GetPatternByGUID(patternGuid);
		if (ensureAlways(pattern != nullptr))
		{
			Pattern = *pattern;
		}
	}

	// A "gap" will either be in the gap scope or be one of the undimensioned gaps used for stud walls indicated by these NPCs (in Module scope)
	static const FBIMTagPath gapModule2D(TEXT("Part_1FlexDim2Fixed_Gap2D"));
	static const FBIMTagPath gapModule1D(TEXT("Part_1FlexDim2Fixed_Gap1D"));

	if (Preset.NodeScope == EBIMValueScope::Gap || gapModule2D.MatchesPartial(Preset.MyTagPath) || gapModule1D.MatchesPartial(Preset.MyTagPath))
	{
		// retrieve material bindings and dimensions for Gap
		FBIMPresetMaterialBindingSet materialBindings;
		if (Preset.TryGetCustomData(materialBindings))
		{
			if (ensureAlways(materialBindings.MaterialBindings.Num() > 0))
			{
				ensureAlways(materialBindings.MaterialBindings[0].GetEngineMaterial(InDB, Gap.Material) == EBIMResult::Success);
				FVector gapExtents3;
				float bevelWidth,thickness;
				Preset.GetModularDimensions(gapExtents3,bevelWidth,thickness);
				Gap.GapExtents = FVector2D(gapExtents3.X,gapExtents3.Y);
				return EBIMResult::Success;
			}
		}
		return EBIMResult::Error;
	}

	/*
	* If we're not building a gap, we're building the module list. This should only happen once per layer.
	*/
	if (!ensureAlways(Modules.Num() == 0))
	{
		return EBIMResult::Error;
	}

	FVector modularDimensions = FVector::ZeroVector;
	float bevelWidth;
	Preset.GetModularDimensions(modularDimensions,bevelWidth,ThicknessCentimeters);

	// Add one module for each material binding
	// TODO: allow heterogenous module sets
	FBIMPresetMaterialBindingSet materialBindings;
	if (Preset.TryGetCustomData(materialBindings) && ensureAlways(materialBindings.MaterialBindings.Num() > 0))
	{
		// Make modules and load their materials and extents
		for (auto& materialBinding : materialBindings.MaterialBindings)
		{
			FLayerPatternModule& module = Modules.AddDefaulted_GetRef();
			if (!ensureAlways(materialBinding.GetEngineMaterial(InDB, module.Material) == EBIMResult::Success))
			{
				const FArchitecturalMaterial* material = InDB.GetArchitecturalMaterialByGUID(InDB.GetDefaultMaterialGUID());
				if (ensureAlways(material != nullptr))
				{
					module.Material = *material;
				}
			}

			module.ModuleExtents = modularDimensions;
			module.BevelWidthCentimeters = bevelWidth;
			module.Key = Preset.GUID;
		}
	}

	Preset.TryGetCustomData(LayerPriority);

	return EBIMResult::Success;
}
