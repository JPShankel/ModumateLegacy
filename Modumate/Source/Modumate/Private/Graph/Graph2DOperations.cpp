// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Graph/Graph2D.h"

#include "DocumentManagement/ModumateSerialization.h"
#include "Graph/Graph2DDelta.h"
#include "ModumateCore/ModumateGeometryStatics.h"

namespace Modumate
{
	bool FGraph2D::AddVertexDirect(FGraph2DDelta &OutDelta, int32 &NextID, const FVector2D Position)
	{
		OutDelta.AddNewVertex(Position, NextID);
		return true;
	}

	bool FGraph2D::AddVertex(TArray<FGraph2DDelta> &OutDeltas, int32 &NextID, const FVector2D Position)
	{
		const FGraph2DVertex *existingVertex = FindVertex(Position);
		if (existingVertex == nullptr)
		{
			FGraph2DDelta addVertexDelta(ID);
			if (!AddVertexDirect(addVertexDelta, NextID, Position))
			{	// no deltas have been applied
				return false;
			}
			OutDeltas.Add(addVertexDelta);
		}

		// TODO: aggregate and split edge by vertex?

		return true;
	}

	bool FGraph2D::AddEdgeDirect(FGraph2DDelta &OutDelta, int32 &NextID, const int32 StartVertexID, const int32 EndVertexID, const TArray<int32> &ParentIDs)
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
			OutDelta.AddNewEdge(FGraphVertexPair(StartVertexID, EndVertexID), NextID, ParentIDs);
		}

		return true;
	}

	bool FGraph2D::AddEdge(TArray<FGraph2DDelta> &OutDeltas, int32 &NextID, const FVector2D StartPosition, const FVector2D EndPosition)
	{
		TArray<int32> addedIDs;

		// start by adding or finding the vertices at the input positions
		for (auto& position : { StartPosition, EndPosition })
		{
			FGraph2DDelta addVertexDelta(ID);
			TSet<int32> addedVertexIDs;
			addedVertexIDs.Reset();
			const FGraph2DVertex *existingVertex = FindVertex(position);
			if (existingVertex == nullptr)
			{
				if (!AddVertexDirect(addVertexDelta, NextID, position))
				{
					return false;
				}

				AggregateAddedVertices({ addVertexDelta }, addedVertexIDs);

				if (!addVertexDelta.IsEmpty())
				{
					if (!ApplyDelta(addVertexDelta))
					{
						ApplyInverseDeltas(OutDeltas);
						return false;
					}

					OutDeltas.Add(addVertexDelta);
				}

				addedIDs.Add(addedVertexIDs.Array()[0]);
			}
			else
			{
				addedIDs.Add(existingVertex->ID);
			}
		}


		if (!ensureAlways(addedIDs.Num() == 2))
		{
			return false;
		}

		if (!SplitEdgesByVertices(OutDeltas, NextID, addedIDs))
		{
			ApplyInverseDeltas(OutDeltas);
			return false;
		}

		if (!AddEdgesBetweenVertices(OutDeltas, NextID, addedIDs[0], addedIDs[1]))
		{
			ApplyInverseDeltas(OutDeltas);
			return false;
		}

		if (!CalculatePolygons(OutDeltas, NextID))
		{
			ApplyInverseDeltas(OutDeltas);
			return false;
		}

		if (!ValidateAgainstBounds())
		{
			ApplyInverseDeltas(OutDeltas);
			return false;
		}

		// return graph in its original state
		ApplyInverseDeltas(OutDeltas);

		return true;
	}

	bool FGraph2D::PasteObjects(TArray<FGraph2DDelta>& OutDeltas, int32& NextID, const FGraph2DRecord* InRecord, TMap<int32, TArray<int32>>& OutCopiedToPastedIDs, bool bIsPreview, const FVector2D &InOffset)
	{
		if (!ensureAlways(InRecord))
		{
			return false;
		}

		FGraph2DDelta vertexDelta(ID);
		for (auto& kvp : InRecord->Vertices)
		{
			FVector2D position = kvp.Value + InOffset;
			const FGraph2DVertex *existingVertex = FindVertex(position);
			if (existingVertex != nullptr)
			{
				OutCopiedToPastedIDs.Add(kvp.Key, { existingVertex->ID });
			}
			else
			{
				AddVertexDirect(vertexDelta, NextID, position);
				OutCopiedToPastedIDs.Add(kvp.Key, { NextID - 1 });
			}
		}

		ApplyDelta(vertexDelta);
		OutDeltas.Add(vertexDelta);

		for (auto& kvp : InRecord->Edges)
		{
			if (!ensure(kvp.Value.VertexIDs.Num() == 2))
			{
				return false;
			}

			int32 startVertexID = OutCopiedToPastedIDs[kvp.Value.VertexIDs[0]][0];
			int32 endVertexID = OutCopiedToPastedIDs[kvp.Value.VertexIDs[1]][0];
			auto startVertex = FindVertex(startVertexID);
			auto endVertex = FindVertex(endVertexID);

			TArray<FGraph2DDelta> edgeDeltas;
			if (!AddEdgesBetweenVertices(edgeDeltas, NextID, startVertexID, endVertexID))
			{
				ApplyInverseDeltas(OutDeltas);
				return false;
			}
			OutDeltas.Append(edgeDeltas);

			TSet<int32> outEdges;
			AggregateAddedEdges(edgeDeltas, outEdges, startVertex->Position, endVertex->Position);

			OutCopiedToPastedIDs.Add(kvp.Key, outEdges.Array());
		}

		if (!CalculatePolygons(OutDeltas, NextID))
		{
			ApplyInverseDeltas(OutDeltas);
			return false;
		}

		if (!ValidateAgainstBounds())
		{
			ApplyInverseDeltas(OutDeltas);
			return false;
		}

		// find edges that were pasted to find the related polys that were created during CalculatePolygons
		for (auto& kvp : InRecord->Polygons)
		{
			TSet<int32> relatedPolys;

			auto& vertexIDs = kvp.Value.VertexIDs;
			int32 numVertices = vertexIDs.Num();

			if (numVertices < 3)
			{
				continue;
			}

			for (int32 idx = 0; idx < numVertices; idx++)
			{
				int32 nextIdx = (idx + 1) % numVertices;
				if (!(OutCopiedToPastedIDs.Contains(vertexIDs[idx]) && OutCopiedToPastedIDs.Contains(vertexIDs[nextIdx])))
				{
					continue;
				}

				TArray<int32> outEdges;
				if (!FindEdgesBetweenVertices(OutCopiedToPastedIDs[vertexIDs[idx]][0], OutCopiedToPastedIDs[vertexIDs[nextIdx]][0], outEdges))
				{
					continue;
				}

				for (int32 edgeID : outEdges)
				{
					auto relatedEdge = FindEdge(edgeID);
					if (!relatedEdge)
					{
						continue;
					}

					int32 relatedPolyID = edgeID > 0 ? relatedEdge->LeftPolyID : relatedEdge->RightPolyID;
					auto relatedPoly = FindPolygon(relatedPolyID);
					if (!relatedPoly)
					{
						continue;
					}

					relatedPolys.Add(relatedPolyID);
				}
			}

			OutCopiedToPastedIDs.Add(kvp.Key, relatedPolys.Array());
		}

		// return graph in its original state
		ApplyInverseDeltas(OutDeltas);

		return true;
	}

	bool FGraph2D::SplitEdgesByVertices(TArray<FGraph2DDelta> &OutDeltas, int32 &NextID, TArray<int32> &VertexIDs)
	{
		TArray<FGraph2DDelta> splitEdgeDeltas;
		for (int32 vertexID : VertexIDs)
		{
			auto vertex = FindVertex(vertexID);
			if (vertex == nullptr)
			{
				continue;
			}

			int32 intersectingEdgeID = MOD_ID_NONE;

			for (auto& edgekvp : Edges)
			{
				const FGraph2DEdge &edge = edgekvp.Value;
				auto startVertex = FindVertex(edge.StartVertexID);
				auto endVertex = FindVertex(edge.EndVertexID);
				if (startVertex == nullptr || endVertex == nullptr)
				{
					continue;
				}
				if (startVertex->ID == vertexID || endVertex->ID == vertexID)
				{
					continue;
				}

				// If the vertex is on the edge, split the edge and apply the delta.
				// It is important that the delta is applied here in the case that more than one
				// provided vertex is on the edge.  In that case, having multiple deltas allows for 
				// iterative splitting.
				FVector2D closestPoint = FMath::ClosestPointOnSegment2D(vertex->Position, startVertex->Position, endVertex->Position);
				if (closestPoint.Equals(vertex->Position, Epsilon))
				{
					if (!ensureAlways(intersectingEdgeID == MOD_ID_NONE))
					{
						ApplyInverseDeltas(splitEdgeDeltas);
						return false;
					}
					intersectingEdgeID = edge.ID;
				}
			}

			if (intersectingEdgeID != MOD_ID_NONE)
			{
				FGraph2DDelta splitEdgeDelta(ID);
				if (!SplitEdge(splitEdgeDelta, NextID, intersectingEdgeID, vertex->ID))
				{
					ApplyInverseDeltas(splitEdgeDeltas);
					return false;
				}
				splitEdgeDeltas.Add(splitEdgeDelta);
			}
		}

		OutDeltas.Append(splitEdgeDeltas);

		return true;
	}

	bool FGraph2D::AddEdgesBetweenVertices(TArray<FGraph2DDelta> &OutDeltas, int32 &NextID, int32 StartVertexID, int32 EndVertexID)
	{
		TArray<FGraph2DDelta> addEdgesBetweenVerticesDeltas;

		// Find intersections between the pending segment (defined by the input points) and the existing edges
		auto pendingStartVertex = FindVertex(StartVertexID);
		auto pendingEndVertex = FindVertex(EndVertexID);
		FVector2D pendingStartPosition = pendingStartVertex->Position;
		FVector2D pendingEndPosition = pendingEndVertex->Position;
		FVector2D pendingEdgeDirection = pendingEndPosition - pendingStartPosition;
		pendingEdgeDirection.Normalize();

		FGraph2DDelta addVertexDelta(ID);
		// all vertices on the pending segment are collected in addedVertices to ultimately make the edges
		TSet<int32> addedVertices;
		TArray<int32> addedIDs = { StartVertexID, EndVertexID };
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
			bool bSegmentsOverlap;
			bool bSegmentsIntersect = UModumateGeometryStatics::SegmentIntersection2D(
				pendingStartPosition,
				pendingEndPosition,
				startVertex->Position,
				endVertex->Position,
				intersection,
				bSegmentsOverlap);
			
			if (bSegmentsIntersect && !bSegmentsOverlap)
			{
				auto existingVertex = FindVertex(intersection);
				if (existingVertex == nullptr)
				{
					if (!AddVertexDirect(addVertexDelta, NextID, intersection))
					{
						ApplyInverseDeltas(addEdgesBetweenVerticesDeltas);
						return false;
					}
					if (!ApplyDelta(addVertexDelta))
					{
						ApplyInverseDeltas(addEdgesBetweenVerticesDeltas);
						return false;
					}
					if (!addVertexDelta.IsEmpty())
					{
						addEdgesBetweenVerticesDeltas.Add(addVertexDelta);
					}

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
			else
			{
				if (FMath::Abs(pendingEdgeDirection | edgeDirection) > THRESH_NORMALS_ARE_PARALLEL)
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
		}

		// split edges
		FGraph2DDelta splitEdgeDelta(ID);
		for (auto& kvp : edgesToSplitByVertex)
		{
			splitEdgeDelta.Reset();
			if (!SplitEdge(splitEdgeDelta, NextID, kvp.Key, kvp.Value))
			{
				ApplyInverseDeltas(addEdgesBetweenVerticesDeltas);
				return false;
			}
			if (!splitEdgeDelta.IsEmpty())
			{
				addEdgesBetweenVerticesDeltas.Add(splitEdgeDelta);
			}
		}

		// aggregate all vertices on the pending segment into addedVertices
		addedVertices.Append(addedIDs);
		AggregateAddedVertices(addEdgesBetweenVerticesDeltas, addedVertices);

		// sort the vertices by their distance along the pending segment
		TArray<TPair<float, int32>> sortedNewVertices;
		float length = FVector2D::Distance(pendingEndPosition, pendingStartPosition);

	 	for (int32 vertexID : addedVertices)
		{
			auto vertex = FindVertex(vertexID);
			if (vertex == nullptr)
			{
				ApplyInverseDeltas(addEdgesBetweenVerticesDeltas);
				return false;
			}

			float currentLength = FVector2D::Distance(vertex->Position, pendingStartPosition);
			sortedNewVertices.Add(TPair<float, int32>(currentLength / length, vertexID));
		}
		
		sortedNewVertices.Sort();

		// add an edge connecting the sorted vertices - AddEdgeDirect detects if the edge
		// exists already, so the edges are only created when there is a gap
		FGraph2DDelta updateEdgesDelta(ID);
		int32 numVertices = sortedNewVertices.Num();

		bool bOutForward;
		auto existingEdge = FindEdgeByVertices(StartVertexID, EndVertexID, bOutForward);
		TArray<int32> previousObjIDs;
		bool bDeleteExistingEdge = existingEdge != nullptr && numVertices != 2;

		if (bDeleteExistingEdge)
		{
			DeleteObjectsDirect(updateEdgesDelta, {}, { existingEdge->ID });
			previousObjIDs = { existingEdge->ID };
		}

		for (int32 idx = 0; idx < numVertices-1; idx++)
		{
			int32 startVertexID = sortedNewVertices[idx].Value;
			int32 endVertexID = sortedNewVertices[idx + 1].Value;

			AddEdgeDirect(updateEdgesDelta, NextID, startVertexID, endVertexID, previousObjIDs);
		}

		if (bDeleteExistingEdge)
		{
			// the edges that are added along the segment replace the existing edge,
			// store that as the ParentObjIDs in the delta
			updateEdgesDelta.EdgeAdditions.GenerateKeyArray(
				updateEdgesDelta.EdgeDeletions[existingEdge->ID].ParentObjIDs);

			TArray<int32> verticesBetweenEdge;
			for (int32 idx = 1; idx < numVertices - 1; idx++)
			{
				verticesBetweenEdge.Add(sortedNewVertices[idx].Value);
			}
			AddVerticesToFace(updateEdgesDelta, existingEdge->ID, verticesBetweenEdge);
		}

		if (!updateEdgesDelta.IsEmpty())
		{
			if (!ApplyDelta(updateEdgesDelta))
			{
				ApplyInverseDeltas(addEdgesBetweenVerticesDeltas);
				return false;
			}
			addEdgesBetweenVerticesDeltas.Add(updateEdgesDelta);
		}

		OutDeltas.Append(addEdgesBetweenVerticesDeltas);

		return true;
	}

	bool FGraph2D::DeleteObjects(TArray<FGraph2DDelta>& OutDeltas, int32& NextID, const TArray<int32>& ObjectIDsToDelete, bool bCheckBounds)
	{
		// the ids that are considered for deletion starts with the input arguments and grows based on object connectivity

		// adjacentVertexIDs contains the vertices that are deleted as well as the vertices on edges that are deleted
		TSet<int32> adjacentVertexIDs;
		TSet<int32> vertexIDsToDelete, edgeIDsToDelete, polyIDsToDelete;

		for (int32 signedIDToDelete : ObjectIDsToDelete)
		{
			int32 objectIDToDelete = FMath::Abs(signedIDToDelete);
			switch (GetObjectType(objectIDToDelete))
			{
			case EGraphObjectType::Vertex:
				vertexIDsToDelete.Add(objectIDToDelete);
				break;
			case EGraphObjectType::Edge:
				edgeIDsToDelete.Add(objectIDToDelete);
				break;
			case EGraphObjectType::Polygon:
				// TODO: this should be derived from CalculatePolygons
				polyIDsToDelete.Add(objectIDToDelete);
				if (FGraph2DPolygon* polygon = FindPolygon(objectIDToDelete))
				{
					vertexIDsToDelete.Append(polygon->VertexIDs);
				}
				break;
			default:
				break;
			}
		}
		adjacentVertexIDs = vertexIDsToDelete;

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
			if (edge == nullptr)
			{
				continue;
			}

			// delete connected vertices that are not connected to any edge that is not being deleted
			for (auto vertexID : { edge->StartVertexID, edge->EndVertexID })
			{
				auto vertex = FindVertex(vertexID);
				if (vertex == nullptr)
				{
					continue;
				}

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
				adjacentVertexIDs.Add(FMath::Abs(vertexID));
			}

			polyIDsToDelete.Add(edge->LeftPolyID);
			polyIDsToDelete.Add(edge->RightPolyID);
		}

		for (int32 polyID : polyIDsToDelete)
		{
			auto poly = FindPolygon(polyID);
			if (!poly)
			{
				continue;
			}

			for (int32 edgeID : poly->Edges)
			{
				// already deleted
				if (edgeIDsToDelete.Contains(FMath::Abs(edgeID)))
				{
					continue;
				}

				auto edge = FindEdge(edgeID);
				if (!edge)
				{
					continue;
				}
			}

			for (int32 vertexID : poly->VertexIDs)
			{
				// already deleted
				if (vertexIDsToDelete.Contains(FMath::Abs(vertexID)))
				{
					continue;
				}

				bool bDeleteVertex = true;
				auto vertex = FindVertex(vertexID);
				if (!vertex)
				{
					continue;
				}

				for (int32 edgeID : vertex->Edges)
				{
					if (!edgeIDsToDelete.Contains(FMath::Abs(edgeID)))
					{
						bDeleteVertex = false;
						break;
					}

					auto edge = FindEdge(edgeID);
					if (!edge)
					{
						continue;
					}

					if (!polyIDsToDelete.Contains(edge->LeftPolyID) || !polyIDsToDelete.Contains(edge->RightPolyID))
					{
						bDeleteVertex = false;
						break;
					}
				}
				if (bDeleteVertex)
				{
					vertexIDsToDelete.Add(FMath::Abs(vertexID));
				}
			}
		}

		// if a deleted vertex is contained in the bounds, delete the whole surface graph
		if (bCheckBounds)
		{
			bool bDeletedVertexOnBounds = false;
			auto outerBoundsPoly = FindPolygon(BoundingPolygonID);
			if (outerBoundsPoly)
			{
				for (int32 outerBoundsVertexID : outerBoundsPoly->VertexIDs)
				{
					if (adjacentVertexIDs.Contains(outerBoundsVertexID))
					{
						bDeletedVertexOnBounds = true;
						break;
					}
				}
			}
			if (!bDeletedVertexOnBounds)
			{
				for (int32 polyID : BoundingContainedPolygonIDs)
				{
					auto innerBoundsPoly = FindPolygon(polyID);
					if (!innerBoundsPoly)
					{
						continue;
					}

					for (int32 innerBoundsVertexID : innerBoundsPoly->VertexIDs)
					{
						if (adjacentVertexIDs.Contains(innerBoundsVertexID))
						{
							bDeletedVertexOnBounds = true;
							break;
						}
					}
				}
			}

			if (bDeletedVertexOnBounds)
			{
				vertexIDsToDelete.Reset();
				edgeIDsToDelete.Reset();
				polyIDsToDelete.Reset();

				TArray<int32> deleteVerticesArray, deleteEdgesArray, deletePolysArray;
				Vertices.GenerateKeyArray(deleteVerticesArray);
				Edges.GenerateKeyArray(deleteEdgesArray);
				Polygons.GenerateKeyArray(deletePolysArray);

				vertexIDsToDelete.Append(deleteVerticesArray);
				edgeIDsToDelete.Append(deleteEdgesArray);
				polyIDsToDelete.Append(deletePolysArray);
			}
		}

		FGraph2DDelta deleteDelta(ID);
		if (!DeleteObjectsDirect(deleteDelta, vertexIDsToDelete, edgeIDsToDelete, polyIDsToDelete) || deleteDelta.IsEmpty())
		{
			return false;
		}
		if (!ApplyDelta(deleteDelta))
		{
			return false;
		}
		OutDeltas.Add(deleteDelta);

		if (!CalculatePolygons(OutDeltas, NextID))
		{
			ApplyInverseDeltas(OutDeltas);
			return false;
		}

		// If the deleted objects would result in an empty graph, then create another delta to delete the whole graph.
		if (IsEmpty())
		{
			FGraph2DDelta graphDeletionDelta(ID, EGraph2DDeltaType::Remove);
			if (ApplyDelta(graphDeletionDelta))
			{
				OutDeltas.Add(graphDeletionDelta);
			}
			else
			{
				ApplyInverseDeltas(OutDeltas);
				return false;
			}
		}
		// Otherwise, we expect the bounds to be valid enough to use for validating the previous deletion deltas
		// TODO: allow for auto-updating the bounds, i.e. after joining two exterior colinear edges.
		else
		{
			auto innerBounds = BoundingContainedPolygonIDs;
			for (int32 polyID : polyIDsToDelete)
			{
				if (innerBounds.Contains(polyID))
				{
					innerBounds.Remove(polyID);
				}
			}

			if (!ValidateAgainstBounds())
			{
				ApplyInverseDeltas(OutDeltas);
				return false;
			}

			if (!SetBounds(BoundingPolygonID, innerBounds))
			{
				ApplyInverseDeltas(OutDeltas);
				return false;
			}
		}

		ApplyInverseDeltas(OutDeltas);
		return true;
	}

	bool FGraph2D::DeleteObjectsDirect(FGraph2DDelta &OutDelta, const TSet<int32> &VertexIDs, const TSet<int32> &EdgeIDs, const TSet<int32> &PolyIDs)
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

			OutDelta.EdgeDeletions.Add(edge->ID, FGraph2DObjDelta({ edge->StartVertexID, edge->EndVertexID }));
		}
		
		for (int32 polyID : PolyIDs)
		{
			auto poly = FindPolygon(polyID);
			if (!ensureAlways(poly != nullptr))
			{
				continue;
			}

			OutDelta.PolygonDeletions.Add(poly->ID, FGraph2DObjDelta(poly->VertexIDs));
		}

		return true;
	}

	bool FGraph2D::SplitEdge(FGraph2DDelta &OutDelta, int32 &NextID, int32 EdgeID, int32 SplittingVertexID)
	{
		OutDelta.Reset();

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

		TArray<int32> verticesToAdd = { SplittingVertexID };
		if (!AddVerticesToFace(OutDelta, edge->ID, verticesToAdd))
		{
			return false;
		}

		if (!DeleteObjectsDirect(OutDelta, {}, { EdgeID }))
		{
			return false;
		}

		if (!AddEdgeDirect(OutDelta, NextID, startVertex->ID, SplittingVertexID, { EdgeID }))
		{
			return false;
		}

		if (!AddEdgeDirect(OutDelta, NextID, SplittingVertexID, endVertex->ID, { EdgeID }))
		{
			return false;
		}

		// link the added edges to the previous deleted edge
		OutDelta.EdgeAdditions.GenerateKeyArray(
			OutDelta.EdgeDeletions[EdgeID].ParentObjIDs);

		if (!ApplyDelta(OutDelta))
		{
			return false;
		}

		return true;
	}

	bool FGraph2D::AddVerticesToFace(FGraph2DDelta &OutDelta, int32 EdgeIDToRemove, const TArray<int32>& VertexIDsToAdd)
	{
		auto edge = FindEdge(EdgeIDToRemove);
		if (edge == nullptr)
		{
			return false;
		}
		TSet<int32> polyIDs;
		polyIDs.Add(edge->LeftPolyID);
		polyIDs.Add(edge->RightPolyID);

		TArray<int32> reversed = VertexIDsToAdd;
		Algo::Reverse(reversed);

		for (int32 polyID : polyIDs)
		{
			auto poly = FindPolygon(polyID);
			if (poly == nullptr)
			{
				continue;
			}

			auto& polyIDUpdate = FindOrAddVertexUpdates(OutDelta, polyID);
			int32 posIdx = poly->Edges.Find(edge->ID);
			int32 negIdx = poly->Edges.Find(-edge->ID);

			TArray<int32> idxs = { posIdx, negIdx };
			idxs.Sort();

			int offset = 1;
			for (int32 idx : idxs)
			{
				if (idx == INDEX_NONE)
				{
					continue;
				}

				if (idx == negIdx)
				{
					polyIDUpdate.NextVertexIDs.Insert(reversed, idx + offset);
					offset += VertexIDsToAdd.Num();
				}
				else
				{
					polyIDUpdate.NextVertexIDs.Insert(VertexIDsToAdd, idx + offset);
					offset += VertexIDsToAdd.Num();
				}
			}
		}

		return true;
	}

	bool FGraph2D::RemoveVertexFromFace(FGraph2DDelta& OutDelta, int32 VertexIDToRemove)
	{
		auto vertex = FindVertex(VertexIDToRemove);
		if (vertex == nullptr)
		{
			return false;
		}

		if (vertex->Edges.Num() != 2)
		{
			return false;
		}

		TSet<int32> connectedPolyIDs;
		for (int32 edgeID : vertex->Edges)
		{
			auto edge = FindEdge(edgeID);
			if (!edge)
			{
				continue;
			}

			connectedPolyIDs.Add(FMath::Abs(edge->LeftPolyID));
			connectedPolyIDs.Add(FMath::Abs(edge->RightPolyID));
		}

		for (int32 polyID : connectedPolyIDs)
		{
			auto poly = FindPolygon(polyID);
			if (!poly)
			{
				continue;
			}

			int32 vertexIdx = poly->VertexIDs.Find(VertexIDToRemove);
			auto& polyVertexIDUpdate = FindOrAddVertexUpdates(OutDelta, poly->ID);
			if (ensureAlways(vertexIdx != -1))
			{
				polyVertexIDUpdate.NextVertexIDs.RemoveAt(vertexIdx);
			}
		}

		return true;
	}

	FGraph2DFaceVertexIDsDelta& FGraph2D::FindOrAddVertexUpdates(FGraph2DDelta& OutDelta, int32 PolyID)
	{
		auto poly = FindPolygon(PolyID);
		check(poly);

		if (OutDelta.PolygonIDUpdates.Contains(PolyID))
		{
			return OutDelta.PolygonIDUpdates[PolyID];
		}
		return OutDelta.PolygonIDUpdates.Add(poly->ID, FGraph2DFaceVertexIDsDelta(poly->VertexIDs, poly->VertexIDs));
	}

	bool FGraph2D::MoveVertices(TArray<FGraph2DDelta> &OutDeltas, int32 &NextID, const TMap<int32, FVector2D>& NewVertexPositions)
	{
		int32 numVertices = NewVertexPositions.Num();

		// move vertices
		FGraph2DDelta moveVertexDelta(ID);
		TMap<int32, int32> joinableVertexIDs;
		TArray<int32> dirtyVertexIDs;

		for (auto& kvp : NewVertexPositions)
		{
			int32 vertexID = kvp.Key;
			const FGraph2DVertex *vertex = FindVertex(vertexID);

			if (vertex == nullptr)
			{
				return false;
			}

			FVector2D oldPos = vertex->Position;
			FVector2D newPos = kvp.Value;

			const FGraph2DVertex *existingVertex = FindVertex(newPos);
			if (existingVertex != nullptr && existingVertex->ID != vertexID && !NewVertexPositions.Contains(existingVertex->ID))
			{
				joinableVertexIDs.Add(existingVertex->ID, vertexID);
				dirtyVertexIDs.Add(existingVertex->ID);
			}
			else if (!newPos.Equals(oldPos))
			{
				moveVertexDelta.VertexMovements.Add(vertexID, { oldPos, newPos });
				dirtyVertexIDs.Add(vertex->ID);
			}
		}

		if (!ApplyDelta(moveVertexDelta))
		{
			return false;
		}
		OutDeltas.Add(moveVertexDelta);

		// join vertices
		FGraph2DDelta joinDelta(ID);
		for (auto& kvp : joinableVertexIDs)
		{
			joinDelta.Reset();
			if (!JoinVertices(joinDelta, NextID, kvp.Key, kvp.Value))
			{
				ApplyInverseDeltas(OutDeltas);
				return false;
			}

			OutDeltas.Add(joinDelta);
		}


		// account for edge side-effects

		// find the vertices that exist at the new positions (post join)
		if (!SplitEdgesByVertices(OutDeltas, NextID, dirtyVertexIDs))
		{
			ApplyInverseDeltas(OutDeltas);
			return false;
		}

		TSet<int32> dirtyEdgeIDs;
		for (int32 vertexID : dirtyVertexIDs)
		{
			auto vertex = FindVertex(vertexID);
			if (vertex == nullptr)
			{
				ApplyInverseDeltas(OutDeltas);
				return false;
			}
			for (int32 id : vertex->Edges)
			{
				dirtyEdgeIDs.Add(FMath::Abs(id));
			}
		}

		for (int32 edgeID : dirtyEdgeIDs)
		{
			auto edge = FindEdge(edgeID);
			if (edge == nullptr)
			{
				continue;
			}
			
			// when moving an edge (since its vertex has moved), check whether there are new edge intersections
			// if there aren't any, this will have no effect
			if (!AddEdgesBetweenVertices(OutDeltas, NextID, edge->StartVertexID, edge->EndVertexID))
			{
				ApplyInverseDeltas(OutDeltas);
				return false;
			}
		}

		if (!CalculatePolygons(OutDeltas, NextID))
		{
			ApplyInverseDeltas(OutDeltas);
			return false;
		}

		if (!ValidateAgainstBounds())
		{
			ApplyInverseDeltas(OutDeltas);
			return false;
		}

		ApplyInverseDeltas(OutDeltas);

		return true;
	}

	bool FGraph2D::MoveVerticesDirect(TArray<FGraph2DDelta> &OutDeltas, int32 &NextID, const TMap<int32, FVector2D>& NewVertexPositions)
	{
		int32 numVertices = NewVertexPositions.Num();

		// move vertices
		FGraph2DDelta moveVertexDelta(ID);
		TMap<int32, int32> joinableVertexIDs;
		TArray<int32> dirtyVertexIDs;

		for (auto& kvp : NewVertexPositions)
		{
			int32 vertexID = kvp.Key;
			const FGraph2DVertex *vertex = FindVertex(vertexID);

			if (vertex == nullptr)
			{
				return false;
			}

			FVector2D oldPos = vertex->Position;
			FVector2D newPos = kvp.Value;

			const FGraph2DVertex *existingVertex = FindVertex(newPos);
			if (!newPos.Equals(oldPos))
			{
				moveVertexDelta.VertexMovements.Add(vertexID, { oldPos, newPos });
			}
		}

		if (!ApplyDelta(moveVertexDelta))
		{
			return false;
		}
		OutDeltas.Add(moveVertexDelta);

		if (!ValidateAgainstBounds())
		{
			ApplyInverseDeltas(OutDeltas);
			return false;
		}

		ApplyInverseDeltas(OutDeltas);

		return true;
	}

	bool FGraph2D::MoveVertices(TArray<FGraph2DDelta> &OutDeltas, int32 &NextID, const TArray<int32> &VertexIDs, const FVector2D& Offset)
	{
		TMap<int32, FVector2D> newVertexPositions;
		int32 numVertices = VertexIDs.Num();
		for (int32 i = 0; i < numVertices; ++i)
		{
			int32 vertexID = VertexIDs[i];
			const FGraph2DVertex *vertex = FindVertex(vertexID);

			if (vertex == nullptr)
			{
				return false;
			}

			FVector2D newVertexPosition = vertex->Position + Offset;
			newVertexPositions.Add(vertexID, newVertexPosition);
		}

		return MoveVertices(OutDeltas, NextID, newVertexPositions);
	}

	bool FGraph2D::SetBounds(int32 &OuterBoundsID, TArray<int32>& InnerBoundsIDs)
	{
		BoundingPolygonID = OuterBoundsID;
		BoundingContainedPolygonIDs = InnerBoundsIDs;

		return true;
	}

	bool FGraph2D::PopulateFromPolygons(TArray<FGraph2DDelta>& OutDeltas, int32& NextID, TMap<int32, TArray<FVector2D>>& InitialPolygons, TMap<int32, TArray<int32>>& FaceToVertices,
			TMap<int32, int32>& OutFaceToPoly, TMap<int32, int32>& OutGraphToSurfaceVertices, bool bUseAsBounds, int32& OutRootPolyID)
	{
		if (InitialPolygons.Num() == 0)
		{
			return false;
		}

		TArray<FGraph2DDelta> appliedDeltas;

		// Populate the target graph with the polygon vertices
		TMap<int32, TSet<int32>> faceToSurfaceVertices;

		for (auto& kvp : InitialPolygons)
		{
			int32 idx = 0;
			FGraph2DDelta& addVerticesDelta = appliedDeltas.Add_GetRef(FGraph2DDelta(ID));

			faceToSurfaceVertices.Add(kvp.Key, TSet<int32>());
			const TArray<FVector2D>& polygonVertices = kvp.Value;

			for (const FVector2D& polygonVertex : polygonVertices)
			{
				const FGraph2DVertex *existingVertex = FindVertex(polygonVertex);
				if (existingVertex == nullptr)
				{
					if (kvp.Key != MOD_ID_NONE)
					{
						OutGraphToSurfaceVertices.Add(FaceToVertices[kvp.Key][idx], NextID);
					}

					if (!AddVertexDirect(addVerticesDelta, NextID, polygonVertex))
					{
						ApplyInverseDeltas(appliedDeltas);
						return false;
					}
				}
				else if (kvp.Key != MOD_ID_NONE)
				{
					OutGraphToSurfaceVertices.Add(FaceToVertices[kvp.Key][idx], existingVertex->ID);
					faceToSurfaceVertices[kvp.Key].Add(existingVertex->ID);
				}
				idx++;
			}

			if (!ApplyDelta(addVerticesDelta))
			{
				ApplyInverseDeltas(appliedDeltas);
				return false;
			}

			AggregateAddedVertices({ addVerticesDelta }, faceToSurfaceVertices[kvp.Key]);
		}

		TSet<int32> addedEdges, addedVertices, addedPolys;
		for (auto& kvp : InitialPolygons)
		{
			TArray<FGraph2DDelta> addEdgeDeltas;
			TArray<FVector2D> polygonVertices;
			UModumateGeometryStatics::GetUniquePoints2D(kvp.Value, polygonVertices);

			int32 numPolygonVerts = polygonVertices.Num();
			for (int32 polyPointIdxA = 0; polyPointIdxA < numPolygonVerts; ++polyPointIdxA)
			{
				int32 polyPointIdxB = (polyPointIdxA + 1) % numPolygonVerts;
				const FGraph2DVertex* polyVertexA = FindVertex(polygonVertices[polyPointIdxA]);
				const FGraph2DVertex* polyVertexB = FindVertex(polygonVertices[polyPointIdxB]);
				int32 polyVertexIDA = polyVertexA ? polyVertexA->ID : MOD_ID_NONE;
				int32 polyVertexIDB = polyVertexB ? polyVertexB->ID : MOD_ID_NONE;

				FGraph2DDelta& addEdgesDelta = appliedDeltas.Add_GetRef(FGraph2DDelta(ID));

				bool bSameDirection;
				if (auto edge = FindEdgeByVertices(polyVertexIDA, polyVertexIDB, bSameDirection))
				{
					addedEdges.Add(edge->ID);
				}
				else
				{
					if (!AddEdgeDirect(addEdgesDelta, NextID, polyVertexIDA, polyVertexIDB))
					{
						ApplyInverseDeltas(appliedDeltas);
						return false;
					}

					if (!ApplyDelta(addEdgesDelta) || !ValidateAgainstBounds())
					{
						ApplyInverseDeltas(appliedDeltas);
						return false;
					}
					AggregateAddedObjects({ addEdgesDelta }, addedVertices, addedEdges, addedPolys);
				}
			}
		}

		if (!CalculatePolygons(appliedDeltas, NextID))
		{
			ApplyInverseDeltas(appliedDeltas);
			return false;
		}

		// Clean polygons now, to ensure that perimeters and interior/exterior values are up-to-date.
		CleanDirtyObjects(true);

		if (!FindVerticesAndPolygons(InitialPolygons, OutFaceToPoly, OutGraphToSurfaceVertices, addedEdges.Array()))
		{
			ApplyInverseDeltas(appliedDeltas);
			return false;
		}

		FGraph2DPolygon* outerPolygon = FindPolygon(GetOuterBoundsPolygonID());
		if (outerPolygon)
		{
			OutRootPolyID = outerPolygon->ID;
		}

		// Now, optionally set the bounds based on the resulting polygons
		if (bUseAsBounds)
		{
			// We require that the graph has a well-defined root polygon (one that contains all the others) in order to set bounds
			if (outerPolygon == nullptr)
			{
				ApplyInverseDeltas(appliedDeltas);
				return false;
			}
			int32 outerBounds = outerPolygon->ID;

			TArray<int32> innerBounds;
			for (auto& kvp : Polygons)
			{
				if ((kvp.Key != outerPolygon->ID) && kvp.Value.bInterior && kvp.Value.ContainingPolyID != MOD_ID_NONE)
				{
					innerBounds.Add(kvp.Key);
				}
			}

			if (!SetBounds(outerBounds, innerBounds))
			{
				ApplyInverseDeltas(appliedDeltas);
				return false;
			}
			if (!ValidateAgainstBounds())
			{
				ApplyInverseDeltas(appliedDeltas);
				return false;
			}
		}

		int32 numVerts = OutGraphToSurfaceVertices.Num();
		// return graph in its original state
		OutDeltas.Append(appliedDeltas);
		ApplyInverseDeltas(appliedDeltas);

		return true;
	}

	bool FGraph2D::FindVerticesAndPolygons(const TMap<int32, TArray<FVector2D>>& InitialPolygons, TMap<int32, int32>& OutFaceToPoly, TMap<int32, int32>& OutGraphToSurfaceVertices, const TArray<int32>& InEdges)
	{
		
		// Find the interior polygons that correspond to the volume graph polygons

		TMap<int32, TSet<int32>> faceToSurfaceEdges;
		for (auto& kvp : InitialPolygons)
		{
			const TArray<FVector2D>& polygonVertices = kvp.Value;
			int32 numPolygonVerts = polygonVertices.Num();

			faceToSurfaceEdges.Add(kvp.Key, TSet<int32>());
			for (int32 polyPointIdxA = 0; polyPointIdxA < numPolygonVerts; ++polyPointIdxA)
			{
				int32 polyPointIdxB = (polyPointIdxA + 1) % numPolygonVerts;

				FVector2D startPosition = polygonVertices[polyPointIdxA];
				FVector2D endPosition = polygonVertices[polyPointIdxB];
				for (int32 id : InEdges)
				{
					auto edge = FindEdge(id);
					if (edge == nullptr)
					{
						continue;
					}

					auto startVertex = FindVertex(edge->StartVertexID);
					auto endVertex = FindVertex(edge->EndVertexID);
					if (startVertex == nullptr || endVertex == nullptr)
					{
						continue;
					}

					FVector2D startOnSegment = FMath::ClosestPointOnSegment2D(startVertex->Position, startPosition, endPosition);
					FVector2D endOnSegment = FMath::ClosestPointOnSegment2D(endVertex->Position, startPosition, endPosition);

					if (startOnSegment.Equals(startVertex->Position, Epsilon) &&
						endOnSegment.Equals(endVertex->Position, Epsilon))
					{
						faceToSurfaceEdges[kvp.Key].Add(edge->ID);
					}
				}
			}
		}

		TMap<int32, int32> graphFaceToSurfacePoly = OutFaceToPoly;
		for (auto& kvp : faceToSurfaceEdges)
		{
			bool bStart = true;
			TSet<int32> surfacePolyCandidates;
			for (int32 edgeID : kvp.Value)
			{
				auto edge = FindEdge(edgeID);
				if (edge == nullptr)
				{
					continue;
				}

				auto leftPoly = FindPolygon(edge->LeftPolyID);
				auto rightPoly = FindPolygon(edge->RightPolyID);
				
				TSet<int32> current;
				if (leftPoly && leftPoly->bInterior)
				{
					current.Add(leftPoly->ID);
				}
				if (rightPoly && rightPoly->bInterior)
				{
					current.Add(rightPoly->ID);
				}

				if (bStart)
				{
					surfacePolyCandidates = current;
					bStart = false;
				}
				else
				{
					surfacePolyCandidates = surfacePolyCandidates.Intersect(current);
					if (surfacePolyCandidates.Num() == 0)
					{
						return false;
					}
				}
			}

			if (surfacePolyCandidates.Num() != 1)
			{
				return false;
			}
			// TODO: exterior is MOD_ID_NONE
			graphFaceToSurfacePoly.Add(kvp.Key, *surfacePolyCandidates.CreateConstIterator());
		}

		OutFaceToPoly = graphFaceToSurfacePoly;

		BoundingContainedPolygonIDs.Reset();
		for (auto& kvp : graphFaceToSurfacePoly)
		{
			auto poly = FindPolygon(kvp.Value);
			if (!poly)
			{
				continue;
			}

			BoundingContainedPolygonIDs.Add(poly->ID);
		}

		return true;
	}

	bool FGraph2D::JoinVertices(FGraph2DDelta &OutDelta, int32 &NextID, int32 SavedVertexID, int32 RemovedVertexID)
	{
		auto joinVertex = FindVertex(SavedVertexID);
		auto oldVertex = FindVertex(RemovedVertexID);

		if (joinVertex == nullptr || oldVertex == nullptr)
		{
			return false;
		}
		if (joinVertex->ID == oldVertex->ID)
		{
			return false;
		}

		// there is no way to reassign an edge's vertexIDs, so delete the edges connected to the vertex
		if (!DeleteObjectsDirect(OutDelta, { oldVertex->ID }, TSet<int32>(oldVertex->Edges)))
		{
			return false;
		}

		// add back edges with updated vertices
		for (int32 edgeID : oldVertex->Edges)
		{
			auto edge = FindEdge(edgeID);
			if (edge == nullptr)
			{
				return false;
			}

			// find the edge's vertexID that is not being removed
			int32 otherVertexID = (oldVertex->ID == edge->StartVertexID) ? edge->EndVertexID : edge->StartVertexID;

			// new edge connects the other vertex of the edge to the vertex that is saved in the join
			if (!AddEdgeDirect(OutDelta, NextID, otherVertexID, SavedVertexID, { edgeID }))
			{
				return false;
			}

			TSet<int32> polys;
			polys.Add(edge->LeftPolyID);
			polys.Add(edge->RightPolyID);
			for (int32 polyID : polys)
			{
				auto poly = FindPolygon(polyID);
				if (!poly)
				{
					continue;
				}

				int32 oldVertexIdx = poly->VertexIDs.Find(RemovedVertexID);
				if (oldVertexIdx == INDEX_NONE)
				{
					return false;
				}

				auto& polyIDUpdate = FindOrAddVertexUpdates(OutDelta, poly->ID);
				polyIDUpdate.NextVertexIDs[oldVertexIdx] = SavedVertexID;
			}
		}

		if (!ApplyDelta(OutDelta))
		{
			return false;
		}

		return true;
	}

	bool FGraph2D::CalculatePolygons(TArray<FGraph2DDelta> &OutDeltas, int32 &NextID)
	{
		FGraph2DDelta updatePolygonsDelta(ID);

		// Aggregate edges that have been dirtied since the last time polygons were calculated
		TSet<FGraphSignedID> dirtyEdges;
		for (auto& edgekvp : Edges)
		{
			FGraph2DEdge& edge = edgekvp.Value;
			if (edge.bPolygonDirty)
			{
				// TODO: remove unnecessary debug validity checks, this should be thoroughly enforced during direct changes and delta applications
				if (!ensureAlways(edge.bValid && !edge.bDerivedDataDirty))
				{
					return false;
				}

				dirtyEdges.Add(edge.ID);
				dirtyEdges.Add(-edge.ID);
			}
		}


		if (bDebugCheck)
		{
			for (auto &kvp : Vertices)
			{
				FGraph2DVertex& vertex = kvp.Value;
				if (!ensureAlways(vertex.bValid && !vertex.bDerivedDataDirty))
				{
					return false;
				}
			}
		}

		TSet<FGraphSignedID> visitedEdges;
		TArray<FGraphSignedID> curPolyEdges;
		TArray<int32> curVertexIDs;
		bool bPolyInterior;

		// iterate through the edges to find every polygon
		for (FGraphSignedID curEdgeID : dirtyEdges)
		{
			if (!TraverseEdges(curEdgeID, curPolyEdges, curVertexIDs, bPolyInterior, visitedEdges))
			{
				continue;
			}

			bool bEdgesOnSamePoly = true;
			bool bPolyIDAssigned = false;
			int32 polyID = MOD_ID_NONE;

			TSet<int32> previousPolys;
			for (FGraphSignedID edgeID : curPolyEdges)
			{
				if (FGraph2DEdge *edge = FindEdge(edgeID))
				{
					// determine which side of the edge this new polygon is on
					int32 adjacentPolyID = (edgeID > 0) ? edge->LeftPolyID : edge->RightPolyID;
					if (adjacentPolyID != MOD_ID_NONE)
					{
						previousPolys.Add(adjacentPolyID);
					}

					if (!bPolyIDAssigned)
					{
						polyID = adjacentPolyID;
						bPolyIDAssigned = true;
					}
					bEdgesOnSamePoly = bEdgesOnSamePoly && (polyID == adjacentPolyID);
				}
			}

			if (!bEdgesOnSamePoly || polyID == MOD_ID_NONE)
			{
				int32 addedPolyID = NextID;
				updatePolygonsDelta.AddNewPolygon(curVertexIDs, NextID, previousPolys.Array());

				for (int32 previousID : previousPolys)
				{
					auto polygon = FindPolygon(previousID);
					if (polygon == nullptr)
					{
						continue;
					}

					if (BoundingContainedPolygonIDs.Find(previousID) != INDEX_NONE || BoundingPolygonID == previousID)
					{
						return false;
					}
					updatePolygonsDelta.PolygonDeletions.Add(previousID, FGraph2DObjDelta(polygon->VertexIDs, { addedPolyID }));
				}
			}
			else if (bEdgesOnSamePoly)
			{
				auto& update = FindOrAddVertexUpdates(updatePolygonsDelta, polyID);
				update.NextVertexIDs = curVertexIDs;
			}
		}


		// Clear edge-polygon dirty flags, so that subsequent calls to CalculatePolygons would be no-ops
		for (auto& edgekvp : Edges)
		{
			edgekvp.Value.bPolygonDirty = false;
		}

		if (!updatePolygonsDelta.IsEmpty())
		{
			if (!ApplyDelta(updatePolygonsDelta))
			{
				return false;
			}
			OutDeltas.Add(updatePolygonsDelta);
		}

		FGraph2DDelta deletePolysDelta(ID);
		for (auto& dirtykvp : Polygons)
		{
			FGraph2DPolygon& dirtyPoly = dirtykvp.Value;
			if (!dirtyPoly.bDerivedDataDirty)
			{
				continue;
			}

			bool bDeletePoly = false;
			for (int32 idx = 0; idx < dirtyPoly.VertexIDs.Num(); idx++)
			{
				int32 startVertexID = dirtyPoly.VertexIDs[idx];
				int32 endVertexID = dirtyPoly.VertexIDs[(idx + 1) % dirtyPoly.VertexIDs.Num()];

				bool bOutForward;
				auto edge = FindEdgeByVertices(startVertexID, endVertexID, bOutForward);
				
				if (!edge)
				{
					bDeletePoly = true;
					break;
				}

				int32 currentPolyID = bOutForward ? edge->LeftPolyID : edge->RightPolyID;

				auto poly = FindPolygon(currentPolyID);
				if (!poly || poly->ID == dirtyPoly.ID)
				{
					continue;
				}
				else
				{
					bDeletePoly = true;
					break;
				}

			}

			if (bDeletePoly)
			{
				deletePolysDelta.PolygonDeletions.Add(dirtykvp.Key, FGraph2DObjDelta(dirtykvp.Value.VertexIDs));
			}
		}

		if (!deletePolysDelta.IsEmpty())
		{
			if (!ApplyDelta(deletePolysDelta))
			{
				return false;
			}
			OutDeltas.Add(deletePolysDelta);
		}

		return true;
	}

	bool FGraph2D::IsVertexIDBounding(int32 VertexID)
	{
		auto boundingPoly = FindPolygon(BoundingPolygonID);
		if (boundingPoly && boundingPoly->VertexIDs.Find(VertexID) != INDEX_NONE)
		{
			return true;
		}

		for (int32 innerPolyID : BoundingContainedPolygonIDs)
		{
			auto innerPoly = FindPolygon(innerPolyID);
			if (innerPoly && innerPoly->VertexIDs.Find(VertexID) != INDEX_NONE)
			{
				return true;
			}
		}

		return false;
	}
}
