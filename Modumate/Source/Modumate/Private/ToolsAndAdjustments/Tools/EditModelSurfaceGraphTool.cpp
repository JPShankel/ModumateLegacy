// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Tools/EditModelSurfaceGraphTool.h"

#include "DocumentManagement/ModumateObjectInstance.h"
#include "ModumateCore/ModumateObjectStatics.h"
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
	, PendingSegmentID(MOD_ID_NONE)
{
}

bool USurfaceGraphTool::Activate()
{
	if (!Super::Activate())
	{
		return false;
	}

	UWorld *world = GetWorld();
	GameState = world ? world->GetGameState<AEditModelGameState_CPP>() : nullptr;
	UModumateGameInstance *gameInstance = world ? world->GetGameInstance<UModumateGameInstance>() : nullptr;
	DimensionManager = gameInstance ? gameInstance->DimensionManager : nullptr;

	if (!(GameState && DimensionManager))
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
		// If there's no graph on the target, then just create an explicit surface graph for the face,
		// before starting any poly-line drawing.
		if (GraphTarget == nullptr)
		{
			TArray<TSharedPtr<FDelta>> deltas;
			if (CreateGraphFromFaceTarget(deltas))
			{
				GameState->Document.ApplyDeltas(deltas, GetWorld());
			}

			return true;
		}

		// Otherwise, start drawing a poly-line on the target surface graph.
		Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(HitLocation, TargetFaceNormal, TargetFaceAxisX, false, false);
		InUse = true;

		auto dimensionActor = DimensionManager->AddDimensionActor(APendingSegmentActor::StaticClass());
		PendingSegmentID = dimensionActor->ID;

		auto pendingSegment = dimensionActor->GetLineActor();
		pendingSegment->Point1 = HitLocation;
		pendingSegment->Point2 = HitLocation;
		pendingSegment->Color = FColor::White;
		pendingSegment->Thickness = 2;
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
	const FSnappedCursor &cursor = Controller->EMPlayerState->SnappedCursor;
	FVector projectedHitPosition = cursor.SketchPlaneProject(cursor.WorldPosition);

	auto *dimensionActor = DimensionManager->GetDimensionActor(PendingSegmentID);
	auto *pendingSegment = dimensionActor ? dimensionActor->GetLineActor() : nullptr;
	if (pendingSegment == nullptr)
	{
		return false;
	}

	FVector edgeStartPos = cursor.SketchPlaneProject(pendingSegment->Point1);
	FVector edgeEndPos = cursor.SketchPlaneProject(pendingSegment->Point2);
	if (edgeStartPos.Equals(edgeEndPos))
	{
		return false;
	}

	FGraph2D *targetSurfaceGraph = GameState->Document.FindSurfaceGraph(GraphTarget->ID);
	if ((targetSurfaceGraph == nullptr) || targetSurfaceGraph->IsEmpty())
	{
		return false;
	}

	FVector2D startPos = UModumateGeometryStatics::ProjectPoint2D(edgeStartPos, TargetFaceAxisX, TargetFaceAxisY, TargetFaceOrigin);
	FVector2D endPos = UModumateGeometryStatics::ProjectPoint2D(edgeEndPos, TargetFaceAxisX, TargetFaceAxisY, TargetFaceOrigin);

	// Try to add the edge to the graph, and apply the delta(s) to the document
	TArray<FGraph2DDelta> addEdgeDeltas;
	int32 nextID = GameState->Document.GetNextAvailableID();
	if (!targetSurfaceGraph->AddEdge(addEdgeDeltas, nextID, startPos, endPos))
	{
		return false;
	}

	if (addEdgeDeltas.Num() > 0)
	{
		TArray<TSharedPtr<FDelta>> deltaPtrs;
		Algo::Transform(addEdgeDeltas, deltaPtrs, [](const FGraph2DDelta &graphDelta) {
			return MakeShareable(new FGraph2DDelta{ graphDelta });
		});

		if (!GameState->Document.ApplyDeltas(deltaPtrs, GetWorld()))
		{
			return false;
		}
	}

	// If successful so far, continue from the end point for adding the next line segment
	pendingSegment->Point1 = edgeEndPos;
	Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(edgeEndPos, TargetFaceNormal, TargetFaceAxisX, false, false);

	return true;
}

