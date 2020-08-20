// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Tools/EditModelSelectTool.h"

#include "Algo/Transform.h"
#include "DocumentManagement/ModumateCommands.h"
#include "DocumentManagement/ModumateSnappingView.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "ModumateCore/ModumateObjectStatics.h"
#include "Runtime/Engine/Classes/Engine/Engine.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"

using namespace Modumate;

const FName FSelectedObjectToolMixin::StateRequestTag(TEXT("SelectedObjectTool"));

FSelectedObjectToolMixin::FSelectedObjectToolMixin(AEditModelPlayerController_CPP *InController)
	: ControllerPtr(InController)
{

}

void FSelectedObjectToolMixin::AcquireSelectedObjects()
{
	if (!ControllerPtr.IsValid())
	{
		return;
	}

	ReleaseSelectedObjects();

	auto addIfMeta = [this](FModumateObjectInstance *ob)
	{
		if (ob == nullptr)
		{
			return false;
		}

		switch (ob->GetObjectType())
		{
		case EObjectType::OTMetaEdge:
		case EObjectType::OTMetaPlane:
		case EObjectType::OTMetaVertex:
		{
			FMOIDataRecord metaObjDataRecord = ob->AsDataRecord();

			// TODO: this reinterpretation of corners as control points is a temporary hack,
			// as long as we're still using data record for temporary object modifications.
			metaObjDataRecord.ControlPoints.Reset();
			int32 numCorners = ob->GetNumCorners();
			for (int32 i = 0; i < numCorners; ++i)
			{
				metaObjDataRecord.ControlPoints.Add(ob->GetCorner(i));
			}
			OriginalObjectData.Add(ob, MoveTemp(metaObjDataRecord));
		}
			return true;
		};

		return false;
	};

	AEditModelPlayerState_CPP *playerState = ControllerPtr->EMPlayerState;

	// This is for plane-hosted objects
	// If a plane-hosted object is selected, we want to acquire its parent instead
	for (auto &ob : playerState->SelectedObjects)
	{
		if (!addIfMeta(ob))
		{
			addIfMeta(ob->GetParentObject());
		}
	}

	// We didn't find any meta objects, so just gather the whole selection list
	if (OriginalObjectData.Num() == 0)
	{
		for (auto &ob : playerState->SelectedObjects)
		{
			OriginalObjectData.Add(ob, ob->AsDataRecord());
			for (auto &kid : ob->GetAllDescendents())
			{
				OriginalObjectData.Add(kid, kid->AsDataRecord());
			}
		}
	}

	for (auto &kvp : OriginalObjectData)
	{
		kvp.Key->RequestCollisionDisabled(StateRequestTag, true);
		kvp.Key->ShowAdjustmentHandles(nullptr, false);
		kvp.Key->BeginPreviewOperation();
	}
}

void FSelectedObjectToolMixin::RestoreSelectedObjects()
{
	for (auto kvp : OriginalObjectData)
	{
		kvp.Key->SetFromDataRecordAndRotation(kvp.Value, FVector::ZeroVector, FQuat::Identity);
	}
}

void FSelectedObjectToolMixin::ReleaseSelectedObjects()
{
	ControllerPtr->GetDocument()->ClearPreviewDeltas(ControllerPtr->GetWorld());

	for (auto &kvp : OriginalObjectData)
	{
		kvp.Key->RequestCollisionDisabled(StateRequestTag, false);
		kvp.Key->EndPreviewOperation();
	}

	OriginalObjectData.Empty();
}

