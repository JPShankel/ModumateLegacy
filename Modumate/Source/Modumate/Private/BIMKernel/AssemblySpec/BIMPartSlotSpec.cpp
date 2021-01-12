// Copyright 2018 Modumate, Inc.All Rights Reserved.

#include "BIMKernel/AssemblySpec/BIMPartSlotSpec.h"
#include "BIMKernel/AssemblySpec/BIMAssemblySpec.h"
#include "Algo/Transform.h"

TMap<FString, Modumate::Units::FUnitValue> FBIMPartSlotSpec::DefaultNamedParameterMap;

EBIMResult FBIMPartSlotSpec::BuildFromProperties(const FModumateDatabase& InDB)
{
	// TODO: not yet implemented
	ensureAlways(false);
	return EBIMResult::Error;
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