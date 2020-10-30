// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "Objects/CutPlane.h"

#include "Drafting/DraftingManager.h"
#include "Drafting/ModumateAutotraceConnect.h"
#include "Drafting/ModumateDraftingElements.h"
#include "Drafting/ModumateDraftingView.h"
#include "Drafting/ModumateTrace.h"
#include "Graph/Graph3DFace.h"
#include "JsonUtilities/Public/JsonObjectConverter.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "ModumateCore/ModumateUnits.h"
#include "ModumateCore/ModumateUserSettings.h"
#include "ToolsAndAdjustments/Handles/AdjustCutPlaneExtentsHandle.h"
#include "UI/EditModelUserWidget.h"
#include "UnrealClasses/CutPlaneCaptureActor.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "UnrealClasses/CompoundMeshActor.h"

static constexpr float PixelsToWorldCentimeters = 0.5f;

FMOICutPlaneImpl::FMOICutPlaneImpl(FModumateObjectInstance *moi)
	: FMOIPlaneImplBase(moi),
	EdgeSelectedColor(28.0f / 255.0f, 159.0f / 255.0f, 255.0f / 255.0f),
	EdgeColor(73.0f / 255.0f, 179.0f / 255.0f, 255.0f / 255.0f)
{
}

void FMOICutPlaneImpl::GetTypedInstanceData(UScriptStruct*& OutStructDef, void*& OutStructPtr)
{
	OutStructDef = InstanceData.StaticStruct();
	OutStructPtr = &InstanceData;
}

AActor* FMOICutPlaneImpl::CreateActor(UWorld* world, const FVector& loc, const FQuat& rot)
{
	AActor *returnActor = FMOIPlaneImplBase::CreateActor(world, loc, rot);
	return returnActor;
}

