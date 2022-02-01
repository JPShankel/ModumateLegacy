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

constexpr float MOI_TRACE_DISTANCE = 5000.0f;

// Stencil values are determined by post process material in DrawingDesignerRender
constexpr int32 DisableMeshStencilValue = 0;
constexpr int32 ForegroundMeshStencilValue  = 1;

FString FDrawingDesignerRenderControl::GetViewList()
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
			view.aspect.x = 1.0f;
			view.aspect.y = size.Y / size.X;
			
			FMOICutPlaneData cutPlaneData;
			cutPlane->GetStateData().CustomData.LoadStructData(cutPlaneData);
			view.name = cutPlaneData.Name;

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

bool FDrawingDesignerRenderControl::GetView(const FString& jsonRequest, FString& OutJsonResponse)
{
	FDrawingDesignerDrawingRequest viewRequest;
	if (!ReadJsonGeneric(jsonRequest, &viewRequest))
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

	static constexpr float minFeatureSizeScale = 3.0f;
	float scaleLength = FMath::Sqrt(viewWidth * viewHeight / (viewRequest.minimum_resolution_pixels.x * viewRequest.minimum_resolution_pixels.y))
		* minFeatureSizeScale;

	CachedXAxis = (corners[0] - corners[1]).GetSafeNormal();
	CachedYAxis = (corners[1] - corners[2]).GetSafeNormal();
	CachedOrigin = corners[2];
	CachedSize.X = planeWidth; CachedSize.Y = planeHeight;

	// Capture actor looks along +X (Y up); cutplane looks along +Z.
	static const FQuat cameraToCutplane(FVector(1, -1, 1).GetSafeNormal(), FMath::DegreesToRadians(120));
	const FQuat cutPlaneRotation(cutPlane->GetRotation());

	FTransform cameraTransform(cutPlaneRotation * cameraToCutplane, cameraCentre, FVector(viewWidth, viewHeight, 1.0f));

	ADrawingDesignerRender* renderer = Doc->GetWorld()->SpawnActor<ADrawingDesignerRender>(gameMode->DrawingDesignerRenderClass.Get());

	if (!ensureAlways(renderer))
	{
		return false;
	}

	renderer->SetViewTransform(cameraTransform);
	renderer->SetDocument(Doc);

	FVector viewDirection(cutPlaneRotation * FVector::ZAxisVector);
	//(A) ~175ms
	AddSceneLines(viewDirection, scaleLength, renderer);
	//(/A)
	SwapPortalMaterials(cutPlane);

	// Prepare cutplane
	int32 originalCullingCutPlane = MOD_ID_NONE;
	AEditModelPlayerController* controller = Cast<AEditModelPlayerController>(Doc->GetWorld()->GetFirstPlayerController());
	if (controller)
	{
		originalCullingCutPlane = controller->CurrentCullingCutPlaneID;
		controller->SetCurrentCullingCutPlane(cutPlane->ID, false);
	}

	// Draw 
	//(B) ~10ms
	renderer->RenderImage(viewRequest.minimum_resolution_pixels.x);
	//(/B)

	// Restore cutplane
	RestorePortalMaterials();
	controller->SetCurrentCullingCutPlane(originalCullingCutPlane);

	TArray<uint8> rawPng;
	bool bSuccess = false;


	//(C) ~100ms
	bSuccess = renderer->GetImagePNG(rawPng);
	//(/C)

	bSuccess = true;
	if (bSuccess)
	{
		FMOICutPlaneData cutPlaneData;
		cutPlane->GetStateData().CustomData.LoadStructData(cutPlaneData);

		FDrawingDesignerDrawingResponse viewResponse;
		viewResponse.request = viewRequest;
		viewResponse.response.resolution_pixels = viewRequest.minimum_resolution_pixels;
		viewResponse.response.view.moi_id = viewRequest.moi_id;
		viewResponse.response.view.aspect = { 1.0f, viewHeight / viewWidth };
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
	return bSuccess;
}

bool FDrawingDesignerRenderControl::GetMoiFromView(FVector2D uv, FDrawingDesignerView& view, int32& OutMoiId)
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
		controller->LineTraceSingleAgainstMOIs(hitResult, worldStart, worldEnd);

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
		EObjectType::OTStructureLine, EObjectType::OTMullion });

	TArray<FDrawingDesignerLine> sceneLines;
	for (const auto* moi : sceneLinesObjects)
	{
		moi->GetDrawingDesignerItems(ViewDirection, sceneLines, MinLength);
	}

	Render->AddLines(sceneLines);
}

