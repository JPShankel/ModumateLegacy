// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "Graph/Graph2D.h"

#include "ModumateCore/ModumateFunctionLibrary.h"

#define LOG_MDM_GRAPH 0

#if LOG_MDM_GRAPH
#define GA_LOG(S,...) UE_LOG(LogTemp,Display,S,__VA_ARGS__)
#else
#define GA_LOG(S,...)
#endif


namespace Modumate
{
	FGraph2D::FGraph2D(int32 InID, float InEpsilon, bool bInDebugCheck)
		: Epsilon(InEpsilon)
		, bDebugCheck(bInDebugCheck)
		, ID(InID)
	{
		Reset();
	}

	void FGraph2D::Reset()
	{
		NextEdgeID = NextVertexID = NextPolyID = 1;
		bDirty = false;

		Edges.Reset();
		Vertices.Reset();
		Polygons.Reset();
		DirtyEdges.Reset();

		ClearBounds();
	}

	bool FGraph2D::IsEmpty() const
	{
		return (Edges.Num() == 0) && (Vertices.Num() == 0) && (Polygons.Num() == 0);
	}

	FGraph2DEdge* FGraph2D::FindEdge(FGraphSignedID EdgeID) 
	{ 
		return Edges.Find(FMath::Abs(EdgeID)); 
	}

	const FGraph2DEdge* FGraph2D::FindEdge(FGraphSignedID EdgeID) const 
	{ 
		return Edges.Find(FMath::Abs(EdgeID)); 
	}

	FGraph2DEdge *FGraph2D::FindEdgeByVertices(int32 VertexIDA, int32 VertexIDB, bool &bOutForward)
	{
		return const_cast<FGraph2DEdge*>(const_cast<const FGraph2D*>(this)->FindEdgeByVertices(VertexIDA, VertexIDB, bOutForward));
	}

	const FGraph2DEdge* FGraph2D::FindEdgeByVertices(int32 VertexIDA, int32 VertexIDB, bool &bOutForward) const
	{
		FGraphVertexPair edgeKey = FGraphVertexPair::MakeEdgeKey(VertexIDA, VertexIDB);
		if (const int32 *edgeIDPtr = EdgeIDsByVertexPair.Find(edgeKey))
		{
			if (const FGraph2DEdge *edge = Edges.Find(*edgeIDPtr))
			{
				bOutForward = (edge->StartVertexID == VertexIDA);
				return edge;
			}
		}

		return nullptr;
	}

	FGraph2DVertex* FGraph2D::FindVertex(int32 VertexID)
	{ 
		return Vertices.Find(VertexID); 
	}

	const FGraph2DVertex* FGraph2D::FindVertex(int32 VertexID) const
	{ 
		return Vertices.Find(VertexID); 
	}

	FGraph2DPolygon* FGraph2D::FindPolygon(int32 PolygonID)
	{ 
		return Polygons.Find(PolygonID); 
	}

	const FGraph2DPolygon* FGraph2D::FindPolygon(int32 PolygonID) const
	{ 
		return Polygons.Find(PolygonID); 
	}

	FGraph2DVertex* FGraph2D::FindVertex(const FVector2D &Position)
	{
		for (auto& vertexkvp : Vertices)
		{
			auto& vertex = vertexkvp.Value;
			if (Position.Equals(vertex.Position, Epsilon))
			{
				return &vertex;
			}
		}
		return nullptr;
	}

	bool FGraph2D::ContainsObject(int32 GraphObjID, EGraphObjectType GraphObjectType) const
	{
		switch (GraphObjectType)
		{
		case EGraphObjectType::Vertex:
			return Vertices.Contains(GraphObjID);
		case EGraphObjectType::Edge:
			return Edges.Contains(GraphObjID);
		case EGraphObjectType::Polygon:
			return Polygons.Contains(GraphObjID);
		default:
			return false;
		}
	}

	bool FGraph2D::GetEdgeAngle(FGraphSignedID EdgeID, float &outEdgeAngle)
	{
		FGraph2DEdge *edge = FindEdge(EdgeID);
		if (edge && edge->bValid)
		{
			outEdgeAngle = edge->Angle;
			if (EdgeID < 0)
			{
				outEdgeAngle = FRotator::ClampAxis(outEdgeAngle + 180.0f);
			}

			return true;
		}

		return false;
	}

