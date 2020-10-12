// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "BIMKernel/BIMProperties.h"
#include "BIMKernel/BIMEnums.h"
#include "BIMKernel/BIMKey.h"
#include "BIMKernel/BIMLegacyPattern.h"
#include "Database/ModumateArchitecturalMaterial.h"
#include "ModumateCore/ModumateUnits.h"

class FModumateDatabase;

class MODUMATE_API FBIMLayerSpec
{
	friend class FBIMAssemblySpec;
private:
	FBIMPropertySheet LayerProperties;
	TArray<FBIMPropertySheet> ModuleProperties;
	FBIMPropertySheet GapProperties;

	ECraftingResult BuildUnpatternedLayer(const FModumateDatabase& InDB);
	ECraftingResult BuildPatternedLayer(const FModumateDatabase& InDB);
	ECraftingResult BuildFromProperties(const FModumateDatabase& InDB);

public:
	ELayerFunction Function = ELayerFunction::None;

	FString CodeName;
	FString PresetSequence;

	Modumate::Units::FUnitValue Thickness;

	// TODO: Modules with materials, patterns that reference modules
	FArchitecturalMaterial Material;

	// TODO: this is the DDL 1.0 pattern loadout...to be extended
	FLayerPattern Pattern;
	TArray<FLayerPatternModule> Modules;
	FLayerPatternGap Gap;
	FCustomColor BaseColor;
};