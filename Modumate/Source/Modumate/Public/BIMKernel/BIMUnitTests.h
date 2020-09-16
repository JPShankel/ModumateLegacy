// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "BIMKernel/BIMKey.h"
#include "BIMUnitTests.generated.h"

USTRUCT()
struct FTestBIMKeyContainer
{
	GENERATED_BODY()

	UPROPERTY()
	TMap<FBIMKey, int32> TestMap;
};