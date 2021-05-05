// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Tools/EditModelSurfaceGraphTool.h"

#include "ModumateCore/ModumateObjectStatics.h"
#include "ModumateCore/ModumateTargetingStatics.h"
#include "Objects/ModumateObjectInstance.h"
#include "Objects/SurfaceGraph.h"
#include "ToolsAndAdjustments/Common/ModumateSnappedCursor.h"
#include "UI/DimensionManager.h"
#include "UI/PendingSegmentActor.h"
#include "UnrealClasses/DimensionWidget.h"
#include "UnrealClasses/EditModelGameState.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "UnrealClasses/LineActor.h"
#include "UnrealClasses/ModumateGameInstance.h"

using namespace Modumate;

USurfaceGraphTool::USurfaceGraphTool()
	: Super()
	, HitGraphHostMOI(nullptr)
	, HitGraphMOI(nullptr)
	, HitSurfaceGraph(nullptr)
	, HitGraphElementMOI(nullptr)
	, HitLocation(ForceInitToZero)
	, HitFaceIndex(INDEX_NONE)
	, TargetGraphMOI(nullptr)
	, TargetSurfaceGraph(nullptr)
	, OriginalMouseMode(EMouseMode::Object)
{
}

bool USurfaceGraphTool::Activate()
{
	if (!Super::Activate())
	{
		return false;
	}

	OriginalMouseMode = Controller->EMPlayerState->SnappedCursor.MouseMode;
	Controller->EMPlayerState->SnappedCursor.MouseMode = EMouseMode::Location;

	ResetTarget();

	return true;
}

bool USurfaceGraphTool::Deactivate()
{
	if (Controller)
	{
		Controller->EMPlayerState->SnappedCursor.MouseMode = OriginalMouseMode;
	}

	return Super::Deactivate();
}

bool USurfaceGraphTool::BeginUse()
{
	// If there's no graph on the target, then just create an explicit surface graph for the face,
	// before starting any poly-line drawing.
	if (HitGraphHostMOI && (HitFaceIndex != INDEX_NONE) && (HitGraphMOI == nullptr))
	{
		int32 newSurfaceGraphID, newRootInteriorPolyID;
		TArray<FDeltaPtr> deltas;
		int32 nextID = GameState->Document->GetNextAvailableID();
		if (CreateGraphFromFaceTarget(nextID, newSurfaceGraphID, newRootInteriorPolyID, deltas) &&
			GameState->Document->ApplyDeltas(deltas, GetWorld()))
		{
			HitGraphMOI = GameState->Document->GetObjectById(newSurfaceGraphID);

			return true;
		}

		return false;
	}

	bool bValidStart = false;
	if (HitAdjacentGraphMOIs.Num() > 0)
	{
		TargetGraphMOI = nullptr;
		TargetSurfaceGraph.Reset();
		TargetAdjacentGraphMOIs = HitAdjacentGraphMOIs;
		bValidStart = true;
	}
	else if (HitGraphMOI && HitSurfaceGraph)
	{
		TargetGraphMOI = HitGraphMOI;
		TargetSurfaceGraph = HitSurfaceGraph;
		TargetAdjacentGraphMOIs.Reset();
		bValidStart = true;
	}

	if (bValidStart && InitializeSegment())
	{
		InUse = true;
		Controller->UpdateMouseTraceParams();
		return true;
	}

	return false;
}

bool USurfaceGraphTool::EnterNextStage()
{
	const FSnappedCursor& cursor = Controller->EMPlayerState->SnappedCursor;

	if (PendingSegment == nullptr)
	{
		return false;
	}

	PendingSegment->Point2 = cursor.SketchPlaneProject(cursor.WorldPosition);
	return CompleteSegment();
}

bool USurfaceGraphTool::EndUse()
{
	TargetGraphMOI = nullptr;
	TargetSurfaceGraph.Reset();
	TargetAdjacentGraphMOIs.Reset();

	Controller->UpdateMouseTraceParams();

	return Super::EndUse();
}

bool USurfaceGraphTool::AbortUse()
{
	return EndUse();
}