	FGraph2DVertex *FGraph2D::AddVertex(const FVector2D &Position, int32 InID)
	{
		int32 newID = InID;
		if (newID == 0)
		{
			newID = NextVertexID++;
		}

		if (Vertices.Contains(newID))
		{
			return nullptr;
		}

		FGraph2DVertex &newVertex = Vertices.Add(newID, FGraph2DVertex(newID, this, Position));
		bDirty = true;
		newVertex.Dirty(false);

		return &newVertex;
	}

	FGraph2DEdge *FGraph2D::AddEdge(int32 StartVertexID, int32 EndVertexID, int32 InID)
	{
		int32 newID = InID;
		if (newID == 0)
		{
			newID = NextEdgeID++;
		}

		if (Edges.Contains(newID))
		{
			return nullptr;
		}
		FGraph2DVertex *startVertex = FindVertex(StartVertexID);
		FGraph2DVertex *endVertex = FindVertex(EndVertexID);
		if (!ensureAlways(startVertex != nullptr && endVertex != nullptr))
		{
			return nullptr;
		}

		FGraph2DEdge &newEdge = Edges.Add(newID, FGraph2DEdge(newID, this, StartVertexID, EndVertexID));

		bDirty = true;
		newEdge.Dirty(false);
		DirtyEdges.Add(newID);
		DirtyEdges.Add(-newID);

		startVertex->AddEdge(newID);
		endVertex->AddEdge(-newID);

		EdgeIDsByVertexPair.Add(FGraphVertexPair::MakeEdgeKey(newEdge.StartVertexID, newEdge.EndVertexID), newEdge.ID);

		return &newEdge;
	}

	FGraph2DPolygon *FGraph2D::AddPolygon(TArray<int32> &VertexIDs, int32 InID, bool bInterior)
	{
		if (InID == MOD_ID_NONE)
		{
			return nullptr;
		}

		if (Polygons.Contains(InID))
		{
			return nullptr;
		}

		FGraph2DPolygon &newPoly = Polygons.Add(InID, FGraph2DPolygon(InID, this, VertexIDs));
		newPoly.bInterior = bInterior;

		bDirty = true;
		newPoly.Dirty(false);

		return &newPoly;
	}

	bool FGraph2D::RemoveVertex(int32 VertexID)
	{
		FGraph2DVertex *vertexToRemove = FindVertex(VertexID);
		if (!vertexToRemove)
		{
			return false;
		}

		for (FGraphSignedID connectedEdgeID : vertexToRemove->Edges)
		{
			bool bEdgeStartsFromVertex = (connectedEdgeID > 0);
			FGraph2DEdge *connectedEdge = FindEdge(connectedEdgeID);
			if (ensureAlways(connectedEdge))
			{
				// Remove the edge from the vertex pair mapping, because it's useless unless both vertices are valid
				if ((connectedEdge->StartVertexID != MOD_ID_NONE) && (connectedEdge->EndVertexID != MOD_ID_NONE))
				{
					EdgeIDsByVertexPair.Remove(FGraphVertexPair::MakeEdgeKey(connectedEdge->StartVertexID, connectedEdge->EndVertexID));
				}

				if (bEdgeStartsFromVertex)
				{
					connectedEdge->StartVertexID = MOD_ID_NONE;
				}
				else
				{
					connectedEdge->EndVertexID = MOD_ID_NONE;
				}

				bDirty = true;
			}
		}

		Vertices.Remove(VertexID);

		return true;
	}

