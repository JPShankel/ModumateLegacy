// Copyright 2018 Modumate, Inc.All Rights Reserved.

#include "BIMKernel/BIMPartSlotSpec.h"
#include "BIMKernel/BIMAssemblySpec.h"
#include "Algo/Transform.h"

TMap<FString, Modumate::Units::FUnitValue> FBIMPartSlotSpec::DefaultNamedParameterMap;

ECraftingResult FBIMPartSlotSpec::BuildFromProperties(const FModumateDatabase& InDB)
{
	// TODO: not yet implemented
	ensureAlways(false);
	return ECraftingResult::Error;
}

bool FBIMPartSlotSpec::TryGetDefaultNamedParameter(const FString& Name, Modumate::Units::FUnitValue& OutVal)
{
	Modumate::Units::FUnitValue* val = DefaultNamedParameterMap.Find(Name);
	if (val == nullptr)
	{
		return false;
	}
	OutVal = *val;
	return true;
}

