// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Tools/EditModelRectangleTool.h"

#include "Components/EditableTextBox.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "Online/ModumateAnalyticsStatics.h"
#include "UI/DimensionManager.h"
#include "UI/PendingSegmentActor.h"
#include "UnrealClasses/DimensionWidget.h"
#include "UnrealClasses/DynamicMeshActor.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "UnrealClasses/EditModelGameState.h"
#include "UnrealClasses/EditModelGameMode.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "UnrealClasses/LineActor.h"

using namespace Modumate;

URectangleTool::URectangleTool(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, State(Neutral)
	, bPendingPlaneValid(false)
	, bExtrudingPlaneFromEdge(false)
	, PendingPlaneGeom(ForceInitToZero)
	, MinPlaneSize(1.0f)
{
	UWorld *world = Controller ? Controller->GetWorld() : nullptr;
	if (world)
	{
		GameMode = world->GetAuthGameMode<AEditModelGameMode>();
		GameState = world->GetGameState<AEditModelGameState>();
	}
}

bool URectangleTool::HandleInputNumber(double n)
{
	Super::HandleInputNumber(n);

	if (!Controller->EMPlayerState->SnappedCursor.Visible)
	{
		return false;
	}

	auto pendingSegment = DimensionManager->GetDimensionActor(PendingSegmentID)->GetLineActor();

	if ((State != Neutral) && pendingSegment != nullptr)
	{
		FVector dir = (pendingSegment->Point2 - pendingSegment->Point1).GetSafeNormal() * n;

		static const FString eventNameSegment(TEXT("EnteredDimStringSegment"));
		static const FString eventNamePlane(TEXT("EnteredDimStringPlane"));
		FString eventName;
		switch (State)
		{
		case NewSegmentPending:
			eventName = eventNameSegment;
			break;
		case NewPlanePending:
			eventName = eventNamePlane;
			break;
		default:
			break;
		}

		if (bExtrudingPlaneFromEdge)
		{
			static const FString eventSuffixExtrusion(TEXT("Extrusion"));
			eventName += eventSuffixExtrusion;
		}

		bool bSuccess = MakeObject(pendingSegment->Point1 + dir);
		if (bSuccess)
		{
			UModumateAnalyticsStatics::RecordSimpleToolEvent(this, GetToolMode(), eventName);
		}

		return bSuccess;
	}
	return true;
}

bool URectangleTool::Activate()
{
	Controller->DeselectAll();
	Controller->EMPlayerState->SetHoveredObject(nullptr);
	OriginalMouseMode = Controller->EMPlayerState->SnappedCursor.MouseMode;
	Controller->EMPlayerState->SnappedCursor.MouseMode = EMouseMode::Location;

	return UEditModelToolBase::Activate();
}

bool URectangleTool::Deactivate()
{
	State = Neutral;
	bExtrudingPlaneFromEdge = false;
	Controller->EMPlayerState->SnappedCursor.MouseMode = OriginalMouseMode;
	Controller->EMPlayerState->SnappedCursor.WantsVerticalAffordanceSnap = false;
	return UEditModelToolBase::Deactivate();
}

bool URectangleTool::BeginUse()
{
	Super::BeginUse();
	if (!Controller->EMPlayerState->SnappedCursor.Visible)
	{
		return false;
	}

	const FVector &hitLoc = Controller->EMPlayerState->SnappedCursor.WorldPosition;

	PendingPlanePoints.Reset();
	PlaneBaseStart = FVector::ZeroVector;
	PlaneBaseEnd = FVector::ZeroVector;

	const FVector &tangent = Controller->EMPlayerState->SnappedCursor.HitTangent;
	Controller->EMPlayerState->SnappedCursor.WantsVerticalAffordanceSnap = true;

	AModumateObjectInstance *newTarget = Controller->EMPlayerState->HoveredObject;
	if (newTarget && newTarget->GetObjectType() == EObjectType::OTMetaEdge)
	{
		UModumateDocument* doc = GameState->Document;
		auto& graph = doc->GetVolumeGraph();
		auto edge = graph.FindEdge(newTarget->ID);
		if (edge == nullptr)
		{
			return false;
		}

		auto startVertex = graph.FindVertex(edge->StartVertexID);
		auto endVertex = graph.FindVertex(edge->EndVertexID);
		if (!startVertex || !endVertex)
		{
			return false;
		}

		PlaneBaseStart = startVertex->Position;
		PlaneBaseEnd = endVertex->Position;
		FVector normal = (PlaneBaseEnd - PlaneBaseStart).GetSafeNormal();
		FVector axisX, axisY;
		UModumateGeometryStatics::FindBasisVectors(axisX, axisY, normal);
		Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(hitLoc, normal, axisX);
		State = NewPlanePending;
		bExtrudingPlaneFromEdge = true;
	}
	else 
	{
		const FVector& normal = Controller->EMPlayerState->SnappedCursor.HitNormal;
		if (!FMath::IsNearlyZero(normal.Size()))
		{
			Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(hitLoc, normal, tangent);
		}
		else
		{
			Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(hitLoc, FVector::UpVector, tangent);
		}
		State = NewSegmentPending;
	}


	auto dimensionActor = DimensionManager->GetDimensionActor(PendingSegmentID);
	if (dimensionActor != nullptr)
	{
		auto pendingSegment = dimensionActor->GetLineActor();
		pendingSegment->Point1 = hitLoc;
		pendingSegment->Point2 = hitLoc;
		pendingSegment->SetVisibilityInApp(false);
	}

	GameState->Document->StartPreviewing();

	return true;
}

bool URectangleTool::EnterNextStage()
{
	Super::EnterNextStage();

	if (!Controller->EMPlayerState->SnappedCursor.Visible)
	{
		return false;
	}
	if (State == NewSegmentPending || State == NewPlanePending)
	{
		static const FString eventNameSegment(TEXT("ClickedSegment"));
		static const FString eventNamePlane(TEXT("ClickedPlane"));
		FString eventName;

		switch (State)
		{
		case NewSegmentPending:
			eventName = eventNameSegment;
			break;
		case NewPlanePending:
			eventName = eventNamePlane;
			break;
		default:
			break;
		}

		if (Controller->EMPlayerState->SnappedCursor.ShiftLocked)
		{
			static const FString eventSuffixShiftSnapped(TEXT("ShiftSnapped"));
			eventName += eventSuffixShiftSnapped;
		}

		if (bExtrudingPlaneFromEdge)
		{
			static const FString eventSuffixExtrusion(TEXT("Extrusion"));
			eventName += eventSuffixExtrusion;
		}

		bool bSuccess = MakeObject(Controller->EMPlayerState->SnappedCursor.WorldPosition);
		if (bSuccess)
		{
			UModumateAnalyticsStatics::RecordSimpleToolEvent(this, GetToolMode(), eventName);
		}

		return bSuccess;
	}

	return false;
}

bool URectangleTool::GetMetaObjectCreationDeltas(const FVector& Location, bool bSplitAndUpdateFaces, FVector& OutConstrainedLocation, TArray<FDeltaPtr>& OutDeltaPtrs)
{
	OutConstrainedLocation = Location;
	OutDeltaPtrs.Reset();

	CurAddedEdgeIDs.Reset();
	CurAddedFaceIDs.Reset();

	UModumateDocument* doc = GameState->Document;

	bool bSuccess = false;
	bool bVerticesExist = false;
	auto dimensionActor = DimensionManager->GetDimensionActor(PendingSegmentID);
	auto pendingSegment = dimensionActor->GetLineActor();
	if (pendingSegment == nullptr)
	{
		return false;
	}

	FVector constrainedStartPoint = pendingSegment->Point1;
	OutConstrainedLocation = Location;
	ConstrainHitPoint(constrainedStartPoint);
	ConstrainHitPoint(OutConstrainedLocation);
	if (FVector::Dist(constrainedStartPoint, OutConstrainedLocation) < MinPlaneSize)
	{
		return true;
	}

	if (State == NewSegmentPending)
	{
		PlaneBaseStart = constrainedStartPoint;
		PlaneBaseEnd = OutConstrainedLocation;

		if (!(PlaneBaseEnd - PlaneBaseStart).IsNearlyZero())
		{
			bSuccess = doc->MakeMetaObject(GetWorld(), { PlaneBaseStart, PlaneBaseEnd }, CurAddedEdgeIDs, OutDeltaPtrs, bSplitAndUpdateFaces);
		}
	}
	else if (State == NewPlanePending)
	{
		// set end of the segment to the hit location
		pendingSegment->Point2 = OutConstrainedLocation;
		UpdatePendingPlane();
		bSuccess = doc->MakeMetaObject(GetWorld(), PendingPlanePoints, CurAddedFaceIDs, OutDeltaPtrs, bSplitAndUpdateFaces);
	}

	if (!bSuccess)
	{
		OutDeltaPtrs.Reset();
	}

	return bSuccess;
}

bool URectangleTool::MakeObject(const FVector& Location)
{
	UModumateDocument* doc = GameState ? GameState->Document : nullptr;
	if (doc == nullptr)
	{
		return false;
	}

	doc->ClearPreviewDeltas(GetWorld());

	auto dimensionActor = DimensionManager->GetDimensionActor(PendingSegmentID);
	auto pendingSegment = dimensionActor ? dimensionActor->GetLineActor() : nullptr;
	if (!pendingSegment)
	{
		return false;
	}

	FVector constrainedLocation, newAffordanceNormal, newAffordanceTangent;

	if (!GetMetaObjectCreationDeltas(Location, true, constrainedLocation, CurDeltas))
	{
		return false;
	}

	if (!doc->ApplyDeltas(CurDeltas, GetWorld()))
	{
		return false;
	}

	if ((pendingSegment->Point2 - pendingSegment->Point1).IsNearlyZero())
	{
		return true;
	}
	if (pendingSegment)
	{
		pendingSegment->Point1 = constrainedLocation;
		pendingSegment->Point2 = constrainedLocation;
	}

	if (State == NewSegmentPending)
	{
		newAffordanceNormal = (PlaneBaseEnd - PlaneBaseStart).GetSafeNormal();
		FVector axisY;
		UModumateGeometryStatics::FindBasisVectors(newAffordanceTangent, axisY, newAffordanceNormal);
		State = NewPlanePending;
		bExtrudingPlaneFromEdge = false;
		constrainedLocation = PlaneBaseStart;
		pendingSegment->Point1 = constrainedLocation;
	}
	else if (State == NewPlanePending)
	{
		PlaneBaseStart = PendingPlanePoints[3];
		PlaneBaseEnd = PendingPlanePoints[2];
		if (!(PlaneBaseEnd - PlaneBaseStart).IsNearlyZero())
		{
			newAffordanceNormal = (PlaneBaseEnd - PlaneBaseStart).GetSafeNormal();
			newAffordanceTangent = (PendingPlanePoints[2] - PendingPlanePoints[1]).GetSafeNormal();
		}
	}
	Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(constrainedLocation, newAffordanceNormal, newAffordanceTangent);

	return true;
}

bool URectangleTool::UpdatePreview()
{
	CurDeltas.Reset();

	if (!Controller->EMPlayerState->SnappedCursor.Visible)
	{
		return false;
	}

	FVector hitLoc = Controller->EMPlayerState->SnappedCursor.WorldPosition;

	if (State == Neutral || !ensure(GameState && GameState->Document))
	{
		return false;
	}

	auto dimensionActor = DimensionManager->GetDimensionActor(PendingSegmentID);
	ALineActor* pendingSegment = nullptr;
	if (dimensionActor != nullptr)
	{
		pendingSegment = dimensionActor->GetLineActor();
	}

	if (pendingSegment != nullptr)
	{
		if (ConstrainHitPoint(hitLoc))
		{
			FAffordanceLine affordance;
			affordance.Color = FLinearColor::Blue;
			affordance.EndPoint = Controller->EMPlayerState->SnappedCursor.WorldPosition;
			affordance.StartPoint = hitLoc;
			affordance.Interval = 4.0f;
			Controller->EMPlayerState->AffordanceLines.Add(affordance);
		}

		pendingSegment->Point2 = hitLoc;
		pendingSegment->UpdateLineVisuals(false);
	}

	UpdatePendingPlane();

	GameState->Document->StartPreviewing();

	return GetMetaObjectCreationDeltas(hitLoc, false, hitLoc, CurDeltas);
}

bool URectangleTool::FrameUpdate()
{
	Super::FrameUpdate();

	if (UpdatePreview())
	{
		GameState->Document->ApplyPreviewDeltas(CurDeltas, GetWorld());
	}

	return true;
}

bool URectangleTool::EndUse()
{
	State = Neutral;
	bExtrudingPlaneFromEdge = false;

	Controller->EMPlayerState->SnappedCursor.WantsVerticalAffordanceSnap = false;

	return Super::EndUse();
}

bool URectangleTool::AbortUse()
{
	EndUse();
	return Super::AbortUse();
}

bool URectangleTool::PostEndOrAbort()
{
	if (GameState && GameState->Document)
	{
		GameState->Document->ClearPreviewDeltas(GetWorld(), false);
	}

	return Super::PostEndOrAbort();
}

void URectangleTool::UpdatePendingPlane()
{
	bPendingPlaneValid = false;

	if (!InUse)
	{
		return;
	}

	auto pendingSegment = DimensionManager ? DimensionManager->GetDimensionActor(PendingSegmentID)->GetLineActor() : nullptr;
	if (State == NewPlanePending && pendingSegment != nullptr &&
		(FVector::Dist(pendingSegment->Point1, pendingSegment->Point2) >= MinPlaneSize))
	{
		FVector diff = pendingSegment->Point2 - pendingSegment->Point1;
		if (!diff.IsNearlyZero())
		{
			bPendingPlaneValid = true;
			PendingPlanePoints = {
				PlaneBaseStart,
				PlaneBaseEnd,
				PlaneBaseEnd + diff,
				PlaneBaseStart + diff
			};
		}

		if (bPendingPlaneValid)
		{
			bPendingPlaneValid = UModumateGeometryStatics::GetPlaneFromPoints(PendingPlanePoints, PendingPlaneGeom);
		}
	}

	if (!bPendingPlaneValid)
	{
		PendingPlanePoints.Reset();
	}
}

bool URectangleTool::ConstrainHitPoint(FVector &hitPoint)
{
	if (State != EState::NewPlanePending)
	{
		return false;
	}

	if (Controller != nullptr)
	{
		FVector sketchPlanePoint = Controller->EMPlayerState->SnappedCursor.SketchPlaneProject(hitPoint);
		bool returnValue = !sketchPlanePoint.Equals(hitPoint);
		hitPoint = sketchPlanePoint;
		return returnValue;
	}

	return false;
}

float URectangleTool::GetDefaultPlaneHeight() const
{
	return Controller->GetDefaultWallHeightFromDoc();
}