	bool FGraph2D::RemoveEdge(int32 EdgeID)
	{
		EdgeID = FMath::Abs(EdgeID);
		FGraph2DEdge *edgeToRemove = FindEdge(EdgeID);
		if (!edgeToRemove)
		{
			return false;
		}

		if (edgeToRemove->StartVertexID != MOD_ID_NONE)
		{
			FGraph2DVertex *startVertex = FindVertex(edgeToRemove->StartVertexID);
			if (ensureAlways(startVertex && startVertex->RemoveEdge(EdgeID)))
			{
				bDirty = true;
			}
		}

		if (edgeToRemove->EndVertexID != MOD_ID_NONE)
		{
			FGraph2DVertex *endVertex = FindVertex(edgeToRemove->EndVertexID);
			if (ensureAlways(endVertex && endVertex->RemoveEdge(-EdgeID)))
			{
				bDirty = true;
			}
		}

		// Remove the edge from the vertex pair mapping if it's still in there
		if ((edgeToRemove->StartVertexID != MOD_ID_NONE) && (edgeToRemove->EndVertexID != MOD_ID_NONE))
		{
			EdgeIDsByVertexPair.Remove(FGraphVertexPair::MakeEdgeKey(edgeToRemove->StartVertexID, edgeToRemove->EndVertexID));
		}

		Edges.Remove(EdgeID);

		return true;
	}

	bool FGraph2D::RemovePolygon(int32 PolyID)
	{
		FGraph2DPolygon *polyToRemove = FindPolygon(PolyID);
		if (!polyToRemove)
		{
			return false;
		}

		polyToRemove->SetParent(MOD_ID_NONE);

		for (int32 edgeID : polyToRemove->Edges)
		{
			auto edge = FindEdge(edgeID);
			if (edge == nullptr)
			{
				continue;
			}

			int32& edgePolyID = (edge->ID < 0 ? edge->LeftPolyID : edge->RightPolyID);

			if (edgePolyID == polyToRemove->ID)
			{
				edgePolyID = polyToRemove->ParentID;
			}
		}

		Polygons.Remove(PolyID);

		return true;
	}

	bool FGraph2D::RemoveObject(int32 GraphObjID, EGraphObjectType GraphObjectType)
	{
		switch (GraphObjectType)
		{
		case EGraphObjectType::Vertex:
			return RemoveVertex(GraphObjID);
		case EGraphObjectType::Edge:
			return RemoveEdge(GraphObjID);
		default:
			ensureAlwaysMsgf(false, TEXT("Attempted to remove a non-vertex/edge from the graph, ID: %d"), ID);
			return false;
		}
	}

