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
	IGraph2DObject::IGraph2DObject(int32 InID, FGraph2D *InGraph)
		: ID(InID)
		, Graph(InGraph)
	{
	}

	IGraph2DObject::~IGraph2DObject()
	{
	}

	void IGraph2DObject::Dirty(bool bConnected)
	{
		bModified = true;
		bDerivedDataDirty = true;
	}

	bool IGraph2DObject::ClearModified()
	{
		if (bModified)
		{
			bModified = false;
			return true;
		}

		return false;
	}


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

		Edges.Reset();
		Vertices.Reset();
		Polygons.Reset();

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

	bool FGraph2D::GetEdgeAngle(FGraphSignedID EdgeID, float& OutEdgeAngle)
	{
		FGraph2DEdge *edge = FindEdge(EdgeID);
		if (edge && edge->bValid && !edge->bDerivedDataDirty)
		{
			OutEdgeAngle = edge->CachedAngle;
			if (EdgeID < 0)
			{
				OutEdgeAngle = FRotator::ClampAxis(OutEdgeAngle + 180.0f);
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

		FGraph2DPolygon &newPoly = Polygons.Add(InID, FGraph2DPolygon(InID, this, VertexIDs, bInterior));

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
			ensureAlways(startVertex && startVertex->RemoveEdge(EdgeID));
		}

		if (edgeToRemove->EndVertexID != MOD_ID_NONE)
		{
			FGraph2DVertex *endVertex = FindVertex(edgeToRemove->EndVertexID);
			ensureAlways(endVertex && endVertex->RemoveEdge(-EdgeID));
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
			// Edges may be missing because they're currently being deleted, and this polygon is being deleted too as a result.
			auto edge = FindEdge(edgeID);
			if (edge == nullptr)
			{
				continue;
			}

			int32& edgePolyID = (edge->ID < 0 ? edge->LeftPolyID : edge->RightPolyID);

			// TODO: is this useful, or even correct, if we know we need to re-calculate polygons anyway?
			if (edgePolyID == polyToRemove->ID)
			{
				edgePolyID = polyToRemove->ParentID;
			}

			edge->bPolygonDirty = true;
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
		// Outside of delta application, this is the last possible moment to update derived vertex/edge data.
		CleanDirtyObjects(false);

		TArray<FGraph2DDelta> deltas;
		ensure(CalculatePolygons(deltas, NextPolyID));

		return Polygons.Num();
	}

	void FGraph2D::ClearPolygons()
	{
		Polygons.Reset();
		NextPolyID = 1;
	}

	int32 FGraph2D::GetID() const
	{
		return ID;
	}

	int32 FGraph2D::GetRootPolygonID() const
	{
		int32 resultID = MOD_ID_NONE;

		for (auto &kvp : Polygons)
		{
			auto &polygon = kvp.Value;
			if (polygon.bInterior && (polygon.ParentID == MOD_ID_NONE))
			{
				if (resultID == MOD_ID_NONE)
				{
					resultID = polygon.ID;
				}
				// If there's already another root polygon, then there's no singular root polygon, so return neither.
				else
				{
					return MOD_ID_NONE;
				}
			}
		}

		return resultID;
	}

	FGraph2DPolygon *FGraph2D::GetRootPolygon()
	{
		return FindPolygon(GetRootPolygonID());
	}

	const FGraph2DPolygon *FGraph2D::GetRootPolygon() const
	{
		return FindPolygon(GetRootPolygonID());
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

			FGraph2DPolygonRecord polyRecord; 
			polyRecord.VertexIDs = poly.VertexIDs; 
			polyRecord.bInterior = poly.bInterior;
			polyRecord.ContainingFaceID = poly.ParentID; 
			polyRecord.ContainedFaceIDs = poly.InteriorPolygons;
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
			if (!ensureAlways(newVertex))
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
			if (!ensureAlways(newEdge && newEdge->Clean()))
			{
				return false;
			}

			NextEdgeID = FMath::Max(NextEdgeID, newEdge->ID + 1);
		}

		for (auto& kvp : Vertices)
		{
			if (!ensureAlways(kvp.Value.Clean()))
			{
				return false;
			}
		}

		for (const auto &kvp : InRecord->Polygons)
		{
			const FGraph2DPolygonRecord &polyRecord = kvp.Value;

			FGraph2DPolygon poly(kvp.Key, this);
			poly.SetVertices(polyRecord.VertexIDs);
			poly.bInterior = polyRecord.bInterior;
			poly.ParentID = polyRecord.ContainingFaceID;
			poly.InteriorPolygons = polyRecord.ContainedFaceIDs;

			Polygons.Add(poly.ID, poly);

			NextPolyID = FMath::Max(NextPolyID, poly.ID + 1);
		}

		for (auto& kvp : Polygons)
		{
			kvp.Value.Clean();
		}

		if (!ensureAlways(!IsDirty()))
		{
			return false;
		}

		return true;
	}

	bool FGraph2D::ApplyDelta(const FGraph2DDelta &Delta)
	{
		FGraph2DDelta appliedDelta(ID);

		for (auto &kvp : Delta.VertexMovements)
		{
			int32 vertexID = kvp.Key;
			const TPair<FVector2D, FVector2D> &vertexDelta = kvp.Value;
			FGraph2DVertex *vertex = FindVertex(vertexID);
			if (ensureAlways(vertex))
			{
				vertex->SetPosition(vertexDelta.Value);
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

		// Clean vertices and edges here, so that while applying deltas we know that the underlying non-derived graph objects are always clean.
		CleanDirtyObjects(false);

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

		if (!Delta.BoundsUpdates.Key.IsEmpty() || !Delta.BoundsUpdates.Value.IsEmpty())
		{
			BoundingPolygon = Delta.BoundsUpdates.Value.OuterBounds;
			BoundingContainedPolygons = Delta.BoundsUpdates.Value.InnerBounds;
		}

		// Polygons may still be dirty at this point, but until we're ready to end applying a series of deltas and/or re-calculate polygons,
		// we can't be sure that the intended Polygon structures will match the current graph traversals, so it's unsafe to clean them now.

		return true;
	}

	bool FGraph2D::ClearModifiedObjects(TArray<int32>& OutModifiedVertices, TArray<int32>& OutModifiedEdges, TArray<int32>& OutModifiedPolygons)
	{
		OutModifiedVertices.Reset();
		OutModifiedEdges.Reset();
		OutModifiedPolygons.Reset();

		for (auto& vertexkvp : Vertices)
		{
			auto& vertex = vertexkvp.Value;
			if (vertex.ClearModified())
			{
				OutModifiedVertices.Add(vertex.ID);
			}
		}

		for (auto& edgekvp : Edges)
		{
			auto& edge = edgekvp.Value;
			if (edge.ClearModified())
			{
				OutModifiedEdges.Add(edge.ID);
			}
		}

		for (auto& polykvp : Polygons)
		{
			auto& poly = polykvp.Value;
			if (poly.ClearModified())
			{
				OutModifiedPolygons.Add(poly.ID);
			}
		}

		return (OutModifiedVertices.Num() > 0) || (OutModifiedEdges.Num() > 0) || (OutModifiedPolygons.Num() > 0);
	}

	bool FGraph2D::CleanDirtyObjects(bool bCleanPolygons)
	{
		bool bCleanedAnyObjects = false;

		for (auto& kvp : Edges)
		{
			bCleanedAnyObjects = kvp.Value.Clean() || bCleanedAnyObjects;

			// If we're cleaning polygons, then it means we've either already calculated polygons and have applied the appropriate deltas,
			// or in the middle of applying changes during polygon calculation. In either case, the edges are no longer polygon-dirty.
			if (bCleanPolygons)
			{
				kvp.Value.bPolygonDirty = false;
			}
		}

		for (auto& kvp : Vertices)
		{
			bCleanedAnyObjects = kvp.Value.Clean() || bCleanedAnyObjects;
		}

		if (bCleanPolygons)
		{
			for (auto& kvp : Polygons)
			{
				bCleanedAnyObjects = kvp.Value.Clean() || bCleanedAnyObjects;
			}
		}

		return bCleanedAnyObjects;
	}

	bool FGraph2D::IsDirty() const
	{
		for (auto& kvp : Vertices)
		{
			if (kvp.Value.bDerivedDataDirty)
			{
				return true;
			}
		}

		for (auto& kvp : Edges)
		{
			if (kvp.Value.bDerivedDataDirty)
			{
				return true;
			}
		}

		for (auto& kvp : Polygons)
		{
			if (kvp.Value.bDerivedDataDirty)
			{
				return true;
			}
		}

		return false;
	}

	void FGraph2D::ClearBounds()
	{
		BoundingPolygon = TPair<int32, TArray<int32>>();
		BoundingContainedPolygons.Reset();

		CachedOuterBounds.Positions.Reset();
		CachedOuterBounds.EdgeNormals.Reset();
	}

	bool FGraph2D::TraverseEdges(FGraphSignedID StartingEdgeID, TArray<FGraphSignedID>& OutEdgeIDs, TArray<int32>& OutVertexIDs,
		bool& bOutInterior, TSet<FGraphSignedID>& RefVisitedEdgeIDs, bool bUseDualEdges, const TSet<FGraphSignedID>* AllowedEdgeIDs) const
	{
		OutEdgeIDs.Reset();
		OutVertexIDs.Reset();
		bOutInterior = false;

		bool bVisitedEdge = RefVisitedEdgeIDs.Contains(StartingEdgeID) || (!bUseDualEdges && RefVisitedEdgeIDs.Contains(-StartingEdgeID));
		if (bVisitedEdge)
		{
			return false;
		}

		float curTraversalAngle = 0.0f;

		FGraphSignedID curEdgeID = StartingEdgeID;
		const FGraph2DEdge *curEdge = FindEdge(curEdgeID);
		if (!ensureAlways(curEdge))
		{
			return false;
		}

		do
		{
			// This shouldn't be possible if we're only calling TraverseEdges on graphs with valid vertex-edge connectivity;
			// traversal should normally only end when we reach the beginning again, but this validation is fast and useful for validating usage.
			bVisitedEdge = RefVisitedEdgeIDs.Contains(curEdgeID) || (!bUseDualEdges && RefVisitedEdgeIDs.Contains(-curEdgeID));
			if (!ensureAlways(!bVisitedEdge))
			{
				break;
			}

			RefVisitedEdgeIDs.Add(curEdgeID);
			OutEdgeIDs.Add(curEdgeID);

			// choose the next vertex based on whether we are traversing the current edge forwards or backwards
			int32 prevVertexID = (curEdgeID > 0) ? curEdge->StartVertexID : curEdge->EndVertexID;
			int32 nextVertexID = (curEdgeID > 0) ? curEdge->EndVertexID : curEdge->StartVertexID;
			const FGraph2DVertex *prevVertex = FindVertex(prevVertexID);
			const FGraph2DVertex *nextVertex = FindVertex(nextVertexID);
			FGraphSignedID nextEdgeID = 0;
			float angleDelta = 0.0f;

			if (!ensureAlways(prevVertex && nextVertex))
			{
				return false;
			}

			OutVertexIDs.Add(prevVertex->ID);

			if (!ensureAlways(nextVertex->GetNextEdge(curEdgeID, nextEdgeID, angleDelta, AllowedEdgeIDs, true)))
			{
				return false;
			}

			curTraversalAngle += angleDelta;
			curEdgeID = nextEdgeID;
			curEdge = FindEdge(nextEdgeID);
			if (!ensureAlways(curEdge))
			{
				return false;
			}

		} while (curEdgeID != StartingEdgeID);

		int32 numEdges = OutEdgeIDs.Num();

		if (!ensureAlways((numEdges >= 2) && (StartingEdgeID == curEdgeID)))
		{
			return false;
		}

		float expectedInteriorAngle = 180.0f * (numEdges - 2);
		const static float traversalAngleEpsilon = 0.1f;
		bOutInterior = FMath::IsNearlyEqual(expectedInteriorAngle, curTraversalAngle, traversalAngleEpsilon);
		return true;
	}

	bool FGraph2D::ValidateAgainstBounds()
	{
		if (BoundingPolygon.Value.Num() == 0) 
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
		FPointInPolyResult pointInPolyResult;
		for (auto& vertexkvp : Vertices)
		{
			auto vertex = &vertexkvp.Value;

			if (BoundingPolygon.Value.Find(vertex->ID) == INDEX_NONE)
			{
				if (!ensure(UModumateGeometryStatics::TestPointInPolygon(vertex->Position, CachedOuterBounds.Positions, pointInPolyResult, Epsilon)) ||
					(!pointInPolyResult.bInside && !pointInPolyResult.bOverlaps))
				{
					return false;
				}
			}

			int32 holeIdx = 0;
			for (auto& kvp : BoundingContainedPolygons)
			{
				auto& bounds = CachedInnerBounds[holeIdx];
				// Checking the holes should be exclusive - being on the edge of the hole is valid

				if (kvp.Value.Find(vertex->ID) == INDEX_NONE)
				{
					if (!ensure(UModumateGeometryStatics::TestPointInPolygon(vertex->Position, bounds.Positions, pointInPolyResult)) ||
						(pointInPolyResult.bInside && !pointInPolyResult.bOverlaps))
					{
						return false;
					}
				}
				holeIdx++;
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

		for (int32 vertexID : BoundingPolygon.Value)
		{
			auto vertex = FindVertex(vertexID);
			if (vertex == nullptr)
			{
				return false;
			}

			CachedOuterBounds.Positions.Add(vertex->Position);
		}

		for (auto& kvp : BoundingContainedPolygons)
		{
			auto& hole = kvp.Value;
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

	bool FGraph2D::ApplyDeltas(const TArray<FGraph2DDelta> &Deltas, bool bApplyInverseOnFailure)
	{
		bool bAllDeltasSucceeded = true;
		TArray<FGraph2DDelta> appliedDeltas;

		for (auto& delta : Deltas)
		{
			if (ApplyDelta(delta))
			{
				appliedDeltas.Add(delta);
			}
			else
			{
				if (bApplyInverseOnFailure)
				{
					ApplyInverseDeltas(appliedDeltas);
					return false;
				}
				else
				{
					bAllDeltasSucceeded = false;
				}
			}
			
		}

		CleanDirtyObjects(true);
		return bAllDeltasSucceeded;
	}

	bool FGraph2D::ApplyInverseDeltas(const TArray<FGraph2DDelta> &Deltas)
	{
		TArray<FGraph2DDelta> inverseDeltas;
		for (int32 i = Deltas.Num() - 1; i >= 0; --i)
		{
			FGraph2DDelta& delta = inverseDeltas.Add_GetRef(Deltas[i]);
			delta.Invert();
		}

		return ApplyDeltas(inverseDeltas, false);
	}

	void FGraph2D::AggregateAddedObjects(const TArray<FGraph2DDelta>& Deltas, TSet<int32>& OutVertices, TSet<int32>& OutEdges, TSet<int32>& OutPolygons)
	{
		for (auto& delta : Deltas)
		{
			// Gather additions
			for (auto& kvp : delta.VertexAdditions)
			{
				OutVertices.Add(kvp.Key);
			}
			for (auto& kvp : delta.EdgeAdditions)
			{
				OutEdges.Add(kvp.Key);
			}
			for (auto& kvp : delta.PolygonAdditions)
			{
				OutPolygons.Add(kvp.Key);
			}

			// Exclude deletions
			for (auto& kvp : delta.VertexDeletions)
			{
				OutVertices.Remove(kvp.Key);
			}
			for (auto& kvp : delta.EdgeDeletions)
			{
				OutEdges.Remove(kvp.Key);
			}
			for (auto& kvp : delta.PolygonDeletions)
			{
				OutPolygons.Remove(kvp.Key);
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

	void FGraph2D::GetOuterBoundsIDs(TArray<int32> &OutVertexIDs) const
	{
		OutVertexIDs = BoundingPolygon.Value;
	}

	const TMap<int32, TArray<int32>>& FGraph2D::GetInnerBounds() const
	{
		return BoundingContainedPolygons;
	}
}
