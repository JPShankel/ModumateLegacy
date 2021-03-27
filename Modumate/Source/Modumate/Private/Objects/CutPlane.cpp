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
#include "UnrealClasses/EditModelGameMode.h"
#include "UnrealClasses/EditModelGameState.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "UnrealClasses/CompoundMeshActor.h"

static constexpr float PixelsToWorldCentimeters = 0.5f;

AMOICutPlane::AMOICutPlane()
	: AMOIPlaneBase(),
	EdgeSelectedColor(28.0f / 255.0f, 159.0f / 255.0f, 255.0f / 255.0f),
	EdgeColor(73.0f / 255.0f, 179.0f / 255.0f, 255.0f / 255.0f),
	bHUDDwgDrafting(true)
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
	auto controller = world->GetFirstPlayerController<AEditModelPlayerController>();
	if (controller && controller->EditModelUserWidget)
	{
		controller->EditModelUserWidget->UpdateCutPlanesList();
	}

	AEditModelGameMode *gameMode = world->GetAuthGameMode<AEditModelGameMode>();
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
	auto controller = GetWorld()->GetFirstPlayerController<AEditModelPlayerController>();
	if (controller && controller->EditModelUserWidget)
	{
		controller->EditModelUserWidget->RemoveCutPlaneFromList(ID);
		if (controller->CurrentCullingCutPlaneID == ID)
		{
			controller->SetCurrentCullingCutPlane(MOD_ID_NONE, false);
		}
	}
	AMOIPlaneBase::PreDestroy();
}

bool AMOICutPlane::CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas)
{
	switch (DirtyFlag)
	{
	case EObjectDirtyFlags::Structure:
		UpdateCachedGeometryData();
		// TODO: re-enable this when it wouldn't be redundant with UModumateDocument::PostApplyDeltas.
		//MarkDirty(EObjectDirtyFlags::Visuals);
		return true;
	case EObjectDirtyFlags::Visuals:
		return TryUpdateVisuals() && UpdateDraftingPreview();
	default:
		return true;
	}
}

bool AMOICutPlane::GetUpdatedVisuals(bool &bOutVisible, bool &bOutCollisionEnabled)
{
	if (!Super::GetUpdatedVisuals(bOutVisible, bOutCollisionEnabled))
	{
		return false;
	}

	auto controller = GetWorld()->GetFirstPlayerController<AEditModelPlayerController>();
	if (controller && controller->EditModelUserWidget)
	{
		controller->EditModelUserWidget->UpdateCutPlaneInList(ID);
	}

	return true;
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

bool AMOICutPlane::OnSelected(bool bIsSelected)
{
	SelectedColor = bHUDDwgDrafting ? FColor::Black : FColor(0x1C, 0x9F, 0xFF);

	if (!AMOIPlaneBase::OnSelected(bIsSelected))
	{
		return false;
	}

	return true;
}

void AMOICutPlane::SetupAdjustmentHandles(AEditModelPlayerController* controller)
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
		DrawingInterface.bUseDwgMode = bHUDDwgDrafting;
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
}

bool AMOICutPlane::UpdateDraftingPreview()
{
	PreviewHUDLines = nullptr;

	if (!IsVisible())
	{
		return true;
	}
	PreviewHUDLines = MakeShared<Modumate::FDraftingComposite>();

	FVector axisY = CachedAxisY * -1.0f;

	if (!GetForegroundLines(PreviewHUDLines, CachedAxisX, axisY))
	{
		return false;
	}

	DrawingInterface.CurrentAxisX = CachedAxisX;
	DrawingInterface.CurrentAxisY = CachedAxisY * -1.0f;
	DrawingInterface.CurrentOrigin = CachedOrigin;

	if (bIsCulling)
	{
		UWorld* world = GetWorld();
		auto controller = world->GetFirstPlayerController<AEditModelPlayerController>();
		if (controller)
		{
			controller->UpdateCutPlaneCullingMaterialInst(ID);
		}
	}

	return true;
}

bool AMOICutPlane::GetForegroundLines(TSharedPtr<Modumate::FDraftingComposite> ParentPage, const FVector &AxisX, const FVector &AxisY, bool bIsDrafting)
{
	AEditModelGameState *gameState = GetWorld()->GetGameState<AEditModelGameState>();
	const Modumate::FGraph3D& volumeGraph = Document->GetVolumeGraph();
	TArray<FVector2D> boxPoints;

	for (auto& point : CachedPoints)
	{
		FVector2D point2D = UModumateGeometryStatics::ProjectPoint2D(point, AxisX, AxisY, CachedOrigin);
		boxPoints.Add(point2D);
	}

	// bounding box is defined by the dimensions of the cut plane as opposed to the contents of the graph
	FBox2D cutPlaneBox = FBox2D(boxPoints);

	TSet<int32> graphObjectIDs;
	TArray<AModumateObjectInstance*> draftingObjectMois;

	volumeGraph.FindObjectsForPlane(AxisX, AxisY, CachedOrigin, cutPlaneBox, graphObjectIDs);

	// Do not get lines if any of the objects is dirty
	for (int32 id : graphObjectIDs)
	{
		auto metaObject = Document->GetObjectById(id);
		if (metaObject == nullptr)
		{
			continue;
		}

		auto graphChildren = metaObject->GetChildObjects();
		for (AModumateObjectInstance* graphChild : graphChildren)
		{
			if ((graphChild == nullptr) || graphChild->IsDirty(EObjectDirtyFlags::Visuals))
			{
				// Returning false here allows us to wait to call GetDraftingLines
				// until everything this CutPlane is dependent on is visually-clean, for correctness.
				// TODO: Optimize CleanObjects to save CutPlanes for last.
				return false;
			}

			auto graphGrandchildren = graphChild->GetChildObjects();
			for (auto* grandchild: graphGrandchildren)
			{
				if (grandchild->GetObjectType() == EObjectType::OTSurfaceGraph)
				{	// Add any surface-graph MOIS, such as finishes & trim.
					if (grandchild->IsDirty(EObjectDirtyFlags::Visuals))
					{
						return false;
					}

					auto surfaceGraph = Document->FindSurfaceGraph(grandchild->ID);
					if (surfaceGraph)
					{
						const auto& surfaceObjects = surfaceGraph->GetAllObjects();

						for (const auto& object: surfaceObjects)
						{
							if (object.Value == Modumate::EGraphObjectType::Vertex)
							{
								continue;
							}

							AModumateObjectInstance* surfaceMoi = Document->GetObjectById(object.Key);
							if (ensure(surfaceMoi))
							{
								draftingObjectMois.Append(surfaceMoi->GetChildObjects());
							}
						}
					}
				}
			}

		}

		draftingObjectMois.Append(graphChildren);
	}

	for (auto moi : draftingObjectMois)
	{
		// TODO: this only is implemented for plane hosted objects right now, this function should be
		// a part of ModumateObjectInstance instead and should propagate down through the children
		TArray<TArray<FVector>> WallCutPerimeters;
		moi->GetDraftingLines(ParentPage, CachedPlane, AxisX, AxisY, CachedOrigin, cutPlaneBox, WallCutPerimeters);
	}

	return true;
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
