// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "DocumentManagement/ModumateObjectInstanceCutPlane.h"

#include "UnrealClasses/CutPlaneCaptureActor.h"
#include "Drafting/DraftingManager.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "Graph/Graph3DFace.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "ModumateCore/ModumateUserSettings.h"
#include "Drafting/ModumateDraftingElements.h"
#include "Drafting/ModumateDraftingView.h"
#include "DocumentManagement/ModumateLayeredObjectInterface.h"
#include "ModumateCore/ModumateUnits.h"

#include "UnrealClasses/ModumateGameInstance.h"

namespace Modumate
{
	FMOICutPlaneImpl::FMOICutPlaneImpl(FModumateObjectInstance *moi)
		: FMOIPlaneImplBase(moi),
		EdgeSelectedColor(28.0f / 255.0f, 159.0f / 255.0f, 255.0f / 255.0f),
		EdgeColor(73.0f / 255.0f, 179.0f / 255.0f, 255.0f / 255.0f)
	{
	}

	void FMOICutPlaneImpl::SetupDynamicGeometry()
	{
		bool bNewPlane = !GotGeometry;
		GotGeometry = true;
		UpdateCachedGeometryData();

		AEditModelGameMode_CPP *gameMode = World.IsValid() ? World->GetAuthGameMode<AEditModelGameMode_CPP>() : nullptr;
		MaterialData.EngineMaterial = gameMode ? gameMode->CutPlaneMaterial : nullptr;

		// TODO: can the cut plane do less?
		DynamicMeshActor->SetupPlaneGeometry(MOI->GetControlPoints(), MaterialData, true, true);

		if (bNewPlane)
		{
			MOI->UpdateVisibilityAndCollision();
		}

		// setup actors
		if (gameMode != nullptr)
		{
			if (!CaptureActor.IsValid())
			{
				CaptureActor = World->SpawnActor<ACutPlaneCaptureActor>(gameMode->CutPlaneCaptureActorClass.Get());
				CaptureActor->AttachToActor(DynamicMeshActor.Get(), FAttachmentTransformRules::KeepWorldTransform);
				CaptureActor->ObjID = MOI->ID;
				CaptureActor->Parent = this;
			}

			if (!MasksActor.IsValid())
			{
				MasksActor = World->SpawnActor<ADynamicMeshActor>(gameMode->DynamicMeshActorClass.Get());
				MasksActor->AttachToActor(DynamicMeshActor.Get(), FAttachmentTransformRules::KeepWorldTransform);
			}
		}
	}

	void FMOICutPlaneImpl::UpdateDynamicGeometry()
	{
		if (!GotGeometry)
		{
			return;
		}

		UpdateCachedGeometryData();

		DynamicMeshActor->SetupPlaneGeometry(MOI->GetControlPoints(), MaterialData, false, true);
	}

	void FMOICutPlaneImpl::OnSelected(bool bNewSelected)
	{
		FDynamicModumateObjectInstanceImpl::OnSelected(bNewSelected);

		if (bNewSelected)
		{
			UpdateDraftingPreview();
		}
		else
		{
			PreviewHUDLines = nullptr;
		}
	}

	void FMOICutPlaneImpl::AddDraftingLines(UHUDDrawWidget *HUDDrawWidget)
	{

		TArray<Modumate::FStructurePoint> CutPlaneStructurePoints;
		TArray<Modumate::FStructureLine> CutPlaneStructureLines;
		GetStructuralPointsAndLines(CutPlaneStructurePoints, CutPlaneStructureLines, false, false);

		for (auto &structureLine : CutPlaneStructureLines)
		{
			FModumateLines objectSelectionLine = FModumateLines();
			objectSelectionLine.Point1 = structureLine.P1;
			objectSelectionLine.Point2 = structureLine.P2;
			objectSelectionLine.OwnerActor = MOI->GetActor();
			objectSelectionLine.Thickness = 2.0f;
			objectSelectionLine.Color = MOI->IsSelected() ? EdgeSelectedColor : EdgeColor;

			HUDDrawWidget->LinesToDraw.Add(MoveTemp(objectSelectionLine));
		}

		if (PreviewHUDLines != nullptr)
		{
			DrawingInterface.HUDDrawWidget = HUDDrawWidget;
			PreviewHUDLines->Draw(&DrawingInterface);
		}
	}