	int32 FGraph2D::CalculatePolygons()
	{
		// clear the existing polygon data before computing new ones
		// TODO: ensure this is unnecessary so that the computation can be iterative
		ClearPolygons();
		DirtyEdges.Empty();
		for (auto &kvp : Edges)
		{
			DirtyEdges.Add(kvp.Key);
			DirtyEdges.Add(-kvp.Key);

			FGraph2DEdge &edge = kvp.Value;
			edge.LeftPolyID = 0;
			edge.RightPolyID = 0;
		}

		// make sure all vertex-edge connections are sorted
		for (auto &kvp : Vertices)
		{
			FGraph2DVertex &vertex = kvp.Value;
			vertex.SortEdges();
		}

		TSet<FGraphSignedID> visitedEdges;
		TArray<FGraphSignedID> curPolyEdges;
		TArray<FVector2D> curPolyPoints;

		// iterate through the edges to find every polygon
		for (FGraphSignedID curEdgeID : DirtyEdges)
		{
			float curPolyTotalAngle = 0.0f;
			bool bPolyHasDuplicateEdge = false;
			FBox2D polyAABB(ForceInitToZero);
			curPolyEdges.Reset();
			curPolyPoints.Reset();

			FGraph2DEdge *curEdge = FindEdge(curEdgeID);

			while (!visitedEdges.Contains(curEdgeID) && curEdge)
			{
				visitedEdges.Add(curEdgeID);
				curPolyEdges.Add(curEdgeID);

				// choose the next vertex based on whether we are traversing the current edge forwards or backwards
				int32 prevVertexID = (curEdgeID > 0) ? curEdge->StartVertexID : curEdge->EndVertexID;
				int32 nextVertexID = (curEdgeID > 0) ? curEdge->EndVertexID : curEdge->StartVertexID;
				FGraph2DVertex *prevVertex = FindVertex(prevVertexID);
				FGraph2DVertex *nextVertex = FindVertex(nextVertexID);
				FGraphSignedID nextEdgeID = 0;
				float angleDelta = 0.0f;

				if (ensureAlways(prevVertex))
				{
					curPolyPoints.Add(prevVertex->Position);
				}

				if (ensureAlways(nextVertex) && nextVertex->GetNextEdge(curEdgeID, nextEdgeID, angleDelta))
				{
					if (curEdgeID == -nextEdgeID)
					{
						bPolyHasDuplicateEdge = true;
					}

					curPolyTotalAngle += angleDelta;
					curEdgeID = nextEdgeID;
					curEdge = FindEdge(nextEdgeID);
				}
			}

			int32 numPolyEdges = curPolyEdges.Num();
			if (numPolyEdges > 1)
			{
				bool bPolyClosed = (curPolyEdges[0] == curEdgeID);
				if (ensureAlways(bPolyClosed))
				{
					float expectedInteriorAngle = 180.0f * (numPolyEdges - 2);
					const static float polyAngleEpsilon = 0.1f;
					bool bPolyInterior = FMath::IsNearlyEqual(expectedInteriorAngle, curPolyTotalAngle, polyAngleEpsilon);

					int32 newID = NextPolyID++;
					FGraph2DPolygon newPolygon(newID, this);
					newPolygon.Edges = curPolyEdges;
					newPolygon.bHasDuplicateEdge = bPolyHasDuplicateEdge;
					newPolygon.bInterior = bPolyInterior;
					newPolygon.AABB = FBox2D(curPolyPoints);
					newPolygon.CachedPoints = curPolyPoints;

					for (FGraphSignedID edgeID : curPolyEdges)
					{
						if (FGraph2DEdge *edge = FindEdge(edgeID))
						{
							// determine which side of the edge this new polygon is on
							int32 &adjacentPolyID = (edgeID > 0) ? edge->LeftPolyID : edge->RightPolyID;
							adjacentPolyID = newID;
						}
					}
					newPolygon.Dirty(false);

					Polygons.Add(newID, MoveTemp(newPolygon));
				}
			}
		}
		DirtyEdges.Reset();

		// determine which polygons are inside of others
		for (auto &childKVP : Polygons)
		{
			FGraph2DPolygon &childPoly = childKVP.Value;

			for (auto &parentKVP : Polygons)
			{
				FGraph2DPolygon &parentPoly = parentKVP.Value;

				if (childPoly.IsInside(parentPoly))
				{
					// if the child polygon doesn't have a parent, then set it to the first polygon it's contained in
					// otherwise, see if the new parent is more appropriate
					if ((childPoly.ParentID == 0) ||
						(parentPoly.IsInside(*FindPolygon(childPoly.ParentID))))
					{
						childPoly.SetParent(parentPoly.ID);
					}
				}
			}
		}

		bDirty = false;
		return Polygons.Num();
	}

	void FGraph2D::ClearPolygons()
	{
		Polygons.Reset();
		NextPolyID = 1;
		bDirty = true;
	}

	int32 FGraph2D::GetID() const
	{
		return ID;
	}

	int32 FGraph2D::GetExteriorPolygonID() const
	{
		int32 resultID = MOD_ID_NONE;

		for (auto &kvp : Polygons)
		{
			auto &polygon = kvp.Value;
			if (!polygon.bInterior)
			{
				if (resultID == MOD_ID_NONE)
				{
					resultID = polygon.ID;
					
				}
				// If there's already another exterior polygon, then there's no singular exterior polygon, so return neither.
				else
				{
					return MOD_ID_NONE;
				}
			}
		}

		return resultID;
	}

	FGraph2DPolygon *FGraph2D::GetExteriorPolygon()
	{
		return FindPolygon(GetExteriorPolygonID());
	}

	const FGraph2DPolygon *FGraph2D::GetExteriorPolygon() const
	{
		return FindPolygon(GetExteriorPolygonID());
	}

