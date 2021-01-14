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

AMOICutPlane::AMOICutPlane()
	: AMOIPlaneBase(),
	EdgeSelectedColor(28.0f / 255.0f, 159.0f / 255.0f, 255.0f / 255.0f),
	EdgeColor(73.0f / 255.0f, 179.0f / 255.0f, 255.0f / 255.0f)
{
}

AActor* AMOICutPlane::CreateActor(const FVector& loc, const FQuat& rot)
{
	AActor *returnActor = AMOIPlaneBase::CreateActor(loc, rot);
	return returnActor;
}

void AMOICutPlane::PostCreateObject(bool bNewObject)
{
	Super::PostCreateObject(bNewObject);

	UWorld* world = GetWorld();
	auto controller = world->GetFirstPlayerController<AEditModelPlayerController_CPP>();
	if (controller && controller->EditModelUserWidget)
	{
		controller->EditModelUserWidget->UpdateCutPlanesList();
	}

	AEditModelGameMode_CPP *gameMode = world->GetAuthGameMode<AEditModelGameMode_CPP>();
	MaterialData.EngineMaterial = gameMode ? gameMode->CutPlaneMaterial : nullptr;

	// TODO: make sure that these are destroyed
	// setup actors
	if (gameMode != nullptr)
	{
		if (!CaptureActor.IsValid())
		{
			CaptureActor = world->SpawnActor<ACutPlaneCaptureActor>(gameMode->CutPlaneCaptureActorClass.Get());
			CaptureActor->AttachToActor(DynamicMeshActor.Get(), FAttachmentTransformRules::KeepWorldTransform);
			CaptureActor->ObjID = ID;
			CaptureActor->Parent = this;
		}

		if (!MasksActor.IsValid())
		{
			MasksActor = world->SpawnActor<ADynamicMeshActor>(gameMode->DynamicMeshActorClass.Get());
			MasksActor->AttachToActor(DynamicMeshActor.Get(), FAttachmentTransformRules::KeepWorldTransform);
		}
	}
}

void AMOICutPlane::PreDestroy()
{
	auto controller = GetWorld()->GetFirstPlayerController<AEditModelPlayerController_CPP>();
	if (controller && controller->EditModelUserWidget)
	{
		controller->EditModelUserWidget->RemoveCutPlaneFromList(ID);
	}
	AMOIPlaneBase::PreDestroy();
}

void AMOICutPlane::GetUpdatedVisuals(bool &bOutVisible, bool &bOutCollisionEnabled)
{
	AMOIPlaneBase::GetUpdatedVisuals(bOutVisible, bOutCollisionEnabled);
	auto controller = GetWorld()->GetFirstPlayerController<AEditModelPlayerController_CPP>();
	if (controller && controller->EditModelUserWidget)
	{
		controller->EditModelUserWidget->UpdateCutPlaneInList(ID);
	}
}

int32 AMOICutPlane::GetCutPlaneVerticalDegree(const FQuat& Rotation)
{
	// Converting from ue4 axis to Modumate axis
	FRotator rot = FRotator(0.f, Rotation.Euler().Z * -1.f, 0.f) + FRotator(0.f, -90.f, 0.f);
	int32 degree = FMath::RoundToInt(rot.Yaw);
	if (degree < 0)
	{
		degree += 360;
	}
	return degree % 180;
}

void AMOICutPlane::SetupDynamicGeometry()
{
	// TODO: Migrate to CleanObject
	UpdateCachedGeometryData();

	DynamicMeshActor->SetupPlaneGeometry(CachedPoints, MaterialData, true, true);

	UpdateVisuals();
}

void AMOICutPlane::UpdateDynamicGeometry()
{
	UpdateCachedGeometryData();

	DynamicMeshActor->SetupPlaneGeometry(CachedPoints, MaterialData, false, true);
}

bool AMOICutPlane::OnSelected(bool bIsSelected)
{
	if (!AMOIPlaneBase::OnSelected(bIsSelected))
	{
		return false;
	}

	if (bIsSelected)
	{
		UpdateDraftingPreview();
	}
	else
	{
		PreviewHUDLines = nullptr;
	}

	return true;
}

void AMOICutPlane::SetupAdjustmentHandles(AEditModelPlayerController_CPP* controller)
{
	if (HasAdjustmentHandles())
	{
		return;
	}

	int32 numPoints = CachedPoints.Num();
	for (int32 i = 0; i < numPoints; ++i)
	{
		auto edgeHandle = MakeHandle<AAdjustCutPlaneExtentsHandle>();
		edgeHandle->SetTargetIndex(i);
	}
}
	
bool AMOICutPlane::GetTransformedLocationState(const FTransform Transform, FMOIStateData& OutState) const
{
	OutState = GetStateData();

	FMOICutPlaneData modifiedFFEData = InstanceData;
	modifiedFFEData.Location = Transform.GetLocation();
	modifiedFFEData.Rotation = Transform.GetRotation();

	return OutState.CustomData.SaveStructData(modifiedFFEData);
}

