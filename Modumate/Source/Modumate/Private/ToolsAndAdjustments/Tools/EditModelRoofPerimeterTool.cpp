// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Tools/EditModelRoofPerimeterTool.h"

#include "Algo/Transform.h"
#include "Graph/Graph3DDelta.h"
#include "Objects/ModumateObjectStatics.h"
#include "Objects/ModumateRoofStatics.h"
#include "UnrealClasses/EditModelGameState.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"



URoofPerimeterTool::URoofPerimeterTool()
	: Super()
{
	SelectedGraph = MakeShared<FGraph2D>();
}

bool URoofPerimeterTool::Activate()
{
	Super::Activate();

	AEditModelGameState *gameState = GetWorld()->GetGameState<AEditModelGameState>();
	UModumateDocument* doc = gameState->Document;
	const FGraph3D& volumeGraph = *doc->GetVolumeGraph();

	TSet<int32> graphObjIDs, connectedGraphIDs;
	UModumateObjectStatics::GetGraphIDsFromMOIs(Controller->EMPlayerState->SelectedObjects, graphObjIDs);

	// Try to make a 2D graph from the selected objects, in order to find a perimeter
	TArray<int32> perimeterEdgeIDs;
	FPlane perimeterPlane;
	if (volumeGraph.Create2DGraph(graphObjIDs, connectedGraphIDs, SelectedGraph, perimeterPlane, true, false))
	{
		// Consider polygons with duplicate edges as invalid loops.
		// TODO: this liberally invalidates polygons that have area, but also extra "peninsula" edges that double back on themselves;
		// we could remove those duplicate edges from the loop and use the rest of the edges as a perimeter in the future.
		const FGraph2DPolygon *perimeterPoly = SelectedGraph->GetRootInteriorPolygon();
		if (ensure(perimeterPoly) && !perimeterPoly->bHasDuplicateVertex)
		{
			for (auto signedEdgeID : perimeterPoly->Edges)
			{
				int32 edgeID = FMath::Abs(signedEdgeID);
				AModumateObjectInstance *metaEdge = gameState->Document->GetObjectById(edgeID);
				const FGraph3DEdge *graphEdge = volumeGraph.FindEdge(edgeID);
				if (metaEdge && graphEdge && (metaEdge->GetObjectType() == EObjectType::OTMetaEdge))
				{
					perimeterEdgeIDs.Add(edgeID);
				}
				else
				{
					perimeterEdgeIDs.Reset();
					break;
				}
			}
		}
	}

	int32 numEdges = perimeterEdgeIDs.Num();

	TSet<int32> newPerimeterGroup(perimeterEdgeIDs);

	// Make sure that the resulting perimeter edge IDs aren't redundant with an existing one.
	int32 existingPerimeterID = MOD_ID_NONE;
	auto &groups = volumeGraph.GetGroups();
	for (auto &kvp : groups)
	{
		// Check for equality between sets with intersection
		// TODO: if there are many groups or roof perimeters, optimize this by searching for group members a different way
		const TSet<int32> &groupMembers = kvp.Value;
		if ((groupMembers.Num() == numEdges) && (groupMembers.Intersect(newPerimeterGroup).Num() == numEdges))
		{
			existingPerimeterID = kvp.Key;
			break;
		}
	}

	// If we've found a perimeter from the 2D graph that has enough valid meta edges, then we can try to make the perimeter object
	if ((numEdges >= 3) && (existingPerimeterID == MOD_ID_NONE))
	{
		TArray<FDeltaPtr> deltasToApply;
		int32 perimeterID = doc->GetNextAvailableID();

		// Create the MOI delta for constructing the perimeter object
		FMOIRoofPerimeterData newPerimeterInstanceData;
		FMOIStateData newPerimeterState(perimeterID, EObjectType::OTRoofPerimeter, Controller->EMPlayerState->GetViewGroupObjectID());
		newPerimeterState.CustomData.SaveStructData(newPerimeterInstanceData);

		auto perimeterCreationDelta = MakeShared<FMOIDelta>();
		perimeterCreationDelta->AddCreateDestroyState(newPerimeterState, EMOIDeltaType::Create);
		deltasToApply.Add(perimeterCreationDelta);

		// Now create the graph delta to assign the perimeter object to the GroupIDs of its edges
		auto graphDelta = MakeShared<FGraph3DDelta>(volumeGraph.GraphID);
		FGraph3DGroupIDsDelta groupIDsDelta(TSet<int32>({ perimeterID }), TSet<int32>());
		for (int32 edgeID : perimeterEdgeIDs)
		{
			graphDelta->GroupIDsUpdates.Add(edgeID, groupIDsDelta);
		}
		deltasToApply.Add(graphDelta);

		// Apply the deltas to create the perimeter and modify the associated graph objects
		bool bAppliedDeltas = doc->ApplyDeltas(deltasToApply, GetWorld());

		// If we succeeded, then select the new roof perimeter object (and only that object)
		AModumateObjectInstance *roofPerimObj = bAppliedDeltas ? doc->GetObjectById(perimeterID) : nullptr;
		if (roofPerimObj)
		{
			Controller->SetObjectSelected(roofPerimObj, true, true);
		}
	}

	Deactivate();
	Controller->SetToolMode(EToolMode::VE_SELECT);

	return false;
}

bool URoofPerimeterTool::Deactivate()
{
	return Super::Deactivate();
}
