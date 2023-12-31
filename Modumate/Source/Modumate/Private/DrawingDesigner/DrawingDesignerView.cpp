// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "DrawingDesigner/DrawingDesignerView.h"
#include "DrawingDesigner/DrawingDesignerRequests.h"
#include "JsonObjectConverter.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "DocumentManagement/ModumateSerialization.h"

/**
 * View Image
 */

bool FDrawingDesignerDrawingImage::operator==(const FDrawingDesignerDrawingImage& RHS) const
{
	if (this->imageBase64 != RHS.imageBase64 || this->resolutionPixels != RHS.resolutionPixels)
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

bool FDrawingDesignerDrawingImage::operator!=(const FDrawingDesignerDrawingImage& RHS) const
{
	return !(*this == RHS);
}
/**
 * SnapId
 */
bool FDrawingDesignerSnapId::operator==(const FDrawingDesignerSnapId& RHS) const
{
	return viewMoiId == RHS.viewMoiId && 
		owningMoiId == RHS.owningMoiId &&
		spanId == RHS.spanId &&
		pointIndex == RHS.pointIndex &&
		id == RHS.id;
}
bool FDrawingDesignerSnapId::operator!=(const FDrawingDesignerSnapId& RHS) const
{
	return !(*this == RHS);
}

FDrawingDesignerSnapId::FDrawingDesignerSnapId(FDrawingDesignerSnapId& copy, int32 pointIndex)
{
	this->viewMoiId = copy.viewMoiId;
	this->owningMoiId = copy.owningMoiId;
	this->spanId = copy.spanId;
	this->id = copy.id;
	this->pointIndex = pointIndex;
	this->type = copy.type;
}


/**
 * Snap
 */
bool FDrawingDesignerSnap::operator==(const FDrawingDesignerSnap& RHS) const
{
	bool rtn = true;

	//Short if the id's don't match
	if (id != RHS.id) return false;
	
	if (this->points.Num() != RHS.points.Num())
	{
		rtn = false;
	}

	for (auto& point : this->points)
	{
		int found = RHS.points.Find(point);
		if (found == INDEX_NONE)
		{
			rtn = false;
		}
	}

	return rtn;
}

bool FDrawingDesignerSnap::operator!=(const FDrawingDesignerSnap& RHS) const
{
	return !(*this == RHS);
}

/**
 * SnapPoint
 */
bool FDrawingDesignerSnapPoint::operator==(const FDrawingDesignerSnapPoint& RHS) const
{
	return id == RHS.id && FMath::IsNearlyEqual(x, RHS.x) && FMath::IsNearlyEqual(y, RHS.y);
}

bool FDrawingDesignerSnapPoint::operator!=(const FDrawingDesignerSnapPoint& RHS) const
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
	return this->a == RHS.a && this->b == RHS.b;
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
	if (this->cutPlanes.Num() != RHS.cutPlanes.Num())
	{
		return false;
	}

	for (const auto& view : this->cutPlanes)
	{
		int found = RHS.cutPlanes.Find(view);
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
	return WriteJsonGeneric<FDrawingDesignerViewList>(OutJson, this);
}

bool FDrawingDesignerViewList::ReadJson(const FString& InJson)
{
	return ReadJsonGeneric<FDrawingDesignerViewList>(InJson, this);
}

/**
 * View Request
 */
bool FDrawingDesignerDrawingRequest::operator==(const FDrawingDesignerDrawingRequest& RHS) const
{
		return this->moiId == RHS.moiId &&
		this->roi == RHS.roi &&
		this->minimumResolutionPixels == RHS.minimumResolutionPixels;
}

bool FDrawingDesignerDrawingRequest::operator!=(const FDrawingDesignerDrawingRequest& RHS) const
{
	return !(*this == RHS);
}

bool FDrawingDesignerDrawingRequest::WriteJson(FString& OutJson) const
{
	return WriteJsonGeneric<FDrawingDesignerDrawingRequest>(OutJson, this);
}

bool FDrawingDesignerDrawingRequest::ReadJson(const FString& InJson)
{
	return ReadJsonGeneric<FDrawingDesignerDrawingRequest>(InJson, this);
}

/**
 * View Response
 */
bool FDrawingDesignerDrawingResponse::operator==(const FDrawingDesignerDrawingResponse& RHS) const
{
	return this->request == RHS.request && this->response == RHS.response;
}

bool FDrawingDesignerDrawingResponse::operator!=(const FDrawingDesignerDrawingResponse& RHS) const
{
	return !(*this == RHS);
}

bool FDrawingDesignerDrawingResponse::ReadJson(const FString& InJson)
{
	return ReadJsonGeneric<FDrawingDesignerDrawingResponse>(InJson, this);
}
bool FDrawingDesignerDrawingResponse::WriteJson(FString& OutJson) const
{
	return WriteJsonGeneric<FDrawingDesignerDrawingResponse>(OutJson, this);
}