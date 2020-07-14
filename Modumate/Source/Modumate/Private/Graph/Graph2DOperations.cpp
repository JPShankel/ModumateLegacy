// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Graph/Graph2D.h"

#include "Graph/Graph2DDelta.h"

namespace Modumate
{
	bool FGraph2D::AddVertexDirect(FGraph2DDelta &OutDelta, int32 &NextID, const FVector2D Position)
	{
		int32 addedVertexID = NextID++;
		OutDelta.VertexAdditions.Add(addedVertexID, Position);

		return true;
	}

	bool FGraph2D::AddVertex(TArray<FGraph2DDelta> &OutDeltas, int32 &NextID, const FVector2D Position)
	{
		const FGraph2DVertex *existingVertex = FindVertex(Position);

		if (existingVertex == nullptr)
		{
			FGraph2DDelta addVertexDelta(ID);
			AddVertexDirect(addVertexDelta, NextID, Position);

			OutDeltas.Add(addVertexDelta);
		}

		return true;
	}

	bool FGraph2D::AddEdgeDirect(FGraph2DDelta &OutDelta, int32 &NextID, const int32 StartVertexID, const int32 EndVertexID)
	{
		auto startVertex = FindVertex(StartVertexID);
		auto endVertex = FindVertex(EndVertexID);
		if (!startVertex || !endVertex)
		{
			return false;
		}

		bool bOutForward;
		const FGraph2DEdge *existingEdge = FindEdgeByVertices(StartVertexID, EndVertexID, bOutForward);
		if (existingEdge == nullptr)
		{
			int32 addedEdgeID = NextID++;
			OutDelta.EdgeAdditions.Add(addedEdgeID, FGraph2DObjDelta({ StartVertexID, EndVertexID }));
		}

		return true;
	}