	bool FGraph2D::ToDataRecord(FGraph2DRecord* OutRecord, bool bSaveOpenPolygons, bool bSaveExteriorPolygons) const
	{
		OutRecord->Vertices.Reset();
		OutRecord->Edges.Reset();
		OutRecord->Polygons.Reset();

		for (const auto &kvp : Vertices)
		{
			const FGraph2DVertex &vertex = kvp.Value;
			OutRecord->Vertices.Add(vertex.ID, vertex.Position);
		}

		for (const auto &kvp : Edges)
		{
			const FGraph2DEdge &edge = kvp.Value;
			TArray<int32> vertexIDs({ edge.StartVertexID, edge.EndVertexID });
			FGraph2DEdgeRecord edgeRecord({ vertexIDs });
			OutRecord->Edges.Add(edge.ID, edgeRecord);
		}

		for (const auto &kvp : Polygons)
		{
			const FGraph2DPolygon &poly = kvp.Value;

			if (!poly.bInterior && !bSaveExteriorPolygons)
			{
				continue;
			}

			FGraph2DPolygonRecord polyRecord({ poly.Edges });
			OutRecord->Polygons.Add(poly.ID, polyRecord);
		}

		return true;
	}

	bool FGraph2D::FromDataRecord(const FGraph2DRecord* InRecord)
	{
		Reset();

		for (const auto &kvp : InRecord->Vertices)
		{
			FGraph2DVertex *newVertex = AddVertex(kvp.Value, kvp.Key);
			if (newVertex == nullptr)
			{
				return false;
			}

			NextVertexID = FMath::Max(NextVertexID, newVertex->ID + 1);
		}

		for (const auto &kvp : InRecord->Edges)
		{
			const FGraph2DEdgeRecord &edgeRecord = kvp.Value;
			if (edgeRecord.VertexIDs.Num() != 2)
			{
				return false;
			}

			FGraph2DEdge *newEdge = AddEdge(edgeRecord.VertexIDs[0], edgeRecord.VertexIDs[1], kvp.Key);
			if (newEdge == nullptr)
			{
				return false;
			}

			NextEdgeID = FMath::Max(NextEdgeID, newEdge->ID + 1);
		}

		for (const auto &kvp : InRecord->Polygons)
		{
			const FGraph2DPolygonRecord &polyRecord = kvp.Value;

			FGraph2DPolygon poly(kvp.Key, this);
			poly.Edges = polyRecord.EdgeIDs;

			// TODO: use the polygon calculation to compute this data, and verify the integrity of the input.
			// For now, we have to assume it was input correctly.
			poly.bInterior = true;

			// At least make sure all of the referenced edges and vertices are correct, also to update the AABB and cached points.
			for (FGraphSignedID edgeID : poly.Edges)
			{
				const FGraph2DEdge *edge = FindEdge(edgeID);
				if (edge == nullptr)
				{
					return false;
				}

				FGraph2DVertex *vertex = FindVertex(edge->StartVertexID);
				if (vertex == nullptr)
				{
					return false;
				}

				poly.CachedPoints.Add(vertex->Position);
			}

			poly.AABB = FBox2D(poly.CachedPoints);
			Polygons.Add(poly.ID, poly);

			NextPolyID = FMath::Max(NextPolyID, poly.ID + 1);
		}

		return true;
	}