bool USurfaceGraphTool::FrameUpdate()
{
	FSnappedCursor &cursor = Controller->EMPlayerState->SnappedCursor;
	auto *cursorHitMOI = GameState->Document->ObjectFromActor(cursor.Actor);

	bool bValidTarget = UpdateTarget(cursorHitMOI, cursor.WorldPosition, cursor.HitNormal);

	if (IsInUse())
	{
		if (bValidTarget)
		{
			// TODO: don't set a different snap affordance every frame, use the results of the segment start hit to get multiple affordances
			FQuat hitFaceRotation = HitFaceOrigin.GetRotation();
			if (hitFaceRotation.IsNormalized() && (!cursor.HasAffordanceSet() || (HitAdjacentGraphMOIs.Num() <= 1)))
			{
				cursor.SetAffordanceFrame(PendingSegment->Point1, hitFaceRotation.GetAxisZ(), hitFaceRotation.GetAxisX(), false, false);
			}

			PendingSegment->Point2 = cursor.SketchPlaneProject(HitLocation);
		}
		else
		{
			PendingSegment->Point2 = PendingSegment->Point1;
		}

		PendingSegment->SetVisibilityInApp(bValidTarget);
		PendingDimensionActor->DimensionText->SetVisibility(bValidTarget ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
	}
	else
	{
		if (bValidTarget && (HitGraphMOI == nullptr))
		{
			int32 numCorners = HitFacePoints.Num();
			for (int32 curCornerIdx = 0; curCornerIdx < numCorners; ++curCornerIdx)
			{
				int32 nextCornerIdx = (curCornerIdx + 1) % numCorners;

				Controller->EMPlayerState->AffordanceLines.Add(FAffordanceLine(HitFacePoints[curCornerIdx], HitFacePoints[nextCornerIdx],
					AffordanceLineColor, AffordanceLineInterval, AffordanceLineThickness)
				);
			}
		}
	}

	return true;
}

bool USurfaceGraphTool::HandleInputNumber(double n)
{
	if (PendingSegment == nullptr)
	{
		return false;
	}

	FVector direction = PendingSegment->Point2 - PendingSegment->Point1;
	if (direction.IsNearlyZero())
	{
		return false;
	}

	direction.Normalize();

	FVector endPoint = PendingSegment->Point1 + direction * n;
	PendingSegment->Point2 = endPoint;

	const AModumateObjectInstance* projectedHitGraphMOI = TargetGraphMOI;

	if (projectedHitGraphMOI == nullptr)
	{
		FMouseWorldHitType projectedHitResult = Controller->GetSimulatedStructureHit(endPoint);
		projectedHitGraphMOI = (projectedHitResult.Valid && projectedHitResult.Actor.IsValid()) ? GameState->Document->ObjectFromActor(projectedHitResult.Actor.Get()) : nullptr;
	}

	if (!UpdateTarget(projectedHitGraphMOI, PendingSegment->Point2, FVector::ZeroVector))
	{
		return false;
	}

	return CompleteSegment();
}

bool USurfaceGraphTool::UpdateTarget(const AModumateObjectInstance* HitObject, const FVector& Location, const FVector& Normal)
{
	ResetTarget();

	HitLocation = Location;

	if (HitObject)
	{
		HitSurfaceGraph = GameState->Document->FindSurfaceGraph(HitObject->ID);
		if (!HitSurfaceGraph.IsValid())
		{
			HitSurfaceGraph = GameState->Document->FindSurfaceGraphByObjID(HitObject->ID);
		}

		if (HitSurfaceGraph.IsValid())
		{
			if (HitSurfaceGraph->ContainsObject(HitObject->ID))
			{
				HitGraphElementMOI = HitObject;
			}
			else if (HitSurfaceGraph->ContainsObject(HitObject->GetParentID()))
			{
				HitGraphElementMOI = HitObject->GetParentObject();
			}

			int32 hitSurfaceGraphID = HitSurfaceGraph->GetID();
			HitGraphMOI = GameState->Document->GetObjectById(hitSurfaceGraphID);
			HitGraphHostMOI = GameState->Document->GetObjectById(HitGraphMOI ? HitGraphMOI->GetParentID() : MOD_ID_NONE);
			HitFaceIndex = UModumateObjectStatics::GetParentFaceIndex(HitGraphMOI);
		}
		else
		{
			HitFaceIndex = UModumateTargetingStatics::GetFaceIndexFromTargetHit(HitObject, Location, Normal);
			if (HitFaceIndex != INDEX_NONE)
			{
				HitGraphHostMOI = HitObject;
				for (auto hostChild : HitGraphHostMOI->GetChildObjects())
				{
					int32 childFaceIndex = UModumateObjectStatics::GetParentFaceIndex(hostChild);
					if ((childFaceIndex == HitFaceIndex) && (hostChild->GetObjectType() == EObjectType::OTSurfaceGraph))
					{
						HitGraphMOI = hostChild;
						break;
					}
				}
			}
		}

		UModumateTargetingStatics::GetConnectedSurfaceGraphs(HitObject, Location, HitAdjacentGraphMOIs);

		// If we are only adjacent to one surface graph, but didn't find it from the original cursor hit result,
		// then pretend like we hit it directly so that subsequent targeting is unambiguous.
		if (HitAdjacentGraphMOIs.Num() == 1)
		{
			HitGraphMOI = HitAdjacentGraphMOIs[0];
			HitAdjacentGraphMOIs.Reset();
		}
	}

	if (IsInUse())
	{
		if ((PendingDimensionActor == nullptr) || (PendingSegment == nullptr))
		{
			return false;
		}

		bool bValidSegmentTarget = false;

		if (TargetAdjacentGraphMOIs.Num() > 0)
		{
			if (HitAdjacentGraphMOIs.Num() > 0)
			{
				for (auto hitAdjacentGraphMOI : HitAdjacentGraphMOIs)
				{
					if (TargetAdjacentGraphMOIs.Contains(hitAdjacentGraphMOI))
					{
						HitGraphMOI = hitAdjacentGraphMOI;
						bValidSegmentTarget = true;
						break;
					}
				}
			}
			else if (HitGraphMOI)
			{
				bValidSegmentTarget = TargetAdjacentGraphMOIs.Contains(HitGraphMOI);
			}
		}
		else if (TargetGraphMOI && TargetSurfaceGraph.IsValid())
		{
			if (HitGraphMOI == TargetGraphMOI)
			{
				bValidSegmentTarget = true;
			}
			else if (HitAdjacentGraphMOIs.Contains(TargetGraphMOI))
			{
				HitGraphMOI = TargetGraphMOI;
				bValidSegmentTarget = true;
			}
		}

		if (!bValidSegmentTarget)
		{
			ResetTarget();
			return false;
		}
	}

	if (HitGraphMOI)
	{
		HitFaceIndex = UModumateObjectStatics::GetParentFaceIndex(HitGraphMOI);
		HitGraphHostMOI = GameState->Document->GetObjectById(HitGraphMOI->GetParentID());
		HitSurfaceGraph = GameState->Document->FindSurfaceGraph(HitGraphMOI->ID);
	}

	return HitGraphHostMOI && UModumateObjectStatics::GetGeometryFromFaceIndex(HitGraphHostMOI, HitFaceIndex, HitFacePoints, HitFaceOrigin);
}

bool USurfaceGraphTool::InitializeSegment()
{
	InitializeDimension();

	// TODO: this could also likely be generalized to be in InitializeDimension, but would have to be 
	// done for all of the tools
	PendingSegment->Point1 = HitLocation;
	PendingSegment->Point2 = HitLocation;
	PendingSegment->Color = FColor(0xBC, 0xBC, 0xBC);
	PendingSegment->Thickness = 3;

	return true;
}

bool USurfaceGraphTool::CompleteSegment()
{
	if (PendingSegment == nullptr)
	{
		return false;
	}

	// Make sure we have a valid edge to add - if not, then try to start over from the current point
	FVector segmentDelta = PendingSegment->Point2 - PendingSegment->Point1;
	if (!(HitGraphHostMOI && (HitGraphHostMOI->ID != MOD_ID_NONE) &&
		HitGraphMOI && (HitFaceIndex != INDEX_NONE) && !segmentDelta.IsNearlyZero()))
	{
		AbortUse();
		return BeginUse();
	}

	TargetGraphMOI = HitGraphMOI;
	TargetSurfaceGraph = HitSurfaceGraph;
	TargetAdjacentGraphMOIs = HitAdjacentGraphMOIs;

	if (!AddEdge(PendingSegment->Point1, PendingSegment->Point2))
	{
		return false;
	}

	// If successful so far, continue from the end point for adding the next line segment
	PendingSegment->Point1 = PendingSegment->Point2;

	return true;
}

bool USurfaceGraphTool::CreateGraphFromFaceTarget(int32& NextID, int32& OutSurfaceGraphID, int32& OutRootInteriorPolyID, TArray<FDeltaPtr>& OutDeltas)
{
	OutSurfaceGraphID = MOD_ID_NONE;
	OutRootInteriorPolyID = MOD_ID_NONE;

	if (!(HitGraphHostMOI && (HitFaceIndex != INDEX_NONE)))
	{
		return false;
	}

	// If we're targeting a surface graph object, it has to be empty
	if (HitGraphMOI)
	{
		OutSurfaceGraphID = HitGraphMOI->ID;
		HitSurfaceGraph = GameState->Document->FindSurfaceGraph(OutSurfaceGraphID);
		if (!HitSurfaceGraph.IsValid() || !HitSurfaceGraph->IsEmpty())
		{
			return false;
		}
	}
	else
	{
		OutSurfaceGraphID = NextID++;
		HitSurfaceGraph = MakeShared<FGraph2D>(OutSurfaceGraphID, THRESH_POINTS_ARE_NEAR);
	}

	// Create the delta for adding the surface graph object, if it didn't already exist
	if (HitGraphMOI == nullptr)
	{
		FMOISurfaceGraphData surfaceCustomData;
		surfaceCustomData.ParentFaceIndex = HitFaceIndex;

		FMOIStateData surfaceStateData;
		surfaceStateData.ID = HitSurfaceGraph->GetID();
		surfaceStateData.ObjectType = EObjectType::OTSurfaceGraph;
		surfaceStateData.ParentID = HitGraphHostMOI->ID;
		surfaceStateData.CustomData.SaveStructData(surfaceCustomData);

		auto surfaceObjectDelta = MakeShared<FMOIDelta>();
		surfaceObjectDelta->AddCreateDestroyState(surfaceStateData, EMOIDeltaType::Create);

		OutDeltas.Add(surfaceObjectDelta);
		OutDeltas.Add(MakeShared<FGraph2DDelta>(HitSurfaceGraph->GetID(), EGraph2DDeltaType::Add, THRESH_POINTS_ARE_NEAR));
	}

	// Make sure we have valid geometry from the target
	int32 numPerimeterPoints = HitFacePoints.Num();
	if ((numPerimeterPoints < 3) || !HitFaceOrigin.IsRotationNormalized())
	{
		return false;
	}

	// Project all of the polygons to add to the target graph
	TMap<int32, TArray<FVector2D>> graphPolygonsToAdd;
	TMap<int32, TArray<int32>> graphFaceToVertices;

	TArray<FVector2D> perimeterPolygon;
	Algo::Transform(HitFacePoints, perimeterPolygon, [this](const FVector &WorldPoint) {
		return UModumateGeometryStatics::ProjectPoint2DTransform(WorldPoint, HitFaceOrigin);
	});
	graphPolygonsToAdd.Add(MOD_ID_NONE, perimeterPolygon);

	// Project the holes that the target has into graph polygons, if any
	const auto &volumeGraph = GameState->Document->GetVolumeGraph();
	const auto *hostParentFace = volumeGraph.FindFace(HitGraphHostMOI->GetParentID());

	// this only counts faces that are contained, not holes without faces (CachedIslands)
	if (hostParentFace->ContainedFaceIDs.Num() != hostParentFace->CachedHoles.Num())
	{
		return false;
	}

	TArray<FGraph2DDelta> fillGraphDeltas;
	TMap<int32, int32> outFaceToPoly, outGraphToSurfaceVertices;
	if (!HitSurfaceGraph->PopulateFromPolygons(fillGraphDeltas, NextID, graphPolygonsToAdd, graphFaceToVertices, outFaceToPoly, outGraphToSurfaceVertices, true, OutRootInteriorPolyID))
	{
		return false;
	}

	// We no longer need neither an existing target surface graph, or a temporary one used to create deltas.
	HitSurfaceGraph.Reset();

	GameState->Document->FinalizeGraph2DDeltas(fillGraphDeltas, NextID, OutDeltas);

	return (OutDeltas.Num() > 0);
}

bool USurfaceGraphTool::AddEdge(FVector StartPos, FVector EndPos)
{
	if (!TargetSurfaceGraph.IsValid() || TargetSurfaceGraph->IsEmpty())
	{
		return false;
	}

	FVector2D startPos = UModumateGeometryStatics::ProjectPoint2DTransform(StartPos, HitFaceOrigin);
	FVector2D endPos = UModumateGeometryStatics::ProjectPoint2DTransform(EndPos, HitFaceOrigin);

	// Try to add the edge to the graph, and apply the delta(s) to the document
	TArray<FGraph2DDelta> addEdgeDeltas;
	int32 nextID = GameState->Document->GetNextAvailableID();
	if (!TargetSurfaceGraph->AddEdge(addEdgeDeltas, nextID, startPos, endPos))
	{
		return false;
	}

	if (addEdgeDeltas.Num() > 0)
	{
		TArray<FDeltaPtr> deltaPtrs;
		GameState->Document->FinalizeGraph2DDeltas(addEdgeDeltas, nextID, deltaPtrs);

		if (!GameState->Document->ApplyDeltas(deltaPtrs, GetWorld()))
		{
			return false;
		}
	}

	return true;
}

void USurfaceGraphTool::ResetTarget()
{
	HitGraphHostMOI = nullptr;
	HitGraphMOI = nullptr;
	HitSurfaceGraph.Reset();
	HitGraphElementMOI = nullptr;
	HitAdjacentGraphMOIs.Reset();
	HitLocation = FVector::ZeroVector;
	HitFaceIndex = INDEX_NONE;
	HitFaceOrigin.SetIdentity();
	HitFacePoints.Reset();
}