bool FDrawingDesignerRenderControl::GetViewAxis(FDrawingDesignerView& view, FVector& outXAxis, FVector& outYAxis, FVector& outZAxis, FVector& outOrigin, FVector2D& outSize)
{
	const AModumateObjectInstance* moi = Doc->GetObjectById(view.moi_id);

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


	outSize.X = (corners[1] - corners[0]).Size();
	outSize.Y = (corners[2] - corners[1]).Size();
	outXAxis = (corners[0] - corners[1]).GetSafeNormal();
	outYAxis = (corners[1] - corners[2]).GetSafeNormal();
	outOrigin = corners[2];
	outZAxis = (outXAxis ^ outYAxis);

	return true;
}

void FDrawingDesignerRenderControl::SwapPortalMaterials(AMOICutPlane* CutPlane)
{
	auto* gameMode = Doc->GetWorld()->GetGameInstance<UModumateGameInstance>()->GetEditModelGameMode();
	if (!gameMode)
	{
		return;
	}

	UMaterialInterface* masterPBR = gameMode->EmissiveUnlitMaterial;
	if (!ensure(masterPBR))
	{
		return;
	}

	// Portals:
	TArray<const AModumateObjectInstance*> portalObjects = static_cast<const UModumateDocument*>(Doc)->GetObjectsOfType({ EObjectType::OTDoor, EObjectType::OTWindow });
	for (const auto* moi: portalObjects)
	{
		const ACompoundMeshActor* actor = CastChecked<ACompoundMeshActor>(moi->GetActor());
		if (!actor)
		{
			continue;
		}

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
					UMaterialInstanceDynamic* curMID = UMaterialInstanceDynamic::Create(masterPBR, meshComponent);
					static const FName emissiveMultiplierParamName("EmissiveMultiplier");
					static const FName emissiveColorMultiplierParamName("EmissiveColorMultiplier");
					curMID->SetScalarParameterValue(emissiveMultiplierParamName, 1.0f);
					curMID->SetVectorParameterValue(emissiveColorMultiplierParamName, FLinearColor(0.8f, 0.8f, 0.8f));

					for (int32 materialIndex = 0; materialIndex < numMaterials; ++materialIndex)
					{
						UMaterialInterface* sceneMaterial = meshComponent->GetMaterial(materialIndex);
						SceneProcMaterialMap.Add(ProcMaterialKey(meshComponent, materialIndex), sceneMaterial);
						meshComponent->SetMaterial(materialIndex, curMID);
					}
					meshComponent->CustomDepthStencilValue = ForegroundMeshStencilValue;
					meshComponent->SetRenderCustomDepth(true);
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

				UMaterialInstanceDynamic* curMID = UMaterialInstanceDynamic::Create(masterPBR, meshComponent);
				static const FName emissiveMultiplierParamName("EmissiveMultiplier");
				static const FName emissiveColorMultiplierParamName("EmissiveColorMultiplier");
				curMID->SetScalarParameterValue(emissiveMultiplierParamName, 1.0f);
				curMID->SetVectorParameterValue(emissiveColorMultiplierParamName, FLinearColor(0.6f, 0.6f, 0.6f));

				const FName fname = meshComponent->GetFName();
				for (int32 materialIndex = 0; materialIndex < numMaterials; ++materialIndex)
				{
					UMaterialInterface* sceneMaterial = meshComponent->GetMaterial(materialIndex);
					SceneStaticMaterialMap.Add(StaticMaterialKey(meshComponent, materialIndex), sceneMaterial);
					meshComponent->SetMaterial(materialIndex, curMID);
				}
				meshComponent->CustomDepthStencilValue = ForegroundMeshStencilValue;
				meshComponent->SetRenderCustomDepth(true);
			}
		}

	}

	// Layered separators, columns:
	TArray<const AModumateObjectInstance*> layeredObjects = static_cast<const UModumateDocument*>(Doc)->GetObjectsOfType({ EObjectType::OTFloorSegment, EObjectType::OTWallSegment, EObjectType::OTRoofFace,
		EObjectType::OTStructureLine, EObjectType::OTMullion });
	for (const auto* moi: layeredObjects)
	{
		const ADynamicMeshActor* actor = CastChecked<ADynamicMeshActor>(moi->GetActor());
		if (!actor)
		{
			continue;
		}

		bool bIsForegroundObj = CutPlane->GetCachedForegroundMoiIDs().Contains(moi->ID);

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
			UMaterialInstanceDynamic* curMID = UMaterialInstanceDynamic::Create(masterPBR, meshComponent);
			static const FName emissiveMultiplierParamName("EmissiveMultiplier");
			static const FName emissiveColorMultiplierParamName("EmissiveColorMultiplier");
			curMID->SetScalarParameterValue(emissiveMultiplierParamName, 1.0f);
			curMID->SetVectorParameterValue(emissiveColorMultiplierParamName, FLinearColor(0.8f, 0.8f, 0.8f));

			for (int32 materialIndex = 0; materialIndex < numMaterials; ++materialIndex)
			{
				UMaterialInterface* sceneMaterial = meshComponent->GetMaterial(materialIndex);
				SceneProcMaterialMap.Add(ProcMaterialKey(meshComponent, materialIndex), sceneMaterial);
				meshComponent->SetMaterial(materialIndex, curMID);
			}

			if (bIsForegroundObj)
			{
				meshComponent->CustomDepthStencilValue = ForegroundMeshStencilValue;
				meshComponent->SetRenderCustomDepth(true);
			}
		}
	}

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


