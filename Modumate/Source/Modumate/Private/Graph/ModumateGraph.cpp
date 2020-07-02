// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "Graph/ModumateGraph.h"

#include "ModumateCore/ModumateFunctionLibrary.h"

#define LOG_MDM_GRAPH 0

#if LOG_MDM_GRAPH
#define GA_LOG(S,...) UE_LOG(LogTemp,Display,S,__VA_ARGS__)
#else
#define GA_LOG(S,...)
#endif


namespace Modumate
{

	FGraphEdge::FGraphEdge(int32 InID, FGraph* InGraph, int32 InStart, int32 InEnd)
		: ID(InID)
		, Graph(InGraph)
		, LeftPolyID(0)
		, RightPolyID(0)
		, Angle(0.0f)
		, EdgeDir(ForceInitToZero)
		, bValid(false)
	{
		SetVertices(InStart, InEnd);
	}

	void FGraphEdge::SetVertices(int32 InStart, int32 InEnd)
	{
		StartVertexID = InStart;
		EndVertexID = InEnd;
		bValid = CacheAngle();
	}

	bool FGraphEdge::CacheAngle()
	{
		Angle = 0.0f;

		if (ensureAlways(Graph) && (StartVertexID != 0) && (EndVertexID != 0))
		{
			FGraphVertex *startVertex = Graph->FindVertex(StartVertexID);
			FGraphVertex *endVertex = Graph->FindVertex(EndVertexID);
			if (ensureAlways(startVertex && endVertex))
			{
				FVector2D edgeDelta = endVertex->Position - startVertex->Position;
				float edgeLength = edgeDelta.Size();
				if (ensureAlways(edgeLength > Graph->Epsilon))
				{
					EdgeDir = edgeDelta / edgeLength;
					Angle = FRotator::ClampAxis(FMath::RadiansToDegrees(FMath::Atan2(EdgeDir.Y, EdgeDir.X)));
					return true;
				}
			}
		}

		return false;
	}


	void FGraphVertex::AddEdge(FEdgeID EdgeID)
	{
		Edges.AddUnique(EdgeID);
		bDirty = true;
	}

	bool FGraphVertex::RemoveEdge(FEdgeID EdgeID)
	{
		int32 numRemoved = Edges.Remove(EdgeID);
		if (numRemoved != 0)
		{
			bDirty = true;
			return true;
		}

		return false;
	}

	void FGraphVertex::SortEdges()
	{
		if (bDirty)
		{
			if (Graph->bDebugCheck)
			{
				for (FEdgeID edgeID : Edges)
				{
					FGraphEdge *edge = Graph->FindEdge(edgeID);
					if (ensureAlways(edge && edge->bValid))
					{
						int32 connectedVertexID = (edgeID > 0) ? edge->StartVertexID : edge->EndVertexID;
						ensureAlways(connectedVertexID == ID);
					}
				}
			}

			auto edgeSorter = [this](const FEdgeID &edgeID1, const FEdgeID &edgeID2) {
				float angle1 = 0.0f, angle2 = 0.0f;
				if (ensureAlways(Graph->GetEdgeAngle(edgeID1, angle1) && Graph->GetEdgeAngle(edgeID2, angle2)))
				{
					return angle1 < angle2;
				}
				else
				{
					return false;
				}
			};

			Edges.Sort(edgeSorter);
			bDirty = false;
		}
	}

