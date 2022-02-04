// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Tools/EditModelSelectTool.h"

#include "Algo/Transform.h"
#include "DocumentManagement/ModumateCommands.h"
#include "DocumentManagement/ModumateSnappingView.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "Objects/ModumateObjectStatics.h"
#include "Online/ModumateAnalyticsStatics.h"
#include "Runtime/Engine/Classes/Engine/Engine.h"
#include "UnrealClasses/EditModelGameState.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"



USelectTool::USelectTool()
	: Super()
	, InitialClickedObject(nullptr)
	, InitialClickLocation(ForceInitToZero)
	, Dragging(false)
{}

bool USelectTool::Activate()
{
	Super::Activate();

	Controller->EMPlayerState->SnappedCursor.MouseMode = EMouseMode::Object;
	Controller->EMPlayerState->PostSelectionChanged();
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

	AEditModelGameState *gameState = Controller->GetWorld()->GetGameState<AEditModelGameState>();
	UModumateDocument* doc = gameState->Document;

	AModumateObjectInstance *newTarget = Controller->EMPlayerState->HoveredObject;
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

	auto& selectedObjects = Controller->EMPlayerState->SelectedObjects;
	int32 numSelections = selectedObjects.Num();
	auto& selectedGroups = Controller->EMPlayerState->SelectedGroupObjects;

	AModumateObjectInstance *currentViewGroup = Controller->EMPlayerState->ViewGroupObject;

	// If we double clicked on a valid object that is graph3D-hosted,
	// then make its graph the active one.
	if (doubleClicked)
	{
		AModumateObjectInstance* newGroupTarget = newTarget;
		while (newGroupTarget->GetParentID() != MOD_ID_NONE)
		{
			newGroupTarget = newGroupTarget->GetParentObject();
		}

		int32 subGroupID = MOD_ID_NONE;
		bool bInGroup;
		TArray<int32> affectedGroups;

		if (UModumateObjectStatics::IsObjectInSubgroup(doc, newTarget, doc->GetActiveVolumeGraphID(), subGroupID, bInGroup))
		{
			const FGraph3D* selectedGraph = doc->GetVolumeGraph(subGroupID);
			if (ensure(selectedGraph))
			{
				if (doc->GetActiveVolumeGraphID() != subGroupID)
				{
					ensure(UModumateObjectStatics::GetGroupIdsForGroupChange(doc, subGroupID, affectedGroups));
					doc->SetActiveVolumeGraphID(subGroupID);
					Controller->DeselectAll();
				}
			}
		}
		else if (UModumateTypeStatics::Graph3DObjectTypeFromObjectType(newGroupTarget->GetObjectType()) != EGraph3DObjectType::None)
		{
			const FGraph3D* selectedGraph = doc->FindVolumeGraph(newGroupTarget->ID);
			if (ensure(selectedGraph))
			{
				const int32 selectedGroupID = selectedGraph->GraphID;
				if (doc->GetActiveVolumeGraphID() != selectedGroupID)
				{
					ensure(UModumateObjectStatics::GetGroupIdsForGroupChange(doc, selectedGroupID, affectedGroups));
					doc->SetActiveVolumeGraphID(selectedGraph->GraphID);
					Controller->DeselectAll();
				}
			}
		}

		if (affectedGroups.Num() > 0)
		{
			Controller->EMPlayerState->PostGroupChanged(affectedGroups);
		}
	}
	else
	{
		bool newTargetIsSelected = selectedObjects.Contains(newTarget);

		if (!Controller->IsShiftDown() && !Controller->IsControlDown())
		{
			Controller->DeselectAll();
		}

		if (newTarget)
		{
			bool bShouldSelect = !newTargetIsSelected;
			if (Controller->IsControlDown())
			{
				bShouldSelect = false;
			}
			if (Controller->IsShiftDown())
			{
				bShouldSelect = true;
			}

			bool bDeselectOthers = !Controller->IsControlDown() && !Controller->IsShiftDown();

			bool bInGroup = false;
			int32 selectedGroupID = MOD_ID_NONE;

			if (UModumateObjectStatics::IsObjectInSubgroup(doc, newTarget, doc->GetActiveVolumeGraphID(), selectedGroupID, bInGroup))
			{
				Controller->EMPlayerState->SetGroupObjectSelected(doc->GetObjectById(selectedGroupID), bShouldSelect, bDeselectOthers);
			}
			else if (bInGroup)
			{
				Controller->SetObjectSelected(newTarget, bShouldSelect, bDeselectOthers);
			}

			static const FString eventNameClick(TEXT("Click"));
			UModumateAnalyticsStatics::RecordSimpleToolEvent(this, GetToolMode(), eventNameClick);
		}
		else if (currentViewGroup && (numSelections == 0))
		{
			AModumateObjectInstance *currentViewGroupParent = currentViewGroup->GetParentObject();
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

	auto delta = MakeShared<FMOIDelta>();
	FMOIStateData invertedState;

	// If a selected object supports inversion, add it to the delta
	for (auto obj : Controller->EMPlayerState->SelectedObjects)
	{
		if (obj->GetInvertedState(invertedState))
		{
			delta->AddMutationState(obj, obj->GetStateData(), invertedState);
		}
	}

	if (!delta->IsValid())
	{
		return false;
	}

	AEditModelGameState* gameState = Controller->GetWorld()->GetGameState<AEditModelGameState>();
	return gameState->Document->ApplyDeltas({ delta }, GetWorld());
}

bool USelectTool::HandleFlip(EAxis::Type FlipAxis)
{
	if (!Active || (Controller->EMPlayerState->SelectedObjects.Num() == 0))
	{
		return false;
	}

	auto delta = MakeShared<FMOIDelta>();
	FMOIStateData flippedState;

	// If a selected object supports flipping, add it to the delta
	for (auto obj : Controller->EMPlayerState->SelectedObjects)
	{
		if (obj->GetFlippedState(FlipAxis, flippedState))
		{
			delta->AddMutationState(obj, obj->GetStateData(), flippedState);
		}
	}

	if (!delta->IsValid())
	{
		return false;
	}

	AEditModelGameState* gameState = Controller->GetWorld()->GetGameState<AEditModelGameState>();
	return gameState->Document->ApplyDeltas({ delta }, GetWorld());
}

bool USelectTool::HandleOffset(const FVector2D& ViewSpaceDirection)
{
	if (!Active || (Controller->EMPlayerState->SelectedObjects.Num() == 0))
	{
		return false;
	}

	FQuat cameraRotation = Controller->PlayerCameraManager->GetCameraRotation().Quaternion();
	FVector worldSpaceDirection =
		(ViewSpaceDirection.X * cameraRotation.GetRightVector()) +
		(ViewSpaceDirection.Y * cameraRotation.GetUpVector());

	auto delta = MakeShared<FMOIDelta>();
	FMOIStateData justifiedState;

	// If a selected object supports justification, add it to the delta
	for (auto obj : Controller->EMPlayerState->SelectedObjects)
	{
		if (obj->GetOffsetState(worldSpaceDirection, justifiedState))
		{
			delta->AddMutationState(obj, obj->GetStateData(), justifiedState);
		}
	}

	if (!delta->IsValid())
	{
		return false;
	}

	AEditModelGameState* gameState = Controller->GetWorld()->GetGameState<AEditModelGameState>();
	return gameState->Document->ApplyDeltas({ delta }, GetWorld());
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

		AEditModelGameState *gameState = Controller->GetWorld()->GetGameState<AEditModelGameState>();
		UModumateDocument* doc = gameState->Document;

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

		TSet<AModumateObjectInstance *> objectsInSelection;

		const auto &snapCorners = snappingView->Corners;
		const auto &snapLines = snappingView->LineSegments;

		for (const auto &kvp : snappingView->SnapIndicesByObjectID)
		{
			int32 objectID = kvp.Key;
			bool objInSelection = false;

			AModumateObjectInstance *object = doc->GetObjectById(objectID);
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

						if (objInSelection)
						{
							break;
						}

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
			FHitResult hitResult;
			FVector traceStart = mouseLoc;
			while (Controller->LineTraceSingleAgainstMOIs(hitResult, traceStart, traceStart + Controller->MaxRaycastDist * mouseDir) &&
				(hitResult.Distance > RAY_INTERSECT_TOLERANCE))
			{
				AModumateObjectInstance* object = doc->ObjectFromActor(hitResult.GetActor());
				if (object && Controller->EMPlayerState->IsObjectReachableInView(object))
				{
					objectsInSelection.Add(object);
				}

				traceStart = hitResult.Location;
			}
		}

		bool bSetObjectsSelected = !Controller->IsControlDown();

		bool bDeselectOthers = (!Controller->IsShiftDown() && bSetObjectsSelected);

		if (bDeselectOthers)
		{
			Controller->DeselectAll();
		}
		const int32 activeGroup = doc->GetActiveVolumeGraphID();
		for (auto* selectedObject: objectsInSelection)
		{
			bool bInGroup = false;
			int32 selectedGroupID = MOD_ID_NONE;

			if (UModumateObjectStatics::IsObjectInSubgroup(doc, selectedObject, activeGroup, selectedGroupID, bInGroup))
			{
				Controller->EMPlayerState->SetGroupObjectSelected(doc->GetObjectById(selectedGroupID), bSetObjectsSelected, false);
			}
			else if (bInGroup)
			{
				Controller->SetObjectSelected(selectedObject, bSetObjectsSelected, false);
			}
		}

		static const FString eventNameDrag(TEXT("Drag"));
		UModumateAnalyticsStatics::RecordSimpleToolEvent(this, GetToolMode(), eventNameDrag);

		return true;
	}

	return false;
}

void USelectTool::RegisterToolDataUI(class UToolTrayBlockProperties* PropertiesUI, int32& OutMaxNumRegistrations)
{
	OutMaxNumRegistrations = Controller->EMPlayerState->SelectedObjects.Num();
	for (auto selectedMOI : Controller->EMPlayerState->SelectedObjects)
	{
		if (selectedMOI)
		{
			selectedMOI->RegisterInstanceDataUI(PropertiesUI);
		}
	}
}