void FSelectedObjectToolMixin::ReleaseObjectsAndApplyDeltas()
{
	FModumateDocument* doc = ControllerPtr->GetDocument();
	UWorld* world = ControllerPtr->GetWorld();
	const FGraph3D& volumeGraph = doc->GetVolumeGraph();

	// For all the acquired targets, collect the previewed changes in a way that can be applied as deltas to the document
	TArray<FModumateObjectInstance*> targetPhysicalMOIs;
	TMap<int32, TMap<int32, FVector2D>> combinedVertex2DMovements;
	TMap<int32, FVector> vertex3DMovements;
	for (auto &kvp : OriginalObjectData)
	{
		FModumateObjectInstance* targetMOI = kvp.Key;
		int32 targetID = targetMOI->ID;
		int32 targetParentID = targetMOI->GetParentID();
		const TArray<FVector>& targetCPs = targetMOI->GetControlPoints();
		int32 numCPs = targetCPs.Num();
		EObjectType objectType = targetMOI->GetObjectType();
		EGraphObjectType graph2DObjType = UModumateTypeStatics::Graph2DObjectTypeFromObjectType(objectType);
		EGraph3DObjectType graph3DObjType = UModumateTypeStatics::Graph3DObjectTypeFromObjectType(objectType);

		if (graph2DObjType != EGraphObjectType::None)
		{
			// TODO: all of parenting information is gathered here for projecting 3D control points into 2D surface graph positions,
			// which should be unnecessary if surface graph MOIs expose their 2D information directly.
			const FModumateObjectInstance* surfaceObj = doc->GetObjectById(targetParentID);
			const FGraph2D* surfaceGraph = doc->FindSurfaceGraph(targetParentID);
			const FModumateObjectInstance* surfaceParent = surfaceObj ? surfaceObj->GetParentObject() : nullptr;
			if (!ensure(surfaceObj && surfaceGraph && surfaceParent))
			{
				continue;
			}

			int32 surfaceGraphFaceIndex = UModumateObjectStatics::GetParentFaceIndex(surfaceObj);

			TArray<FVector> facePoints;
			FVector faceNormal, faceAxisX, faceAxisY;
			if (!ensure(UModumateObjectStatics::GetGeometryFromFaceIndex(surfaceParent, surfaceGraphFaceIndex, facePoints, faceNormal, faceAxisX, faceAxisY)))
			{
				continue;
			}
			FVector faceOrigin = facePoints[0];

			TMap<int32, FVector2D>& vertex2DMovements = combinedVertex2DMovements.FindOrAdd(targetParentID);

			switch (graph2DObjType)
			{
			case EGraphObjectType::Vertex:
			{
				if (ensure(numCPs == 1))
				{
					vertex2DMovements.Add(targetID, FVector2D(targetCPs[0]));
					vertex2DMovements.Add(targetID, UModumateGeometryStatics::ProjectPoint2D(targetCPs[0], faceAxisX, faceAxisY, faceOrigin));
				}
				break;
			}
			case EGraphObjectType::Edge:
			{
				const FGraph2DEdge* edge = surfaceGraph->FindEdge(targetID);
				if (ensure(edge && (numCPs == 2)))
				{
					vertex2DMovements.Add(edge->StartVertexID, UModumateGeometryStatics::ProjectPoint2D(targetCPs[0], faceAxisX, faceAxisY, faceOrigin));
					vertex2DMovements.Add(edge->EndVertexID, UModumateGeometryStatics::ProjectPoint2D(targetCPs[1], faceAxisX, faceAxisY, faceOrigin));
				}
				break;
			}
			case EGraphObjectType::Polygon:
			{
				const FGraph2DPolygon* polygon = surfaceGraph->FindPolygon(targetID);
				if (ensure(polygon && (polygon->VertexIDs.Num() == numCPs)))
				{
					for (int32 i = 0; i < numCPs; ++i)
					{
						vertex2DMovements.Add(polygon->VertexIDs[i], UModumateGeometryStatics::ProjectPoint2D(targetCPs[i], faceAxisX, faceAxisY, faceOrigin));
					}
				}
				break;
			}
			default:
				break;
			}
		}
		else if (auto graphObject = volumeGraph.FindObject(Modumate::FTypedGraphObjID(targetID, graph3DObjType)))
		{
			TArray<int32> vertexIDs;
			graphObject->GetVertexIDs(vertexIDs);
			int32 numCorners = targetMOI->GetNumCorners();
			if (!ensureAlways(vertexIDs.Num() == numCorners))
			{
				return;
			}
			for (int32 idx = 0; idx < numCorners; idx++)
			{
				vertex3DMovements.Add(vertexIDs[idx], targetMOI->GetCorner(idx));
			}
		}
		else
		{
			targetPhysicalMOIs.Add(kvp.Key);
		}
	}

	TArray<TSharedPtr<FDelta>> deltas;

	doc->ClearPreviewDeltas(world);
	// First, get deltas for applying volume graph changes as vertex movements
	// TODO: this might be better structured as just a delta collection step, but FinalizeGraphDeltas etc. use the temporary graph,
	// so as long as this function is appropriate in directly applying the deltas, we'll just perform the vertex movement right away.
	if (vertex3DMovements.Num() > 0)
	{
		TArray<int32> vertexMoveIDs;
		TArray<FVector> vertexMovePositions;
		for (auto& kvp : vertex3DMovements)
		{
			vertexMoveIDs.Add(kvp.Key);
			vertexMovePositions.Add(kvp.Value);
		}
		doc->GetVertexMovementDeltas(vertexMoveIDs, vertexMovePositions, deltas);
	}

	// Next, get deltas for surface graph changes
	if (combinedVertex2DMovements.Num() > 0)
	{
		int32 nextID = doc->GetNextAvailableID();
		TArray<FGraph2DDelta> surfaceGraphDeltas;
		for (auto& kvp : combinedVertex2DMovements)
		{
			surfaceGraphDeltas.Reset();
			FGraph2D* surfaceGraph = doc->FindSurfaceGraph(kvp.Key);
			if (!ensure(surfaceGraph) || (kvp.Value.Num() == 0) ||
				!surfaceGraph->MoveVertices(surfaceGraphDeltas, nextID, kvp.Value))
			{
				continue;
			}

			for (auto& delta : surfaceGraphDeltas)
			{
				deltas.Add(MakeShareable(new FGraph2DDelta{ delta }));
			}
		}
	}

	// Next, get deltas for physical MOI movements as regular state data changes with an FMOIDelta
	if (targetPhysicalMOIs.Num() > 0)
	{
		deltas.Add(MakeShareable(new FMOIDelta(targetPhysicalMOIs)));
	}

	// Release the acquired objects
	for (auto &kvp : OriginalObjectData)
	{
		kvp.Key->RequestCollisionDisabled(StateRequestTag, false);
		kvp.Key->EndPreviewOperation();
	}

	// And finally, apply the deltas now that the objects are no longer being previewed and the desired changes have been captured as deltas.
	doc->ApplyDeltas(deltas, world);

	OriginalObjectData.Empty();
}