	bool FGraphVertex::GetNextEdge(FEdgeID curEdgeID, FEdgeID &outNextEdgeID, float &outAngleDelta, int32 indexDelta) const
	{
		outNextEdgeID = 0;
		outAngleDelta = 0.0f;

		FEdgeID curEdgeFromVertexID = -curEdgeID;
		int32 curEdgeIdx = Edges.Find(curEdgeFromVertexID);
		if (curEdgeIdx == INDEX_NONE)
		{
			return false;
		}

		if (indexDelta == 0)
		{
			return false;
		}

		int32 numEdges = Edges.Num();
		int32 nextEdgeIdx = (curEdgeIdx + indexDelta) % numEdges;
		outNextEdgeID = Edges[nextEdgeIdx];
		if (outNextEdgeID == 0)
		{
			return false;
		}

		// If we're doubling back on the same edge, then consider it a full rotation
		if (curEdgeFromVertexID == outNextEdgeID)
		{
			outAngleDelta = 360.0f;
			return true;
		}

		float curEdgeAngle = 0.0f, nextEdgeAngle = 0.0f;
		if (Graph->GetEdgeAngle(curEdgeFromVertexID, curEdgeAngle) &&
			Graph->GetEdgeAngle(outNextEdgeID, nextEdgeAngle) &&
			ensureAlways(!FMath::IsNearlyEqual(curEdgeAngle, nextEdgeAngle)))
		{
			outAngleDelta = nextEdgeAngle - curEdgeAngle;
			if (outAngleDelta < 0.0f)
			{
				outAngleDelta += 360.0f;
			}

			return true;
		}

		return false;
	}


	bool FGraphPolygon::IsInside(const FGraphPolygon &otherPoly) const
	{
		// Polygons cannot be contained inside exterior polygons
		if (!otherPoly.bInterior)
		{
			return false;
		}

		// If we've already established the other poly as a parent, return the cached result
		if (ParentID == otherPoly.ID)
		{
			return true;
		}

		// If this is a parent of the other polygon, the parent cannot be contained within the child
		if (otherPoly.ParentID == ID)
		{
			return false;
		}

		// Early out by checking the AABBs
		if (!otherPoly.AABB.IsInside(AABB))
		{
			return false;
		}

		for (const FVector2D &point : Points)
		{
			if (UModumateFunctionLibrary::PointInPoly2D(point, otherPoly.Points))
			{
				return true;
			}
		}

		return false;
	}

	void FGraphPolygon::SetParent(int32 inParentID)
	{
		if (ParentID != inParentID)
		{
			// remove self from old parent
			if (ParentID != 0)
			{
				FGraphPolygon *parentPoly = Graph->FindPolygon(ParentID);
				if (ensureAlways(parentPoly))
				{
					int32 numRemoved = parentPoly->InteriorPolygons.Remove(ID);
					ensureAlways(numRemoved == 1);
				}
			}

			ParentID = inParentID;

			// add self to new parent
			if (ParentID != 0)
			{
				FGraphPolygon *parentPoly = Graph->FindPolygon(ParentID);
				if (ensureAlways(parentPoly && !parentPoly->InteriorPolygons.Contains(ID)))
				{
					parentPoly->InteriorPolygons.Add(ID);
				}
			}
		}
	}


	FGraph::FGraph(float InEpsilon, bool bInDebugCheck)
		: Epsilon(InEpsilon)
		, bDebugCheck(bInDebugCheck)
	{
		Reset();
	}

	void FGraph::Reset()
	{
		NextEdgeID = NextVertexID = NextPolyID = 1;
		bDirty = false;

		Edges.Reset();
		Vertices.Reset();
		Polygons.Reset();
		DirtyEdges.Reset();
	}

	FGraphEdge* FGraph::FindEdge(FEdgeID EdgeID) 
	{ 
		return Edges.Find(FMath::Abs(EdgeID)); 
	}

	const FGraphEdge* FGraph::FindEdge(FEdgeID EdgeID) const 
	{ 
		return Edges.Find(FMath::Abs(EdgeID)); 
	}

	FGraphVertex* FGraph::FindVertex(int32 ID) 
	{ 
		return Vertices.Find(ID); 
	}

	const FGraphVertex* FGraph::FindVertex(int32 ID) const 
	{ 
		return Vertices.Find(ID); 
	}

	FGraphPolygon* FGraph::FindPolygon(int32 ID) 
	{ 
		return Polygons.Find(ID); 
	}

	const FGraphPolygon* FGraph::FindPolygon(int32 ID) const 
	{ 
		return Polygons.Find(ID); 
	}

	FGraphVertex* FGraph::FindVertex(const FVector2D &Position)
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

