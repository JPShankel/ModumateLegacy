// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "EditModelGraph2DTool.h"

#include "Algo/Transform.h"
#include "EditModelGameState_CPP.h"
#include "EditModelPlayerController_CPP.h"
#include "EditModelPlayerState_CPP.h"
#include "JsonObjectConverter.h"

#include "ModumateCommands.h"

using namespace Modumate;

UGraph2DTool::UGraph2DTool(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

bool UGraph2DTool::Activate()
{
	UEditModelToolBase::Activate();

	FPlane selectedPlane;
	FGraph selectedGraph;
	AEditModelGameState_CPP *gameState = GetWorld()->GetGameState<AEditModelGameState_CPP>();
	const FGraph3D &volumeGraph = gameState->Document.GetVolumeGraph();
	if (GetGraph2DFromObjs(volumeGraph, Controller->EMPlayerState->SelectedObjects, selectedPlane, selectedGraph))
	{
		FGraph2DRecord graphRecord;
		FName graphKey;
		if (selectedGraph.ToDataRecord(graphRecord) &&
			(gameState->Document.PresetManager.AddOrUpdateGraph2DRecord(NAME_None, graphRecord, graphKey) == ECraftingResult::Success))
		{
			UE_LOG(LogTemp, Log, TEXT("Added graph record \"%s\""), *graphKey.ToString());
		}
	}

	Deactivate();
	Controller->SetToolMode(EToolMode::VE_SELECT);

	return false;
}

bool UGraph2DTool::Deactivate()
{
	return UEditModelToolBase::Deactivate();
}

bool UGraph2DTool::GetGraph2DFromObjs(const FGraph3D &VolumeGraph, const TArray<FModumateObjectInstance*> Objects, FPlane &OutPlane, FGraph &OutGraph, bool bRequireConnected)
{
	OutPlane = FPlane(ForceInitToZero);
	OutGraph.Reset();

	TSet<const FGraph3DVertex*> selectedVertices;
	TSet<const FGraph3DEdge*> selectedEdges;
	TSet<const FGraph3DFace*> selectedFaces;

	// First, find a plane for the selected objects
	for (const FModumateObjectInstance *obj : Objects)
	{
		if (obj && (obj->GetObjectType() == EObjectType::OTMetaPlane))
		{
			const FGraph3DFace *face = VolumeGraph.FindFace(obj->ID);
			if (!ensure(face))
			{
				return false;
			}

			if (OutPlane.IsZero())
			{
				OutPlane = face->CachedPlane;
			}
			else if (!face->CachedPlane.Equals(OutPlane, PLANAR_DOT_EPSILON) &&
				!face->CachedPlane.Equals(OutPlane.Flip(), PLANAR_DOT_EPSILON))
			{
				return false;
			}

			selectedFaces.Add(face);
			Algo::Transform(face->VertexIDs, selectedVertices, [&VolumeGraph](const int32 &vertexID) { return VolumeGraph.FindVertex(vertexID); });
			Algo::Transform(face->EdgeIDs, selectedEdges, [&VolumeGraph](const FSignedID &edgeID) { return VolumeGraph.FindEdge(edgeID); });
		}
	}

	if (OutPlane.IsZero())
	{
		return false;
	}

	FVector axisX, axisY;
	UModumateGeometryStatics::FindBasisVectors(axisX, axisY, FVector(OutPlane));

	// Find a reasonable origin for the potential graph
	FVector origin(ForceInitToZero);
	FVector2D minPoint2D(FLT_MAX, FLT_MAX);
	for (const FGraph3DVertex *vertex3D : selectedVertices)
	{
		FVector2D projectedPos = UModumateGeometryStatics::ProjectPoint2D(vertex3D->Position, axisX, axisY, FVector::ZeroVector);
		if ((projectedPos.X < minPoint2D.X) || ((projectedPos.Y < minPoint2D.Y) &&
			FMath::IsNearlyEqual(projectedPos.X, minPoint2D.X, PLANAR_DOT_EPSILON)))
		{
			minPoint2D = projectedPos;
			origin = vertex3D->Position;
		}
	}

	// Next, try to create the 2D graph

	// Add the vertices
	TMap<int32, int32> vertexIDMapping;
	for (const FGraph3DVertex *vertex3D : selectedVertices)
	{
		FVector2D projectedPos = UModumateGeometryStatics::ProjectPoint2D(vertex3D->Position, axisX, axisY, origin);
		FGraphVertex *vertex2D = OutGraph.AddVertex(projectedPos);
		if (!ensure(vertex2D))
		{
			return false;
		}

		vertexIDMapping.Add(vertex3D->ID, vertex2D->ID);
	}

	// Add the edges
	for (const FGraph3DEdge *edge3D : selectedEdges)
	{
		int32 startVertex2DID = vertexIDMapping.FindRef(edge3D->StartVertexID);
		int32 endVertex2DID = vertexIDMapping.FindRef(edge3D->EndVertexID);

		if ((startVertex2DID == MOD_ID_NONE) || (endVertex2DID == MOD_ID_NONE))
		{
			return false;
		}

		FGraphEdge *edge2D = OutGraph.AddEdge(startVertex2DID, endVertex2DID);
		if (!ensure(edge2D))
		{
			return false;
		}
	}

	OutGraph.CalculatePolygons();

	int32 numInteriorPolygons = 0, numExteriorPolygons = 0;
	for (auto &kvp : OutGraph.GetPolygons())
	{
		const FGraphPolygon &polygon = kvp.Value;
		if (polygon.bClosed)
		{
			if (polygon.bInterior)
			{
				++numInteriorPolygons;
			}
			else
			{
				++numExteriorPolygons;
			}
		}
	}
	bool bFullyConnected = (numExteriorPolygons == 1);

	// At least check if the number of interior polygons of the 2D graph matches the selected faces
	// TODO: create a mapping between 2D polygons and 3D faces by ID to be more precise
	// Also, check the connected requirement, which detects if there are multiple exterior polygons that indicate disconnected graphs
	if (!ensure(numInteriorPolygons == selectedFaces.Num()) || (bRequireConnected && !bFullyConnected))
	{
		return false;
	}

	return true;
}
