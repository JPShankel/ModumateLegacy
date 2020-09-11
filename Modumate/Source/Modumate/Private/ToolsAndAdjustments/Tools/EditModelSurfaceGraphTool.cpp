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
			return CreateGraphFromFaceTarget();
		}

		// Otherwise, start drawing a poly-line on the target surface graph.
		// TODO: use the normal and tangent of the hit cursor, and override the "global axes";
		// this will allow appropriately-colored green and red lines for the surface graph's local X and Y axes,
		// while also allowing tangent affordance(s) for any connected surface graph vertices that are the hit target.
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

	auto targetSurfaceGraph = GameState->Document.FindSurfaceGraph(GraphTarget->ID);
	if (!targetSurfaceGraph.IsValid() || targetSurfaceGraph->IsEmpty())
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

bool USurfaceGraphTool::CreateGraphFromFaceTarget()
{
	if (!(HostTarget && HitHostActor && (HitFaceIndex != INDEX_NONE)))
	{
		return false;
	}

	// If we're targeting a surface graph object, it has to be empty
	int32 nextID = GameState->Document.GetNextAvailableID();
	TArray<TSharedPtr<FDelta>> deltas;

	if (GraphTarget)
	{
		TargetSurfaceGraph = GameState->Document.FindSurfaceGraph(GraphTarget->ID);
		if (!TargetSurfaceGraph.IsValid() || !TargetSurfaceGraph->IsEmpty())
		{
			return false;
		}
	}
	else
	{
		TargetSurfaceGraph = MakeShared<FGraph2D>(nextID);
		nextID++;
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
		surfaceObjectData.ObjectID = TargetSurfaceGraph->GetID();

		deltas.Add(MakeShareable(new FMOIDelta({ surfaceObjectData })));
		deltas.Add(MakeShareable(new FGraph2DDelta(TargetSurfaceGraph->GetID(), EGraph2DDeltaType::Add)));
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

	// If we created a surface graph for the purpose of generating deltas, then reset it now since we won't need it.
	if (GraphTarget == nullptr)
	{
		TargetSurfaceGraph->Reset();
	}
	TargetSurfaceGraph.Reset();

	for (FGraph2DDelta& graphDelta : fillGraphDeltas)
	{
		deltas.Add(MakeShareable(new FGraph2DDelta{ graphDelta }));
	}
	return GameState->Document.ApplyDeltas(deltas, GetWorld());
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
