// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "DrawingDesigner/DrawingDesignerRenderControl.h"

#include "DocumentManagement/ModumateDocument.h"
#include "DrawingDesigner/DrawingDesignerRequests.h"
#include "DrawingDesigner/DrawingDesignerView.h"
#include "DrawingDesigner/DrawingDesignerLine.h"
#include "DrawingDesigner/DrawingDesignerRender.h"
#include "Objects/CutPlane.h"

#include "UnrealClasses/CompoundMeshActor.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "UnrealClasses/EditModelGameMode.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/LineActor.h"

constexpr float MOI_TRACE_DISTANCE = 5000.0f;

// Stencil values are determined by post process material in DrawingDesignerRender
constexpr int32 DisableMeshStencilValue = 0;
constexpr int32 ForegroundMeshStencilValue  = 1;


FDrawingDesignerRenderControl::~FDrawingDesignerRenderControl()
{
	DestroyLineActors();
}

FString FDrawingDesignerRenderControl::GetViewList() const
{
	FDrawingDesignerViewList viewList;

	TArray<const AModumateObjectInstance*> cutPlanes = static_cast<const UModumateDocument*>(Doc)->GetObjectsOfType(EObjectType::OTCutPlane);
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
			view.size.x = size.X;
			view.size.y = size.Y;
			
			FMOICutPlaneData cutPlaneData;
			cutPlane->GetStateData().CustomData.LoadStructData(cutPlaneData);
			view.name = cutPlaneData.Name;

			viewList.views.Add(view);
		}
	}

	TArray<const AModumateObjectInstance*> cameraViews = static_cast<const UModumateDocument*>(Doc)->GetObjectsOfType(EObjectType::OTCameraView);
	TArray<FWebMOI> cameraViewsFWebMOI;
	UModumateObjectStatics::GetWebMOIArrayForObjects(cameraViews, cameraViewsFWebMOI);
	
	viewList.cameraViews = cameraViewsFWebMOI;

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

