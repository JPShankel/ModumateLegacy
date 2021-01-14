// Copyright 2018 Modumate, Inc.All Rights Reserved.

#include "BIMKernel/AssemblySpec/BIMPartSlotSpec.h"
#include "BIMKernel/AssemblySpec/BIMAssemblySpec.h"
#include "Algo/Transform.h"

TMap<FString, FModumateUnitValue> FBIMPartSlotSpec::DefaultNamedParameterMap;

EBIMResult FBIMPartSlotSpec::BuildFromProperties(const FModumateDatabase& InDB)
{
	// TODO: not yet implemented
	ensureAlways(false);
	return EBIMResult::Error;
}

bool FBIMPartSlotSpec::TryGetDefaultNamedParameter(const FString& Name, FModumateUnitValue& OutVal)
{
	FModumateUnitValue* val = DefaultNamedParameterMap.Find(Name);
	if (val == nullptr)
	{
		return false;
	}
	OutVal = *val;
	return true;
}