void AMOICutPlane::AddDraftingLines(UHUDDrawWidget *HUDDrawWidget)
{

	TArray<FStructurePoint> CutPlaneStructurePoints;
	TArray<FStructureLine> CutPlaneStructureLines;
	GetStructuralPointsAndLines(CutPlaneStructurePoints, CutPlaneStructureLines, false, false);

	for (auto &structureLine : CutPlaneStructureLines)
	{
		FModumateLines objectSelectionLine = FModumateLines();
		objectSelectionLine.Point1 = structureLine.P1;
		objectSelectionLine.Point2 = structureLine.P2;
		objectSelectionLine.OwnerActor = GetActor();
		objectSelectionLine.Thickness = 2.0f;
		objectSelectionLine.Color = IsSelected() ? EdgeSelectedColor : EdgeColor;

		HUDDrawWidget->LinesToDraw.Add(MoveTemp(objectSelectionLine));
	}

	if (PreviewHUDLines != nullptr)
	{
		DrawingInterface.HUDDrawWidget = HUDDrawWidget;
		PreviewHUDLines->Draw(&DrawingInterface);
	}
}

void AMOICutPlane::GetDraftingLines(const TSharedPtr<Modumate::FDraftingComposite> &ParentPage, const FPlane &Plane,
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
		const FVector traceScale(-traceResponse.Value.Scale, -traceResponse.Value.Scale, 1.0f);
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
				static constexpr int32 numSmoothingIterations = 2;
				
				FVector u(entry.cubic[0].x, entry.cubic[0].y, 0.0f);
				FVector v(entry.cubic[1].x, entry.cubic[1].y, 0.0f);
				FVector w(entry.cubic[2].x, entry.cubic[2].y, 0.0f);
				FVector controlPoints[4] = { currentPoint, u, v, w };
				TArray<FVector> smoothedCurve;
				UModumateGeometryStatics::DeCasteljau(controlPoints, numSmoothingIterations, smoothedCurve);
				for (int32 i = 0; i < smoothedCurve.Num() - 1; ++i)
				{
					outlineLines.Emplace(smoothedCurve[i], smoothedCurve[i + 1]);
				}
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
			line.Vertex[0] *= traceScale;
			line.Vertex[1] *= traceScale;
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
						FModumateUnitCoord2D::WorldCentimeters(boxClipped0),
						FModumateUnitCoord2D::WorldCentimeters(boxClipped1),
						ModumateUnitParams::FThickness::Points(0.15f), Modumate::FMColor::Gray144));
					singleOutlineElement->Children.Add(draftingLine);
					draftingLine->SetLayerTypeRecursive(traceResponse.Value.LayerType);
				}
			}
		}

	}
}

bool AMOICutPlane::AddCaptureArea(int32 ScopeBoxID, TArray<FVector> CaptureArea)
{
	PendingCaptureAreas.Push(TPair<int32, TArray<FVector>>(ScopeBoxID, CaptureArea));

	return true;
}

bool AMOICutPlane::StartRender(UModumateDocument* doc /*= nullptr*/)
{
	FPendingObjectRender objectRender;
	AModumateObjectInstance* object = nullptr;
	do
	{
		if (!PendingObjectRenders.Dequeue(objectRender))
		{
			return false;
		}
		object = GetDocument()->GetObjectById(objectRender.MoiId);
	} while (object == nullptr);

	static constexpr float pixelsPerWorldCentimeter = 1.0f / PixelsToWorldCentimeters;

	FMatrix endMatrix;

	const FVector viewDirection = GetNormal();
	FVector endAxisX, endAxisY;
	UModumateGeometryStatics::FindBasisVectors(endAxisX, endAxisY, viewDirection);
	if (!FVector::Parallel(viewDirection, FVector::UpVector))
	{
		endAxisX *= -1.0f;
		endAxisY *= -1.0f;
	}
	endMatrix = FRotationMatrix::MakeFromXY(viewDirection, endAxisX);

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
	float maxDimension = bounds.GetSize().Size();

	static constexpr int32 renderTargetSize = 1500;

	FVector location = actor->GetActorLocation();
	FVector2D projectedLocation = UModumateGeometryStatics::ProjectPoint2D(location, endAxisX, endAxisY, CachedPoints[0]);
	FVector2D renderOrigin = projectedLocation + FVector2D(maxDimension, maxDimension);
	FVector2D renderSize = 2.0f * FVector2D(maxDimension, maxDimension);
	float depth = CachedPlane.PlaneDot(location);
	objectRender.PagePosition = renderOrigin;
	objectRender.Size = renderSize;
	objectRender.Scale = 2.0f * maxDimension / renderTargetSize;
	objectRender.Depth = depth;
	
	// setup texture target
	CaptureActor->CurrentTextureTarget = UKismetRenderingLibrary::CreateRenderTarget2D(DynamicMeshActor->GetWorld(), renderTargetSize,
		renderTargetSize, ETextureRenderTargetFormat::RTF_RGBA8);
	CaptureActor->CurrentTextureTarget->ClearColor = FLinearColor(1.0f, 1.0f, 1.0f);
	CaptureActor->ScopeBoxID = objectRender.LocalId;

	// setup capture component
	auto* captureComponent = CaptureActor->CaptureComponent;
	FTransform cutPlaneTransform;
	cutPlaneTransform.SetTranslation(location - 2.0f * maxDimension * viewDirection);

	cutPlaneTransform.SetRotation(rotation);

	captureComponent->TextureTarget = CaptureActor->CurrentTextureTarget;
	captureComponent->OrthoWidth = 2.0f * maxDimension;
	captureComponent->SetWorldTransform(cutPlaneTransform);

	captureComponent->ClearShowOnlyComponents();
	captureComponent->ShowOnlyActorComponents(object->GetActor(), true);

	// capture scene
	captureComponent->bAlwaysPersistRenderingState = true;
	captureComponent->CaptureScene();
	CaptureActor->bCaptureStarted = true;
	CaptureActor->TickCount = 2;

	CurrentObjectRender = objectRender;

	return true;
}