bool FDrawingDesignerRenderControl::GetView(const FString& JsonRequest, FString& OutJsonResponse)
{
	double currentTime = FPlatformTime::Seconds();

	FDrawingDesignerDrawingRequest viewRequest;
	if (!ReadJsonGeneric(JsonRequest, &viewRequest))
	{
		return false;
	}

	AModumateObjectInstance* moi = Doc->GetObjectById(viewRequest.moi_id);
	auto* gameMode = Doc->GetWorld()->GetGameInstance<UModumateGameInstance>()->GetEditModelGameMode();
	if (!ensureAlways(moi) || !ensureAlways(moi->GetObjectType() == EObjectType::OTCutPlane) || !gameMode)
	{
		return false;
	}

	AMOICutPlane* cutPlane = Cast<AMOICutPlane>(moi);
	FVector corners[4];
	for (int32 c = 0; c < 4; ++c)
	{
		corners[c] = cutPlane->GetCorner(c);
	}

	const FVector planeCentre((corners[0] + corners[2]) / 2.0f);
	const float planeWidth = (corners[1] - corners[0]).Size();
	const float planeHeight = (corners[2] - corners[1]).Size();
	const FVector ll(corners[2] + viewRequest.roi.A.x * (corners[0] - corners[1])
		+ viewRequest.roi.A.y * (corners[1] - corners[2]));
	const FVector ur(corners[2] + viewRequest.roi.B.x * (corners[0] - corners[1])
		+ viewRequest.roi.B.y * (corners[1] - corners[2]));
	FVector cameraCentre((ll + ur) / 2.0f);
	const float viewWidth = planeWidth * FMath::Abs(viewRequest.roi.B.x - viewRequest.roi.A.x);
	const float viewHeight = planeHeight * FMath::Abs(viewRequest.roi.B.y - viewRequest.roi.A.y);

	static constexpr float minFeatureSizeScale = 3.0f;
	float scaleLength = FMath::Sqrt(viewWidth * viewHeight / (viewRequest.minimum_resolution_pixels.x * viewRequest.minimum_resolution_pixels.y))
		* minFeatureSizeScale;

	CachedXAxis = (corners[0] - corners[1]).GetSafeNormal();
	CachedYAxis = (corners[1] - corners[2]).GetSafeNormal();
	CachedOrigin = corners[2];
	CachedSize.X = planeWidth; CachedSize.Y = planeHeight;

	// Capture actor looks along +X (Y up); cutplane looks along +Z.
	static const FQuat cameraToCutplane(FVector(1, -1, 1).GetSafeNormal(), FMath::DegreesToRadians(120));
	FQuat cutPlaneRotation(cutPlane->GetRotation());

	if (FVector::Parallel(cutPlaneRotation * FVector::UpVector, FVector::UpVector))
	{
		// Horizontal cut planes are rotated 180 degrees relative to other cut planes.
		cutPlaneRotation *= FQuat(FVector::UpVector, FMath::DegreesToRadians(180));
		
		//This rotates our camera center 180 degrees as well.
		// This works because the Z component is always 0 in this case (due to the parallel check)
		FVector cameraZO = cameraCentre - planeCentre;
		cameraZO.Y = -cameraZO.Y;
		cameraZO.X = -cameraZO.X;
		cameraCentre = cameraZO + planeCentre;
	}

	FTransform cameraTransform(cutPlaneRotation * cameraToCutplane, cameraCentre, FVector(viewWidth, viewHeight, 1.0f));

	ADrawingDesignerRender* renderer = Doc->GetWorld()->SpawnActor<ADrawingDesignerRender>(gameMode->DrawingDesignerRenderClass.Get());

	if (!ensureAlways(renderer))
	{
		return false;
	}

	renderer->SetViewTransform(cameraTransform);
	renderer->SetDocument(Doc, this);

	FVector viewDirection(cutPlaneRotation * FVector::ZAxisVector);

	// Prepare cutplane
	int32 originalCullingCutPlane = MOD_ID_NONE;
	AEditModelPlayerController* controller = Cast<AEditModelPlayerController>(Doc->GetWorld()->GetFirstPlayerController());
	if (controller)
	{
		originalCullingCutPlane = controller->CurrentCullingCutPlaneID;
		controller->SetCurrentCullingCutPlane(cutPlane->ID, false);
	}

	renderer->SetupRenderTarget(viewRequest.minimum_resolution_pixels.x);
	renderer->RenderImage(cutPlane, scaleLength);

	// Restore cutplane
	controller->SetCurrentCullingCutPlane(originalCullingCutPlane);

	TArray<uint8> rawPng;
	bool bSuccess = false;

	bSuccess = renderer->GetImagePNG(rawPng);

	bSuccess = true;
	if (bSuccess)
	{
		FMOICutPlaneData cutPlaneData;
		cutPlane->GetStateData().CustomData.LoadStructData(cutPlaneData);

		FDrawingDesignerDrawingResponse viewResponse;
		viewResponse.request = viewRequest;
		viewResponse.response.resolution_pixels = viewRequest.minimum_resolution_pixels;
		viewResponse.response.view.moi_id = viewRequest.moi_id;
		viewResponse.response.view.size = { viewWidth, viewHeight };
		viewResponse.response.resolution_pixels = viewRequest.minimum_resolution_pixels;
		viewResponse.response.view.name = cutPlaneData.Name;
		viewResponse.response.scale = FModumateUnitValue(CachedSize.Y, EModumateUnitType::WorldCentimeters).AsWorldInches();

		//(D) 100ms
		FString b64Png(FBase64::Encode(rawPng));
		viewResponse.response.image_base64 = MoveTemp(b64Png);
		//(/D)
		GetSnapPoints(viewRequest.moi_id, viewResponse.response.snaps);
		bSuccess = WriteJsonGeneric(OutJsonResponse, &viewResponse);

		//Anything remaining is ~5-10ms
	}

	renderer->Destroy();

	SceneStaticMaterialMap.Empty();
	SceneProcMaterialMap.Empty();

	double endTime = FPlatformTime::Seconds();
	UE_LOG(LogTemp, Warning, TEXT("Created view for cut plane id %d in %lf s"), viewRequest.moi_id, endTime - currentTime);

	return bSuccess;
}

