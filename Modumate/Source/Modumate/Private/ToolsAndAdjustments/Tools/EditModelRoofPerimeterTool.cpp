// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "EditModelRoofPerimeterTool.h"

#include "EditModelGameState_CPP.h"
#include "EditModelPlayerController_CPP.h"
#include "EditModelPlayerState_CPP.h"
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
		const FGraphPolygon *perimeterPoly = nullptr;

		auto &polygons = selectedGraph.GetPolygons();
		for (auto &kvp : polygons)
		{
			auto &polygon = kvp.Value;
			if (polygon.bClosed && !polygon.bInterior)
			{
				perimeterPoly = &polygon;
				break;
			}
		}

		if (perimeterPoly)
		{
			for (auto edgeID : perimeterPoly->Edges)
			{
				FModumateObjectInstance *metaEdge = gameState->Document.GetObjectById(FMath::Abs(edgeID));
				if (metaEdge && (metaEdge->GetObjectType() == EObjectType::OTMetaEdge))
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
		FMOIStateData state;

		state.ControlPoints = { };
		state.ControlIndices = perimeterEdgeIDs;
		state.ParentID = Controller->EMPlayerState->GetViewGroupObjectID();
		state.ObjectType = EObjectType::OTRoofPerimeter;
		state.ObjectID = doc.GetNextAvailableID();

		FMOIDelta delta = FMOIDelta::MakeCreateObjectDelta(state);

		Controller->ModumateCommand(delta.AsCommand());
	}

	Deactivate();
	Controller->SetToolMode(EToolMode::VE_SELECT);

	return false;
}

bool URoofPerimeterTool::Deactivate()
{
	return Super::Deactivate();
}
