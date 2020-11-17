// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BIMKernel/Core/BIMKey.h"
#include "BIMUnitTests.generated.h"

USTRUCT()
struct FTestBIMKeyContainer
{
	GENERATED_BODY()

	UPROPERTY()
	TMap<FBIMKey, int32> TestMap;
};