// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "ProjectConnection.generated.h"

struct MODUMATE_API FProjectConnectionHelpers
{
	static const FString ProjectsEndpointPrefix;
	static const FString ConnectionEndpointSuffix;

	static FString MakeProjectConnectionEndpoint(const FString& ProjectID);
};

// Must match the API spec for components/schemas/Connection in the AMS
USTRUCT()
struct MODUMATE_API FProjectConnectionResponse
{
	GENERATED_BODY()

	UPROPERTY()
	FString IP;

	UPROPERTY()
	int32 Port;

	UPROPERTY()
	FString Key;
};
