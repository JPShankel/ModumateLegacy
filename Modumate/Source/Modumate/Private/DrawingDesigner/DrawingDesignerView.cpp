// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "DrawingDesigner/DrawingDesignerView.h"
#include "JsonObjectConverter.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "DocumentManagement/ModumateSerialization.h"

static const TCHAR* DrawingDesignerViewResponseLabel = TEXT("view_response");
static const TCHAR* DrawingDesignerViewRequestLabel = TEXT("view_request");
static const TCHAR* DrawingDesignerViewListLabel = TEXT("view_list");


/**
 * View
 */
bool FDrawingDesignerView::operator==(const FDrawingDesignerView& RHS) const
{
	return this->moi_id == RHS.moi_id && this->aspect == RHS.aspect;
}

bool FDrawingDesignerView::operator!=(const FDrawingDesignerView& RHS) const
{
	return !(*this == RHS);
}

/**
 * View Image
 */

bool FDrawingDesignerViewImage::operator==(const FDrawingDesignerViewImage& RHS) const
{
	if (this->request_id != RHS.request_id ||
		this->resolution_pixels != RHS.resolution_pixels ||
		this->image_base64 != RHS.image_base64 ||
		this->line_stride_bytes != RHS.line_stride_bytes ||
		this->pixel_stride_bytes != RHS.pixel_stride_bytes)
	{
		return false;
	}

	if (snaps.Num() != RHS.snaps.Num())
	{
		return false;
	}

	for (auto& kvp : snaps)
	{
		auto* other = RHS.snaps.Find(kvp.Key);
		if (!other || *other != kvp.Value)
		{
			return false;
		}
	}

	return true;
}

bool FDrawingDesignerViewImage::operator!=(const FDrawingDesignerViewImage& RHS) const
{
	return !(*this == RHS);
}

/**
 * Snap
 */
bool FDrawingDesignerViewSnap::operator==(const FDrawingDesignerViewSnap& RHS) const
{
	return id == RHS.id && FMath::IsNearlyEqual(x, RHS.x) && FMath::IsNearlyEqual(y, RHS.y);
}

bool FDrawingDesignerViewSnap::operator!=(const FDrawingDesignerViewSnap& RHS) const
{
	return !(*this == RHS);
}

/**
 * Point
 */
bool FDrawingDesignerPoint::operator==(const FDrawingDesignerPoint& RHS) const
{
	return FMath::IsNearlyEqual(x, RHS.x) && FMath::IsNearlyEqual(y, RHS.y);
}

bool FDrawingDesignerPoint::operator!=(const FDrawingDesignerPoint& RHS) const
{
	return !(*this == RHS);
}

/**
 * View Region
 */
bool FDrawingDesignerViewRegion::operator==(const FDrawingDesignerViewRegion& RHS) const
{
	return this->A == RHS.A && this->B == RHS.B;
}

bool FDrawingDesignerViewRegion::operator!=(const FDrawingDesignerViewRegion& RHS) const
{
	return !(*this == RHS);
}

/**
 * View List
 */
bool FDrawingDesignerViewList::operator==(const FDrawingDesignerViewList& RHS) const
{
	if (this->views.Num() != RHS.views.Num())
	{
		return false;
	}

	for (auto& view : this->views)
	{
		int found = RHS.views.Find(view);
		if (found == INDEX_NONE)
		{
			return false;
		}
	}

	return true;
}

bool FDrawingDesignerViewList::operator!=(const FDrawingDesignerViewList& RHS) const
{
	return !(*this == RHS);
}

bool FDrawingDesignerViewList::WriteJson(FString& OutJson) const
{
	return WriteJsonGeneric<FDrawingDesignerViewList>(OutJson, DrawingDesignerViewListLabel, this);
}

bool FDrawingDesignerViewList::ReadJson(const FString& InJson)
{
	return ReadJsonGeneric<FDrawingDesignerViewList>(InJson, DrawingDesignerViewListLabel, this);
}

/**
 * View Request
 */
bool FDrawingDesignerViewRequest::operator==(const FDrawingDesignerViewRequest& RHS) const
{
	return this->request_id == RHS.request_id &&
		this->moi_id == RHS.moi_id &&
		this->roi == RHS.roi &&
		this->minimum_resolution_pixels == RHS.minimum_resolution_pixels;
}

bool FDrawingDesignerViewRequest::operator!=(const FDrawingDesignerViewRequest& RHS) const
{
	return !(*this == RHS);
}

bool FDrawingDesignerViewRequest::WriteJson(FString& OutJson) const
{
	return WriteJsonGeneric<FDrawingDesignerViewRequest>(OutJson, DrawingDesignerViewRequestLabel, this);
}

bool FDrawingDesignerViewRequest::ReadJson(const FString& InJson)
{
	return ReadJsonGeneric<FDrawingDesignerViewRequest>(InJson, DrawingDesignerViewRequestLabel, this);
}

/**
 * View Response
 */
bool FDrawingDesignerViewResponse::operator==(const FDrawingDesignerViewResponse& RHS) const
{
	return this->request == RHS.request && this->response == RHS.response;
}

bool FDrawingDesignerViewResponse::operator!=(const FDrawingDesignerViewResponse& RHS) const
{
	return !(*this == RHS);
}

bool FDrawingDesignerViewResponse::ReadJson(const FString& InJson)
{
	return ReadJsonGeneric<FDrawingDesignerViewResponse>(InJson, DrawingDesignerViewResponseLabel, this);
}
bool FDrawingDesignerViewResponse::WriteJson(FString& OutJson) const
{
	return WriteJsonGeneric<FDrawingDesignerViewResponse>(OutJson, DrawingDesignerViewResponseLabel, this);
}
