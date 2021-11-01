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
struct MODUMATE_API FDrawingDesignerViewSnap
{
	GENERATED_BODY()
	FDrawingDesignerViewSnap() = default;

	UPROPERTY()
	int32 id = INDEX_NONE;

	// Normalized in range 0-1
	UPROPERTY()
	float x = 0.0f;

	UPROPERTY()
	float y = 0.0f;

	bool operator==(const FDrawingDesignerViewSnap& RHS) const;
	bool operator!=(const FDrawingDesignerViewSnap& RHS) const;
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

	//TODO: Add Object Type def pending more discussion

	bool operator==(const FDrawingDesignerView& RHS) const;
	bool operator!=(const FDrawingDesignerView& RHS) const;
};

USTRUCT()
struct MODUMATE_API FDrawingDesignerViewImage
{
	GENERATED_BODY()

	FDrawingDesignerViewImage() = default;

	UPROPERTY()
	FDrawingDesignerView view;

	UPROPERTY()
	int32 request_id = INDEX_NONE;

	UPROPERTY()
	int32 line_stride_bytes = 0;

	UPROPERTY()
	int32 pixel_stride_bytes = 0;

	UPROPERTY()
	FDrawingDesignerPoint resolution_pixels;

	UPROPERTY() // FString key encoded from int32 id
	TMap<FString, FDrawingDesignerViewSnap> snaps;

	UPROPERTY()
	FString image_base64;

	bool operator==(const FDrawingDesignerViewImage& RHS) const;
	bool operator!=(const FDrawingDesignerViewImage& RHS) const;
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
struct MODUMATE_API FDrawingDesignerViewRequest
{
	GENERATED_BODY()
	FDrawingDesignerViewRequest() = default;

	UPROPERTY()
	int32 request_id = INDEX_NONE;

	UPROPERTY()
	int32 moi_id = INDEX_NONE;

	UPROPERTY()
	FDrawingDesignerViewRegion roi;

	UPROPERTY()
	FDrawingDesignerPoint minimum_resolution_pixels;
	
	bool WriteJson(FString& OutJson) const;
	bool ReadJson(const FString& InJson);

	bool operator==(const FDrawingDesignerViewRequest& RHS) const;
	bool operator!=(const FDrawingDesignerViewRequest& RHS) const;
};

USTRUCT()
struct MODUMATE_API FDrawingDesignerViewResponse
{
	GENERATED_BODY()
	FDrawingDesignerViewResponse() = default;

	UPROPERTY()
	FDrawingDesignerViewRequest request;

	UPROPERTY()
	FDrawingDesignerViewImage response;

	bool WriteJson(FString& OutJson) const;
	bool ReadJson(const FString& InJson);

	bool operator==(const FDrawingDesignerViewResponse& RHS) const;
	bool operator!=(const FDrawingDesignerViewResponse& RHS) const;
};

template <class T>
static bool WriteJsonGeneric(FString& OutJson, const FString& Label, const T* InObject)
{
	TSharedPtr<FJsonObject> docOb = FJsonObjectConverter::UStructToJsonObject<T>(*InObject);
	TSharedRef<FPrettyJsonStringWriter> JsonStringWriter = FPrettyJsonStringWriterFactory::Create(&OutJson);
	TSharedPtr<FJsonObject> FileJsonWrite = MakeShared<FJsonObject>();

	//Return it prettified
	FileJsonWrite->SetObjectField(Label, docOb);
	return FJsonSerializer::Serialize(FileJsonWrite.ToSharedRef(), JsonStringWriter);
};

template <class T>
static bool ReadJsonGeneric(const FString& InJson, const FString& Label, T* OutObject)
{
	TSharedPtr<FJsonObject> FileJsonRead;
	const TSharedPtr<FJsonObject>* jsonDocument;
	auto JsonReader = TJsonReaderFactory<>::Create(InJson);
	bool bSuccess = FJsonSerializer::Deserialize(JsonReader, FileJsonRead) && FileJsonRead.IsValid();

	if (bSuccess)
	{
		if (FileJsonRead->TryGetObjectField(Label, jsonDocument))
		{
			FJsonObjectConverter::JsonObjectToUStruct<T>(jsonDocument->ToSharedRef(), OutObject);
		}
		else
		{
			bSuccess = false;
		}
	}

	return bSuccess;
};
