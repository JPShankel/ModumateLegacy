// Copyright 2021 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "ModumateCore/PrettyJSONWriter.h"
#include "Objects/MOIStructureData.h"
#include "DrawingDesigner/DrawingDesignerView.h"
#include "DrawingDesignerRequests.generated.h"


UENUM()
enum class EDrawingDesignerRequestType
{
	unknown = 0,
	getClickedMoi,
	stringToInches,
	stringToCentimeters,
	centimetersToString,
	getCutplaneLines,
	getPresetThumbnail
};

USTRUCT()
struct MODUMATE_API FDrawingDesignerGenericRequest
{
	GENERATED_BODY()

	UPROPERTY()
	FDrawingDesignerView view;
	UPROPERTY()
	FDrawingDesignerPoint uvPosition;
	UPROPERTY()
	FString data;

	UPROPERTY()
	EDrawingDesignerRequestType requestType;
	UPROPERTY()
	int32 handler = 0;

	bool WriteJson(FString& OutJson) const {
		return WriteJsonGeneric<FDrawingDesignerGenericRequest>(OutJson, this);
	}
	bool ReadJson(const FString& InJson) {
		return ReadJsonGeneric<FDrawingDesignerGenericRequest>(InJson, this);
	}
};

USTRUCT()
struct MODUMATE_API FDrawingDesignerMoiResponse
{
	GENERATED_BODY()

	UPROPERTY()
	FDrawingDesignerGenericRequest request;

	UPROPERTY()
	int32 moiId = INDEX_NONE;

	UPROPERTY()
	int32 spanId = INDEX_NONE; 

	UPROPERTY()
	FString presetId = TEXT("");

	//TODO: Use MOI data in DD store once it's done and delete this -JN
	UPROPERTY()
	FString typeMark = TEXT("");

	bool WriteJson(FString& OutJson) const {
		return WriteJsonGeneric<FDrawingDesignerMoiResponse>(OutJson, this);
	}
	bool ReadJson(const FString& InJson) {
		return ReadJsonGeneric<FDrawingDesignerMoiResponse>(InJson, this);
	}
};


USTRUCT()
struct MODUMATE_API FDrawingDesignerGenericFloatResponse
{
	GENERATED_BODY()

	UPROPERTY()
	FDrawingDesignerGenericRequest request;

	UPROPERTY()
	float answer = 0.0f;

	bool WriteJson(FString& OutJson) const {
		return WriteJsonGeneric<FDrawingDesignerGenericFloatResponse>(OutJson, this);
	}
	bool ReadJson(const FString& InJson) {
		return ReadJsonGeneric<FDrawingDesignerGenericFloatResponse>(InJson, this);
	}
};


USTRUCT()
struct MODUMATE_API FDrawingDesignerGenericStringResponse
{
	GENERATED_BODY()

	UPROPERTY()
	FDrawingDesignerGenericRequest request;

	UPROPERTY()
	FString answer;

	bool WriteJson(FString& OutJson) const {
		return WriteJsonGeneric<FDrawingDesignerGenericStringResponse>(OutJson, this);
	}
	bool ReadJson(const FString& InJson) {
		return ReadJsonGeneric<FDrawingDesignerGenericStringResponse>(InJson, this);
	}
};