void FDrawingDesignerRenderControl::GetSnapPoints(int32 viewId, TMap<FString, FDrawingDesignerSnap>& OutSnapPoints)
{
	//Derive our cut plane
	const FVector zAxis(CachedXAxis ^ CachedYAxis);
	const FPlane cutPlane(CachedOrigin, zAxis);
	
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
		FVector2D projectedPoint(UModumateGeometryStatics::ProjectPoint2D(Position, CachedXAxis, CachedYAxis, CachedOrigin));
		FVector2D scaledPoint(projectedPoint / CachedSize);
		OutSnap.x = scaledPoint.X;
		OutSnap.y = scaledPoint.Y;
		OutSnap.id = Id;
	};

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

		//One snap per edge
		FDrawingDesignerSnap newSnap;
		FDrawingDesignerSnapId snapId;
		snapId.viewMoiId = FString::FromInt(viewId);
		snapId.owningMoiId = INDEX_NONE;
		snapId.id = kvp.Key;
		snapId.pointIndex = INDEX_NONE;

		//Two points per snap
		FDrawingDesignerSnapPoint p1, p2;
		copyPoint(FDrawingDesignerSnapId(snapId, 0), v1.Position, p1);
		copyPoint(FDrawingDesignerSnapId(snapId, 1), v2.Position, p2);
		newSnap.points.Add(p1);
		newSnap.points.Add(p2);

		//KLUDGE: Javascript sucks -JN
		FString entryKey = TEXT("");
		snapId.EncodeKey(entryKey);
		OutSnapPoints.Add(entryKey, newSnap);
	}

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
				FDrawingDesignerSnap newSnap;
				FDrawingDesignerSnapId snapId;
				snapId.viewMoiId = FString::FromInt(viewId);
				snapId.owningMoiId = moi->ID;
				snapId.id = i;
				snapId.pointIndex = INDEX_NONE;

				//Two points per snap
				FDrawingDesignerSnapPoint p1, p2;
				copyPoint(FDrawingDesignerSnapId(snapId, 0), v1, p1);
				copyPoint(FDrawingDesignerSnapId(snapId, 1), v2, p2);
				newSnap.points.Add(p1);
				newSnap.points.Add(p2);

				FString entryKey = TEXT("");
				snapId.EncodeKey(entryKey);
				OutSnapPoints.Add(entryKey, newSnap);
			}

		}
		
	}
}
