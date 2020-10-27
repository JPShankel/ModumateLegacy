// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Tools/EditModelSurfaceGraphTool.h"

#include "ModumateCore/ModumateObjectStatics.h"
#include "Objects/ModumateObjectInstance.h"
#include "Objects/SurfaceGraph.h"
#include "ToolsAndAdjustments/Common/ModumateSnappedCursor.h"
#include "UI/DimensionManager.h"
#include "UI/PendingSegmentActor.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "UnrealClasses/LineActor.h"
#include "UnrealClasses/ModumateGameInstance.h"

using namespace Modumate;

USurfaceGraphTool::USurfaceGraphTool(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, HostTarget(nullptr)
	, GraphTarget(nullptr)
	, GraphElementTarget(nullptr)
	, HitHostActor(nullptr)
	, HitLocation(ForceInitToZero)
	, HitNormal(ForceInitToZero)
	, HitFaceIndex(INDEX_NONE)
	, TargetFaceOrigin(ForceInitToZero)
	, TargetFaceNormal(ForceInitToZero)
	, TargetFaceAxisX(ForceInitToZero)
	, TargetFaceAxisY(ForceInitToZero)
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
	if (HostTarget && (HostTarget->ID != 0))
	{
		// Otherwise, start drawing a poly-line on the target surface graph.
		// TODO: use the normal and tangent of the hit cursor, and override the "global axes";
		// this will allow appropriately-colored green and red lines for the surface graph's local X and Y axes,
		// while also allowing tangent affordance(s) for any connected surface graph vertices that are the hit target.
		Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(HitLocation, TargetFaceNormal, TargetFaceAxisX, false, false);
		InUse = true;

		// If there's no graph on the target, then just create an explicit surface graph for the face,
		// before starting any poly-line drawing.
		if (GraphTarget == nullptr)
		{
			int32 newSurfaceGraphID;
			if (!CreateGraphFromFaceTarget(newSurfaceGraphID))
			{
				return false;
			}
			GraphTarget = GameState->Document.GetObjectById(newSurfaceGraphID);
		}
		else
		{
			if (!InitializeSegment())
			{
				return false;
			}
		}
	}

	return true;
}

bool USurfaceGraphTool::EnterNextStage()
{
	// Make sure we have a valid target first
	if (!(HostTarget && (HostTarget->ID != 0) && (HitFaceIndex != INDEX_NONE) && (GraphTarget != nullptr)))
	{
		return false;
	}

	// Make sure we have a valid edge to add to a valid target graph
	const FSnappedCursor& cursor = Controller->EMPlayerState->SnappedCursor;
	FVector projectedHitPosition = cursor.SketchPlaneProject(cursor.WorldPosition);

	auto* dimensionActor = DimensionManager->GetDimensionActor(PendingSegmentID);
	auto* pendingSegment = dimensionActor ? dimensionActor->GetLineActor() : nullptr;
	if (pendingSegment == nullptr)
	{
		HitLocation = projectedHitPosition;
		return InitializeSegment();
	}

	FVector edgeStartPos = cursor.SketchPlaneProject(pendingSegment->Point1);
	FVector edgeEndPos = cursor.SketchPlaneProject(pendingSegment->Point2);
	if (edgeStartPos.Equals(edgeEndPos))
	{
		return false;
	}

	if (!AddEdge(edgeStartPos, edgeEndPos))
	{
		return false;
	}

	// If successful so far, continue from the end point for adding the next line segment
	pendingSegment->Point1 = edgeEndPos;
	Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(edgeEndPos, TargetFaceNormal, TargetFaceAxisX, false, false);

	return true;
}

bool USurfaceGraphTool::EndUse()
{
	return Super::EndUse();
}

bool USurfaceGraphTool::AbortUse()
{
	return EndUse();
}