	void FMOICutPlaneImpl::GetDraftingLines(const TSharedPtr<FDraftingComposite> &ParentPage, const FPlane &Plane, const FVector &AxisX, const FVector &AxisY, const FVector &Origin, const FBox2D &BoundingBox, TArray<TArray<FVector>> &OutPerimeters) const
	{
		// TODO: this is unused, currently the drawings are doing this for themselves,
		// based on the scopebox intersections instead of the whole cutplane
		ParentPage->Children.Add(DraftingForegroundLines);
	}

	bool FMOICutPlaneImpl::AddCaptureArea(int32 ScopeBoxID, TArray<FVector> CaptureArea)
	{
		PendingCaptureAreas.Push(TPair<int32, TArray<FVector>>(ScopeBoxID, CaptureArea));

		return true;
	}

	bool FMOICutPlaneImpl::StartRender(FModumateDocument* doc /*= nullptr*/)
{
		if (PendingCaptureAreas.Num() == 0)
		{
			return false;
		}

		auto nextRenderAreaKvp = PendingCaptureAreas.Pop();

		// for now, using the entire cut plane area instead of the scope box intersection
		CaptureActor->ScopeBoxID = nextRenderAreaKvp.Key;
#if 0
		auto CaptureArea = nextRenderAreaKvp.Value;
#else
		auto CaptureArea = MOI->GetControlPoints();
#endif

		float pixelsPerWorldCentimeter = 2.0f;

		FMatrix endMatrix;

		FVector endAxisX, endAxisY;
		UModumateGeometryStatics::FindBasisVectors(endAxisX, endAxisY, GetNormal());
		if (!FVector::Parallel(GetNormal(), FVector::UpVector))
		{
			endAxisX *= -1.0f;
		}
		endMatrix = FRotationMatrix::MakeFromXY(GetNormal(), endAxisX);

		FQuat rotation;
		rotation = FQuat(endMatrix);

		FVector scopeBoxOrigin = CaptureArea[0];
		TArray<FVector2D> boxPoints;
		for (auto& point : CaptureArea)
		{
			FVector2D point2D = UModumateGeometryStatics::ProjectPoint2D(point, endAxisX, endAxisY, scopeBoxOrigin);
			boxPoints.Add(point2D);
		}
		FBox2D box = FBox2D(boxPoints);

		float orthoWidth = box.GetSize().X;
		float orthoHeight = box.GetSize().Y;
		FVector captureAreaOrigin = scopeBoxOrigin + box.GetCenter().X * endAxisX + box.GetCenter().Y * endAxisY;

		// setup texture target
		CaptureActor->CurrentTextureTarget = UKismetRenderingLibrary::CreateRenderTarget2D(DynamicMeshActor->GetWorld(), (int)(orthoWidth*pixelsPerWorldCentimeter), (int)(orthoHeight*pixelsPerWorldCentimeter), ETextureRenderTargetFormat::RTF_RGBA8);
		CaptureActor->CurrentTextureTarget->ClearColor = FLinearColor(1.f, 1.f, 1.f);

		// setup capture component
		auto* captureComponent = CaptureActor->CaptureComponent;
		FTransform cutPlaneTransform;
		cutPlaneTransform.SetTranslation(captureAreaOrigin);

		cutPlaneTransform.SetRotation(rotation);

		captureComponent->TextureTarget = CaptureActor->CurrentTextureTarget;
		captureComponent->OrthoWidth = orthoWidth;
		captureComponent->SetWorldTransform(cutPlaneTransform);

		// Setup scene.
		if (doc)
		{
			captureComponent->ClearShowOnlyComponents();
			auto bitmapObjects = doc->GetObjectsOfType(EObjectType::OTFurniture)
				+= doc->GetObjectsOfType(EObjectType::OTCabinet);
			for (FModumateObjectInstance* moi : bitmapObjects)
			{
				captureComponent->ShowOnlyActorComponents(moi->GetActor(), true);
			}
		}

		// capture scene
		captureComponent->bCaptureEveryFrame = true;
		CaptureActor->bRenderOnTick = true;

		return true;
	}

	float FMOICutPlaneImpl::GetAlpha() const
	{
		return FMOIPlaneImplBase::GetAlpha() * ((MOI && MOI->IsSelected()) ? 0.4f : 0.2f);
	}

	void FMOICutPlaneImpl::UpdateCachedGeometryData()
	{
		if (!ensureAlways(MOI->GetControlPoints().Num() > 0))
		{
			return;
		}

		CachedOrigin = MOI->GetControlPoint(0);
		TArray<FVector2D> cached2DPositions;
		UModumateGeometryStatics::AnalyzeCachedPositions(MOI->GetControlPoints(), CachedPlane, CachedAxisX, CachedAxisY, cached2DPositions, CachedCenter);
		UpdateDraftingPreview();
	}

