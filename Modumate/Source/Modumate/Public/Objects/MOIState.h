// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "BIMKernel/Core/BIMKey.h"
#include "Objects/ModumateObjectEnums.h"
#include "ModumateCore/StructDataWrapper.h"

// TODO: remove these
#include "BIMKernel/Core/BIMProperties.h"
#include "ModumateCore/ModumateConsoleCommand.h"

#include "MOIState.generated.h"


USTRUCT()
struct MODUMATE_API FMOIStateData
{
	GENERATED_BODY()

	FMOIStateData();
	FMOIStateData(int32 InID, EObjectType InObjectType, int32 InParentID = MOD_ID_NONE);

	UPROPERTY()
	int32 ID = MOD_ID_NONE;

	UPROPERTY()
	EObjectType ObjectType = EObjectType::OTNone;

	UPROPERTY()
	int32 ParentID = MOD_ID_NONE;

	UPROPERTY()
	FBIMKey AssemblyKey;

	UPROPERTY()
	FGuid AssemblyGUID;

	UPROPERTY()
	FString DisplayName;

	UPROPERTY()
	FStructDataWrapper CustomData;

	bool operator==(const FMOIStateData& Other) const;
	bool operator!=(const FMOIStateData& Other) const;
};