bool FDrawingDesignerRenderControl::GetMoiFromView(FVector2D uv, FDrawingDesignerView& view, int32& OutMoiId) const
{
	FVector2D size;
	FVector xaxis, yaxis, zaxis, origin;
	if (!GetViewAxis(view, xaxis, yaxis, zaxis, origin, size)) {
		return false;
	}

	//***********************************//
	FVector worldStart = UModumateGeometryStatics::Deproject2DPoint(uv * size, xaxis, yaxis, origin);
	auto forward = zaxis.GetSafeNormal();
	const FVector worldEnd = worldStart + forward * MOI_TRACE_DISTANCE;

	FHitResult hitResult;
	AEditModelPlayerController* controller = Cast<AEditModelPlayerController>(Doc->GetWorld()->GetFirstPlayerController());

	if (controller)
	{
		FCollisionQueryParams params = FCollisionQueryParams(MOITraceTag, SCENE_QUERY_STAT_ONLY(EditModelPlayerController), true);
		controller->GetWorld()->LineTraceSingleByObjectType(hitResult, worldStart, worldEnd, FCollisionObjectQueryParams::AllObjects, params);

		AModumateObjectInstance* object = Doc->ObjectFromActor(hitResult.GetActor());
		if (object)
		{
			OutMoiId = object->ID;
			return true;
		}
	}

	OutMoiId = INDEX_NONE;
	return false;
}

void FDrawingDesignerRenderControl::AddSceneLines(const FVector& ViewDirection, float MinLength, ADrawingDesignerRender* Render)
{
	TArray<const AModumateObjectInstance*> sceneLinesObjects = static_cast<const UModumateDocument*>(Doc)->GetObjectsOfType(
		{ EObjectType::OTDoor, EObjectType::OTWindow, EObjectType::OTFloorSegment, EObjectType::OTWallSegment, EObjectType::OTRoofFace,
		EObjectType::OTStructureLine, EObjectType::OTMullion, EObjectType::OTCabinet, EObjectType::OTFinish,EObjectType::OTTrim });

	TArray<FDrawingDesignerLine> sceneLines;
	for (const auto* moi : sceneLinesObjects)
	{
		moi->GetDrawingDesignerItems(ViewDirection, sceneLines, MinLength);
	}

	Render->AddLines(sceneLines, false);
}

ALineActor* FDrawingDesignerRenderControl::GetLineActor()
{
	if (LinePool.Num() > 0)
	{
		ALineActor* line = LinePool.Pop(false);
		line->SetVisibilityInApp(true);
		return line;
	}
	else
	{
		ALineActor* line = Doc->GetWorld()->SpawnActor<ALineActor>();
		line->SetIsHUD(false);
		line->MakeGeometry();
		line->ToggleForDrawingRender(true);
		return line;
	}
}

void FDrawingDesignerRenderControl::FreeLineActor(ALineActor* LineActor)
{
	if (LinePool.Num() >= LinePoolMaxSize)
	{
		LineActor->Destroy();
	}
	else
	{
		LineActor->SetVisibilityInApp(false);
		LinePool.Add(LineActor);
	}
}

bool FDrawingDesignerRenderControl::GetViewAxis(FDrawingDesignerView& View, FVector& OutXAxis, FVector& OutYAxis, FVector& OutZAxis, FVector& OutOrigin, FVector2D& OutSize) const
{
	const AModumateObjectInstance* moi = Doc->GetObjectById(View.moi_id);

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


	OutSize.X = (corners[1] - corners[0]).Size();
	OutSize.Y = (corners[2] - corners[1]).Size();
	OutXAxis = (corners[0] - corners[1]).GetSafeNormal();
	OutYAxis = (corners[1] - corners[2]).GetSafeNormal();
	OutOrigin = corners[2];
	OutZAxis = (OutXAxis ^ OutYAxis);

	return true;
}

