// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "Online/ProjectConnection.h"

const FString FProjectConnectionHelpers::ProjectsEndpointPrefix(TEXT("/projects"));
const FString FProjectConnectionHelpers::ConnectionEndpointSuffix(TEXT("connections"));

FString FProjectConnectionHelpers::MakeProjectConnectionEndpoint(const FString& ProjectID)
{
	return ProjectsEndpointPrefix / ProjectID / ConnectionEndpointSuffix;
}
