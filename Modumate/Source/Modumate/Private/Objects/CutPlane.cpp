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
	EdgeSelectedColor(0x93, 0x93, 0x93),
	EdgeColor(0x63, 0xC3, 0xBA),
	bHUDDwgDrafting(true)
{
	FWebMOIProperty prop;

	prop.Name = TEXT("Rotation");
	prop.Type = EWebMOIPropertyType::quatRotation;
	prop.DisplayName = TEXT("Flip Direction");
	prop.isEditable = true;
	prop.isVisible = true;
	WebProperties.Add(prop.Name, prop);
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

	auto* gameMode = world->GetGameInstance<UModumateGameInstance>()->GetEditModelGameMode();
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
#if UE_SERVER
		return true;
#else
		return TryUpdateVisuals() && UpdateDraftingPreview();
#endif
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

void AMOICutPlane::SetIsCulling(bool NewIsCulling)
{
	bIsCulling = NewIsCulling;
	for (auto curMOIID : CachedForegroundMOIs)
	{
		const auto curMOI = GetDocument()->GetObjectById(curMOIID);
		if (curMOI)
		{
			curMOI->ToggleAndUpdateCapGeometry(NewIsCulling);
		}
	}
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

void AMOICutPlane::GetDraftingLines(const TSharedPtr<FDraftingComposite> &ParentPage, const FPlane &Plane,
	const FVector &AxisX, const FVector &AxisY, const FVector &Origin, const FBox2D &BoundingBox,
	TArray<TArray<FVector>> &OutPerimeters) const
{
	// Add any traced splines.
	for(const auto& traceResponse: InprocessRenders)
	{
		const FPendingObjectRender& renderInfo = traceResponse.Value;
		FModumateTraceObject trace;
		TSharedPtr<FDraftingComposite> singleOutlineElement = MakeShareable(new FDraftingComposite);
		ParentPage->Children.Add(singleOutlineElement);

		bool result = FJsonObjectConverter::JsonObjectStringToUStruct<FModumateTraceObject>(renderInfo.JsonTrace, &trace, 0, 0);
		if (!ensureMsgf(result, TEXT("Couldn't parse trace JSON")) )
		{
			return;
		}

		const int32 numInstances = renderInfo.PagePosition.Num();

		for (int32 instance = 0; instance < numInstances; ++instance)
		{

			FVector currentPoint(0.0f);
			FVector startPoint(0.0f);
			bool bClosedCurve = false;
			const float lineDepth = renderInfo.Depth[instance];
			const FVector tracePosition(renderInfo.PagePosition[instance], lineDepth);
			const FVector traceScale(-renderInfo.Scale, -renderInfo.Scale, 1.0f);
			TArray<FEdge> outlineLines;
			int lineStartIndex = outlineLines.Num();
			for (const FModumateTraceSplineEntry& entry : trace.lines)
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

			for (auto& line : outlineLines)
			{
				line.Vertex[0] *= traceScale;
				line.Vertex[1] *= traceScale;
				line.Vertex[0] += tracePosition;
				line.Vertex[1] += tracePosition;
			}

			for (auto& outlineLine : outlineLines)
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
						TSharedPtr<FDraftingLine> draftingLine = MakeShareable(new FDraftingLine(
							FModumateUnitCoord2D::WorldCentimeters(boxClipped0),
							FModumateUnitCoord2D::WorldCentimeters(boxClipped1),
							ModumateUnitParams::FThickness::Points(0.15f), FMColor::Gray144));
						singleOutlineElement->Children.Add(draftingLine);
						draftingLine->SetLayerTypeRecursive(renderInfo.LayerType);
					}
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

void AMOICutPlane::SetupPendingRenders()
{
	int32 renderId = 0;
	PendingObjectRenders.Empty();
	InprocessRenders.Empty();

	const FVector viewDirection = GetNormal();
	FVector endAxisX, endAxisY;
	UModumateGeometryStatics::FindBasisVectors(endAxisX, endAxisY, viewDirection);
	if (!FVector::Parallel(viewDirection, FVector::UpVector))
	{
		endAxisX *= -1.0f;
		endAxisY *= -1.0f;
	}

	const FQuat cutplaneRotation(FRotationMatrix::MakeFromXY(viewDirection, endAxisX));

	TArray<AModumateObjectInstance*> bitmapObjects = GetDocument()->GetObjectsOfType(EObjectType::OTFurniture);
	for (const auto object : bitmapObjects)
	{
		FGuid guid = object->GetStateData().AssemblyGUID;

		const ACompoundMeshActor* actor = CastChecked<ACompoundMeshActor>(object->GetActor());
		FQuat itemWorldRotation = actor->GetActorQuat();
		FVector location = actor->GetActorLocation();

		FPendingObjectRender* nextRender = nullptr;

		bool bReuseRender = false;
		for (auto& existingRender: PendingObjectRenders)
		{
			if (existingRender.Assembly == guid && itemWorldRotation.Equals(existingRender.ItemRotation, 0.04f))
			{
				nextRender = &existingRender;
				bReuseRender = true;
				break;
			}
		}

		if (!bReuseRender)
		{
			nextRender = &PendingObjectRenders.AddDefaulted_GetRef();
			nextRender->LocalId = renderId++;
			nextRender->MoiId = object->ID;
			nextRender->LayerType = FModumateLayerType::kFfeOutline;
			nextRender->Assembly = guid;
			nextRender->ItemRotation = itemWorldRotation;

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
			nextRender->maxDimension = bounds.GetSize().Size();
			nextRender->Scale = 2.0f * nextRender->maxDimension / RenderTargetSize;
			FTransform cutPlaneTransform;
			// TODO: Revisit backoff of view.
			cutPlaneTransform.SetTranslation(location - 2.0f * nextRender->maxDimension * viewDirection);
			cutPlaneTransform.SetRotation(cutplaneRotation);
			nextRender->RenderTransform = cutPlaneTransform;
		}

		FVector2D projectedLocation = UModumateGeometryStatics::ProjectPoint2D(location, endAxisX, endAxisY, CachedPoints[0]);
		FVector2D renderOrigin = projectedLocation + FVector2D(nextRender->maxDimension, nextRender->maxDimension);
		nextRender->PagePosition.Add(renderOrigin);
		float depth = CachedPlane.PlaneDot(location);
		nextRender->Depth.Add(depth);
	}
}

bool AMOICutPlane::StartRender(UModumateDocument* doc /*= nullptr*/)
{
	FPendingObjectRender objectRender;
	AModumateObjectInstance* object = nullptr;
	do
	{
		if (PendingObjectRenders.Num() == 0)
		{
			return false;
		}
		objectRender = PendingObjectRenders.Pop();
		object = GetDocument()->GetObjectById(objectRender.MoiId);
	} while (object == nullptr);

	// setup texture target
	CaptureActor->CurrentTextureTarget = UKismetRenderingLibrary::CreateRenderTarget2D(DynamicMeshActor->GetWorld(), RenderTargetSize,
		RenderTargetSize, ETextureRenderTargetFormat::RTF_RGBA8);
	CaptureActor->CurrentTextureTarget->ClearColor = FLinearColor(1.0f, 1.0f, 1.0f);
	CaptureActor->ScopeBoxID = objectRender.LocalId;

	// setup capture component
	auto* captureComponent = CaptureActor->CaptureComponent;
	FTransform cutPlaneTransform;
	captureComponent->TextureTarget = CaptureActor->CurrentTextureTarget;
	captureComponent->OrthoWidth = 2.0f * objectRender.maxDimension;
	captureComponent->SetWorldTransform(objectRender.RenderTransform);

	captureComponent->ClearShowOnlyComponents();
	captureComponent->ShowOnlyActorComponents(object->GetActor(), true);

	// capture scene
	captureComponent->bAlwaysPersistRenderingState = true;
	captureComponent->CaptureScene();
	CaptureActor->bCaptureStarted = true;
	CaptureActor->TickCount = 2;

	CurrentObjectRender = objectRender;
	++PendingTraceRequests;

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

	if (InstanceData.Extents.IsNearlyZero())
	{   // Some scenes may have a zero-size plane.
		InstanceData.Extents = FVector2D(100.0f, 100.0f);
	}
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

bool AMOICutPlane::UpdateDraftingPreview(bool bForce /*= false*/)
{
	PreviewHUDLines = nullptr;

	if (!IsVisible() && !bForce)
	{
		return true;
	}
	PreviewHUDLines = MakeShared<FDraftingComposite>();

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

// This routine is called for the HUD lines. The related routine
// FDraftingDrawing::GetForegroundLines() is called for drafting lines.
bool AMOICutPlane::GetForegroundLines(TSharedPtr<FDraftingComposite> ParentPage, const FVector &AxisX, const FVector &AxisY, bool bIsDrafting)
{
	AEditModelGameState *gameState = GetWorld()->GetGameState<AEditModelGameState>();
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

	TArray<const AModumateObjectInstance*> groupObjects(Document->GetObjectsOfType(EObjectType::OTMetaGraph));
	for (const auto* groupObject: groupObjects)
	{
		const FGraph3D& volumeGraph = *Document->GetVolumeGraph(groupObject->ID);

		volumeGraph.FindObjectsForPlane(AxisX, AxisY, CachedOrigin, cutPlaneBox, graphObjectIDs);

		// Do not get lines if any of the objects is dirty
		for (int32 id : graphObjectIDs)
		{
			auto metaObject = Document->GetObjectById(id);
			if (metaObject == nullptr)
			{
				continue;
			}

			TArray<int32> spanIds;
			if (metaObject->GetObjectType() == EObjectType::OTMetaPlane)
			{
				UModumateObjectStatics::GetSpansForFaceObject(Document, metaObject, spanIds);
			}
			else if (metaObject->GetObjectType() == EObjectType::OTMetaEdge)
			{
				UModumateObjectStatics::GetSpansForEdgeObject(Document, metaObject, spanIds);
			}

			// Any items w/o spans:
			TArray<AModumateObjectInstance*> graphChildren = metaObject->GetChildObjects()
				;
			for (int32 spanId : spanIds)
			{
				auto spanObject = Document->GetObjectById(spanId);
				if (spanObject)
				{
					graphChildren.Append(spanObject->GetChildObjects());
				}
			}
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
				for (auto* grandchild : graphGrandchildren)
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

							for (const auto& object : surfaceObjects)
							{
								if (object.Value == EGraphObjectType::Vertex)
								{
									continue;
								}

								AModumateObjectInstance* surfaceMoi = Document->GetObjectById(object.Key);
								if (ensure(surfaceMoi))
								{
									draftingObjectMois.Append(surfaceMoi->GetChildObjects().FilterByPredicate(
										[](const AModumateObjectInstance* moi)
									{ return moi->GetObjectType() != EObjectType::OTCabinet; }));
								}
							}
						}
					}
				}

			}

			draftingObjectMois.Append(graphChildren);
		}
	}

	// Add cabinets globally.
	draftingObjectMois.Append(Document->GetObjectsOfType(EObjectType::OTCabinet));
	// Add terrain globally, since it's not a child of a massing element.
	draftingObjectMois.Append(Document->GetObjectsOfType(EObjectType::OTTerrain));

	// Add hosted objs
	TArray<AModumateObjectInstance*> hostedObjectMois;
	hostedObjectMois.Append(Document->GetObjectsOfType(EObjectType::OTPointHosted));
	hostedObjectMois.Append(Document->GetObjectsOfType(EObjectType::OTEdgeHosted));
	for (auto curCp : hostedObjectMois)
	{
		if (!draftingObjectMois.Contains(curCp) && curCp->GetActor())
		{
			FBox moiBox = curCp->GetActor()->GetComponentsBoundingBox();
			FVector boxCenter, boxExtents;
			moiBox.GetCenterAndExtents(boxCenter, boxExtents);

			// Find distance of box center from plane
			FPlane slicePlane(GetLocation(), GetNormal());
			float boxCenterDist = slicePlane.PlaneDot(boxCenter);

			// Compare size of box in plane normal direction
			float boxSize = FVector::BoxPushOut(slicePlane, boxExtents);
			if (FMath::Abs(boxCenterDist) < boxSize)
			{
				draftingObjectMois.Add(curCp);
			}
		}
	}

	TArray<int32> curDraftingObjectIds;

	for (auto moi : draftingObjectMois)
	{
		// TODO: this only is implemented for plane hosted objects right now, this function should be
		// a part of ModumateObjectInstance instead and should propagate down through the children
		TArray<TArray<FVector>> WallCutPerimeters;
		moi->GetDraftingLines(ParentPage, CachedPlane, AxisX, AxisY, CachedOrigin, cutPlaneBox, WallCutPerimeters);

		// Create cap geometry for Mois
		curDraftingObjectIds.Add(moi->ID);
		if (bIsCulling)
		{
			moi->ToggleAndUpdateCapGeometry(true);
		}
	}

	// Check previous cached Mois with cap, and remove them if necessary
	for (auto curId : CachedForegroundMOIs)
	{
		if (!curDraftingObjectIds.Contains(curId))
		{
			const auto curMOI = GetDocument()->GetObjectById(curId);
			if (curMOI)
			{
				curMOI->ToggleAndUpdateCapGeometry(false);
			}
		}
	}
	CachedForegroundMOIs = curDraftingObjectIds;

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

void AMOICutPlane::TraceRequestComplete(int32 TraceID, FString TraceString)
{
	FPendingObjectRender* render = InprocessRenders.Find(TraceID);
	if (render)
	{
		render->JsonTrace = MoveTemp(TraceString);
	}
	else
	{
		UE_LOG(LogAutoDrafting, Error, TEXT("Unknown Autotrace ID %d"), TraceID);
	}

	if (--PendingTraceRequests == 0)
	{
		CaptureDelegate.Broadcast();
	}
}

void AMOICutPlane::ConvertToOutlines(const FString& renderTargetFilename)
{
	FModumateAutotraceConnect autotraceServer;

	if (autotraceServer.ConvertImageFromFile(renderTargetFilename, CurrentObjectRender.LocalId, ID, GetWorld()))
	{
		InprocessRenders.Add(CurrentObjectRender.LocalId, CurrentObjectRender);
	}
	else
	{
		--PendingTraceRequests;
	}
}
