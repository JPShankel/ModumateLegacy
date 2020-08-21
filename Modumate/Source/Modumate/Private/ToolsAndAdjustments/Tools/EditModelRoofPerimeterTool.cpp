// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Tools/EditModelRoofPerimeterTool.h"

#include "Algo/Transform.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "Graph/Graph3DDelta.h"
#include "Graph/Graph2D.h"
#include "ModumateCore/ModumateObjectStatics.h"
#include "ModumateCore/ModumateRoofStatics.h"

using namespace Modumate;

URoofPerimeterTool::URoofPerimeterTool(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool URoofPerimeterTool::Activate()
{
	Super::Activate();

	AEditModelGameState_CPP *gameState = GetWorld()->GetGameState<AEditModelGameState_CPP>();
	FModumateDocument &doc = gameState->Document;
	const FGraph3D &volumeGraph = doc.GetVolumeGraph();

	TSet<int32> graphObjIDs, connectedGraphIDs;
	UModumateObjectStatics::GetGraphIDsFromMOIs(Controller->EMPlayerState->SelectedObjects, graphObjIDs);

	// Try to make a 2D graph from the selected objects, in order to find a perimeter
	TArray<int32> perimeterEdgeIDs;
	FGraph2D selectedGraph;
	FPlane perimeterPlane;
	if (volumeGraph.Create2DGraph(graphObjIDs, connectedGraphIDs, selectedGraph, perimeterPlane, true, false))
	{
		// Consider polygons with duplicate edges as invalid loops.
		// TODO: this liberally invalidates polygons that have area, but also extra "peninsula" edges that double back on themselves;
		// we could remove those duplicate edges from the loop and use the rest of the edges as a perimeter in the future.
		const FGraph2DPolygon *perimeterPoly = selectedGraph.GetRootPolygon();
		if (ensure(perimeterPoly) && !perimeterPoly->bHasDuplicateVertex)
		{
			for (auto signedEdgeID : perimeterPoly->Edges)
			{
				int32 edgeID = FMath::Abs(signedEdgeID);
				FModumateObjectInstance *metaEdge = gameState->Document.GetObjectById(edgeID);
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
		// Create the MOI delta for constructing the perimeter object
		FMOIStateData state;
		state.StateType = EMOIDeltaType::Create;
		state.ControlPoints = { };
		state.ControlIndices = perimeterEdgeIDs;

		int32 perimeterID = doc.GetNextAvailableID();
		state.ParentID = Controller->EMPlayerState->GetViewGroupObjectID();
		state.ObjectType = EObjectType::OTRoofPerimeter;
		state.ObjectID = perimeterID;
		UModumateRoofStatics::InitializeProperties(&state.ObjectProperties, numEdges);

		TSharedPtr<FMOIDelta> perimeterCreationDelta = MakeShareable(new FMOIDelta({ state }));

		TArray<TSharedPtr<FDelta>> deltasToApply;
		deltasToApply.Add(perimeterCreationDelta);

		// Now create the graph delta to assign the perimeter object to the GroupIDs of its edges
		TSharedPtr<FGraph3DDelta> graphDelta = MakeShareable(new FGraph3DDelta());
		FGraph3DGroupIDsDelta groupIDsDelta(TSet<int32>({ perimeterID }), TSet<int32>());
		for (int32 edgeID : perimeterEdgeIDs)
		{
			graphDelta->GroupIDsUpdates.Add(edgeID, groupIDsDelta);
		}
		deltasToApply.Add(graphDelta);

		// Apply the deltas to create the perimeter and modify the associated graph objects
		bool bAppliedDeltas = doc.ApplyDeltas(deltasToApply, GetWorld());

		// If we succeeded, then select the new roof perimeter object (and only that object)
		FModumateObjectInstance *roofPerimObj = bAppliedDeltas ? doc.GetObjectById(perimeterID) : nullptr;
		if (roofPerimObj)
		{
			Controller->DeselectAll();
			Controller->SetObjectSelected(roofPerimObj, true);
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