void FDrawingDesignerRenderControl::RestorePortalMaterials()
{
	// Portals:
	TArray<const AModumateObjectInstance*> portalObjects = static_cast<const UModumateDocument*>(Doc)->GetObjectsOfType({ EObjectType::OTDoor, EObjectType::OTWindow });
	for (const auto* moi: portalObjects)
	{
		const ACompoundMeshActor* actor = CastChecked<ACompoundMeshActor>(moi->GetActor());
		const int32 numComponents = actor->UseSlicedMesh.Num();
		for (int32 componentIndex = 0; componentIndex < numComponents; ++componentIndex)
		{
			if (actor->UseSlicedMesh[componentIndex])
			{
				const int32 sliceStart = 9 * componentIndex;
				const int32 sliceEnd = 9 * (componentIndex + 1);
				for (int32 slice = sliceStart; slice < sliceEnd; ++slice)
				{
					UProceduralMeshComponent* meshComponent = actor->NineSliceComps[slice];
					if (!meshComponent)
					{
						continue;
					}

					const int32 numMaterials = meshComponent->GetNumMaterials();
					for (int32 materialIndex = 0; materialIndex < numMaterials; ++materialIndex)
					{
						UMaterialInterface** scenematerial = SceneProcMaterialMap.Find(ProcMaterialKey(meshComponent, materialIndex));
						if (ensure(scenematerial))
						{
							meshComponent->SetMaterial(materialIndex, *scenematerial);
						}
					}
					meshComponent->CustomDepthStencilValue = DisableMeshStencilValue;
					meshComponent->SetRenderCustomDepth(false);
				}
			}
			else
			{
				UStaticMeshComponent* meshComponent = actor->StaticMeshComps[componentIndex];
				if (!meshComponent)
				{
					continue;
				}

				const int32 numMaterials = meshComponent->GetNumMaterials();
				UStaticMesh* mesh = meshComponent->GetStaticMesh();

				const FName fname = meshComponent->GetFName();
				for (int32 materialIndex = 0; materialIndex < numMaterials; ++materialIndex)
				{
					UMaterialInterface** scenematerial = SceneStaticMaterialMap.Find(StaticMaterialKey(meshComponent, materialIndex));
					if (ensure(scenematerial))
					{
						meshComponent->SetMaterial(materialIndex, *scenematerial);
					}
				}
				meshComponent->CustomDepthStencilValue = DisableMeshStencilValue;
				meshComponent->SetRenderCustomDepth(false);
			}
		}

	}

	// Layered separators:
	TArray<const AModumateObjectInstance*> layeredObjects = static_cast<const UModumateDocument*>(Doc)->GetObjectsOfType({ EObjectType::OTFloorSegment, EObjectType::OTWallSegment, EObjectType::OTRoofFace,
		EObjectType::OTStructureLine, EObjectType::OTMullion });
	for (const auto* moi: layeredObjects)
	{
		const ADynamicMeshActor* actor = CastChecked<ADynamicMeshActor>(moi->GetActor());
		if (!actor)
		{
			continue;
		}

		TArray<UProceduralMeshComponent*> meshComponents = { actor->Mesh, actor->MeshCap };
		meshComponents.Append(actor->ProceduralSubLayers);
		meshComponents.Append(actor->ProceduralSubLayerCaps);

		for (UProceduralMeshComponent* meshComponent: meshComponents)
		{
			if (!meshComponent || meshComponent->GetNumMaterials() == 0)
			{
				continue;
			}

			const int32 numMaterials = meshComponent->GetNumMaterials();
			for (int32 materialIndex = 0; materialIndex < numMaterials; ++materialIndex)
			{
				UMaterialInterface** scenematerial = SceneProcMaterialMap.Find(ProcMaterialKey(meshComponent, materialIndex));
				if (ensure(scenematerial))
				{
					meshComponent->SetMaterial(materialIndex, *scenematerial);
				}
			}
			meshComponent->CustomDepthStencilValue = DisableMeshStencilValue;
			meshComponent->SetRenderCustomDepth(false);
		}
	}
}


