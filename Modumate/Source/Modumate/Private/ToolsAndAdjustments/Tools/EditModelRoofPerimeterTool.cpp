// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "EditModelRoofPerimeterTool.h"

#include "EditModelGameState_CPP.h"
#include "EditModelPlayerController_CPP.h"
#include "EditModelPlayerState_CPP.h"
#include "Graph3DDelta.h"
#include "ModumateGraph.h"
#include "ModumateObjectStatics.h"

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

	TSet<int32> vertexIDs, edgeIDs, faceIDs;
	UModumateObjectStatics::GetGraphIDsFromMOIs(Controller->EMPlayerState->SelectedObjects, vertexIDs, edgeIDs, faceIDs);

	// Try to make a 2D graph from the selected objects, in order to find a perimeter
	TArray<int32> perimeterEdgeIDs;
	FGraph selectedGraph;
	if (volumeGraph.Create2DGraph(vertexIDs, edgeIDs, faceIDs, selectedGraph, true, true))
	{
		const FGraphPolygon *perimeterPoly = selectedGraph.GetExteriorPolygon();
		if (ensure(perimeterPoly))
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
	if (perimeterEdgeIDs.Num() >= 3)
	{
		int32 perimeterID = doc.GetNextAvailableID();
		TArray<TSharedPtr<FDelta>> deltasToApply;

		// Create the MOI delta for constructing the perimeter object
		FMOIStateData state;
		state.ParentID = Controller->EMPlayerState->GetViewGroupObjectID();
		state.ObjectType = EObjectType::OTRoofPerimeter;
		state.ObjectID = perimeterID;

		TSharedPtr<FMOIDelta> perimeterCreationDelta = FMOIDelta::MakeCreateObjectDelta(state);
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