	bool FGraph2D::ApplyDelta(const FGraph2DDelta &Delta)
	{
		// TODO: do graph objects need dirty capabilities?
		FGraph2DDelta appliedDelta(ID);

		for (auto &kvp : Delta.VertexMovements)
		{
			int32 vertexID = kvp.Key;
			const TPair<FVector2D, FVector2D> &vertexDelta = kvp.Value;
			FGraph2DVertex *vertex = FindVertex(vertexID);
			if (ensureAlways(vertex))
			{
				vertex->Position = vertexDelta.Value;
				vertex->Dirty(true);
			}

			appliedDelta.VertexMovements.Add(kvp);
		}

		for (auto &kvp : Delta.VertexAdditions)
		{
			auto newVertex = AddVertex(kvp.Value, kvp.Key);
			if (!ensure(newVertex))
			{
				ApplyInverseDeltas({ appliedDelta });
				return false;
			}
			appliedDelta.VertexAdditions.Add(kvp);
		}

		for (auto &kvp : Delta.VertexDeletions)
		{
			bool bRemovedVertex = RemoveVertex(kvp.Key);
			if (!ensure(bRemovedVertex))
			{
				ApplyInverseDeltas({ appliedDelta });
				return false;
			}
			appliedDelta.VertexDeletions.Add(kvp);
		}

		for (auto &kvp : Delta.EdgeAdditions)
		{
			int32 edgeID = kvp.Key;
			const TArray<int32> &edgeVertexIDs = kvp.Value.Vertices;
			if (!ensureAlways(edgeVertexIDs.Num() == 2))
			{
				ApplyInverseDeltas({ appliedDelta });
				return false;
			}

			auto newEdge = AddEdge(edgeVertexIDs[0], edgeVertexIDs[1], edgeID);
			if (!ensure(newEdge))
			{
				ApplyInverseDeltas({ appliedDelta });
				return false;
			}
			appliedDelta.EdgeAdditions.Add(kvp);
		}

		for (auto &kvp : Delta.EdgeDeletions)
		{
			bool bRemovedEdge = RemoveEdge(kvp.Key);
			if (!ensure(bRemovedEdge))
			{
				ApplyInverseDeltas({ appliedDelta });
				return false;
			}
			appliedDelta.EdgeDeletions.Add(kvp);
		}

		for (auto& kvp : Delta.PolygonAdditions)
		{
			int32 polyID = kvp.Key;
			auto polyVertexIDs = kvp.Value.Vertices;

			if (!ensureAlways(polyVertexIDs.Num() > 1))
			{
				ApplyInverseDeltas({ appliedDelta });
				return false;
			}

			auto newPoly = AddPolygon(polyVertexIDs, polyID, kvp.Value.bInterior);
			if (!ensure(newPoly))
			{
				ApplyInverseDeltas({ appliedDelta });
				return false;
			}
			appliedDelta.PolygonAdditions.Add(kvp);
		}

		for (auto &kvp : Delta.PolygonDeletions)
		{
			bool bRemovedPoly = RemovePolygon(kvp.Key);
			if (!ensure(bRemovedPoly))
			{
				ApplyInverseDeltas({ appliedDelta });
				return false;
			}
			appliedDelta.PolygonDeletions.Add(kvp);
		}

		for (auto &kvp : Delta.PolygonParentIDUpdates)
		{
			auto childPoly = FindPolygon(kvp.Key);
			if (childPoly != nullptr)
			{
				childPoly->SetParent(kvp.Value.Value);
			}
		}

		return true;
	}

	bool FGraph2D::CleanGraph(TArray<int32> &OutCleanedVertices, TArray<int32> &OutCleanedEdges, TArray<int32> &OutCleanedPolygons)
	{
		OutCleanedVertices.Reset();
		OutCleanedEdges.Reset();
		OutCleanedPolygons.Reset();

		for (auto &vertexkvp : Vertices)
		{
			auto& vertex = vertexkvp.Value;
			if (vertex.bDirty)
			{
				vertex.Clean();
				OutCleanedVertices.Add(vertex.ID);
			}
		}

		for (auto &edgekvp : Edges)
		{
			auto& edge = edgekvp.Value;
			if (edge.bDirty)
			{
				edge.Clean();
				OutCleanedEdges.Add(edge.ID);
			}
		}

		for (auto &polykvp : Polygons)
		{
			auto& poly = polykvp.Value;
			if (poly.bDirty)
			{
				poly.Clean();
				OutCleanedPolygons.Add(poly.ID);
			}
		}

		return true;
	}

	bool FGraph2D::SetBounds(TArray<int32> &InOuterBounds, TArray<TArray<int32>> &InInnerBounds)
	{
		// verify that the provided vertices exist
		for (int32 vertexID : BoundingPolygon)
		{
			auto vertex = FindVertex(vertexID);
			if (vertex == nullptr)
			{
				return false;
			}
		}

		for (int32 holeIdx = 0; holeIdx < InInnerBounds.Num(); holeIdx++)
		{
			auto& bounds = InInnerBounds[holeIdx];
			for (int32 vertexID : bounds)
			{
				auto vertex = FindVertex(vertexID);
				if (vertex == nullptr)
				{
					return false;
				}
			}
		}

		BoundingPolygon = InOuterBounds;
		BoundingContainedPolygons = InInnerBounds;

		return true;
	}

