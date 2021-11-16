// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "DrawingDesigner/DrawingDesignerRenderControl.h"

#include "DocumentManagement/ModumateDocument.h"
#include "DrawingDesigner/DrawingDesignerView.h"
#include "DrawingDesigner/DrawingDesignerLine.h"
#include "DrawingDesigner/DrawingDesignerRender.h"
#include "Objects/CutPlane.h"
#include "UnrealClasses/CompoundMeshActor.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "UnrealClasses/EditModelGameMode.h"

FString FDrawingDesignerRenderControl::GetViewList()
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

bool FDrawingDesignerRenderControl::GetView(const FString& jsonRequest, FString& OutJsonResponse)
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

	static constexpr float minFeatureSizeScale = 3.0f;
	float scaleLength = FMath::Sqrt(viewWidth * viewHeight / (viewRequest.minimum_resolution_pixels.x * viewRequest.minimum_resolution_pixels.y))
		* minFeatureSizeScale;

	// Capture actor looks along +X (Y up); cutplane looks along +Z.
	static const FQuat cameraToCutplane(FVector(1, -1, 1).GetSafeNormal(), FMath::DegreesToRadians(120));
	const FQuat cutPlaneRotation(cutPlane->GetRotation());

	FTransform cameraTransform(cutPlaneRotation * cameraToCutplane, cameraCentre, FVector(viewWidth, viewHeight, 1.0f));

	ADrawingDesignerRender* renderer = Doc->GetWorld()->SpawnActor<ADrawingDesignerRender>();

	renderer->SetViewTransform(cameraTransform);
	renderer->SetDocument(Doc);

	FVector viewDirection(cutPlaneRotation * FVector::ZAxisVector);
	AddSceneLines(viewDirection, scaleLength, renderer);

	SwapPortalMaterials();

	renderer->RenderImage(viewRequest.minimum_resolution_pixels.x);

	RestorePortalMaterials();

	TArray<uint8> rawPng;
	bool bSuccess = false;
	if (ensure(renderer->GetImagePNG(rawPng)) )
	{
		FString b64Png(FBase64::Encode(rawPng));
		FMOICutPlaneData cutPlaneData;
		cutPlane->GetStateData().CustomData.LoadStructData(cutPlaneData);

		FDrawingDesignerDrawingResponse viewResponse;
		viewResponse.request = viewRequest;
		viewResponse.response.resolution_pixels = viewRequest.minimum_resolution_pixels;
		viewResponse.response.view.moi_id = viewRequest.moi_id;
		viewResponse.response.view.aspect = { 1.0f, viewHeight / viewWidth };
		viewResponse.response.resolution_pixels = viewRequest.minimum_resolution_pixels;
		viewResponse.response.view.name = cutPlaneData.Name;

		viewResponse.response.image_base64 = MoveTemp(b64Png);
		bSuccess = WriteJsonGeneric(OutJsonResponse, &viewResponse);
	}

	renderer->Destroy();

	SceneStaticMaterialMap.Empty();
	SceneProcMaterialMap.Empty();
	return bSuccess;
}

void FDrawingDesignerRenderControl::AddSceneLines(const FVector& ViewDirection, float MinLength, ADrawingDesignerRender* Render)
{
	TArray<const AModumateObjectInstance*> sceneLinesObjects = Doc->GetObjectsOfType(
		{ EObjectType::OTDoor, EObjectType::OTWindow, EObjectType::OTFloorSegment, EObjectType::OTWallSegment, EObjectType::OTRoofFace,
		EObjectType::OTStructureLine, EObjectType::OTMullion });

	TArray<FDrawingDesignerLine> sceneLines;
	for (const auto* moi : sceneLinesObjects)
	{
		moi->GetDrawingDesignerItems(ViewDirection, sceneLines, MinLength);
	}

	Render->AddLines(sceneLines);
}

void FDrawingDesignerRenderControl::SwapPortalMaterials()
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
	TArray<const AModumateObjectInstance*> portalObjects = Doc->GetObjectsOfType({ EObjectType::OTDoor, EObjectType::OTWindow });
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
			}
		}

	}

	// Layered separators, columns:
	TArray<const AModumateObjectInstance*> layeredObjects = Doc->GetObjectsOfType({ EObjectType::OTFloorSegment, EObjectType::OTWallSegment, EObjectType::OTRoofFace,
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
		}
	}

}

void FDrawingDesignerRenderControl::RestorePortalMaterials()
{
	// Portals:
	TArray<const AModumateObjectInstance*> portalObjects = Doc->GetObjectsOfType({ EObjectType::OTDoor, EObjectType::OTWindow });
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
			}
		}

	}

	// Layered separators:
	TArray<const AModumateObjectInstance*> layeredObjects = Doc->GetObjectsOfType({ EObjectType::OTFloorSegment, EObjectType::OTWallSegment, EObjectType::OTRoofFace,
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
		}
	}
}
