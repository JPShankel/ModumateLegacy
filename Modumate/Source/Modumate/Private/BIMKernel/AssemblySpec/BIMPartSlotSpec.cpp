// Copyright 2018 Modumate, Inc.All Rights Reserved.

#include "BIMKernel/AssemblySpec/BIMPartSlotSpec.h"
#include "BIMKernel/AssemblySpec/BIMAssemblySpec.h"
#include "BIMKernel/Presets/BIMPresetInstance.h"
#include "Algo/Transform.h"

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
	Preset->Properties.ForEachProperty([this](const FBIMPropertyKey& PropKey, float Value) {
		if (PropKey.Scope == EBIMValueScope::Dimension)
		{
			NamedDimensionValues.Add(PropKey.Name.ToString(), FModumateUnitValue::WorldCentimeters(Value));
		}
	});
}
