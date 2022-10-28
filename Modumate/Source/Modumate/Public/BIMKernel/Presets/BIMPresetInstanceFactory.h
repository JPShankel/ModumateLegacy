// Copyright 2022 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BIMPresetNCPTaxonomy.h"
#include "BIMKernel/Presets/BIMPresetInstance.h"
#include "BIMPresetInstanceFactory.generated.h"

UENUM()
enum class EPresetPropertyMatrixNames : uint8
{
	None = 0,
	IESLight,
	ConstructionCost,
	MiterPriority,
	PatternRef,
	ProfileRef,
	MeshRef,
	Material,
	Dimensions,
	Slots,
	InputPins,
	Dimension,
	RawMaterial,
	Profile,
	Mesh,
	Slot,
	IESProfile,
	Pattern,
	SlotConfig,
	Part,
	Preset,
	Error = 255
};

USTRUCT()
struct MODUMATE_API FBIMPresetInstanceFactory
{
	GENERATED_BODY()
	
	static UStruct* GetPresetMatrixStruct(EPresetPropertyMatrixNames MatrixName);
	static void SetBlankCustomDataByPropertyName(EPresetPropertyMatrixNames MatrixName, FBIMPresetInstance& OutPreset);
	
	static bool CreateBlankPreset(FBIMTagPath& TagPath, const FBIMPresetNCPTaxonomy& Taxonomy, FBIMPresetInstance& OutPreset);
	static bool AddMissingCustomDataToPreset(FBIMPresetInstance& Preset, const FBIMPresetNCPTaxonomy& Taxonomy);
};
