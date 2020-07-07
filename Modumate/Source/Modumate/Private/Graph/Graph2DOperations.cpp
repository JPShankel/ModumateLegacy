// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Graph/ModumateGraph.h"

#include "Graph/Graph2DDelta.h"

namespace Modumate
{
	bool FGraph::AddVertex(TArray<FGraph2DDelta> &OutDeltas, int32 &NextID, const FVector2D Position)
	{
		const FGraphVertex *existingVertex = FindVertex(Position);

		if (existingVertex == nullptr)
		{
			FGraph2DDelta addVertexDelta;
			int32 addedVertexID = NextID++;
			addVertexDelta.VertexAdditions.Add(addedVertexID, Position);

			OutDeltas.Add(addVertexDelta);
		}

		return true;
	}

	bool FGraph::AddEdge(TArray<FGraph2DDelta> &OutDeltas, int32 &NextID, const int32 StartVertexID, const int32 EndVertexID)
	{
		bool bOutForward;
		const FGraphEdge *existingEdge = FindEdgeByVertices(StartVertexID, EndVertexID, bOutForward);

		auto startVertex = FindVertex(StartVertexID);
		auto endVertex = FindVertex(EndVertexID);
		if (!startVertex || !endVertex)
		{
			return false;
		}

		if (existingEdge == nullptr)
		{
			FGraph2DDelta addEdgeDelta;
			int32 addedEdgeID = NextID++;
			addEdgeDelta.EdgeAdditions.Add(addedEdgeID, FGraph2DObjDelta({ StartVertexID, EndVertexID }));

			OutDeltas.Add(addEdgeDelta);
		}

		return true;
	}

	bool FGraph::AddEdge(TArray<FGraph2DDelta> &OutDeltas, int32 &NextID, const FVector2D StartPosition, const FVector2D EndPosition)
	{
		TArray<int32> addedIDs;
		TSet<int32> addedVertexIDs, addedEdgeIDs;

		for (auto& position : { StartPosition, EndPosition })
		{
			TArray<FGraph2DDelta> addVertexDeltas;
			addedVertexIDs.Reset();
			addedEdgeIDs.Reset();

			const FGraphVertex *existingVertex = FindVertex(position);

			if (existingVertex == nullptr)
			{
				if (!AddVertex(addVertexDeltas, NextID, position))
				{
					return false;
				}

				AggregateAddedObjects(addVertexDeltas, addedVertexIDs, addedEdgeIDs);

				if (!ensureAlways(addedVertexIDs.Num() == 1))
				{
					return false;
				}
				addedIDs.Add(addedVertexIDs.Array()[0]);
			}
			else
			{
				addedIDs.Add(existingVertex->ID);
			}

			if (!ApplyDeltas(addVertexDeltas))
			{
				return false;
			}
			OutDeltas.Append(addVertexDeltas);
		}

		if (!ensureAlways(addedIDs.Num() == 2))
		{
			return false;
		}

		TArray<FGraph2DDelta> addEdgeDeltas;
		AddEdge(addEdgeDeltas, NextID, addedIDs[0], addedIDs[1]);

		if (!ApplyDeltas(addEdgeDeltas))
		{
			return false;
		}
		OutDeltas.Append(addEdgeDeltas);

		// TODO: deltas for side-effects of adding the edge (splitting)

		// return graph in its original state
		ApplyInverseDeltas(OutDeltas);

		return true;
	}
}
