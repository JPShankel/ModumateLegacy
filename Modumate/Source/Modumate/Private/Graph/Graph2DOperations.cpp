// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Graph/ModumateGraph.h"

#include "Graph/Graph2DDelta.h"

namespace Modumate
{
	bool FGraph::AddVertexDirect(FGraph2DDelta &OutDelta, int32 &NextID, const FVector2D Position)
	{
		int32 addedVertexID = NextID++;
		OutDelta.VertexAdditions.Add(addedVertexID, Position);

		return true;
	}

	bool FGraph::AddVertex(TArray<FGraph2DDelta> &OutDeltas, int32 &NextID, const FVector2D Position)
	{
		const FGraphVertex *existingVertex = FindVertex(Position);

		if (existingVertex == nullptr)
		{
			FGraph2DDelta addVertexDelta;
			AddVertexDirect(addVertexDelta, NextID, Position);

			OutDeltas.Add(addVertexDelta);
		}

		return true;
	}

	bool FGraph::AddEdgeDirect(FGraph2DDelta &OutDelta, int32 &NextID, const int32 StartVertexID, const int32 EndVertexID)
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
			int32 addedEdgeID = NextID++;
			OutDelta.EdgeAdditions.Add(addedEdgeID, FGraph2DObjDelta({ StartVertexID, EndVertexID }));
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

		FGraph2DDelta addEdgeDelta;
		AddEdgeDirect(addEdgeDelta, NextID, addedIDs[0], addedIDs[1]);

		if (!ApplyDelta(addEdgeDelta))
		{
			return false;
		}
		OutDeltas.Add(addEdgeDelta);

		// TODO: deltas for side-effects of adding the edge (splitting)

		// return graph in its original state
		ApplyInverseDeltas(OutDeltas);

		return true;
	}

	bool FGraph::DeleteObjects(TArray<FGraph2DDelta> &OutDeltas, const TArray<int32> &VertexIDs, const TArray<int32> &EdgeIDs)
	{
		// the ids that are considered for deletion starts with the input arguments and grows based on object connectivity
		TSet<int32> vertexIDsToDelete;
		for (int32 vertexID : VertexIDs)
		{
			vertexIDsToDelete.Add(FMath::Abs(vertexID));
		}

		TSet<int32> edgeIDsToDelete = TSet<int32>(EdgeIDs);
		for (int32 edgeID : EdgeIDs)
		{
			edgeIDsToDelete.Add(FMath::Abs(edgeID));
		}

		for (int32 vertexID : vertexIDsToDelete)
		{
			auto vertex = FindVertex(vertexID);
			if (vertex != nullptr)
			{
				// consider all edges that are connected to the vertex for deletion
				for (int32 connectedEdgeID : vertex->Edges)
				{
					edgeIDsToDelete.Add(FMath::Abs(connectedEdgeID));
				}
			}
		}

		for (int32 edgeID : edgeIDsToDelete)
		{
			auto edge = FindEdge(edgeID);
			if (edge != nullptr)
			{
				// delete connected vertices that are not connected to any edge that is not being deleted
				for (auto vertexID : { edge->StartVertexID, edge->EndVertexID })
				{
					auto vertex = FindVertex(vertexID);
					if (vertex != nullptr)
					{
						bool bDeleteVertex = true;
						for (int32 connectedEdgeID : vertex->Edges)
						{
							if (!edgeIDsToDelete.Contains(FMath::Abs(connectedEdgeID)))
							{
								bDeleteVertex = false;
							}
						}

						if (bDeleteVertex)
						{
							vertexIDsToDelete.Add(FMath::Abs(vertexID));
						}
					}
				}
			}
		}

		FGraph2DDelta deleteDelta;
		if (!DeleteObjectsDirect(deleteDelta, vertexIDsToDelete, edgeIDsToDelete))
		{
			return false;
		}
		OutDeltas.Add(deleteDelta);

		return true;
	}

	bool FGraph::DeleteObjectsDirect(FGraph2DDelta &OutDelta, const TSet<int32> &VertexIDs, const TSet<int32> &EdgeIDs)
	{
		for (int32 vertexID : VertexIDs)
		{
			auto vertex = FindVertex(vertexID);
			if (!ensureAlways(vertex != nullptr))
			{
				continue;
			}

			OutDelta.VertexDeletions.Add(vertexID, vertex->Position);
		}

		for (int32 edgeID : EdgeIDs)
		{
			auto edge = FindEdge(edgeID);
			if (!ensureAlways(edge != nullptr))
			{
				continue;
			}

			OutDelta.EdgeDeletions.Add(edgeID, FGraph2DObjDelta({ edge->StartVertexID, edge->EndVertexID }));
		}

		return true;
	}
}
