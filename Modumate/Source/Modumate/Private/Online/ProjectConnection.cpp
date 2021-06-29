// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "Online/ProjectConnection.h"

const FString FProjectConnectionHelpers::ProjectsEndpointPrefix(TEXT("/projects"));
const FString FProjectConnectionHelpers::ConnectionEndpointSuffix(TEXT("connections"));
const FString FProjectConnectionHelpers::DataEndpointSuffix(TEXT("data"));

FString FProjectConnectionHelpers::MakeProjectInfoEndpoint(const FString& ProjectID)
{
	return ProjectsEndpointPrefix / ProjectID;
}

FString FProjectConnectionHelpers::MakeProjectConnectionEndpoint(const FString& ProjectID)
{
	return ProjectsEndpointPrefix / ProjectID / ConnectionEndpointSuffix;
}

FString FProjectConnectionHelpers::MakeProjectDataEndpoint(const FString& ProjectID)
{
	return ProjectsEndpointPrefix / ProjectID / DataEndpointSuffix;
}