	void FGraph2D::ClearBounds()
	{
		BoundingPolygon.Reset();
		BoundingContainedPolygons.Reset();

		CachedOuterBounds.Positions.Reset();
		CachedOuterBounds.EdgeNormals.Reset();
	}

	bool FGraph2D::ValidateGraph()
	{
		if (BoundingPolygon.Num() == 0) 
		{
			return true;
		}

		// Convert the vertices saved in the bounds into BoundsInformation structs
		// All vertices saved in the bounds must be in the graph - operations 
		// that would destroy any of these vertices are invalid, for example joining a vertex in the bounds
		if (!UpdateCachedBoundsPositions())
		{
			return false;
		}

		// Check whether each vertex is inside the outer bounds and outside the holes
		for (auto& vertexkvp : Vertices)
		{
			auto vertex = &vertexkvp.Value;
			bool bVertexOverlapsBounds;

			if (BoundingPolygon.Find(vertex->ID) == INDEX_NONE &&
				!UModumateGeometryStatics::IsPointInPolygon(vertex->Position, CachedOuterBounds.Positions, bVertexOverlapsBounds) &&
				!bVertexOverlapsBounds)
			{
				return false;
			}

			for (int32 holeIdx = 0; holeIdx < CachedInnerBounds.Num(); holeIdx++)
			{
				auto& bounds = CachedInnerBounds[holeIdx];
				// Checking the holes should be exclusive - being on the edge of the hole is valid
				if (BoundingContainedPolygons[holeIdx].Find(vertex->ID) == INDEX_NONE && 
					UModumateGeometryStatics::IsPointInPolygon(vertex->Position, bounds.Positions, bVertexOverlapsBounds) &&
					!bVertexOverlapsBounds)
				{
					return false;
				}
			}
		}

		bool bClockwise, bConcave;
		UModumateGeometryStatics::GetPolygonWindingAndConcavity(CachedOuterBounds.Positions, bClockwise, bConcave);
		// if the outer bounds is convex and there are no holes, the edges do not need to be checked
		if (CachedInnerBounds.Num() == 0 && !bConcave)
		{
			return true;
		}

		// populate edge normals
		UpdateCachedBoundsNormals();

		for (auto& edgekvp : Edges)
		{
			int32 edgeID = edgekvp.Key;
			for (int32 boundsIdx = 0; boundsIdx <= CachedInnerBounds.Num(); boundsIdx++)
			{
				auto& bounds = boundsIdx == CachedInnerBounds.Num() ? CachedOuterBounds : CachedInnerBounds[boundsIdx];

				auto edge = FindEdge(edgeID);
				if (edge == nullptr)
				{
					return false;
				}
				auto startVertex = FindVertex(edge->StartVertexID);
				auto endVertex = FindVertex(edge->EndVertexID);
				if (startVertex == nullptr || endVertex == nullptr)
				{
					return false;
				}

				if (UModumateGeometryStatics::IsLineSegmentBoundedByPoints2D(startVertex->Position, endVertex->Position, 
					bounds.Positions, bounds.EdgeNormals, Epsilon))
				{
					return false;
				}
			}
		} 

		return true;
	}

	bool FGraph2D::UpdateCachedBoundsPositions()
	{
		CachedOuterBounds.Positions.Reset();
		CachedOuterBounds.EdgeNormals.Reset();
		CachedInnerBounds.Reset();

		for (int32 vertexID : BoundingPolygon)
		{
			auto vertex = FindVertex(vertexID);
			if (vertex == nullptr)
			{
				return false;
			}

			CachedOuterBounds.Positions.Add(vertex->Position);
		}

		for (int32 holeIdx = 0; holeIdx < BoundingContainedPolygons.Num(); holeIdx++)
		{
			auto& hole = BoundingContainedPolygons[holeIdx];
			auto& bounds = CachedInnerBounds.AddDefaulted_GetRef();

			for (int32 vertexID : hole)
			{
				auto vertex = FindVertex(vertexID);
				if (vertex == nullptr)
				{
					return false;
				}

				bounds.Positions.Add(vertex->Position);
			}
		}

		return true;
	}