	void FMOICutPlaneImpl::UpdateDraftingPreview()
	{
		PreviewHUDLines = nullptr;

		if (!MOI->IsSelected())
		{
			return;
		}
		PreviewHUDLines = MakeShareable(new FDraftingComposite());

		FVector axisY = CachedAxisY * -1.0f;

		GetForegroundLines(PreviewHUDLines, CachedAxisX, axisY);

		DrawingInterface.CurrentAxisX = CachedAxisX;
		DrawingInterface.CurrentAxisY = CachedAxisY * -1.0f;
		DrawingInterface.CurrentOrigin = CachedOrigin;
	}

	void FMOICutPlaneImpl::GetForegroundLines(TSharedPtr<FDraftingComposite> ParentPage, const FVector &AxisX, const FVector &AxisY, bool bIsDrafting)
	{
		CaptureActor->ResetHiddenActorsToDefault();
		MasksActor->ClearProceduralLayers();

		AEditModelGameState_CPP *gameState = World->GetGameState<AEditModelGameState_CPP>();
		FModumateDocument &doc = gameState->Document;
		auto volumeGraph = doc.GetVolumeGraph();
		TArray<FVector2D> boxPoints;

		for (auto& point : MOI->GetControlPoints())
		{
			FVector2D point2D = UModumateGeometryStatics::ProjectPoint2D(point, AxisX, AxisY, CachedOrigin);
			boxPoints.Add(point2D);
		}

		// bounding box is defined by the dimensions of the cut plane as opposed to the contents of the graph
		FBox2D cutPlaneBox = FBox2D(boxPoints);

		FGraph2D graph;
		TMap<int32, int32> objMap;
		volumeGraph.Create2DGraph(CachedPlane, AxisX, AxisY, CachedOrigin, cutPlaneBox, graph, objMap);

		Units::FThickness lineThickness = Units::FThickness::Points(0.15f);
		FMColor lineColor = FMColor::Black;

		TArray<TArray<FVector>> allMaskVerts;

		for (auto& edgekvp : graph.GetEdges())
		{
			auto& edge = edgekvp.Value;

			auto startVertex = graph.FindVertex(edge.StartVertexID);
			auto endVertex = graph.FindVertex(edge.EndVertexID);

			Units::FCoordinates2D start = Units::FCoordinates2D::WorldCentimeters(startVertex->Position);
			Units::FCoordinates2D end = Units::FCoordinates2D::WorldCentimeters(endVertex->Position);

			int32 metaplaneID = objMap[edge.ID];
			auto metaplane = doc.GetObjectById(metaplaneID);
			if (metaplane == nullptr)
			{
				continue;
			}

			auto children = metaplane->GetChildObjects();
			if (!children.Num() == 1)
			{
				continue;
			}
			auto wall = children[0];
			if (wall == nullptr)
			{
				continue;
			}

			TArray<TPair<FVector2D, FVector2D>> OutLines;

			// TODO: this only is implemented for plane hosted objects right now, this function should be
			// a part of ModumateObjectInstance instead and should propagate down through the children
			TArray<TArray<FVector>> WallCutPerimeters;
			wall->GetDraftingLines(ParentPage, CachedPlane, AxisX, AxisY, CachedOrigin, cutPlaneBox, WallCutPerimeters);

			for (auto& perimeter : WallCutPerimeters)
			{
				// TODO: remove this limitation
				if (perimeter.Num() == 4)
				{
					allMaskVerts.Add(perimeter);
				}
			}

			CaptureActor->CaptureComponent->HiddenActors.Add(wall->GetActor());

			if (bIsDrafting)
			{
				for (int32 objID : wall->GetChildIDs())
				{
					auto moi = doc.GetObjectById(objID);
					if (!moi) continue;
					
					// not used
					TArray<TArray<FVector>> portalPerimeters;

					moi->GetDraftingLines(ParentPage, CachedPlane, AxisX, AxisY, CachedOrigin, cutPlaneBox, portalPerimeters);

					CaptureActor->CaptureComponent->HiddenActors.Add(moi->GetActor());
				}

				MasksActor->SetupMasksGeometry(allMaskVerts, CachedPlane, CachedOrigin, AxisX, AxisY);
			}
		}
	}

	void FMOICutPlaneImpl::PublishPage()
	{

		CaptureActor->ResetHiddenActorsToDefault();
		MasksActor->ClearProceduralLayers();

		if (!StartRender())
		{
			// TODO: potentially this can happen each time one render finishes, instead of when all of the renders finish
			CaptureDelegate.Broadcast();
		}

	}
}
