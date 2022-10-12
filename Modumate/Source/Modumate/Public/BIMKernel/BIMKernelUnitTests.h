// Copyright 2022 Modumate, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "ModumateCore/StructDataWrapper.h"
#include "VectorTypes.h"

#include "BIMKernelUnitTests.generated.h"

USTRUCT()
struct MODUMATE_API FBIMUnitTestProperties
{
	GENERATED_BODY()

	UPROPERTY()
	FString stringProp;

	UPROPERTY()
	int32 intProp;
};

USTRUCT()
struct MODUMATE_API FBIMUnitTestProperties2
{
	GENERATED_BODY()

	UPROPERTY()
	FString templateTest;
};
