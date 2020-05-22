// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Tools/EditModelRoofPerimeterTool.h"

#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "Graph/Graph3DDelta.h"
#include "Graph/ModumateGraph.h"
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

	TSet<FTypedGraphObjID> graphObjIDs, connectedGraphIDs;
	UModumateObjectStatics::GetGraphIDsFromMOIs(Controller->EMPlayerState->SelectedObjects, graphObjIDs);

	// Try to make a 2D graph from the selected objects, in order to find a perimeter
	TArray<int32> perimeterEdgeIDs;
	FGraph selectedGraph;
	FPlane perimeterPlane;
	if (volumeGraph.Create2DGraph(graphObjIDs, connectedGraphIDs, selectedGraph, perimeterPlane, true, true))
	{
		// Consider polygons with duplicate edges as invalid loops.
		// TODO: this liberally invalidates polygons that have area, but also extra "peninsula" edges that double back on themselves;
		// we could remove those duplicate edges from the loop and use the rest of the edges as a perimeter in the future.
		const FGraphPolygon *perimeterPoly = selectedGraph.GetExteriorPolygon();
		if (ensure(perimeterPoly) && !perimeterPoly->bHasDuplicateEdge)
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

	// If we've found a perimeter from the 2D graph that has enough valid meta edges, then we can try to make the perimeter object
	int32 numEdges = perimeterEdgeIDs.Num();
	if (numEdges >= 3)
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
			FTypedGraphObjID typedEdgeID(edgeID, EGraph3DObjectType::Edge);
			graphDelta->GroupIDsUpdates.Add(typedEdgeID, groupIDsDelta);
		}
		deltasToApply.Add(graphDelta);

		// Apply the deltas to create the perimeter and modify the associated graph objects
		doc.ApplyDeltas(deltasToApply, GetWorld());
	}

	Deactivate();
	Controller->SetToolMode(EToolMode::VE_SELECT);

	return false;
}

bool URoofPerimeterTool::Deactivate()
{
	return Super::Deactivate();
}
