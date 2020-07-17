// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Tools/EditModelSurfaceGraphTool.h"

#include "DocumentManagement/ModumateObjectInstance.h"
#include "ModumateCore/ModumateObjectStatics.h"
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
	, LastHostTarget(nullptr)
	, LastGraphTarget(nullptr)
	, LastHitHostActor(nullptr)
	, LastValidHitLocation(ForceInitToZero)
	, LastValidHitNormal(ForceInitToZero)
	, LastValidFaceIndex(INDEX_NONE)
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
	if (LastHostTarget && (LastHostTarget->ID != 0) && (LastValidFaceIndex != INDEX_NONE))
	{
		// TODO: replace parent class's default affordance with our contextual understanding of the surface graph's snap axes
		Super::BeginUse();

		if (LastGraphTarget == nullptr)
		{
			TArray<TSharedPtr<FDelta>> deltas;
			if (CreateGraphFromFaceTarget(deltas))
			{
				GameState->Document.ApplyDeltas(deltas, GetWorld());
			}
		}

		auto dimensionActor = DimensionManager->AddDimensionActor(APendingSegmentActor::StaticClass());
		PendingSegmentID = dimensionActor->ID;

		auto pendingSegment = dimensionActor->GetLineActor();
		pendingSegment->Point1 = LastValidHitLocation;
		pendingSegment->Point2 = LastValidHitLocation;
		pendingSegment->Color = FColor::White;
		pendingSegment->Thickness = 2;
	}

	return true;
}

