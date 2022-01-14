// Copyright 2021 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "ModumateCore/PrettyJSONWriter.h"

#include "DrawingDesigner/DrawingDesignerView.h"
#include "DrawingDesignerRequests.generated.h"


UENUM()
enum class EDrawingDesignerRequestType
{
	unknown = 0,
	moi
};

USTRUCT()
struct MODUMATE_API FDrawingDesignerClickRequest
{
	GENERATED_BODY()

	UPROPERTY()
	FDrawingDesignerView view;
	UPROPERTY()
	FDrawingDesignerPoint uvPosition;
	UPROPERTY()
	EDrawingDesignerRequestType requestType;
	UPROPERTY()
	FString handler;

	bool WriteJson(FString& OutJson) const {
		return WriteJsonGeneric<FDrawingDesignerClickRequest>(OutJson, this);
	}
	bool ReadJson(const FString& InJson) {
		return ReadJsonGeneric<FDrawingDesignerClickRequest>(InJson, this);
	}
};

USTRUCT()
struct MODUMATE_API FDrawingDesignerMoiResponse
{
	GENERATED_BODY()

	UPROPERTY()
	FDrawingDesignerClickRequest request;
	UPROPERTY()
	int32 moiId = INDEX_NONE;

	bool WriteJson(FString& OutJson) const {
		return WriteJsonGeneric<FDrawingDesignerMoiResponse>(OutJson, this);
	}
	bool ReadJson(const FString& InJson) {
		return ReadJsonGeneric<FDrawingDesignerMoiResponse>(InJson, this);
	}
};
