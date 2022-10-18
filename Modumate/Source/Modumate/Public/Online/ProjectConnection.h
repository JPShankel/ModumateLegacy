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

// Used to convey information to the client in connect_to_server
USTRUCT()
struct MODUMATE_API FProjectConnectToServerRequest
{
	GENERATED_BODY()

	UPROPERTY()
	FString ip;

	UPROPERTY()
	FString port;

	UPROPERTY()
	FString key;

	UPROPERTY()
	FString userId;

	UPROPERTY()
	FString projectId;
};

// Must match the API spec for components/schemas/Connection in the AMS
USTRUCT()
struct MODUMATE_API FProjectConnectionResponse
{
	GENERATED_BODY()

	FProjectConnectionResponse() = default;
	FProjectConnectionResponse(const FProjectConnectToServerRequest& Request) : 
		IP(Request.ip),
		Port(FCString::Atoi(*Request.port)),
		Key(Request.key)
	{}

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

USTRUCT()
struct MODUMATE_API FMultiplayerError
{
	GENERATED_BODY()

	UPROPERTY()
	FString Category;

	UPROPERTY()
	FString Details;

	UPROPERTY()
	FString NetworkInfo;
};