bool USurfaceGraphTool::FrameUpdate()
{
	const FSnappedCursor &cursor = Controller->EMPlayerState->SnappedCursor;

	auto *moi = GameState->Document.ObjectFromActor(cursor.Actor);
	if ((moi == nullptr) || !cursor.bValid || (cursor.Actor == nullptr))
	{
		return true;
	}

	int32 newHitFaceIndex = UModumateObjectStatics::GetFaceIndexFromTargetHit(moi, cursor.WorldPosition, cursor.HitNormal);

	if (IsInUse())
	{
		FVector projectedHitPosition = cursor.SketchPlaneProject(cursor.WorldPosition);

		auto *dimensionActor = DimensionManager->GetDimensionActor(PendingSegmentID);
		auto *pendingSegment = dimensionActor ? dimensionActor->GetLineActor() : nullptr;

		if (pendingSegment)
		{
			pendingSegment->Point2 = projectedHitPosition;
		}
	}
	else
	{
		ResetTarget();

		// If we're targeting a face, then figure out whether we're targeting an object onto which we can apply a surface graph,
		// or if we're targeting an existing surface graph that we want to edit and snap against.
		HitFaceIndex = newHitFaceIndex;

		EGraphObjectType hitObjectGraphType = UModumateTypeStatics::Graph2DObjectTypeFromObjectType(moi->GetObjectType());
		if (hitObjectGraphType != EGraphObjectType::None)
		{
			FModumateObjectInstance *surfaceGraphObj = moi->GetParentObject();
			FModumateObjectInstance *surfaceGraphHost = surfaceGraphObj ? surfaceGraphObj->GetParentObject() : nullptr;
			if (!ensure(surfaceGraphObj && (surfaceGraphObj->GetObjectType() == EObjectType::OTSurfaceGraph) && surfaceGraphHost))
			{
				return true;
			}

			HostTarget = surfaceGraphHost;
			HitHostActor = surfaceGraphHost->GetActor();
			HitLocation = cursor.WorldPosition;
			HitNormal = cursor.HitNormal;
			GraphTarget = surfaceGraphObj;
			GraphElementTarget = moi;

			HitFaceIndex = UModumateObjectStatics::GetFaceIndexFromTargetHit(surfaceGraphHost, cursor.WorldPosition, cursor.HitNormal);
		}
		else if (HitFaceIndex != INDEX_NONE)
		{
			HostTarget = moi;
			HitHostActor = cursor.Actor;
			HitLocation = cursor.WorldPosition;
			HitNormal = cursor.HitNormal;

			// See if there's already a surface graph mounted to the same target MOI, at the same target face as the current target.
			for (int32 childID : HostTarget->GetChildIDs())
			{
				FModumateObjectInstance *childObj = GameState->Document.GetObjectById(childID);
				int32 childFaceIdx = UModumateObjectStatics::GetParentFaceIndex(childObj);
				if (childObj && (childObj->GetObjectType() == EObjectType::OTSurfaceGraph) && (childFaceIdx == HitFaceIndex))
				{
					GraphTarget = childObj;
					break;
				}
			}
		}

		if (HostTarget && UModumateObjectStatics::GetGeometryFromFaceIndex(HostTarget, HitFaceIndex,
			HostCornerPositions, TargetFaceNormal, TargetFaceAxisX, TargetFaceAxisY))
		{
			TargetFaceOrigin = HostCornerPositions[0];
			if (GraphTarget == nullptr)
			{
				int32 numCorners = HostCornerPositions.Num();
				for (int32 curCornerIdx = 0; curCornerIdx < numCorners; ++curCornerIdx)
				{
					int32 nextCornerIdx = (curCornerIdx + 1) % numCorners;

					Controller->EMPlayerState->AffordanceLines.Add(FAffordanceLine(HostCornerPositions[curCornerIdx], HostCornerPositions[nextCornerIdx],
						AffordanceLineColor, AffordanceLineInterval, AffordanceLineThickness)
					);
				}
			}
		}
	}

	return true;
}