void FMOICutPlaneImpl::PostCreateObject(bool bNewObject)
{
	FModumateObjectInstanceImplBase::PostCreateObject(bNewObject);

	auto controller = World.IsValid() ? World->GetFirstPlayerController<AEditModelPlayerController_CPP>() : nullptr;
	if (controller && controller->EditModelUserWidget)
	{
		controller->EditModelUserWidget->UpdateCutPlanesList();
	}

	AEditModelGameMode_CPP *gameMode = World.IsValid() ? World->GetAuthGameMode<AEditModelGameMode_CPP>() : nullptr;
	MaterialData.EngineMaterial = gameMode ? gameMode->CutPlaneMaterial : nullptr;

	// TODO: make sure that these are destroyed
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

void FMOICutPlaneImpl::Destroy()
{
	auto controller = World.IsValid() ? World->GetFirstPlayerController<AEditModelPlayerController_CPP>() : nullptr;
	if (controller && controller->EditModelUserWidget)
	{
		controller->EditModelUserWidget->RemoveCutPlaneFromList(MOI->ID);
	}
	FMOIPlaneImplBase::Destroy();
}

void FMOICutPlaneImpl::UpdateVisibilityAndCollision(bool &bOutVisible, bool &bOutCollisionEnabled)
{
	FMOIPlaneImplBase::UpdateVisibilityAndCollision(bOutVisible, bOutCollisionEnabled);
	auto controller = World.IsValid() ? World->GetFirstPlayerController<AEditModelPlayerController_CPP>() : nullptr;
	if (controller && controller->EditModelUserWidget)
	{
		controller->EditModelUserWidget->UpdateCutPlaneInList(MOI->ID);
	}
}

void FMOICutPlaneImpl::SetupDynamicGeometry()
{
	// TODO: Migrate to CleanObject
	UpdateCachedGeometryData();

	DynamicMeshActor->SetupPlaneGeometry(CachedPoints, MaterialData, true, true);

	MOI->UpdateVisibilityAndCollision();
}

void FMOICutPlaneImpl::UpdateDynamicGeometry()
{
	UpdateCachedGeometryData();

	DynamicMeshActor->SetupPlaneGeometry(CachedPoints, MaterialData, false, true);
}

void FMOICutPlaneImpl::OnSelected(bool bIsSelected)
{
	FMOIPlaneImplBase::OnSelected(bIsSelected);

	if (bIsSelected)
	{
		UpdateDraftingPreview();
	}
	else
	{
		PreviewHUDLines = nullptr;
	}
}

void FMOICutPlaneImpl::SetupAdjustmentHandles(AEditModelPlayerController_CPP* controller)
{
	if (MOI->HasAdjustmentHandles())
	{
		return;
	}

	int32 numPoints = CachedPoints.Num();
	for (int32 i = 0; i < numPoints; ++i)
	{
		auto edgeHandle = MOI->MakeHandle<AAdjustCutPlaneExtentsHandle>();
		edgeHandle->SetTargetIndex(i);
	}
}
	
bool FMOICutPlaneImpl::GetTransformedLocationState(const FTransform Transform, FMOIStateData& OutState) const
{
	OutState = MOI->GetStateData();

	FMOICutPlaneData modifiedFFEData = InstanceData;
	modifiedFFEData.Location = Transform.GetLocation();
	modifiedFFEData.Rotation = Transform.GetRotation();

	return OutState.CustomData.SaveStructData(modifiedFFEData);
}

void FMOICutPlaneImpl::AddDraftingLines(UHUDDrawWidget *HUDDrawWidget)
{

	TArray<FStructurePoint> CutPlaneStructurePoints;
	TArray<FStructureLine> CutPlaneStructureLines;
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

void FMOICutPlaneImpl::GetDraftingLines(const TSharedPtr<Modumate::FDraftingComposite> &ParentPage, const FPlane &Plane,
	const FVector &AxisX, const FVector &AxisY, const FVector &Origin, const FBox2D &BoundingBox,
	TArray<TArray<FVector>> &OutPerimeters) const
{
	// In-sync with OutDepthScale in EdgeDetection.usf:
	static constexpr float depthScaleFactor = 3000.0f;

	// Add any traced splines.
	for(const auto& traceResponse: InprocessRenders)
	{
		FModumateTraceObject trace;
		TSharedPtr<Modumate::FDraftingComposite> singleOutlineElement = MakeShareable(new Modumate::FDraftingComposite);
		ParentPage->Children.Add(singleOutlineElement);

		bool result = FJsonObjectConverter::JsonObjectStringToUStruct<FModumateTraceObject>(traceResponse.Value.JsonTrace, &trace, 0, 0);
		if (!ensureMsgf(result, TEXT("Couldn't parse trace JSON")) )
		{
			return;
		}

		FVector currentPoint(0.0f);
		FVector startPoint(0.0f);
		bool bClosedCurve = false;
		const float lineDepth = traceResponse.Value.Depth;
		const FVector tracePosition(traceResponse.Value.PagePosition, lineDepth);
		TArray<FEdge> outlineLines;
		int lineStartIndex = outlineLines.Num();
		for (const FModumateTraceSplineEntry& entry: trace.lines)
		{
			if (!FMath::IsNaN(entry.open.x))
			{
				bClosedCurve = false;
				FVector newPoint(entry.open.x, entry.open.y, 0.0f);
				startPoint = newPoint;
				currentPoint = newPoint;
			}
			else if (!FMath::IsNaN(entry.closed.x))
			{
				bClosedCurve = true;
				FVector newPoint(entry.closed.x, entry.closed.y, 0.0f);
				startPoint = newPoint;
				currentPoint = newPoint;
			}
			else if (!FMath::IsNaN(entry.linear.x))
			{
				FVector newPoint(entry.linear.x, entry.linear.y, 0.0f);
				outlineLines.Emplace(currentPoint, newPoint);
				currentPoint = newPoint;
			}
			else if (entry.cubic.Num() > 0)
			{
				FVector u(entry.cubic[0].x, entry.cubic[0].y, 0.0f);
				FVector v(entry.cubic[1].x, entry.cubic[1].y, 0.0f);
				FVector w(entry.cubic[2].x, entry.cubic[2].y, 0.0f);

				// One level of de Casteljau:
				FVector au((currentPoint + u) * 0.5);
				FVector uv((u + v) * 0.5);
				FVector vw((v + w) * 0.5);
				outlineLines.Emplace(currentPoint, au);
				outlineLines.Emplace(au, uv);
				outlineLines.Emplace(uv, vw);
				outlineLines.Emplace(vw, w);
				currentPoint = w;
			}
			else if (!FMath::IsNaN(entry.depth))
			{
				if (bClosedCurve)
				{
					// close current curve.
					outlineLines.Emplace(currentPoint, startPoint);
				}
			}
			else
			{	// Empty entry
				ensure(false);
			}
		}

		for (auto& line: outlineLines)
		{
			line.Vertex[0] *= FVector(-PixelsToWorldCentimeters, -PixelsToWorldCentimeters, 1.0f);
			line.Vertex[1] *= FVector(-PixelsToWorldCentimeters, -PixelsToWorldCentimeters, 1.0f);
			line.Vertex[0] += tracePosition;
			line.Vertex[1] += tracePosition;
		}

		for (auto& outlineLine: outlineLines)
		{
			auto clippedLines = ParentPage->lineClipping->ClipViewLineToView(outlineLine);
			for (const auto& clippedLine : clippedLines)
			{
				FVector2D vert0(clippedLine.Vertex[0]);
				FVector2D vert1(clippedLine.Vertex[1]);
				FVector2D boxClipped0;
				FVector2D boxClipped1;

				if (UModumateFunctionLibrary::ClipLine2DToRectangle(vert0, vert1, BoundingBox, boxClipped0, boxClipped1))
				{
					TSharedPtr<Modumate::FDraftingLine> draftingLine = MakeShareable(new Modumate::FDraftingLine(
						Modumate::Units::FCoordinates2D::WorldCentimeters(boxClipped0),
						Modumate::Units::FCoordinates2D::WorldCentimeters(boxClipped1),
						Modumate::Units::FThickness::Points(0.15f), Modumate::FMColor::Gray144));
					singleOutlineElement->Children.Add(draftingLine);
					draftingLine->SetLayerTypeRecursive(traceResponse.Value.LayerType);
				}
			}
		}

	}
}

bool FMOICutPlaneImpl::AddCaptureArea(int32 ScopeBoxID, TArray<FVector> CaptureArea)
{
	PendingCaptureAreas.Push(TPair<int32, TArray<FVector>>(ScopeBoxID, CaptureArea));

	return true;
}

bool FMOICutPlaneImpl::StartRender(FModumateDocument* doc /*= nullptr*/)
{
	FPendingObjectRender objectRender;
	FModumateObjectInstance* object = nullptr;
	do
	{
		if (!PendingObjectRenders.Dequeue(objectRender))
		{
			return false;
		}
		object = MOI->GetDocument()->GetObjectById(objectRender.MoiId);
	} while (object == nullptr);

	static constexpr float pixelsPerWorldCentimeter = 1.0f / PixelsToWorldCentimeters;

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
	ACompoundMeshActor* actor = CastChecked<ACompoundMeshActor>(object->GetActor());

	FBox bounds(ForceInitToZero);
	for (const auto* mesh : actor->StaticMeshComps)
	{
		if (mesh != nullptr)
		{
			FVector minPoint(ForceInitToZero);
			FVector maxPoint(ForceInitToZero);
			mesh->GetLocalBounds(minPoint, maxPoint);
			bounds += minPoint;
			bounds += maxPoint;
		}
	}
	float maxDimension = bounds.GetSize().GetMax();

	FVector location = object->GetObjectLocation();
	FVector2D projectedLocation = UModumateGeometryStatics::ProjectPoint2D(location, endAxisX, endAxisY, CachedPoints[0]);
	FVector2D renderOrigin = projectedLocation + FVector2D(maxDimension, -maxDimension);
	FVector2D renderSize = 2.0f * FVector2D(maxDimension, maxDimension);
	float depth = CachedPlane.PlaneDot(location);
	renderOrigin.Y = -renderOrigin.Y;  // !?
	objectRender.PagePosition = renderOrigin;
	objectRender.Size = renderSize;
	objectRender.Depth = depth;
	
	// setup texture target
	CaptureActor->CurrentTextureTarget = UKismetRenderingLibrary::CreateRenderTarget2D(DynamicMeshActor->GetWorld(), (int)(renderSize.X * pixelsPerWorldCentimeter),
		(int)(renderSize.Y * pixelsPerWorldCentimeter), ETextureRenderTargetFormat::RTF_RGBA8);
	CaptureActor->CurrentTextureTarget->ClearColor = FLinearColor(1.0f, 1.0f, 1.0f);
	CaptureActor->ScopeBoxID = objectRender.LocalId;

	// setup capture component
	auto* captureComponent = CaptureActor->CaptureComponent;
	FTransform cutPlaneTransform;
	cutPlaneTransform.SetTranslation(location);

	cutPlaneTransform.SetRotation(rotation);

	captureComponent->TextureTarget = CaptureActor->CurrentTextureTarget;
	captureComponent->OrthoWidth = renderSize.X;
	captureComponent->SetWorldTransform(cutPlaneTransform);

	captureComponent->ClearShowOnlyComponents();
	captureComponent->ShowOnlyActorComponents(object->GetActor(), true);

	// capture scene
	captureComponent->bCaptureEveryFrame = true;
	CaptureActor->bRenderOnTick = true;

	CurrentObjectRender = objectRender;

	return true;
}

float FMOICutPlaneImpl::GetAlpha() const
{
	return FMOIPlaneImplBase::GetAlpha() * ((MOI && MOI->IsSelected()) ? 0.4f : 0.2f);
}

void FMOICutPlaneImpl::UpdateCachedGeometryData()
{
	CachedCenter = InstanceData.Location;
	CachedAxisX = InstanceData.Rotation.GetAxisX();
	CachedAxisY = InstanceData.Rotation.GetAxisY();
	CachedPlane = FPlane(CachedCenter, InstanceData.Rotation.GetAxisZ());

	FVector halfExtentsX = 0.5f * InstanceData.Extents.X * CachedAxisX;
	FVector halfExtentsY = 0.5f * InstanceData.Extents.Y * CachedAxisY;
	CachedPoints = {
		CachedCenter - halfExtentsX - halfExtentsY,
		CachedCenter + halfExtentsX - halfExtentsY,
		CachedCenter + halfExtentsX + halfExtentsY,
		CachedCenter - halfExtentsX + halfExtentsY
	};

	CachedOrigin = CachedPoints[0];

	UpdateDraftingPreview();
}

void FMOICutPlaneImpl::UpdateDraftingPreview()
{
	PreviewHUDLines = nullptr;

	if (!MOI->IsSelected())
	{
		return;
	}
	PreviewHUDLines = MakeShared<Modumate::FDraftingComposite>();

	FVector axisY = CachedAxisY * -1.0f;

	GetForegroundLines(PreviewHUDLines, CachedAxisX, axisY);

	DrawingInterface.CurrentAxisX = CachedAxisX;
	DrawingInterface.CurrentAxisY = CachedAxisY * -1.0f;
	DrawingInterface.CurrentOrigin = CachedOrigin;
}

void FMOICutPlaneImpl::GetForegroundLines(TSharedPtr<Modumate::FDraftingComposite> ParentPage, const FVector &AxisX, const FVector &AxisY, bool bIsDrafting)
{
	CaptureActor->ResetHiddenActorsToDefault();
	MasksActor->ClearProceduralLayers();

	AEditModelGameState_CPP *gameState = World->GetGameState<AEditModelGameState_CPP>();
	FModumateDocument &doc = gameState->Document;
	auto volumeGraph = doc.GetVolumeGraph();
	TArray<FVector2D> boxPoints;

	for (auto& point : CachedPoints)
	{
		FVector2D point2D = UModumateGeometryStatics::ProjectPoint2D(point, AxisX, AxisY, CachedOrigin);
		boxPoints.Add(point2D);
	}

	// bounding box is defined by the dimensions of the cut plane as opposed to the contents of the graph
	FBox2D cutPlaneBox = FBox2D(boxPoints);

	auto graph = MakeShared<Modumate::FGraph2D>();
	TMap<int32, int32> objMap;
	volumeGraph.Create2DGraph(CachedPlane, AxisX, AxisY, CachedOrigin, cutPlaneBox, graph, objMap);

	Modumate::Units::FThickness lineThickness = Modumate::Units::FThickness::Points(0.15f);
	Modumate::FMColor lineColor = Modumate::FMColor::Black;

	TArray<TArray<FVector>> allMaskVerts;

	for (auto& edgekvp : graph->GetEdges())
	{
		auto& edge = edgekvp.Value;

		auto startVertex = graph->FindVertex(edge.StartVertexID);
		auto endVertex = graph->FindVertex(edge.EndVertexID);

		Modumate::Units::FCoordinates2D start = Modumate::Units::FCoordinates2D::WorldCentimeters(startVertex->Position);
		Modumate::Units::FCoordinates2D end = Modumate::Units::FCoordinates2D::WorldCentimeters(endVertex->Position);

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

void FMOICutPlaneImpl::CaptureComplete()
{
	TPair<int32, int32> SceneCaptureObjID = TPair<int32, int32>(CaptureActor->ObjID, CaptureActor->ScopeBoxID);
	FString imageFilePath;
	UDraftingManager::GetImageFullPath(SceneCaptureObjID, imageFilePath);
	ConvertToOutlines(imageFilePath);

	CaptureActor->ResetHiddenActorsToDefault();
	MasksActor->ClearProceduralLayers();

	StartRender();
}

void FMOICutPlaneImpl::SetupPendingRenders()
{
	int32 renderId = 0;
	PendingObjectRenders.Empty();
	InprocessRenders.Empty();

	TArray<FModumateObjectInstance*> bitmapObjects = MOI->GetDocument()->GetObjectsOfType(EObjectType::OTFurniture);
	for (const auto object: bitmapObjects)
	{
		FPendingObjectRender renderInfo;
		renderInfo.LocalId = renderId++;
		renderInfo.MoiId = object->ID;
		renderInfo.LayerType = Modumate::FModumateLayerType::kFfeOutline;
		PendingObjectRenders.Enqueue(renderInfo);
	}
}

void FMOICutPlaneImpl::TraceRequestComplete(int32 TraceID, FString TraceString)
{
	FPendingObjectRender* render = InprocessRenders.Find(TraceID);
	if (render)
	{
		render->JsonTrace = MoveTemp(TraceString);
		if (--PendingTraceRequests == 0 && PendingObjectRenders.IsEmpty())
		{
			CaptureDelegate.Broadcast();
		}
	}
	else
	{
		UE_LOG(LogAutoDrafting, Error, TEXT("Unknown Autotrace ID %d"), TraceID);
	}
}

void FMOICutPlaneImpl::ConvertToOutlines(const FString& renderTargetFilename)
{
	Modumate::FModumateAutotraceConnect autotraceServer;

	if (autotraceServer.ConvertImageFromFile(renderTargetFilename, CurrentObjectRender.LocalId, this, MOI->ID, MOI->GetWorld()))
	{
		InprocessRenders.Add(CurrentObjectRender.LocalId, CurrentObjectRender);
		++PendingTraceRequests;
	}
}
