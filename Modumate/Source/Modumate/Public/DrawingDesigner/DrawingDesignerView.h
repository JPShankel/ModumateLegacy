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

UENUM()
enum class EDDSnapType
{
	unknown = 0,
	point,
	line,
	midpoint
};

USTRUCT()
struct MODUMATE_API FDrawingDesignerSnapId {
	GENERATED_BODY()
	FDrawingDesignerSnapId() = default;
	FDrawingDesignerSnapId(EDDSnapType SnapType, FString View, int32 Owner, int32 Idx, int32 Id) :
		type(SnapType),
		viewMoiId(View),
		owningMoiId(Owner),
		pointIndex(Idx),
		id(Id)
	{
	}
	FDrawingDesignerSnapId(FDrawingDesignerSnapId& copy, int32 newIdx);

	UPROPERTY()
	EDDSnapType type = EDDSnapType::unknown;

	UPROPERTY()
	FString viewMoiId = FString::FromInt(INDEX_NONE);

	UPROPERTY()
	int32 owningMoiId = INDEX_NONE;

	UPROPERTY()
	int32 pointIndex = INDEX_NONE;

	UPROPERTY()
	int32 id = INDEX_NONE;

	void EncodeKey(FString& outEncoded) {
		FString left, right;
		FString fqName = UEnum::GetValueAsString(this->type);
		fqName.Split(TEXT("::"), &left, &right);
		outEncoded = right + TEXT(",") + FString(this->viewMoiId) + TEXT(",") + FString::FromInt(this->owningMoiId) + TEXT(",") + FString::FromInt(this->id) + TEXT(",") + FString::FromInt(this->pointIndex);
	}

	bool operator==(const FDrawingDesignerSnapId& RHS) const;
	bool operator!=(const FDrawingDesignerSnapId& RHS) const;
};

USTRUCT()
struct MODUMATE_API FDrawingDesignerSnapPoint {
	GENERATED_BODY()
	FDrawingDesignerSnapPoint() = default;

	UPROPERTY()
	FDrawingDesignerSnapId id;
	
	// Normalized in range 0-1
	UPROPERTY()
	float x = 0.0f;

	UPROPERTY()
	float y = 0.0f;

	bool operator==(const FDrawingDesignerSnapPoint& RHS) const;
	bool operator!=(const FDrawingDesignerSnapPoint& RHS) const;
};

USTRUCT()
struct MODUMATE_API FDrawingDesignerSnap
{
	GENERATED_BODY()
	FDrawingDesignerSnap() = default;


	FDrawingDesignerSnap(FDrawingDesignerSnapId newId) :
	id (newId)
	{
	}
	
	UPROPERTY()
	FDrawingDesignerSnapId id;

	UPROPERTY()
	TArray<FDrawingDesignerSnapPoint> points;

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
	FDrawingDesignerPoint size;

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

	UPROPERTY() //FString key encoded from view ID
	TMap<FString, FDrawingDesignerSnap> snaps;

	UPROPERTY()
	FString image_base64;

	UPROPERTY()
	float scale = 0.0f;

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


