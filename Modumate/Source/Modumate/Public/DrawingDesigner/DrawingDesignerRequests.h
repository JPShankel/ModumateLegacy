// Copyright 2021 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "ModumateCore/PrettyJSONWriter.h"

#include "DrawingDesigner/DrawingDesignerView.h"
#include "DrawingDesignerRequests.generated.h"

template <class T>
static bool WriteJsonGeneric(FString& OutJson, const T* InObject)
{
	TSharedPtr<FJsonObject> docOb = FJsonObjectConverter::UStructToJsonObject<T>(*InObject);
	TSharedRef<FPrettyJsonStringWriter> JsonStringWriter = FPrettyJsonStringWriterFactory::Create(&OutJson);

	//Return it prettified
	return FJsonSerializer::Serialize(docOb.ToSharedRef(), JsonStringWriter);
};

template <class T>
static bool ReadJsonGeneric(const FString& InJson, T* OutObject)
{
	TSharedPtr<FJsonObject> FileJsonRead;
	auto JsonReader = TJsonReaderFactory<>::Create(InJson);
	bool bSuccess = FJsonSerializer::Deserialize(JsonReader, FileJsonRead) && FileJsonRead.IsValid();

	if (bSuccess)
	{
		FJsonObjectConverter::JsonObjectToUStruct<T>(FileJsonRead.ToSharedRef(), OutObject);
	}

	return bSuccess;
};

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
