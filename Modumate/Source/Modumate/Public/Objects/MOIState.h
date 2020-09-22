// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "BIMKernel/BIMKey.h"
#include "Database/ModumateObjectEnums.h"

// TODO: remove these
#include "BIMKernel/BIMProperties.h"
#include "ModumateCore/ModumateConsoleCommand.h"

struct MODUMATE_API FMOIStateData
{
	EMOIDeltaType StateType = EMOIDeltaType::None;
	int32 ObjectID = MOD_ID_NONE;

	// TODO: use this for instance-level overrides
	FBIMPropertySheet ObjectProperties;

	//<Thickness, Height, UNUSED>
	FVector Extents = FVector::ZeroVector;

	TArray<FVector> ControlPoints;
	TArray<int32> ControlIndices;
	bool bObjectInverted = false;

	FVector Location = FVector::ZeroVector;
	FQuat Orientation = FQuat::Identity;

	int32 ParentID = MOD_ID_NONE;

	// Store key instead of whole assembly to avoid old versions of an assembly from being re-applied
	FBIMKey ObjectAssemblyKey;

	EObjectType ObjectType = EObjectType::OTNone;

	// TODO: to be deprecated when Commands 2.0 is developed, meantime...
	bool ToParameterSet(const FString& Prefix, Modumate::FModumateFunctionParameterSet& OutParameterSet) const;
	bool FromParameterSet(const FString& Prefix, const Modumate::FModumateFunctionParameterSet& ParameterSet);

	bool operator==(const FMOIStateData& Other) const;
};
