// Copyright 2022 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BIMPresetNCPTaxonomy.h"
#include "BIMKernel/AssemblySpec/BIMLegacyPattern.h"
#include "BIMKernel/Presets/BIMPresetInstance.h"
#include "BIMPresetInstanceFactory.generated.h"

USTRUCT()
struct MODUMATE_API FBIMPresetInstanceFactory
{
	GENERATED_BODY()

	static bool DeserializeCustomData(EPresetPropertyMatrixNames MatrixName ,FBIMPresetInstance& OutPreset, const FBIMWebPreset& WebPreset);
	static void SetBlankCustomDataByPropertyName(EPresetPropertyMatrixNames MatrixName, FBIMPresetInstance& OutPreset);
	static bool CreateBlankPreset(FBIMTagPath& TagPath, const FBIMPresetNCPTaxonomy& Taxonomy, FBIMPresetInstance& OutPreset);
	static bool AddMissingCustomDataToPreset(FBIMPresetInstance& Preset, const FBIMPresetNCPTaxonomy& Taxonomy);
	static bool GetCustomDataStaticStruct(EPresetPropertyMatrixNames MatrixName, UStruct*& OutStruct);
};
