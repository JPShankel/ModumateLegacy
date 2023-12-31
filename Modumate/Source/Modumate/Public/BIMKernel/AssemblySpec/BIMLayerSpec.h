// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "BIMKernel/Core/BIMProperties.h"
#include "BIMKernel/Core/BIMEnums.h"
#include "BIMKernel/Presets/BIMPresetInstance.h"
#include "BIMKernel/AssemblySpec/BIMLegacyPattern.h"
#include "BIMKernel/Presets/BIMPresetMaterialBinding.h"
#include "BIMKernel/Presets/BIMPresetLayerPriority.h"
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

	EBIMResult BuildPatternedLayer(const FBIMPresetCollectionProxy& InDB);
	EBIMResult BuildFromProperties(const FBIMPresetCollectionProxy& PresetCollection);
	void EvaluateParameterizedPatternExtents();
	static void PopulatePatternModuleVariables(TMap<FString, float>& patternExprVars, const FVector& moduleDims, int32 moduleIdx);

	EBIMResult UpdatePatternFromPreset(const FBIMPresetCollectionProxy& InDB, const FBIMPresetInstance& Preset);

public:
	UPROPERTY()
	FString CodeName;

	UPROPERTY()
	FString PresetSequence;

	UPROPERTY()
	float ThicknessCentimeters = 0.0f;

	// TODO: this is the DDL 1.0 pattern loadout...to be extended
	UPROPERTY()
	FLayerPattern Pattern;

	UPROPERTY()
	TArray<FLayerPatternModule> Modules;

	UPROPERTY()
	FLayerPatternGap Gap;

	UPROPERTY()
	FGuid PresetGUID;

	UPROPERTY()
	FString PresetZoneID;

	UPROPERTY()
	FString ZoneDisplayName;

	UPROPERTY()
	FBIMPresetLayerPriority LayerPriority;

	TMap<FString, float> CachedPatternExprVars;
  
	UPROPERTY()
	EPresetMeasurementMethod MeasurementMethod = EPresetMeasurementMethod::None;
};