	bool FGraph::ContainsObject(int32 ID, EGraphObjectType GraphObjectType) const
	{
		switch (GraphObjectType)
		{
		case EGraphObjectType::Vertex:
			return Vertices.Contains(ID);
		case EGraphObjectType::Edge:
			return Edges.Contains(ID);
		case EGraphObjectType::Polygon:
			return Polygons.Contains(ID);
		default:
			return false;
		}
	}

	bool FGraph::GetEdgeAngle(FEdgeID EdgeID, float &outEdgeAngle)
	{
		FGraphEdge *edge = FindEdge(EdgeID);
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

	FGraphVertex *FGraph::AddVertex(const FVector2D &Position, int32 InID)
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

		if (bDebugCheck)
		{
			for (auto &kvp : Vertices)
			{
				FGraphVertex &otherVertex = kvp.Value;
				if (!ensureAlways(!Position.Equals(otherVertex.Position, Epsilon)))
				{
					return nullptr;
				}
			}
		}

		FGraphVertex &newVertex = Vertices.Add(newID, FGraphVertex(newID, this, Position));
		bDirty = true;

		return &newVertex;
	}

	FGraphEdge *FGraph::AddEdge(int32 StartVertexID, int32 EndVertexID, int32 InID)
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

		FGraphEdge &newEdge = Edges.Add(newID, FGraphEdge(newID, this, StartVertexID, EndVertexID));

		bDirty = true;
		DirtyEdges.Add(newID);
		DirtyEdges.Add(-newID);

		FGraphVertex *startVertex = FindVertex(StartVertexID);
		FGraphVertex *endVertex = FindVertex(EndVertexID);
		if (startVertex && endVertex)
		{
			startVertex->AddEdge(newID);
			endVertex->AddEdge(-newID);
		}

