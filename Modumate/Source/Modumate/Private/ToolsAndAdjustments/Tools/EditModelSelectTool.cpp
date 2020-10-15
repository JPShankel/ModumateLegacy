// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Tools/EditModelSelectTool.h"

#include "Algo/Transform.h"
#include "DocumentManagement/ModumateCommands.h"
#include "DocumentManagement/ModumateSnappingView.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "Runtime/Engine/Classes/Engine/Engine.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"

using namespace Modumate;

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

	auto &sels = Controller->EMPlayerState->SelectedObjects;
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

	auto delta = MakeShared<FMOIDelta>();
	FMOIStateData invertedState;

	// Put a selected object supports inversion, add it to the delta
	for (auto obj : Controller->EMPlayerState->SelectedObjects)
	{
		if (obj->GetInvertedState(invertedState))
		{
			delta->AddMutationState(obj, obj->GetStateData(), invertedState);
		}
	}

	AEditModelGameState_CPP* gameState = Controller->GetWorld()->GetGameState<AEditModelGameState_CPP>();
	gameState->Document.ApplyDeltas({ delta }, GetWorld());

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