	bool FGraph2D::AddEdge(TArray<FGraph2DDelta> &OutDeltas, int32 &NextID, const FVector2D StartPosition, const FVector2D EndPosition)
	{
		TArray<int32> addedIDs;
		TSet<int32> addedVertexIDs;

		// start by adding or finding the vertices at the input positions
		for (auto& position : { StartPosition, EndPosition })
		{
			TArray<FGraph2DDelta> addVertexDeltas;
			addedVertexIDs.Reset();

			const FGraph2DVertex *existingVertex = FindVertex(position);

			if (existingVertex == nullptr)
			{
				if (!AddVertex(addVertexDeltas, NextID, position))
				{
					return false;
				}

				AggregateAddedVertices(addVertexDeltas, addedVertexIDs);

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

		// TODO: depending on the implementation of future operations (like add polygon) this 
		// splitting code may be best in a separate function

		// Find intersections between the pending segment (defined by the input points) and the existing edges
		auto pendingStartVertex = FindVertex(addedIDs[0]);
		auto pendingEndVertex = FindVertex(addedIDs[1]);
		FVector2D pendingStartPosition = pendingStartVertex->Position;
		FVector2D pendingEndPosition = pendingEndVertex->Position;
		FVector2D pendingEdgeDirection = pendingEndPosition - pendingStartPosition;
		pendingEdgeDirection.Normalize();

		FGraph2DDelta addVertexDelta(ID);
		// all vertices on the pending segment are collected in addedVertices to ultimately make the edges
		TSet<int32> addedVertices;
		// if a vertex created at an intersection and it is in-between the edge's vertices, the edge needs to be split
		TArray<TPair<int32, int32>> edgesToSplitByVertex;

		for (auto& edgekvp : Edges)
		{
			addVertexDelta.Reset();
			addedVertices.Reset();

			auto& edge = edgekvp.Value;
			auto startVertex = FindVertex(edge.StartVertexID);
			auto endVertex = FindVertex(edge.EndVertexID);

			FVector2D edgeDirection = (endVertex->Position - startVertex->Position);
			edgeDirection.Normalize();

			FVector2D intersection;
			if (UModumateGeometryStatics::SegmentIntersection2D(
				pendingStartPosition,
				pendingEndPosition,
				startVertex->Position,
				endVertex->Position,
				intersection))
			{
				auto existingVertex = FindVertex(intersection);
				if (existingVertex == nullptr)
				{
					AddVertexDirect(addVertexDelta, NextID, intersection);
					if (!ApplyDelta(addVertexDelta))
					{
						ApplyInverseDeltas(OutDeltas);
						return false;
					}
					OutDeltas.Add(addVertexDelta);

					AggregateAddedVertices({ addVertexDelta }, addedVertices);
					edgesToSplitByVertex.Add(TPair<int32, int32>(edge.ID, addedVertices.Array()[0]));
				}
				else 
				{
					addedIDs.Add(existingVertex->ID);
					if (existingVertex->ID != edge.StartVertexID && existingVertex->ID != edge.EndVertexID)
					{
						edgesToSplitByVertex.Add(TPair<int32, int32>(edge.ID, existingVertex->ID));
					}
				}
			}
			// if the edge is overlapping the pending segment, SegmentIntersection2D returns false.
			// however, all of the points that are on the pending segment are needed to add the correct edges
			else if ((pendingEdgeDirection | edgeDirection) > THRESH_NORMALS_ARE_PARALLEL)
			{
				FVector2D startVertexOnSegment = FMath::ClosestPointOnSegment2D(startVertex->Position, pendingStartPosition, pendingEndPosition);
				if (startVertexOnSegment.Equals(startVertex->Position, Epsilon))
				{
					addedIDs.Add(startVertex->ID);
				}

				FVector2D endVertexOnSegment = FMath::ClosestPointOnSegment2D(endVertex->Position, pendingStartPosition, pendingEndPosition);
				if (endVertexOnSegment.Equals(endVertex->Position, Epsilon))
				{
					addedIDs.Add(endVertex->ID);
				}
			}
		}

		// split edges
		FGraph2DDelta splitEdgeDelta(ID);
		for (auto& kvp : edgesToSplitByVertex)
		{
			splitEdgeDelta.Reset();
			SplitEdge(splitEdgeDelta, NextID, kvp.Key, kvp.Value);
			if (!ApplyDelta(splitEdgeDelta))
			{
				ApplyInverseDeltas(OutDeltas);
				return false;
			}
			OutDeltas.Add(splitEdgeDelta);
		}

		// aggregate all vertices on the pending segment into addedVertices
		addedVertices.Append(addedIDs);
		AggregateAddedVertices(OutDeltas, addedVertices);

		// sort the vertices by their distance along the pending segment
		TArray<TPair<float, int32>> sortedNewVertices;
		float length = FVector2D::Distance(pendingEndPosition, pendingStartPosition);

	 	for (int32 vertexID : addedVertices)
		{
			auto vertex = FindVertex(vertexID);
			if (vertex == nullptr)
			{
				ApplyInverseDeltas(OutDeltas);
				return false;
			}

			float currentLength = FVector2D::Distance(vertex->Position, pendingStartPosition);
			sortedNewVertices.Add(TPair<float, int32>(currentLength / length, vertexID));
		}
		
		sortedNewVertices.Sort();

		// add an edge connecting the sorted vertices - AddEdgeDirect detects if the edge
		// exists already, so the edges are only created when there is a gap
		FGraph2DDelta addEdgesDelta(ID);
		int32 numVertices = sortedNewVertices.Num();
		for (int32 idx = 0; idx < numVertices-1; idx++)
		{
			int32 startVertexID = sortedNewVertices[idx].Value;
			int32 endVertexID = sortedNewVertices[idx + 1].Value;

			AddEdgeDirect(addEdgesDelta, NextID, startVertexID, endVertexID);
		}

		if (!ApplyDelta(addEdgesDelta))
		{
			ApplyInverseDeltas(OutDeltas);
			return false;
		}
		OutDeltas.Add(addEdgesDelta);

		// return graph in its original state
		ApplyInverseDeltas(OutDeltas);

		return true;
	}

	bool FGraph2D::DeleteObjects(TArray<FGraph2DDelta> &OutDeltas, const TArray<int32> &VertexIDs, const TArray<int32> &EdgeIDs)
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

		FGraph2DDelta deleteDelta(ID);
		if (!DeleteObjectsDirect(deleteDelta, vertexIDsToDelete, edgeIDsToDelete))
		{
			return false;
		}
		OutDeltas.Add(deleteDelta);

		return true;
	}

	bool FGraph2D::DeleteObjectsDirect(FGraph2DDelta &OutDelta, const TSet<int32> &VertexIDs, const TSet<int32> &EdgeIDs)
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

	bool FGraph2D::SplitEdge(FGraph2DDelta &OutDelta, int32 &NextID, int32 EdgeID, int32 SplittingVertexID)
	{
		auto edge = FindEdge(EdgeID);
		if (!ensureAlways(edge != nullptr))
		{
			return false;
		}

		auto startVertex = FindVertex(edge->StartVertexID);
		auto endVertex = FindVertex(edge->EndVertexID);
		auto splitVertex = FindVertex(SplittingVertexID);

		if (!ensureAlways(startVertex != nullptr && endVertex != nullptr && splitVertex != nullptr))
		{
			return false;
		}

		if (!DeleteObjectsDirect(OutDelta, {}, { EdgeID }))
		{
			return false;
		}

		if (!AddEdgeDirect(OutDelta, NextID, startVertex->ID, SplittingVertexID))
		{
			return false;
		}

		if (!AddEdgeDirect(OutDelta, NextID, SplittingVertexID, endVertex->ID))
		{
			return false;
		}

		return true;
	}
}