USelectTool::USelectTool(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, InitialClickedObject(nullptr)
	, InitialClickLocation(ForceInitToZero)
	, Dragging(false)
{}

bool USelectTool::Activate()
{
	Super::Activate();

	Controller->EMPlayerState->SnappedCursor.MouseMode = EMouseMode::Object;

	const TArray<FModumateObjectInstance*> &sels = Controller->EMPlayerState->SelectedObjects;
	for (auto &ob : sels)
	{
		ob->ShowAdjustmentHandles(Controller, true);
	}

	Controller->EMPlayerState->SetShowHoverEffects(true);

	return true;
}

bool USelectTool::Deactivate()
{
	Controller->EMPlayerState->SetShowHoverEffects(false);

	return UEditModelToolBase::Deactivate();
}

bool USelectTool::BeginUse()
{
	Super::BeginUse();

	InitialClickedObject = Controller->EMPlayerState->HoveredObject;
	Controller->EMPlayerState->SetShowHoverEffects(true);
	Controller->GetMousePosition(InitialClickLocation.X, InitialClickLocation.Y);

	return true;
}

bool USelectTool::HandleMouseUp()
{
	if (Dragging)
	{
		ProcessDragSelect();
		EndUse();
		return true;
	}

	AEditModelGameState_CPP *gameState = Controller->GetWorld()->GetGameState<AEditModelGameState_CPP>();
	FModumateDocument *doc = &gameState->Document;

	FModumateObjectInstance *newTarget = Controller->EMPlayerState->HoveredObject;
	bool doubleClicked = false;

	if (newTarget)
	{
		float curTime = UGameplayStatics::GetTimeSeconds(newTarget->GetActor());
		float lastSelectedTime = LastObjectSelectionAttemptTimes.FindRef(newTarget);
		LastObjectSelectionAttemptTimes.Emplace(newTarget, curTime);

		if ((curTime - lastSelectedTime) <= DoubleClickTime)
		{
			doubleClicked = true;
		}
	}

	const TArray<FModumateObjectInstance*> &sels = Controller->EMPlayerState->SelectedObjects;
	int32 numSelections = sels.Num();

	FModumateObjectInstance *currentViewGroup = Controller->EMPlayerState->ViewGroupObject;

	// If we double clicked on a valid object that is within the hierarchy of the current
	// view group, then go into that level of the hierarchy.
	if (doubleClicked)
	{
		if (newTarget && (newTarget->GetParentObject() == currentViewGroup) &&
			(newTarget->GetObjectType() == EObjectType::OTGroup))
		{
			Controller->DeselectAll();
			Controller->SetViewGroupObject(newTarget);
		}
	}
	else
	{
		bool newTargetIsSelected = sels.Contains(newTarget);

		if (!Controller->IsShiftDown() && !Controller->IsControlDown())
		{
			Controller->DeselectAll();
		}

		if (newTarget)
		{
			bool shouldSelect = !newTargetIsSelected;
			if (Controller->IsControlDown())
			{
				shouldSelect = false;
			}
			if (Controller->IsShiftDown())
			{
				shouldSelect = true;
			}
			Controller->SetObjectSelected(newTarget, shouldSelect);
		}
		else if (currentViewGroup && (numSelections == 0))
		{
			FModumateObjectInstance *currentViewGroupParent = currentViewGroup->GetParentObject();
			if (currentViewGroupParent && (currentViewGroupParent->GetObjectType() == EObjectType::OTGroup))
			{
				Controller->SetViewGroupObject(currentViewGroupParent);
			}
			else
			{
				Controller->SetViewGroupObject(nullptr);
			}
		}
	}

	EndUse();

	return true;
}

bool USelectTool::FrameUpdate()
{
	Super::FrameUpdate();

	Controller->EMPlayerState->SnappedCursor.Visible = false;

	FVector2D curMousePosition;
	if (IsInUse() && Controller->GetMousePosition(curMousePosition.X, curMousePosition.Y))
	{
		if (FVector2D::Distance(InitialClickLocation, curMousePosition) > MinDragDist)
		{
			Dragging = true;
			Controller->EMPlayerState->SetShowHoverEffects(false);
		}

		if (Dragging)
		{
			bool draggingRight = curMousePosition.X > InitialClickLocation.X;
			FVector2D dragCorner1(curMousePosition.X, InitialClickLocation.Y);
			FVector2D dragCorner2(InitialClickLocation.X, curMousePosition.Y);

			auto& affordanceLines = Controller->EMPlayerState->AffordanceLines;
			FAffordanceLine dragRectLine;
			dragRectLine.Color = draggingRight ? FLinearColor::Green : FLinearColor::Blue;
			dragRectLine.Interval = 2.0f;

			TArray<FVector2D> dragCornerScreenPoints({ curMousePosition, dragCorner1, InitialClickLocation, dragCorner2 });
			TArray<FVector> dragCornerWorldPoints;
			Algo::Transform(
				dragCornerScreenPoints,
				dragCornerWorldPoints,
				[this](const FVector2D &dragCornerScreenPoint) 
				{
					FVector dragCornerWorldPos, dragCornerWorldDir;
					if (UGameplayStatics::DeprojectScreenToWorld(Controller, dragCornerScreenPoint, dragCornerWorldPos, dragCornerWorldDir))
					{
						return dragCornerWorldPos;
					}
					else
					{
						return FVector::ZeroVector;
					}
				});

			for (int32 i = 0; i < dragCornerWorldPoints.Num(); ++i)
			{
				const FVector &lineStart = dragCornerWorldPoints[i];
				const FVector &lineEnd = dragCornerWorldPoints[(i + 1) % dragCornerWorldPoints.Num()];

				dragRectLine.StartPoint = lineStart;
				dragRectLine.EndPoint = lineEnd;
				affordanceLines.Add(dragRectLine);
			}
		}
	}

	return true;
}

bool USelectTool::HandleInvert()
{
	if (!Active)
	{
		return false;
	}

	if (Controller->EMPlayerState->SelectedObjects.Num() == 0)
	{
		return false;
	}

	// Put selected objects into preview mode and invert
	for (auto &ob : Controller->EMPlayerState->SelectedObjects)
	{
		ob->BeginPreviewOperation();
		ob->InvertObject();
		ob->EndPreviewOperation();
	}

	TArray<TSharedPtr<FDelta>> deltas;
	deltas.Add(MakeShareable(new FMOIDelta(Controller->EMPlayerState->SelectedObjects)));

	AEditModelGameState_CPP* gameState = Controller->GetWorld()->GetGameState<AEditModelGameState_CPP>();
	FModumateDocument* doc = &gameState->Document;
	doc->ApplyDeltas(deltas, GetWorld());

	return true;
}


bool USelectTool::EndUse()
{
	Super::EndUse();

	Dragging = false;
	Controller->EMPlayerState->SetShowHoverEffects(true);

	return true;
}

bool USelectTool::ProcessDragSelect()
{
	FVector2D curMousePosition;
	if (IsInUse() && Dragging && Controller->GetMousePosition(curMousePosition.X, curMousePosition.Y))
	{
		if (!Controller->IsShiftDown() && !Controller->IsControlDown())
		{
			Controller->EMPlayerState->DeselectAll();
		}

		AEditModelGameState_CPP *gameState = Controller->GetWorld()->GetGameState<AEditModelGameState_CPP>();
		FModumateDocument *doc = &gameState->Document;

		bool requireEnclosure = curMousePosition.X > InitialClickLocation.X;
		FBox2D screenSelectRect(ForceInitToZero);
		screenSelectRect += curMousePosition;
		screenSelectRect += InitialClickLocation;

		FVector mouseLoc, mouseDir;
		if (!Controller->DeprojectMousePositionToWorld(mouseLoc, mouseDir))
		{
			return false;
		}

		// Update snapping view, without ignoring anything and with all abstract objects
		// TODO: the snapping view should already be cached from the last update
		const static TSet<int32> idsToIgnore;
		int32 collisionChannelMask = ~0;
		bool bForSnapping = false, bForSelection = true;

		auto *snappingView = Controller->GetSnappingView();
		snappingView->UpdateSnapPoints(idsToIgnore, collisionChannelMask, bForSnapping, bForSelection);

		TSet<FModumateObjectInstance *> objectsInSelection;

		const auto &snapCorners = snappingView->Corners;
		const auto &snapLines = snappingView->LineSegments;

		for (const auto &kvp : snappingView->SnapIndicesByObjectID)
		{
			int32 objectID = kvp.Key;
			bool objInSelection = false;

			FModumateObjectInstance *object = doc->GetObjectById(objectID);
			if ((object == nullptr) || !Controller->EMPlayerState->IsObjectReachableInView(object))
			{
				continue;
			}

			// First, check the corners in order to see if the selection rectangle either fully encloses
			// any of the objects, or if any of the corners are inside the rectangle.
			int32 cornerStartIndex = kvp.Value.CornerStartIndex;
			int32 numCorners = kvp.Value.NumCorners;

			for (int32 i = cornerStartIndex; i < (cornerStartIndex + numCorners); ++i)
			{
				const auto &snapCorner = snapCorners[i];
				const FVector &snapCornerPos = snapCorner.Point;
				FVector2D snapCornerScreenPos;

				if (ensureAlways(snapCorner.ObjID == objectID) &&
					UGameplayStatics::ProjectWorldToScreen(Controller, snapCornerPos, snapCornerScreenPos) &&
					screenSelectRect.IsInside(snapCornerScreenPos))
				{
					objInSelection = true;
				}
				else if (requireEnclosure)
				{
					objInSelection = false;
					break;
				}
			}

			// If only overlap is required, we also have to check for edge overlap rather than just corners.
			if (!requireEnclosure && !objInSelection)
			{
				int32 lineStartIndex = kvp.Value.LineStartIndex;
				int32 numLines = kvp.Value.NumLines;

				for (int32 i = lineStartIndex; i < (lineStartIndex + numLines); ++i)
				{
					const auto &snapLine = snapLines[i];

					FVector2D snapLineScreenStart, snapLineScreenEnd;
					if (ensureAlways(snapLine.ObjID == objectID) &&
						UGameplayStatics::ProjectWorldToScreen(Controller, snapLine.P1, snapLineScreenStart) &&
						UGameplayStatics::ProjectWorldToScreen(Controller, snapLine.P2, snapLineScreenEnd) &&
						UModumateFunctionLibrary::LineBoxIntersection(screenSelectRect, snapLineScreenStart, snapLineScreenEnd))
					{
						objInSelection = true;
						break;
					}
				}
			}

			if (objInSelection)
			{
				objectsInSelection.Add(object);
				continue;
			}
		}

		// If only overlap is required, we also have to check for face overlap, in case the selection rectangle
		// is entirely contained within one of the faces formed by an object's control points
		if (!requireEnclosure)
		{
			TArray<FHitResult> hitResults;
			if (Controller->GetWorld()->LineTraceMultiByChannel(hitResults, mouseLoc, mouseLoc + Controller->MaxRaycastDist * mouseDir, ECC_Camera))
			{
				for (const auto &hitResult : hitResults)
				{
					FModumateObjectInstance *object = doc->ObjectFromActor(hitResult.GetActor());
					if (object && Controller->EMPlayerState->IsObjectReachableInView(object))
					{
						objectsInSelection.Add(object);
					}
				}
			}
		}

		bool newObjectsSelected = !Controller->IsControlDown();
		// actually perform the selection on the filtered objects
		for (auto *object : objectsInSelection)
		{
			Controller->SetObjectSelected(object, newObjectsSelected);
		}
		return true;
	}

	return false;
}

