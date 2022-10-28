// Copyright 2018 Modumate, Inc.All Rights Reserved.

#include "BIMKernel/AssemblySpec/BIMPartSlotSpec.h"
#include "BIMKernel/AssemblySpec/BIMAssemblySpec.h"
#include "BIMKernel/Presets/BIMPresetInstance.h"
#include "Algo/Transform.h"
#include "BIMKernel/Presets/CustomData/BIMDimensions.h"

TMap<FString, FPartNamedDimension> FBIMPartSlotSpec::NamedDimensionMap;

EBIMResult FBIMPartSlotSpec::BuildFromProperties(const FBIMPresetCollectionProxy& InDB)
{
	// TODO: not yet implemented
	ensureAlways(false);
	return EBIMResult::Error;
}

bool FBIMPartSlotSpec::TryGetDefaultNamedDimension(const FString& Name, FModumateUnitValue& OutVal)
{
	FPartNamedDimension* val = NamedDimensionMap.Find(Name);
	if (val == nullptr)
	{
		return false;
	}
	OutVal = val->DefaultValue;
	return true;
}

void FBIMPartSlotSpec::GetNamedDimensionValuesFromPreset(const FBIMPresetInstance* Preset)
{
	FBIMDimensions presetDimensions;
	Preset->TryGetCustomData(presetDimensions);
	presetDimensions.ForEachDimension([this](const FBIMNameType& DimKey, float Value) {
		NamedDimensionValues.Add(DimKey.ToString(), FModumateUnitValue::WorldCentimeters(Value));
	});
}