bool USurfaceGraphTool::HandleInputNumber(double n)
{
	auto *dimensionActor = DimensionManager->GetDimensionActor(PendingSegmentID);
	auto *pendingSegment = dimensionActor ? dimensionActor->GetLineActor() : nullptr;
	if (!pendingSegment)
	{
		return false;
	}

	FVector direction = pendingSegment->Point2 - pendingSegment->Point1;
	if (direction.IsNearlyZero())
	{
		return false;
	}

	direction.Normalize();

	FVector newEndPos = pendingSegment->Point1 + direction * n;

	if (!AddEdge(pendingSegment->Point1, newEndPos))
	{
		return false;
	}

	// If successful so far, continue from the end point for adding the next line segment
	pendingSegment->Point1 = newEndPos;
	pendingSegment->Point2 = newEndPos;
	Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(newEndPos, TargetFaceNormal, TargetFaceAxisX, false, false);

	return true;
}

TArray<EEditViewModes> USurfaceGraphTool::GetRequiredEditModes() const
{
	return { EEditViewModes::SurfaceGraphs };
}

bool USurfaceGraphTool::InitializeSegment()
{
	InitializeDimension();

	auto dimensionActor = DimensionManager->GetDimensionActor(PendingSegmentID);
	if (!dimensionActor)
	{
		return false;
	}

	auto pendingSegment = dimensionActor->GetLineActor();
	if (!pendingSegment)
	{
		return false;
	}

	// TODO: this could also likely be generalized to be in InitializeDimension, but would have to be 
	// done for all of the tools
	pendingSegment->Point1 = HitLocation;
	pendingSegment->Point2 = HitLocation;
	pendingSegment->Color = FColor::Black;
	pendingSegment->Thickness = 3;

	return true;
}

bool USurfaceGraphTool::CreateGraphFromFaceTarget(int32& OutSurfaceGraphID)
{
	OutSurfaceGraphID = MOD_ID_NONE;

	if (!(HostTarget && HitHostActor && (HitFaceIndex != INDEX_NONE)))
	{
		return false;
	}

	// If we're targeting a surface graph object, it has to be empty
	int32 nextID = GameState->Document.GetNextAvailableID();
	TArray<FDeltaPtr> deltas;

	if (GraphTarget)
	{
		OutSurfaceGraphID = GraphTarget->ID;
		TargetSurfaceGraph = GameState->Document.FindSurfaceGraph(OutSurfaceGraphID);
		if (!TargetSurfaceGraph.IsValid() || !TargetSurfaceGraph->IsEmpty())
		{
			return false;
		}
	}
	else
	{
		OutSurfaceGraphID = nextID++;
		TargetSurfaceGraph = MakeShared<FGraph2D>(OutSurfaceGraphID);
	}

	// Create the delta for adding the surface graph object, if it didn't already exist
	if (GraphTarget == nullptr)
	{
		FMOISurfaceGraphData surfaceCustomData;
		surfaceCustomData.ParentFaceIndex = HitFaceIndex;

		FMOIStateData surfaceStateData;
		surfaceStateData.ID = TargetSurfaceGraph->GetID();
		surfaceStateData.ObjectType = EObjectType::OTSurfaceGraph;
		surfaceStateData.ParentID = HostTarget->ID;
		surfaceStateData.CustomData.SaveStructData(surfaceCustomData);

		auto surfaceObjectDelta = MakeShared<FMOIDelta>();
		surfaceObjectDelta->AddCreateDestroyState(surfaceStateData, EMOIDeltaType::Create);

		deltas.Add(surfaceObjectDelta);
		deltas.Add(MakeShared<FGraph2DDelta>(TargetSurfaceGraph->GetID(), EGraph2DDeltaType::Add));
	}

	// Make sure we have valid geometry from the target
	int32 numPerimeterPoints = HostCornerPositions.Num();
	if ((numPerimeterPoints < 3) || !TargetFaceNormal.IsNormalized() || !TargetFaceAxisX.IsNormalized() || !TargetFaceAxisY.IsNormalized())
	{
		return false;
	}

	// Project all of the polygons to add to the target graph
	TArray<TArray<FVector2D>> graphPolygonsToAdd;

	TArray<FVector2D> &perimeterPolygon = graphPolygonsToAdd.AddDefaulted_GetRef();
	Algo::Transform(HostCornerPositions, perimeterPolygon, [this](const FVector &WorldPoint) {
		return UModumateGeometryStatics::ProjectPoint2D(WorldPoint, TargetFaceAxisX, TargetFaceAxisY, TargetFaceOrigin);
	});

	// Project the holes that the target has into graph polygons, if any
	const auto &volumeGraph = GameState->Document.GetVolumeGraph();
	const auto *hostParentFace = volumeGraph.FindFace(HostTarget->GetParentID());

	// this only counts faces that are contained, not holes without faces (CachedIslands)
	if (hostParentFace->ContainedFaceIDs.Num() != hostParentFace->CachedHoles.Num())
	{
		return false;
	}

	if (hostParentFace)
	{
		for (int32 containedFaceID : hostParentFace->ContainedFaceIDs)
		{
			const auto *containedFace = volumeGraph.FindFace(containedFaceID);
			TArray<FVector2D> &holePolygon = graphPolygonsToAdd.AddDefaulted_GetRef();
			Algo::Transform(containedFace->CachedPositions, holePolygon, [this](const FVector &WorldPoint) {
				return UModumateGeometryStatics::ProjectPoint2D(WorldPoint, TargetFaceAxisX, TargetFaceAxisY, TargetFaceOrigin);
			});
		}
	}

	TArray<FGraph2DDelta> fillGraphDeltas;
	if (!TargetSurfaceGraph->PopulateFromPolygons(fillGraphDeltas, nextID, graphPolygonsToAdd, true))
	{
		return false;
	}

	// We no longer need neither an existing target surface graph, or a temporary one used to create deltas.
	TargetSurfaceGraph.Reset();

	for (FGraph2DDelta& graphDelta : fillGraphDeltas)
	{
		deltas.Add(MakeShared<FGraph2DDelta>(graphDelta));
	}
	return GameState->Document.ApplyDeltas(deltas, GetWorld());
}