		return &newEdge;
	}

	bool FGraph::RemoveVertex(int32 VertexID)
	{
		FGraphVertex *vertexToRemove = FindVertex(VertexID);
		if (!vertexToRemove)
		{
			return false;
		}

		for (FEdgeID connectedEdgeID : vertexToRemove->Edges)
		{
			bool bEdgeStartsFromVertex = (connectedEdgeID > 0);
			FGraphEdge *connectedEdge = FindEdge(connectedEdgeID);
			if (ensureAlways(connectedEdge))
			{
				int32 &vertexIDRef = bEdgeStartsFromVertex ? connectedEdge->StartVertexID : connectedEdge->EndVertexID;
				vertexIDRef = 0;
				bDirty = true;
			}
		}

		Vertices.Remove(VertexID);

		return true;
	}

	bool FGraph::RemoveEdge(int32 EdgeID)
	{
		EdgeID = FMath::Abs(EdgeID);
		FGraphEdge *edgeToRemove = FindEdge(EdgeID);
		if (!edgeToRemove)
		{
			return false;
		}

		if (edgeToRemove->StartVertexID != 0)
		{
			FGraphVertex *startVertex = FindVertex(edgeToRemove->StartVertexID);
			if (ensureAlways(startVertex && startVertex->RemoveEdge(EdgeID)))
			{
				bDirty = true;
			}
		}

		if (edgeToRemove->EndVertexID != 0)
		{
			FGraphVertex *endVertex = FindVertex(edgeToRemove->EndVertexID);
			if (ensureAlways(endVertex && endVertex->RemoveEdge(-EdgeID)))
			{
				bDirty = true;
			}
		}

		Edges.Remove(EdgeID);

		return true;
	}

	bool FGraph::RemoveObject(int32 ID, EGraphObjectType GraphObjectType)
	{
		switch (GraphObjectType)
		{
		case EGraphObjectType::Vertex:
			return RemoveVertex(ID);
		case EGraphObjectType::Edge:
			return RemoveEdge(ID);
		default:
			ensureAlwaysMsgf(false, TEXT("Attempted to remove a non-vertex/edge from the graph, ID: %d"), ID);
			return false;
		}
	}

	int32 FGraph::CalculatePolygons()
	{
		// clear the existing polygon data before computing new ones
		// TODO: ensure this is unnecessary so that the computation can be iterative
		ClearPolygons();
		DirtyEdges.Empty();
		for (auto &kvp : Edges)
		{
			DirtyEdges.Add(kvp.Key);
			DirtyEdges.Add(-kvp.Key);

			FGraphEdge &edge = kvp.Value;
			edge.LeftPolyID = 0;
			edge.RightPolyID = 0;
		}

		// make sure all vertex-edge connections are sorted
		for (auto &kvp : Vertices)
		{
			FGraphVertex &vertex = kvp.Value;
			vertex.SortEdges();
		}

		TSet<FEdgeID> visitedEdges;
		TArray<FEdgeID> curPolyEdges;
		TArray<FVector2D> curPolyPoints;

		// iterate through the edges to find every polygon
		for (FEdgeID curEdgeID : DirtyEdges)
		{
			float curPolyTotalAngle = 0.0f;
			bool bPolyHasDuplicateEdge = false;
			FBox2D polyAABB(ForceInitToZero);
			curPolyEdges.Reset();
			curPolyPoints.Reset();

			FGraphEdge *curEdge = FindEdge(curEdgeID);

			while (!visitedEdges.Contains(curEdgeID) && curEdge)
			{
				visitedEdges.Add(curEdgeID);
				curPolyEdges.Add(curEdgeID);

				// choose the next vertex based on whether we are traversing the current edge forwards or backwards
				int32 prevVertexID = (curEdgeID > 0) ? curEdge->StartVertexID : curEdge->EndVertexID;
				int32 nextVertexID = (curEdgeID > 0) ? curEdge->EndVertexID : curEdge->StartVertexID;
				FGraphVertex *prevVertex = FindVertex(prevVertexID);
				FGraphVertex *nextVertex = FindVertex(nextVertexID);
				FEdgeID nextEdgeID = 0;
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
					FGraphPolygon newPolygon(newID, this);
					newPolygon.Edges = curPolyEdges;
					newPolygon.bHasDuplicateEdge = bPolyHasDuplicateEdge;
					newPolygon.bInterior = bPolyInterior;
					newPolygon.AABB = FBox2D(curPolyPoints);
					newPolygon.Points = curPolyPoints;

					for (FEdgeID edgeID : curPolyEdges)
					{
						if (FGraphEdge *edge = FindEdge(edgeID))
						{
							// determine which side of the edge this new polygon is on
							int32 &adjacentPolyID = (edgeID > 0) ? edge->LeftPolyID : edge->RightPolyID;
							adjacentPolyID = newID;
						}
					}

					Polygons.Add(newID, MoveTemp(newPolygon));
				}
			}
		}
		DirtyEdges.Reset();

		// determine which polygons are inside of others
		for (auto &childKVP : Polygons)
		{
			FGraphPolygon &childPoly = childKVP.Value;

			for (auto &parentKVP : Polygons)
			{
				FGraphPolygon &parentPoly = parentKVP.Value;

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

	void FGraph::ClearPolygons()
	{
		Polygons.Reset();
		NextPolyID = 1;
		bDirty = true;
	}

	int32 FGraph::GetExteriorPolygonID() const
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

	FGraphPolygon *FGraph::GetExteriorPolygon()
	{
		return FindPolygon(GetExteriorPolygonID());
	}

	const FGraphPolygon *FGraph::GetExteriorPolygon() const
	{
		return FindPolygon(GetExteriorPolygonID());
	}

	bool FGraph::ToDataRecord(FGraph2DRecord &OutRecord, bool bSaveOpenPolygons, bool bSaveExteriorPolygons) const
	{
		OutRecord.Vertices.Reset();
		OutRecord.Edges.Reset();
		OutRecord.Polygons.Reset();

		for (const auto &kvp : Vertices)
		{
			const FGraphVertex &vertex = kvp.Value;
			OutRecord.Vertices.Add(vertex.ID, vertex.Position);
		}

		for (const auto &kvp : Edges)
		{
			const FGraphEdge &edge = kvp.Value;
			TArray<int32> vertexIDs({ edge.StartVertexID, edge.EndVertexID });
			FGraph2DEdgeRecord edgeRecord({ vertexIDs });
			OutRecord.Edges.Add(edge.ID, edgeRecord);
		}

		for (const auto &kvp : Polygons)
		{
			const FGraphPolygon &poly = kvp.Value;

			if (!poly.bInterior && !bSaveExteriorPolygons)
			{
				continue;
			}

			FGraph2DPolygonRecord polyRecord({ poly.Edges });
			OutRecord.Polygons.Add(poly.ID, polyRecord);
		}

		return true;
	}

	bool FGraph::FromDataRecord(const FGraph2DRecord &InRecord)
	{
		Reset();

		for (const auto &kvp : InRecord.Vertices)
		{
			FGraphVertex *newVertex = AddVertex(kvp.Value, kvp.Key);
			if (newVertex == nullptr)
			{
				return false;
			}

			NextVertexID = FMath::Max(NextVertexID, newVertex->ID + 1);
		}

		for (const auto &kvp : InRecord.Edges)
		{
			const FGraph2DEdgeRecord &edgeRecord = kvp.Value;
			if (edgeRecord.VertexIDs.Num() != 2)
			{
				return false;
			}

			FGraphEdge *newEdge = AddEdge(edgeRecord.VertexIDs[0], edgeRecord.VertexIDs[1], kvp.Key);
			if (newEdge == nullptr)
			{
				return false;
			}

			NextEdgeID = FMath::Max(NextEdgeID, newEdge->ID + 1);
		}

		for (const auto &kvp : InRecord.Polygons)
		{
			const FGraph2DPolygonRecord &polyRecord = kvp.Value;

			FGraphPolygon poly(kvp.Key, this);
			poly.Edges = polyRecord.EdgeIDs;

			// TODO: use the polygon calculation to compute this data, and verify the integrity of the input.
			// For now, we have to assume it was input correctly.
			poly.bInterior = true;

			// At least make sure all of the referenced edges and vertices are correct, also to update the AABB and cached points.
			for (FEdgeID edgeID : poly.Edges)
			{
				const FGraphEdge *edge = FindEdge(edgeID);
				if (edge == nullptr)
				{
					return false;
				}

				FGraphVertex *vertex = FindVertex(edge->StartVertexID);
				if (vertex == nullptr)
				{
					return false;
				}

				poly.Points.Add(vertex->Position);
			}

			poly.AABB = FBox2D(poly.Points);
			Polygons.Add(poly.ID, poly);

			NextPolyID = FMath::Max(NextPolyID, poly.ID + 1);
		}

		return true;
	}

	bool FGraph::ApplyDelta(const FGraph2DDelta &Delta)
	{
		// TODO: do graph objects need dirty capabilities?

		for (auto &kvp : Delta.VertexMovements)
		{
			int32 vertexID = kvp.Key;
			const TPair<FVector2D, FVector2D> &vertexDelta = kvp.Value;
			FGraphVertex *vertex = FindVertex(vertexID);
			if (ensureAlways(vertex))
			{
				vertex->Position = vertexDelta.Value;
			}
		}

		for (auto &kvp : Delta.VertexAdditions)
		{
			auto newVertex = AddVertex(kvp.Value, kvp.Key);
			if (!ensure(newVertex))
			{
				return false;
			}
		}

		for (auto &kvp : Delta.VertexDeletions)
		{
			bool bRemovedVertex = RemoveVertex(kvp.Key);
			if (!ensure(bRemovedVertex))
			{
				return false;
			}
		}

		for (auto &kvp : Delta.EdgeAdditions)
		{
			int32 edgeID = kvp.Key;
			const TArray<int32> &edgeVertexIDs = kvp.Value.Vertices;
			if (!ensureAlways(edgeVertexIDs.Num() == 2))
			{
				return false;
			}

			auto newEdge = AddEdge(edgeVertexIDs[0], edgeVertexIDs[1], edgeID);
			if (!ensure(newEdge))
			{
				return false;
			}
		}

		for (auto &kvp : Delta.EdgeDeletions)
		{
			bool bRemovedEdge = RemoveEdge(kvp.Key);
			if (!ensure(bRemovedEdge))
			{
				return false;
			}
		}

		return true;
	}
}
