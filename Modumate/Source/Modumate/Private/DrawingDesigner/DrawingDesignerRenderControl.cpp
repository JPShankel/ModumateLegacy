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

bool FDrawingDesignerRenderControl::GetView(const FString& JsonRequest, FString& OutJsonResponse)
{
	double currentTime = FPlatformTime::Seconds();

	FDrawingDesignerDrawingRequest viewRequest;
	if (!ReadJsonGeneric(JsonRequest, &viewRequest))
	{
		return false;
	}

	AModumateObjectInstance* moi = Doc->GetObjectById(viewRequest.moiId);
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
	const FVector ll(corners[2] + viewRequest.roi.a.x * (corners[0] - corners[1])
		+ viewRequest.roi.a.y * (corners[1] - corners[2]));
	const FVector ur(corners[2] + viewRequest.roi.b.x * (corners[0] - corners[1])
		+ viewRequest.roi.b.y * (corners[1] - corners[2]));
	FVector cameraCentre((ll + ur) / 2.0f);
	const float viewWidth = planeWidth * FMath::Abs(viewRequest.roi.b.x - viewRequest.roi.a.x);
	const float viewHeight = planeHeight * FMath::Abs(viewRequest.roi.b.y - viewRequest.roi.a.y);

	static constexpr float minFeatureSizeScale = 3.0f;
	float scaleLength = FMath::Sqrt(viewWidth * viewHeight / (viewRequest.minimumResolutionPixels.x * viewRequest.minimumResolutionPixels.y))
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

	TArray<int32> selectedOptions;
	if (viewRequest.attributes.JsonObject.IsValid())
	{
		// One current render attribute: designOptions
		static const FString designOptionsName(TEXT("designOptions"));

		const auto designOptionsAttrib = viewRequest.attributes.JsonObject->TryGetField(designOptionsName);
		if (designOptionsAttrib.IsValid() && designOptionsAttrib->Type == EJson::Array)
		{
			for (const auto& opt: designOptionsAttrib->AsArray())
			{
				selectedOptions.Add(FCString::Atoi(*opt->AsString()) );
			}
		}
	}

	renderer->SetupRenderTarget(viewRequest.minimumResolutionPixels.x);
	renderer->RenderImage(cutPlane, scaleLength, &selectedOptions);

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

		FWebMOI webMoi;
		cutPlane->ToWebMOI(webMoi);

		FDrawingDesignerDrawingResponse viewResponse;
		viewResponse.request = viewRequest;
		viewResponse.response.resolutionPixels = viewRequest.minimumResolutionPixels;
		viewResponse.response.view = webMoi;
		viewResponse.response.resolutionPixels = viewRequest.minimumResolutionPixels;
		viewResponse.response.scale = FModumateUnitValue(CachedSize.Y, EModumateUnitType::WorldCentimeters).AsWorldInches();

		//(D) 100ms
		FString b64Png(FBase64::Encode(rawPng));
		viewResponse.response.imageBase64 = MoveTemp(b64Png);
		//(/D)
		GetSnapPoints(viewRequest.moiId, viewResponse.response.snaps);
		bSuccess = WriteJsonGeneric(OutJsonResponse, &viewResponse);

		//Anything remaining is ~5-10ms
	}

	renderer->Destroy();

	SceneStaticMaterialMap.Empty();
	SceneProcMaterialMap.Empty();

	double endTime = FPlatformTime::Seconds();
	UE_LOG(LogTemp, Warning, TEXT("Created view for cut plane id %d in %lf s"), viewRequest.moiId, endTime - currentTime);

	return bSuccess;
}

bool FDrawingDesignerRenderControl::GetMoiFromView(FVector2D uv, AMOICutPlane& view, int32& OutMoiId) const
{
	FVector2D size;
	FVector xaxis, yaxis, zaxis, origin;
	if (!GetViewAxis(view, xaxis, yaxis, zaxis, origin, size)) {
		return false;
	}

	if(IsFloorplan(view))
	{
		//Flip the uvs around the center (0.5, 0.5)
		uv = FVector2D::UnitVector - uv;
	}
	
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
		line->SetActorEnableCollision(false);
		return line;
	}
	else
	{
		ALineActor* line = Doc->GetWorld()->SpawnActor<ALineActor>();
		line->SetIsHUD(false);
		line->MakeGeometry();
		line->ToggleForDrawingRender(true);
		line->SetActorEnableCollision(false);
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
		LineActor->SetActorEnableCollision(false);
		LinePool.Add(LineActor);
	}
}

bool FDrawingDesignerRenderControl::GetViewAxis(AMOICutPlane& View, FVector& OutXAxis, FVector& OutYAxis, FVector& OutZAxis, FVector& OutOrigin, FVector2D& OutSize) const
{
	
	FVector corners[4];
	for (int32 c = 0; c < 4; ++c)
	{
		corners[c] = View.GetCorner(c);
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
				FDrawingDesignerSnap candidateSnap = FDrawingDesignerSnap(edgeId);
				
				//Two points per snap
				FDrawingDesignerSnapPoint p1, p2;
				copyPoint(FDrawingDesignerSnapId(edgeId, 0), v1, p1);
				copyPoint(FDrawingDesignerSnapId(edgeId, 1), v2, p2);
				
				//Only add lines/mids that are >length(0) after projection
				bool same = FMath::IsNearlyEqual(p1.x, p2.x) && FMath::IsNearlyEqual(p1.y, p2.y);
				if(same) continue;
				
				candidateSnap.points.Add(p1);
				candidateSnap.points.Add(p2);
				
				candidateSnap.points[0].z = FVector::DistSquared(line.P1, v1);
				candidateSnap.points[1].z = FVector::DistSquared(line.P2, v2);
				
				//Add the mid snap as well
				FDrawingDesignerSnapId midSnapId = FDrawingDesignerSnapId(candidateSnap.id, INDEX_NONE);
				midSnapId.type = EDDSnapType::midpoint;
				midSnapId.id = candidateSnap.id.id;
				FDrawingDesignerSnap midSnap = FDrawingDesignerSnap(midSnapId);

				FVector worldMid = (line.P1 + line.P2)/2;
				FVector planeMid = (v1 + v2)/2; 
				float midZ = FVector::DistSquared(worldMid, planeMid);
				
				//mid point for mid snap
				FDrawingDesignerSnapPoint mid;
				FVector2D P1 = FVector2D{candidateSnap.points[0].x, candidateSnap.points[0].y};
				FVector2D P2 = FVector2D{candidateSnap.points[1].x, candidateSnap.points[1].y};
				mid.x = (P1.X + P2.X) / 2;
				mid.y = (P1.Y + P2.Y) / 2;
				
				mid.z = midZ;
				mid.id = midSnapId;
				mid.id.pointIndex = 0;
				midSnap.points.Add(mid);
		
				FString midKey = TEXT("");
				midSnap.id.EncodeKey(midKey);
				OutSnapPoints.Add(midKey, midSnap);
		
				FString edgeEntryKey = TEXT("");
				candidateSnap.id.EncodeKey(edgeEntryKey);
				OutSnapPoints.Add(edgeEntryKey, candidateSnap);
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

bool FDrawingDesignerRenderControl::IsFloorplan(const AMOICutPlane& View) const
{	
	const FQuat cutPlaneRotation(View.GetRotation());
	return (FVector::Parallel(cutPlaneRotation * FVector::UpVector, FVector::UpVector));
}