	void FGraph2D::UpdateCachedBoundsNormals()
	{
		bool bConcave = false;
		bool bClockwise = false;

		int32 numBounds = CachedInnerBounds.Num();
		for (int32 boundsIdx = 0; boundsIdx <= numBounds; boundsIdx++)
		{
			bool bUseOuterBounds = boundsIdx == numBounds;
			auto& bounds = bUseOuterBounds ? CachedOuterBounds : CachedInnerBounds[boundsIdx];

			// for outer bounds, edge normals should point toward the inside of the polygon
			// for inner bounds, edge normals should point toward the outside of the polygon
			float sign = bClockwise ? -1 : 1;
			// the sign used for the inner bounds is flipped relative to the sign for the outer bounds
			sign *= bUseOuterBounds ? -1 : 1;
			
			int32 numPositions = bounds.Positions.Num();
			for (int32 idx = 0; idx < numPositions; idx++)
			{
				FVector2D p1 = bounds.Positions[idx];
				FVector2D p2 = bounds.Positions[(idx + 1) % numPositions];

				FVector2D direction = (p2 - p1).GetSafeNormal();
				bounds.EdgeNormals.Add(sign * FVector2D(direction.Y, -direction.X));
			}
		}
	}

	bool FGraph2D::ApplyDeltas(const TArray<FGraph2DDelta> &Deltas)
	{
		TArray<FGraph2DDelta> appliedDeltas;
		for (auto& delta : Deltas)
		{
			if (!ApplyDelta(delta))
			{
				ApplyInverseDeltas(appliedDeltas);
				return false;
			}
			appliedDeltas.Add(delta);
		}
		return true;
	}

	void FGraph2D::ApplyInverseDeltas(const TArray<FGraph2DDelta> &Deltas)
	{
		auto inverseDeltas = Deltas;
		Algo::Reverse(inverseDeltas);

		for (auto& delta : inverseDeltas)
		{
			ApplyDelta(*delta.MakeGraphInverse());
		}

		TArray<int32> cleanedVertices, cleanedEdges, cleanedPolygons;
		CleanGraph(cleanedVertices, cleanedEdges, cleanedPolygons);
	}

	void FGraph2D::AggregateAddedObjects(const TArray<FGraph2DDelta> &Deltas, TSet<int32> &OutVertices, TSet<int32> &OutEdges)
	{
		for (auto& delta : Deltas)
		{
			for (auto& kvp : delta.VertexAdditions)
			{
				OutVertices.Add(kvp.Key);
			}

			for (auto& kvp : delta.EdgeAdditions)
			{
				OutEdges.Add(kvp.Key);
			}

			for (auto& kvp : delta.VertexDeletions)
			{
				OutVertices.Remove(kvp.Key);
			}

			for (auto& kvp : delta.EdgeDeletions)
			{
				OutEdges.Remove(kvp.Key);
			}
		}
	}

	void FGraph2D::AggregateAddedVertices(const TArray<FGraph2DDelta> &Deltas, TSet<int32> &OutVertices)
	{
		for (auto& delta : Deltas)
		{
			for (auto& kvp : delta.VertexAdditions)
			{
				OutVertices.Add(kvp.Key);
			}

			for (auto& kvp : delta.VertexDeletions)
			{
				OutVertices.Remove(kvp.Key);
			}
		}
	}

	void FGraph2D::AggregateAddedEdges(const TArray<FGraph2DDelta> &Deltas, TSet<int32> &OutEdges, const FVector2D &StartPosition, const FVector2D &EndPosition)
	{
		for (auto& delta : Deltas)
		{
			for (auto& kvp : delta.EdgeAdditions)
			{
				auto edge = FindEdge(kvp.Key);
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

				FVector2D startOnSegment = FMath::ClosestPointOnSegment2D(startVertex->Position, StartPosition, EndPosition);
				FVector2D endOnSegment = FMath::ClosestPointOnSegment2D(endVertex->Position, StartPosition, EndPosition);

				if (startOnSegment.Equals(startVertex->Position, Epsilon) &&
					endOnSegment.Equals(endVertex->Position, Epsilon))
				{
					OutEdges.Add(edge->ID);
				}

			}
			for (auto& kvp : delta.EdgeDeletions)
			{
				OutEdges.Remove(kvp.Key);
			}
		}
	}
}
