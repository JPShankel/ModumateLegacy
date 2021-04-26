// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "BIMKernel/Core/BIMProperties.h"
#include "BIMKernel/Core/BIMEnums.h"
#include "BIMKernel/Core/BIMKey.h"
#include "BIMKernel/Presets/BIMPresetInstance.h"
#include "BIMKernel/AssemblySpec/BIMLegacyPattern.h"
#include "BIMKernel/Presets/BIMPresetMaterialBinding.h"
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

	EBIMResult BuildPatternedLayer(const FModumateDatabase& InDB);
	EBIMResult BuildFromProperties(const FModumateDatabase& InDB);
	void EvaluateParameterizedPatternExtents();
	static void PopulatePatternModuleVariables(TMap<FString, float>& patternExprVars, const FVector& moduleDims, int32 moduleIdx);

	EBIMResult UpdatePatternFromPreset(const FModumateDatabase& InDB, const FBIMPresetInstance& Preset);

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

	UPROPERTY()
	FGuid PresetGUID;

	TMap<FString, float> CachedPatternExprVars;
  
	UPROPERTY()
	EPresetMeasurementMethod MeasurementMethod = EPresetMeasurementMethod::None;
};
