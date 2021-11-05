// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "DrawingDesigner/DrawingDesignerRenderControl.h"

#include "DocumentManagement/ModumateDocument.h"
#include "DrawingDesigner/DrawingDesignerView.h"
#include "DrawingDesigner/DrawingDesignerRender.h"
#include "Objects/CutPlane.h"

FString FDrawingDesignerRenderControl::GetViewList(const UModumateDocument* Doc)
{
	FDrawingDesignerViewList viewList;

	TArray<const AModumateObjectInstance*> cutPlanes = Doc->GetObjectsOfType(EObjectType::OTCutPlane);
	for (const auto* cutPlane : cutPlanes)
	{
		if (cutPlane->GetNumCorners() != 4)
		{
			continue;
		}

		FDrawingDesignerView view;
		view.moi_id = cutPlane->ID;
		TArray<FVector> controlPoints;
		for (int32 c = 0; c < 4; ++c)
		{
			controlPoints.Add(cutPlane->GetCorner(c));
		}

		FPlane plane;
		FVector axisX, axisY, center;
		TArray<FVector2D> points2d;
		if (UModumateGeometryStatics::AnalyzeCachedPositions(controlPoints, plane, axisX, axisY, points2d, center))
		{
			FVector2D size((points2d[2] - points2d[0]).GetAbs());
			view.aspect.x = 1.0f;
			view.aspect.y = size.Y / size.X;
			viewList.views.Add(view);
		}
	}

	FString responseString;
	if (ensureAlways(viewList.WriteJson(responseString)))
	{
		return responseString;
	}
	else
	{
		return FString();
	}
}

bool FDrawingDesignerRenderControl::GetView(const UModumateDocument* Doc, const FString& jsonRequest, FString& OutJsonResponse)
{
	FDrawingDesignerDrawingRequest viewRequest;
	if (!ReadJsonGeneric(jsonRequest, &viewRequest))
	{
		return false;
	}

	const AModumateObjectInstance* moi = Doc->GetObjectById(viewRequest.moi_id);
	if (!ensureAlways(moi) || !ensureAlways(moi->GetObjectType() == EObjectType::OTCutPlane))
	{
		return false;
	}

	const AMOICutPlane* cutPlane = Cast<const AMOICutPlane>(moi);
	FVector corners[4];
	for (int32 c = 0; c < 4; ++c)
	{
		corners[c] = cutPlane->GetCorner(c);
	}

	const FVector planeCentre((corners[1] + corners[2]) / 2.0f);
	const float planeWidth = (corners[1] - corners[0]).Size();
	const float planeHeight = (corners[2] - corners[1]).Size();
	const FVector ll(corners[2] + viewRequest.roi.A.x * (corners[0] - corners[1])
		+ viewRequest.roi.A.y * (corners[1] - corners[2]));
	const FVector ur(corners[2] + viewRequest.roi.B.x * (corners[0] - corners[1])
		+ viewRequest.roi.B.y * (corners[1] - corners[2]));
	const FVector cameraCentre((ll + ur) / 2.0f);
	const float viewWidth = planeWidth * FMath::Abs(viewRequest.roi.B.x - viewRequest.roi.A.x);
	const float viewHeight = planeHeight * FMath::Abs(viewRequest.roi.B.y - viewRequest.roi.A.y);

	// Capture actor looks along +X (Y up); cutplane looks along +Z.
	static const FQuat cameraToCutplane(FVector(1, -1, 1).GetSafeNormal(), FMath::DegreesToRadians(120));
	FTransform cameraTransform(cutPlane->GetRotation()  * cameraToCutplane, cameraCentre, FVector(viewWidth, viewHeight, 1.0f));
	ADrawingDesignerRender* renderer = Doc->GetWorld()->SpawnActor<ADrawingDesignerRender>();

	renderer->SetViewTransform(cameraTransform);

	renderer->RenderImage(viewRequest.minimum_resolution_pixels.x);

	TArray<uint8> rawPng;
	bool bSuccess = false;
	if (ensure(renderer->GetImagePNG(rawPng)) )
	{
		FString b64Png(FBase64::Encode(rawPng));
		FMOICutPlaneData cutPlaneData;
		cutPlane->GetStateData().CustomData.LoadStructData(cutPlaneData);

		FDrawingDesignerDrawingResponse viewResponse;
		viewResponse.request = viewRequest;
		viewResponse.response.view.moi_id = viewRequest.moi_id;
		viewResponse.response.view.aspect = { 1.0f, viewHeight / viewWidth };
		viewResponse.response.resolution_pixels = viewRequest.minimum_resolution_pixels;
		viewResponse.response.view.name = cutPlaneData.Name;

		viewResponse.response.image_base64 = MoveTemp(b64Png);
		bSuccess = WriteJsonGeneric(OutJsonResponse, &viewResponse);
	}

	renderer->Destroy();

	return bSuccess;
}
