// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UModumateDocument;

class MODUMATE_API FDrawingDesignerRenderControl
{
public:
	static FString GetViewList(const UModumateDocument* Doc);
	static bool GetView(const UModumateDocument* Doc, const FString& jsonRequest, FString& OutJsonResponse);
};