bool USurfaceGraphTool::AddEdge(FVector StartPos, FVector EndPos)
{
	auto targetSurfaceGraph = GameState->Document.FindSurfaceGraph(GraphTarget->ID);
	if (!targetSurfaceGraph.IsValid() || targetSurfaceGraph->IsEmpty())
	{
		return false;
	}

	FVector2D startPos = UModumateGeometryStatics::ProjectPoint2D(StartPos, TargetFaceAxisX, TargetFaceAxisY, TargetFaceOrigin);
	FVector2D endPos = UModumateGeometryStatics::ProjectPoint2D(EndPos, TargetFaceAxisX, TargetFaceAxisY, TargetFaceOrigin);

	// Try to add the edge to the graph, and apply the delta(s) to the document
	TArray<FGraph2DDelta> addEdgeDeltas;
	int32 nextID = GameState->Document.GetNextAvailableID();
	if (!targetSurfaceGraph->AddEdge(addEdgeDeltas, nextID, startPos, endPos))
	{
		return false;
	}

	if (addEdgeDeltas.Num() > 0)
	{
		TArray<FDeltaPtr> deltaPtrs;
		Algo::Transform(addEdgeDeltas, deltaPtrs, [](const FGraph2DDelta &graphDelta) {
			return MakeShared<FGraph2DDelta>(graphDelta);
		});

		if (!GameState->Document.ApplyDeltas(deltaPtrs, GetWorld()))
		{
			return false;
		}
	}

	return true;
}

void USurfaceGraphTool::ResetTarget()
{
	HostTarget = nullptr;
	GraphTarget = nullptr;
	GraphElementTarget = nullptr;
	HitHostActor = nullptr;
	HitLocation = HitNormal = FVector::ZeroVector;
	HitFaceIndex = INDEX_NONE;
}