float AMOICutPlane::GetAlpha() const
{
	return AMOIPlaneBase::GetAlpha() * (IsSelected() ? 0.4f : 0.2f);
}

void AMOICutPlane::UpdateCachedGeometryData()
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

void AMOICutPlane::UpdateDraftingPreview()
{
	PreviewHUDLines = nullptr;

	if (!IsSelected())
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

void AMOICutPlane::GetForegroundLines(TSharedPtr<Modumate::FDraftingComposite> ParentPage, const FVector &AxisX, const FVector &AxisY, bool bIsDrafting)
{
	CaptureActor->ResetHiddenActorsToDefault();
	MasksActor->ClearProceduralLayers();

	AEditModelGameState_CPP *gameState = GetWorld()->GetGameState<AEditModelGameState_CPP>();
	UModumateDocument* doc = gameState->Document;
	auto volumeGraph = doc->GetVolumeGraph();
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

	ModumateUnitParams::FThickness lineThickness = ModumateUnitParams::FThickness::Points(0.15f);
	Modumate::FMColor lineColor = Modumate::FMColor::Black;

	TArray<TArray<FVector>> allMaskVerts;

	for (auto& edgekvp : graph->GetEdges())
	{
		auto& edge = edgekvp.Value;

		auto startVertex = graph->FindVertex(edge.StartVertexID);
		auto endVertex = graph->FindVertex(edge.EndVertexID);

		FModumateUnitCoord2D start = FModumateUnitCoord2D::WorldCentimeters(startVertex->Position);
		FModumateUnitCoord2D end = FModumateUnitCoord2D::WorldCentimeters(endVertex->Position);

		int32 metaplaneID = objMap[edge.ID];
		auto metaplane = doc->GetObjectById(metaplaneID);
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
				auto moi = doc->GetObjectById(objID);
				if (!moi) continue;
					
				// not used
				TArray<TArray<FVector>> portalPerimeters;

				GetDraftingLines(ParentPage, CachedPlane, AxisX, AxisY, CachedOrigin, cutPlaneBox, portalPerimeters);

				CaptureActor->CaptureComponent->HiddenActors.Add(moi->GetActor());
			}

			MasksActor->SetupMasksGeometry(allMaskVerts, CachedPlane, CachedOrigin, AxisX, AxisY);
		}
	}
}

void AMOICutPlane::CaptureComplete()
{
	TPair<int32, int32> SceneCaptureObjID = TPair<int32, int32>(CaptureActor->ObjID, CaptureActor->ScopeBoxID);
	FString imageFilePath;
	UDraftingManager::GetImageFullPath(SceneCaptureObjID, imageFilePath);
	ConvertToOutlines(imageFilePath);

	CaptureActor->ResetHiddenActorsToDefault();
	MasksActor->ClearProceduralLayers();

	StartRender();
}

void AMOICutPlane::SetupPendingRenders()
{
	int32 renderId = 0;
	PendingObjectRenders.Empty();
	InprocessRenders.Empty();

	TArray<AModumateObjectInstance*> bitmapObjects = GetDocument()->GetObjectsOfType(EObjectType::OTFurniture);
	for (const auto object: bitmapObjects)
	{
		FPendingObjectRender renderInfo;
		renderInfo.LocalId = renderId++;
		renderInfo.MoiId = object->ID;
		renderInfo.LayerType = Modumate::FModumateLayerType::kFfeOutline;
		PendingObjectRenders.Enqueue(renderInfo);
	}
}

void AMOICutPlane::TraceRequestComplete(int32 TraceID, FString TraceString)
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

void AMOICutPlane::ConvertToOutlines(const FString& renderTargetFilename)
{
	Modumate::FModumateAutotraceConnect autotraceServer;

	if (autotraceServer.ConvertImageFromFile(renderTargetFilename, CurrentObjectRender.LocalId, this, ID, GetWorld()))
	{
		InprocessRenders.Add(CurrentObjectRender.LocalId, CurrentObjectRender);
		++PendingTraceRequests;
	}
}