bool USurfaceGraphTool::EnterNextStage()
{
	return Super::EnterNextStage();
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
	auto *gameState = Cast<AEditModelGameState_CPP>(GetWorld()->GetGameState());
	const FSnappedCursor &cursor = Controller->EMPlayerState->SnappedCursor;

	if (IsInUse())
	{
		auto *dimensionActor = DimensionManager->GetDimensionActor(PendingSegmentID);
		auto *pendingSegment = dimensionActor ? dimensionActor->GetLineActor() : nullptr;

		if (pendingSegment)
		{
			pendingSegment->Point2 = cursor.WorldPosition;
		}
	}
	else
	{
		ResetTarget();

		auto *moi = gameState ? gameState->Document.ObjectFromActor(cursor.Actor) : nullptr;
		if ((moi == nullptr) || !cursor.bValid || (cursor.Actor == nullptr))
		{
			return true;
		}

		// If we're targeting a face, then figure out whether we're targeting an object onto which we can apply a surface graph,
		// or if we're targeting an existing surface graph that we want to edit and snap against.
		LastValidFaceIndex = UModumateObjectStatics::GetFaceIndexFromTargetHit(moi, cursor.WorldPosition, cursor.HitNormal);

		if (LastValidFaceIndex != INDEX_NONE)
		{
			LastHostTarget = moi;
			LastHitHostActor = cursor.Actor;
			LastValidHitLocation = cursor.WorldPosition;
			LastValidHitNormal = cursor.HitNormal;

			// See if there's already a surface graph mounted to the same target MOI, at the same target face as the current target.
			for (int32 childID : LastHostTarget->GetChildIDs())
			{
				FModumateObjectInstance *childObj = gameState->Document.GetObjectById(childID);
				int32 childFaceIdx = UModumateObjectStatics::GetParentFaceIndex(childObj);
				if (childObj && (childObj->GetObjectType() == EObjectType::OTSurfaceGraph) && (childFaceIdx == LastValidFaceIndex))
				{
					LastGraphTarget = childObj;
					break;
				}
			}
		}

		if (LastHostTarget && UModumateObjectStatics::GetGeometryFromFaceIndex(LastHostTarget, LastValidFaceIndex, LastCornerIndices, LastTargetFacePlane))
		{
			LastCornerPositions.Reset();
			Algo::Transform(LastCornerIndices, LastCornerPositions, [this](const int32 &CornerIdx) {
				return LastHostTarget->GetCorner(CornerIdx);
			});

			if (LastGraphTarget == nullptr)
			{
				int32 numCorners = LastCornerPositions.Num();
				for (int32 curCornerIdx = 0; curCornerIdx < numCorners; ++curCornerIdx)
				{
					int32 nextCornerIdx = (curCornerIdx + 1) % numCorners;

					Controller->EMPlayerState->AffordanceLines.Add(FAffordanceLine(LastCornerPositions[curCornerIdx], LastCornerPositions[nextCornerIdx],
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
	auto *gameState = GetWorld()->GetGameState<AEditModelGameState_CPP>();

	if (!(gameState && LastHostTarget && LastHitHostActor && (LastValidFaceIndex != INDEX_NONE)))
	{
		return false;
	}

	// If we're targeting a surface graph object, it has to be empty
	int32 surfaceGraphID = MOD_ID_NONE;
	EGraph2DDeltaType graphDeltaType = EGraph2DDeltaType::Add;
	int32 nextGraphObjID = gameState->Document.GetNextAvailableID();

	if (LastGraphTarget)
	{
		const FGraph2D *targetSurfaceGraph = gameState->Document.FindSurfaceGraph(LastGraphTarget->ID);
		if ((targetSurfaceGraph == nullptr) || !targetSurfaceGraph->IsEmpty())
		{
			return false;
		}

		surfaceGraphID = LastGraphTarget->ID;
		graphDeltaType = EGraph2DDeltaType::Edit;
	}
	else
	{
		surfaceGraphID = nextGraphObjID++;
	}

	int32 numPerimeterPoints = LastCornerPositions.Num();
	if (!LastTargetFacePlane.IsNormalized() || (numPerimeterPoints < 3))
	{
		return false;
	}

	TSharedPtr<FGraph2DDelta> fillGraphDelta = MakeShareable(new FGraph2DDelta(surfaceGraphID, graphDeltaType));

	// Generate the geometry for the target's explicit graph, starting with the perimeter, basis vectors, and origin
	FVector graphWorldOrigin = LastCornerPositions[0];
	FVector graphAxisX, graphAxisY;
	UModumateGeometryStatics::FindBasisVectors(graphAxisX, graphAxisY, LastTargetFacePlane);

	// Project all of the polygons to add to the target graph
	TArray<TArray<FVector2D>> graphPolygonsToAdd;
	TArray<FVector2D> &perimeterPolygon = graphPolygonsToAdd.AddDefaulted_GetRef();
	Algo::Transform(LastCornerPositions, perimeterPolygon, [graphAxisX, graphAxisY, graphWorldOrigin](const FVector &WorldPoint) {
		return UModumateGeometryStatics::ProjectPoint2D(WorldPoint, graphAxisX, graphAxisY, graphWorldOrigin);
	});

	// Project the holes that the target has into graph polygons, if any
	const auto &volumeGraph = gameState->Document.GetVolumeGraph();
	const auto *hostParentFace = volumeGraph.FindFace(LastHostTarget->GetParentID());
	if (hostParentFace)
	{
		for (const FPolyHole3D &hostWorldHole : hostParentFace->CachedHoles3D)
		{
			TArray<FVector2D> &holePolygon = graphPolygonsToAdd.AddDefaulted_GetRef();
			Algo::Transform(hostWorldHole.Points, holePolygon, [graphAxisX, graphAxisY, graphWorldOrigin](const FVector &WorldPoint) {
				return UModumateGeometryStatics::ProjectPoint2D(WorldPoint, graphAxisX, graphAxisY, graphWorldOrigin);
			});
		}
	}

	// Populate the target graph with the input polygons
	TArray<int32> polygonVertexIDs;
	for (const TArray<FVector2D> &graphPolygonToAdd : graphPolygonsToAdd)
	{
		// Populate the target graph with the hole vertices
		polygonVertexIDs.Reset();
		for (const FVector2D &vertex : graphPolygonToAdd)
		{
			polygonVertexIDs.Add(nextGraphObjID);
			fillGraphDelta->AddNewVertex(vertex, nextGraphObjID);
		}

		// Populate the target graph with the hole edges
		int32 numPolygonVerts = graphPolygonToAdd.Num();
		for (int32 polyPointIdxA = 0; polyPointIdxA < numPolygonVerts; ++polyPointIdxA)
		{
			int32 polyPointIdxB = (polyPointIdxA + 1) % numPolygonVerts;
			fillGraphDelta->AddNewEdge(TPair<int32, int32>(polygonVertexIDs[polyPointIdxA], polygonVertexIDs[polyPointIdxB]), nextGraphObjID);
		}
	}

	OutDeltas.Add(fillGraphDelta);

	// Create the delta for adding the surface graph object, if it didn't already exist
	if (LastGraphTarget == nullptr)
	{
		FMOIStateData surfaceObjectData;
		surfaceObjectData.StateType = EMOIDeltaType::Create;
		surfaceObjectData.ObjectType = EObjectType::OTSurfaceGraph;
		surfaceObjectData.ObjectAssemblyKey = NAME_None;
		surfaceObjectData.ParentID = LastHostTarget->ID;
		surfaceObjectData.ControlIndices = { LastValidFaceIndex };
		surfaceObjectData.ObjectID = surfaceGraphID;

		TSharedPtr<FMOIDelta> addObjectDelta = MakeShareable(new FMOIDelta({ surfaceObjectData }));
		OutDeltas.Add(addObjectDelta);
	}

	return true;
}

void USurfaceGraphTool::ResetTarget()
{
	LastHostTarget = nullptr;
	LastGraphTarget = nullptr;
	LastHitHostActor = nullptr;
	LastValidHitLocation = LastValidHitNormal = FVector::ZeroVector;
	LastValidFaceIndex = INDEX_NONE;
}
