// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Graph/Graph2D.h"

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
		TSet<int32> addedVertexIDs;

		// start by adding or finding the vertices at the input positions
		for (auto& position : { StartPosition, EndPosition })
		{
			FGraph2DDelta addVertexDelta(ID);
			addedVertexIDs.Reset();

			const FGraph2DVertex *existingVertex = FindVertex(position);

			if (existingVertex == nullptr)
			{
				if (!AddVertexDirect(addVertexDelta, NextID, position))
				{
					return false;
				}

				AggregateAddedVertices({ addVertexDelta }, addedVertexIDs);

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

			if (!addVertexDelta.IsEmpty())
			{
				if (!ApplyDelta(addVertexDelta))
				{
					ApplyInverseDeltas(OutDeltas);
					return false;
				}

				OutDeltas.Add(addVertexDelta);
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

	bool FGraph2D::DeleteObjects(TArray<FGraph2DDelta> &OutDeltas, int32 &NextID, const TArray<int32> &VertexIDs, const TArray<int32> &EdgeIDs)
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

		if (!ValidateAgainstBounds())
		{
			ApplyInverseDeltas(OutDeltas);
			return false;
		}

		ApplyInverseDeltas(OutDeltas);
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

			OutDelta.EdgeDeletions.Add(edge->ID, FGraph2DObjDelta({ edge->StartVertexID, edge->EndVertexID }));

			// Since polygons can't be directly added or deleted, maintaining polygons is a side-effect that
			// is accomplished here.
			for (int32 polyID : { edge->LeftPolyID, edge->RightPolyID })
			{
				auto poly = FindPolygon(polyID);
				if (poly != nullptr && !OutDelta.PolygonDeletions.Contains(poly->ID))
				{
					OutDelta.PolygonDeletions.Add(poly->ID, FGraph2DObjDelta(poly->VertexIDs, {}));
				}
			}
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
				moveVertexDelta.VertexMovements.Add(vertexID, TPair<FVector2D, FVector2D>(oldPos, newPos));
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
				moveVertexDelta.VertexMovements.Add(vertexID, TPair<FVector2D, FVector2D>(oldPos, newPos));
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

	bool FGraph2D::SetBounds(TArray<FGraph2DDelta> &OutDeltas, TPair<int32, TArray<int32>> &OuterBounds, TMap<int32, TArray<int32>> &InnerBounds)
	{
		FGraph2DDelta boundsDelta(ID);

		// test to make sure provided polygons are valid
		if (OuterBounds.Value.Num() < 3)
		{
			return false;
		}
		for (auto& kvp : InnerBounds)
		{
			if (kvp.Value.Num() < 3)
			{
				return false;
			}
		}

		FBoundsUpdate currentBounds;
		currentBounds.OuterBounds = BoundingPolygon;
		currentBounds.InnerBounds = BoundingContainedPolygons;

		FBoundsUpdate nextBounds;
		nextBounds.OuterBounds = OuterBounds;
		nextBounds.InnerBounds = InnerBounds;

		boundsDelta.BoundsUpdates = TPair<FBoundsUpdate, FBoundsUpdate>(currentBounds, nextBounds);

		OutDeltas.Add(boundsDelta);

		return true;
	}

	bool FGraph2D::PopulateFromPolygons(TArray<FGraph2DDelta>& OutDeltas, int32& NextID, TArray<TArray<FVector2D>>& InitialPolygons, bool bUseAsBounds)
	{
		if (!IsEmpty() || (InitialPolygons.Num() == 0))
		{
			return false;
		}

		TArray<FGraph2DDelta> appliedDeltas;

		// Populate the target graph with the polygon vertices
		FGraph2DDelta& addVerticesDelta = appliedDeltas.Add_GetRef(FGraph2DDelta(ID));
		for (int32 polygonIdx = 0; polygonIdx < InitialPolygons.Num(); polygonIdx++)
		{
			const TArray<FVector2D>& polygonVertices = InitialPolygons[polygonIdx];

			for (const FVector2D& polygonVertex : polygonVertices)
			{
				if (!AddVertexDirect(addVerticesDelta, NextID, polygonVertex))
				{
					ApplyInverseDeltas(appliedDeltas);
					return false;
				}
			}
		}

		if (!ApplyDelta(addVerticesDelta))
		{
			ApplyInverseDeltas(appliedDeltas);
			return false;
		}

		FGraph2DDelta& addEdgesDelta = appliedDeltas.Add_GetRef(FGraph2DDelta(ID));
		for (int32 polygonIdx = 0; polygonIdx < InitialPolygons.Num(); polygonIdx++)
		{
			const TArray<FVector2D>& polygonVertices = InitialPolygons[polygonIdx];
			int32 numPolygonVerts = polygonVertices.Num();
			for (int32 polyPointIdxA = 0; polyPointIdxA < numPolygonVerts; ++polyPointIdxA)
			{
				int32 polyPointIdxB = (polyPointIdxA + 1) % numPolygonVerts;
				const FGraph2DVertex* polyVertexA = FindVertex(polygonVertices[polyPointIdxA]);
				const FGraph2DVertex* polyVertexB = FindVertex(polygonVertices[polyPointIdxB]);
				int32 polyVertexIDA = polyVertexA ? polyVertexA->ID : MOD_ID_NONE;
				int32 polyVertexIDB = polyVertexB ? polyVertexB->ID : MOD_ID_NONE;

				if (!AddEdgeDirect(addEdgesDelta, NextID, polyVertexA->ID, polyVertexB->ID))
				{
					ApplyInverseDeltas(appliedDeltas);
					return false;
				}
			}
		}

		if (!ApplyDelta(addEdgesDelta))
		{
			ApplyInverseDeltas(appliedDeltas);
			return false;
		}

		if (!CalculatePolygons(appliedDeltas, NextID))
		{
			ApplyInverseDeltas(appliedDeltas);
			return false;
		}

		// Clean polygons now, to ensure that perimeters and interior/exterior values are up-to-date.
		CleanDirtyObjects(true);

		// Make sure that the calculated polygons correspond to those that were intended to populate the graph
		int32 numInteriorPolygons = 0;
		for (auto& kvp : Polygons)
		{
			if (kvp.Value.bInterior)
			{
				numInteriorPolygons++;
			}
		}

		if (numInteriorPolygons != InitialPolygons.Num())
		{
			ApplyInverseDeltas(appliedDeltas);
			return false;
		}

		// Now, optionally set the bounds based on the resulting polygons
		if (bUseAsBounds)
		{
			// We require that the graph has a well-defined root polygon (one that contains all the others) in order to set bounds
			FGraph2DPolygon* outerPolygon = GetRootPolygon();
			if (outerPolygon == nullptr)
			{
				ApplyInverseDeltas(appliedDeltas);
				return false;
			}
			TPair<int32, TArray<int32>> outerBounds(outerPolygon->ID, outerPolygon->CachedPerimeterVertexIDs);

			TMap<int32, TArray<int32>> innerBounds;
			for (auto& kvp : Polygons)
			{
				if ((kvp.Key != outerPolygon->ID) && kvp.Value.bInterior)
				{
					innerBounds.Add(kvp.Key, kvp.Value.CachedPerimeterVertexIDs);
				}
			}

			if (!SetBounds(appliedDeltas, outerBounds, innerBounds) || !ValidateAgainstBounds())
			{
				ApplyInverseDeltas(appliedDeltas);
				return false;
			}
		}

		// return graph in its original state
		OutDeltas.Append(appliedDeltas);
		ApplyInverseDeltas(appliedDeltas);

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
			if (!AddEdgeDirect(OutDelta, NextID, otherVertexID, SavedVertexID))
			{
				return false;
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
				}
			}

			int32 addedPolyID = NextID;
			updatePolygonsDelta.AddNewPolygon(curVertexIDs, NextID, previousPolys.Array());

			for (int32 previousID : previousPolys)
			{
				auto polygon = FindPolygon(previousID);
				if (polygon == nullptr)
				{
					continue;
				}

				updatePolygonsDelta.PolygonDeletions.Add(previousID, FGraph2DObjDelta(polygon->VertexIDs, { addedPolyID }));
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

#if 0
		// Clean all graph object derived data (including polygons, new and old), so we can use perimeters to calculate containment.
		CleanDirtyObjects(true);

		// determine which polygons are inside of others
		FGraph2DDelta parentIDUpdatesDelta(ID);
		for (auto &childKVP : Polygons)
		{
			FGraph2DPolygon &childPoly = childKVP.Value;

			for (auto &parentKVP : Polygons)
			{
				FGraph2DPolygon &parentPoly = parentKVP.Value;

				if (childPoly.IsInside(parentPoly))
				{
					bool bNoParent = (childPoly.ParentID == MOD_ID_NONE);

					FGraph2DPolygon* currentBestParentPoly = FindPolygon(childPoly.ParentID);
					if (parentIDUpdatesDelta.PolygonParentIDUpdates.Contains(childPoly.ID))
					{
						currentBestParentPoly = FindPolygon(parentIDUpdatesDelta.PolygonParentIDUpdates[childPoly.ID].Value);
					}
					bool bBetterParent = !bNoParent && currentBestParentPoly && parentPoly.IsInside(*currentBestParentPoly);

					// if the child polygon doesn't have a parent, then set it to the first polygon it's contained in
					// otherwise, see if the new parent is more appropriate
					if (bNoParent || bBetterParent)
					{
						auto& parentIDUpdate = parentIDUpdatesDelta.PolygonParentIDUpdates.FindOrAdd(childPoly.ID);
						parentIDUpdate.Key = childPoly.ParentID;
						parentIDUpdate.Value = parentPoly.ID;
					}
				}
			}
		}

		if (!parentIDUpdatesDelta.IsEmpty())
		{
			if (!ApplyDelta(parentIDUpdatesDelta))
			{
				return false;
			}

			OutDeltas.Add(parentIDUpdatesDelta);
		}
#endif

		return true;
	}
}
