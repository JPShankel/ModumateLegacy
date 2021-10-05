// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "ProjectConnection.generated.h"

struct MODUMATE_API FProjectConnectionHelpers
{
	static const FString ProjectsEndpointPrefix;
	static const FString ConnectionEndpointSuffix;
	static const FString DataEndpointSuffix;
	static const FString ThumbnailEndpointSuffix;

	static FString MakeProjectInfoEndpoint(const FString& ProjectID);
	static FString MakeProjectConnectionEndpoint(const FString& ProjectID);
	static FString MakeProjectDataEndpoint(const FString& ProjectID);
	static FString MakeProjectThumbnailEndpoint(const FString& ProjectID);
};

// Must match the API spec for components/schemas/Connection in the AMS
USTRUCT()
struct MODUMATE_API FProjectConnectionResponse
{
	GENERATED_BODY()

	UPROPERTY()
	FString IP;

	UPROPERTY()
	int32 Port = 0;

	UPROPERTY()
	FString Key;

	UPROPERTY()
	TArray<FString> Permissions;
};

// Must match the API spec for components/schemas/Project in the AMS
USTRUCT()
struct MODUMATE_API FProjectInfoResponse
{
	GENERATED_BODY()

	UPROPERTY()
	FString ID;

	UPROPERTY()
	FString Name;

	UPROPERTY()
	FString Workspace;

	UPROPERTY()
	int32 Date_created = 0;

	UPROPERTY()
	int32 Date_modified = 0;

	UPROPERTY()
	bool bLocked = false;
};
