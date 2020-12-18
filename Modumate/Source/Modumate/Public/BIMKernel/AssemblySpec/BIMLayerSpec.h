// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "BIMKernel/Core/BIMProperties.h"
#include "BIMKernel/Core/BIMEnums.h"
#include "BIMKernel/Core/BIMKey.h"
#include "BIMKernel/AssemblySpec/BIMLegacyPattern.h"
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

	EBIMResult BuildUnpatternedLayer(const FModumateDatabase& InDB);
	EBIMResult BuildPatternedLayer(const FModumateDatabase& InDB);
	EBIMResult BuildFromProperties(const FModumateDatabase& InDB);

public:
	ELayerFunction Function = ELayerFunction::None;

	FString CodeName;
	FString PresetSequence;

	Modumate::Units::FUnitValue Thickness;

	// TODO: this is the DDL 1.0 pattern loadout...to be extended
	FLayerPattern Pattern;
	TArray<FLayerPatternModule> Modules;
	FLayerPatternGap Gap;
};