bool USurfaceGraphTool::EndUse()
{
	if (DimensionManager)
	{
		DimensionManager->ReleaseDimensionActor(PendingSegmentID);
	}

	PendingSegmentID = MOD_ID_NONE;

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

bool USurfaceGraphTool::CreateGraphFromFaceTarget(TArray<TSharedPtr<FDelta>> &OutDeltas)
{
	int32 originalNumDeltas = OutDeltas.Num();

	if (!(HostTarget && HitHostActor && (HitFaceIndex != INDEX_NONE)))
	{
		return false;
	}

	// If we're targeting a surface graph object, it has to be empty
	int32 surfaceGraphID = MOD_ID_NONE;
	EGraph2DDeltaType graphDeltaType = EGraph2DDeltaType::Add;
	int32 nextGraphObjID = GameState->Document.GetNextAvailableID();

	if (GraphTarget)
	{
		const FGraph2D *targetSurfaceGraph = GameState->Document.FindSurfaceGraph(GraphTarget->ID);
		if ((targetSurfaceGraph == nullptr) || !targetSurfaceGraph->IsEmpty())
		{
			return false;
		}

		surfaceGraphID = GraphTarget->ID;
		graphDeltaType = EGraph2DDeltaType::Edit;
	}
	else
	{
		surfaceGraphID = nextGraphObjID++;
	}

	// Create the delta for adding the surface graph object, if it didn't already exist
	if (GraphTarget == nullptr)
	{
		FMOIStateData surfaceObjectData;
		surfaceObjectData.StateType = EMOIDeltaType::Create;
		surfaceObjectData.ObjectType = EObjectType::OTSurfaceGraph;
		surfaceObjectData.ObjectAssemblyKey = NAME_None;
		surfaceObjectData.ParentID = HostTarget->ID;
		surfaceObjectData.ControlIndices = { HitFaceIndex };
		surfaceObjectData.ObjectID = surfaceGraphID;

		TSharedPtr<FMOIDelta> addObjectDelta = MakeShareable(new FMOIDelta({ surfaceObjectData }));
		OutDeltas.Add(addObjectDelta);
	}

	// Make sure we have valid geometry from the target
	int32 numPerimeterPoints = HostCornerPositions.Num();
	if ((numPerimeterPoints < 3) || !TargetFaceNormal.IsNormalized() || !TargetFaceAxisX.IsNormalized() || !TargetFaceAxisY.IsNormalized())
	{
		return false;
	}

	TSharedPtr<FGraph2DDelta> fillGraphDelta = MakeShareable(new FGraph2DDelta(surfaceGraphID, graphDeltaType));

	// Project all of the polygons to add to the target graph
	TArray<TArray<FVector2D>> graphPolygonsToAdd;
	TArray<int32> graphPolygonHostIDs;

	TArray<FVector2D> &perimeterPolygon = graphPolygonsToAdd.AddDefaulted_GetRef();
	Algo::Transform(HostCornerPositions, perimeterPolygon, [this](const FVector &WorldPoint) {
		return UModumateGeometryStatics::ProjectPoint2D(WorldPoint, TargetFaceAxisX, TargetFaceAxisY, TargetFaceOrigin);
	});

	// Project the holes that the target has into graph polygons, if any
	const auto &volumeGraph = GameState->Document.GetVolumeGraph();
	const auto *hostParentFace = volumeGraph.FindFace(HostTarget->GetParentID());
	graphPolygonHostIDs.Add(hostParentFace->ID);

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
			graphPolygonHostIDs.Add(containedFace->ID);
		}
	}

	// Populate the target graph with the input polygons
	TArray<int32> polygonVertexIDs;
	for (int32 polygonIdx = 0; polygonIdx < graphPolygonsToAdd.Num(); polygonIdx++)
	{
		const TArray<FVector2D> &graphPolygonToAdd = graphPolygonsToAdd[polygonIdx];
		// Populate the target graph with the polygon vertices
		polygonVertexIDs.Reset();
		for (const FVector2D &vertex : graphPolygonToAdd)
		{
			polygonVertexIDs.Add(nextGraphObjID);
			fillGraphDelta->AddNewVertex(vertex, nextGraphObjID);
		}

		// Populate the target graph with the polygon edges
		int32 numPolygonVerts = graphPolygonToAdd.Num();
		for (int32 polyPointIdxA = 0; polyPointIdxA < numPolygonVerts; ++polyPointIdxA)
		{
			int32 polyPointIdxB = (polyPointIdxA + 1) % numPolygonVerts;
			fillGraphDelta->AddNewEdge(FGraphVertexPair(polygonVertexIDs[polyPointIdxA], polygonVertexIDs[polyPointIdxB]), nextGraphObjID);
		}

		// Populate the target graph with the polygon itself
		fillGraphDelta->AddNewPolygon(polygonVertexIDs, nextGraphObjID, true);

		// Populate the bounds formed by the polygon
		if (polygonIdx == 0)
		{
			fillGraphDelta->BoundsUpdates.Value.OuterBounds = TPair<int32, TArray<int32>>(graphPolygonHostIDs[polygonIdx], polygonVertexIDs);
		}
		else
		{
			fillGraphDelta->BoundsUpdates.Value.InnerBounds.Add(graphPolygonHostIDs[polygonIdx], polygonVertexIDs);
		}
	}

	OutDeltas.Add(fillGraphDelta);

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