void FDrawingDesignerRenderControl::GetSnapPoints(int32 viewId, TMap<FString, FDrawingDesignerSnap>& OutSnapPoints) const
{
	//Derive our cut plane
	const FVector zAxis(CachedXAxis ^ CachedYAxis);
	const FPlane cutPlane(CachedOrigin, zAxis);

	FVector xAxis(CachedXAxis);
	FVector yAxis(CachedYAxis);
	FVector origin(CachedOrigin);
	if (FVector::Parallel(zAxis, FVector::UpVector))
	{   // Horizontal cut planes are rotated.
		origin += CachedSize.X * xAxis + CachedSize.Y * yAxis;
		xAxis = -xAxis;
		yAxis = -yAxis;
	}
	
	//TODO: Get non-active graphs as well (meta graph objects) -JN
	//Mass Graph Vertices
	FGraph3D* graph = Doc->GetVolumeGraph();

	//Mass Graph Lines-->
	auto& edges = graph->GetEdges();
	auto& vertices = graph->GetVertices();


	//This lambda will cut short a line given 2 points around the cut plane
	auto correctPointVector = [&](const FVector& Valid, FVector& InOutReproject) {

		FVector corrected = FMath::LinePlaneIntersection(Valid, InOutReproject, cutPlane.GetOrigin(), cutPlane.GetNormal());

		InOutReproject.X = corrected.X;
		InOutReproject.Y = corrected.Y;
		InOutReproject.Z = corrected.Z;
	};

	auto correctPoint = [&](const FGraph3DVertex& Valid, FGraph3DVertex& InOutReproject) {
		FVector first; first.X = Valid.Position.X; first.Y = Valid.Position.Y; first.Z = Valid.Position.Z;
		FVector second; second.X = InOutReproject.Position.X; second.Y = InOutReproject.Position.Y; second.Z = InOutReproject.Position.Z;
		correctPointVector(first, second);

		InOutReproject.Position.X = second.X;
		InOutReproject.Position.Y = second.Y;
		InOutReproject.Position.Z = second.Z;
	};

	auto copyPoint = [&](FDrawingDesignerSnapId Id, FVector& Position, FDrawingDesignerSnapPoint& OutSnap) {
		FVector2D projectedPoint(UModumateGeometryStatics::ProjectPoint2D(Position, xAxis, yAxis, origin));
		FVector2D scaledPoint(projectedPoint / CachedSize);
		OutSnap.x = scaledPoint.X;
		OutSnap.y = scaledPoint.Y;
		OutSnap.id = Id;
	};

#ifdef USE_GRAPH_SNAPS_FOR_DD
	for (auto& kvp : edges) {
		const FGraph3DEdge& val = kvp.Value;

		FGraph3DVertex v1 = vertices[val.StartVertexID];
		FGraph3DVertex v2 = vertices[val.EndVertexID];

		//Three possible conditions
		//	1) line lies entirely on the correct side of the cut plane
		//  2) both points are behind the cut plane
		//  3) One of the points are on the correct side of the cut plane
		if ((cutPlane.PlaneDot(v1.Position) > -PLANAR_DOT_EPSILON) && (cutPlane.PlaneDot(v2.Position) > -PLANAR_DOT_EPSILON))
		{
			//Both points are good, do nothing
		}
		else if (cutPlane.PlaneDot(v1.Position) > -PLANAR_DOT_EPSILON) {
			//v1 is good, v2 needs correcting
			correctPoint(v1, v2);
		}
		else if (cutPlane.PlaneDot(v2.Position) > -PLANAR_DOT_EPSILON) {
			//v2 is good, v1 need correcting
			correctPoint(v2, v1);
		}
		else {
			//Neither is good
			continue;
		}


		//One snap per edge/line
		FDrawingDesignerSnapId edgeId = FDrawingDesignerSnapId(EDDSnapType::line, FString::FromInt(viewId), INDEX_NONE, INDEX_NONE, kvp.Key);
		FDrawingDesignerSnap edgeSnap = FDrawingDesignerSnap(edgeId);

		//Two points per line snap
		FDrawingDesignerSnapPoint p1, p2;
		copyPoint(FDrawingDesignerSnapId(edgeId, 0), v1.Position, p1);
		copyPoint(FDrawingDesignerSnapId(edgeId, 1), v2.Position, p2);
		edgeSnap.points.Add(p1);
		edgeSnap.points.Add(p2);

		//Add the mid snap as well
		FDrawingDesignerSnapId midSnapId = FDrawingDesignerSnapId(EDDSnapType::midpoint, FString::FromInt(viewId), INDEX_NONE, INDEX_NONE, kvp.Key);
		FDrawingDesignerSnap midSnap = FDrawingDesignerSnap(midSnapId);

		//mid point for mid snap
		FDrawingDesignerSnapPoint mid;
		mid.x = (p1.x + p2.x) / 2;
		mid.y = (p1.y + p2.y) / 2;
		mid.id = midSnapId;
		mid.id.pointIndex = 0;
		midSnap.points.Add(mid);

		//KLUDGE: Javascript sucks -JN
		FString edgeEntryKey = TEXT("");
		edgeSnap.id.EncodeKey(edgeEntryKey);
		OutSnapPoints.Add(edgeEntryKey, edgeSnap);

		FString midEntryKey = TEXT("");
		midSnap.id.EncodeKey(midEntryKey);
		OutSnapPoints.Add(midEntryKey, midSnap);
	}
#endif

	//MOI-Based bounding points
	TArray<AModumateObjectInstance*> snapObjects = Doc->GetObjectsOfType(
		{
		EObjectType::OTWallSegment,
		EObjectType::OTRailSegment,
		EObjectType::OTFloorSegment,
		EObjectType::OTRoofFace,
		EObjectType::OTDoor,
		EObjectType::OTWindow,
		EObjectType::OTCountertop,
		EObjectType::OTCeiling,
		//TODO: These are commented out as a 'They're not done yet'. -JN
		//EObjectType::OTFurniture, 
		//EObjectType::OTCabinet,
		//EObjectType::OTStaircase,
		//EObjectType::OTTrim,
		//EObjectType::OTMullion,
		//EObjectType::OTPointHosted,
		//EObjectType::OTEdgeHosted
		});

	
	for (AModumateObjectInstance* moi : snapObjects)
	{
		TArray<FDrawingDesignerLine> bounds;
		if (moi->GetBoundingLines(bounds)) {
			//Evaluate each line
			for (int i = 0; i < bounds.Num(); i++)
			{
				auto& line = bounds[i];
				auto& v1 = line.P1;
				auto& v2 = line.P2;
				
				//Same three possibilities as above:
				if ((cutPlane.PlaneDot(v1) > -PLANAR_DOT_EPSILON) && (cutPlane.PlaneDot(v2) > -PLANAR_DOT_EPSILON))
				{
					//Both valid
				}
				else if (cutPlane.PlaneDot(v1) > -PLANAR_DOT_EPSILON)
				{
					correctPointVector(v1, v2);
				}
				else if (cutPlane.PlaneDot(v2) > -PLANAR_DOT_EPSILON)
				{
					correctPointVector(v2, v1);
				}
				else 
				{
					continue;
				}

				//One snap per edge
				FDrawingDesignerSnapId edgeId = FDrawingDesignerSnapId(EDDSnapType::line, FString::FromInt(viewId), moi->ID, moi->GetParentID(),INDEX_NONE, i);
				FDrawingDesignerSnap edgeSnap = FDrawingDesignerSnap(edgeId);
				
				//Two points per snap
				FDrawingDesignerSnapPoint p1, p2;
				copyPoint(FDrawingDesignerSnapId(edgeId, 0), v1, p1);
				copyPoint(FDrawingDesignerSnapId(edgeId, 1), v2, p2);
				edgeSnap.points.Add(p1);
				edgeSnap.points.Add(p2);

				//Add the mid snap as well
				FDrawingDesignerSnapId midSnapId = FDrawingDesignerSnapId(EDDSnapType::midpoint, FString::FromInt(viewId), moi->ID, moi->GetParentID(), INDEX_NONE, i);
				FDrawingDesignerSnap midSnap = FDrawingDesignerSnap(midSnapId);

				//mid point for mid snap
				FDrawingDesignerSnapPoint mid;
				mid.x = (p1.x + p2.x) / 2;
				mid.y = (p1.y + p2.y) / 2;
				mid.id = midSnapId;
				mid.id.pointIndex = 0;
				midSnap.points.Add(mid);

				FString edgeEntryKey = TEXT("");
				edgeSnap.id.EncodeKey(edgeEntryKey);
				OutSnapPoints.Add(edgeEntryKey, edgeSnap);

				FString midKey = TEXT("");
				midSnap.id.EncodeKey(midKey);
				OutSnapPoints.Add(midKey, midSnap);
			}

		}
		
	}
}

void FDrawingDesignerRenderControl::DestroyLineActors()
{
	for (auto& line: LinePool)
	{
		line->Destroy();
	}
	LinePool.Empty();
}
