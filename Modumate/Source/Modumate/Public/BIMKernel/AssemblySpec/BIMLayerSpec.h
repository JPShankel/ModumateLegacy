// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "BIMKernel/Core/BIMProperties.h"
#include "BIMKernel/Core/BIMEnums.h"
#include "BIMKernel/Core/BIMKey.h"
#include "BIMKernel/AssemblySpec/BIMLegacyPattern.h"
#include "Database/ModumateArchitecturalMaterial.h"
#include "ModumateCore/ModumateUnits.h"
#include "BIMLayerSpec.generated.h"

class FModumateDatabase;

USTRUCT()
struct MODUMATE_API FBIMLayerSpec
{
	GENERATED_BODY()
	friend struct FBIMAssemblySpec;
private:
	
	UPROPERTY()
	FBIMPropertySheet LayerProperties;

	UPROPERTY()
	TArray<FBIMPropertySheet> ModuleProperties;

	UPROPERTY()
	FBIMPropertySheet GapProperties;

	EBIMResult BuildUnpatternedLayer(const FModumateDatabase& InDB);
	EBIMResult BuildPatternedLayer(const FModumateDatabase& InDB);
	EBIMResult BuildFromProperties(const FModumateDatabase& InDB);

public:
	UPROPERTY()
	ELayerFunction Function = ELayerFunction::None;

	UPROPERTY()
	FString CodeName;

	UPROPERTY()
	FString PresetSequence;

	UPROPERTY()
	float ThicknessCentimeters;

	// TODO: this is the DDL 1.0 pattern loadout...to be extended
	UPROPERTY()
	FLayerPattern Pattern;

	UPROPERTY()
	TArray<FLayerPatternModule> Modules;

	UPROPERTY()
	FLayerPatternGap Gap;
};
