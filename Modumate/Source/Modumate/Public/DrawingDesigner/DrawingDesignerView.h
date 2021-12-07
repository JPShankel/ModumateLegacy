// Copyright 2021 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "ModumateCore/PrettyJSONWriter.h"
#include "DrawingDesignerView.generated.h"

USTRUCT()
struct MODUMATE_API FDrawingDesignerPoint
{
	GENERATED_BODY()
	FDrawingDesignerPoint() = default;

	UPROPERTY()
	float x = 0.0f;
	UPROPERTY()
	float y = 0.0f;

	bool operator==(const FDrawingDesignerPoint& RHS) const;
	bool operator!=(const FDrawingDesignerPoint& RHS) const;
};

USTRUCT()
struct MODUMATE_API FDrawingDesignerViewRegion
{
	GENERATED_BODY()

	UPROPERTY()
	FDrawingDesignerPoint A;
	UPROPERTY()
	FDrawingDesignerPoint B;

	bool operator==(const FDrawingDesignerViewRegion& RHS) const;
	bool operator!=(const FDrawingDesignerViewRegion& RHS) const;
};

USTRUCT()
struct MODUMATE_API FDrawingDesignerSnap
{
	GENERATED_BODY()
	FDrawingDesignerSnap() = default;

	UPROPERTY()
	FString id = FString::FromInt(INDEX_NONE);

	// Normalized in range 0-1
	UPROPERTY()
	float x = 0.0f;

	UPROPERTY()
	float y = 0.0f;

	bool operator==(const FDrawingDesignerSnap& RHS) const;
	bool operator!=(const FDrawingDesignerSnap& RHS) const;
};

USTRUCT()
struct MODUMATE_API FDrawingDesignerView
{
	GENERATED_BODY()
	FDrawingDesignerView() = default;

	UPROPERTY()
	int32 moi_id = INDEX_NONE;

	UPROPERTY()
	FDrawingDesignerPoint aspect;

	UPROPERTY()
	FString name;

	//TODO: Add Object Type def pending more discussion

	bool operator==(const FDrawingDesignerView& RHS) const;
	bool operator!=(const FDrawingDesignerView& RHS) const;
};

USTRUCT()
struct MODUMATE_API FDrawingDesignerDrawingImage
{
	GENERATED_BODY()

	FDrawingDesignerDrawingImage() = default;

	UPROPERTY()
	FDrawingDesignerView view;

	UPROPERTY() // FString key encoded from int32 id
	TMap<FString, FDrawingDesignerSnap> snaps;

	UPROPERTY()
	FString image_base64;

	UPROPERTY()
	FDrawingDesignerPoint resolution_pixels;

	bool operator==(const FDrawingDesignerDrawingImage& RHS) const;
	bool operator!=(const FDrawingDesignerDrawingImage& RHS) const;
};

USTRUCT()
struct MODUMATE_API FDrawingDesignerViewList
{
	GENERATED_BODY()
	FDrawingDesignerViewList() = default;

	UPROPERTY()
	TArray<FDrawingDesignerView> views;

	bool WriteJson(FString& OutJson) const;
	bool ReadJson(const FString& InJson);

	bool operator==(const FDrawingDesignerViewList& RHS) const;
	bool operator!=(const FDrawingDesignerViewList& RHS) const;
};

USTRUCT()
struct MODUMATE_API FDrawingDesignerDrawingRequest
{
	GENERATED_BODY()
	FDrawingDesignerDrawingRequest() = default;

	UPROPERTY()
	int32 moi_id = INDEX_NONE;

	UPROPERTY()
	FDrawingDesignerViewRegion roi;

	UPROPERTY()
	FDrawingDesignerPoint minimum_resolution_pixels;
	
	bool WriteJson(FString& OutJson) const;
	bool ReadJson(const FString& InJson);

	bool operator==(const FDrawingDesignerDrawingRequest& RHS) const;
	bool operator!=(const FDrawingDesignerDrawingRequest& RHS) const;
};

USTRUCT()
struct MODUMATE_API FDrawingDesignerDrawingResponse
{
	GENERATED_BODY()
	FDrawingDesignerDrawingResponse() = default;

	UPROPERTY()
	FDrawingDesignerDrawingRequest request;

	UPROPERTY()
	FDrawingDesignerDrawingImage response;

	bool WriteJson(FString& OutJson) const;
	bool ReadJson(const FString& InJson);

	bool operator==(const FDrawingDesignerDrawingResponse& RHS) const;
	bool operator!=(const FDrawingDesignerDrawingResponse& RHS) const;
};

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
