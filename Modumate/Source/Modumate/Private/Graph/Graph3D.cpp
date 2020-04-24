// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "Graph3D.h"

#include "Graph3DDelta.h"

#include "ModumateGraph.h"

#include "ModumateFunctionLibrary.h"
#include "ModumateGeometryStatics.h"

namespace Modumate
{
	FGraph3D::FGraph3D(float InEpsilon, bool bInDebugCheck)
		: Epsilon(InEpsilon)
		, bDebugCheck(bInDebugCheck)
	{
		Reset();
	}

	void FGraph3D::Reset()
	{
		NextPolyhedronID = 1;
		bDirty = false;

		EdgeIDsByVertexPair.Reset();
		Vertices.Reset();
		Edges.Reset();
		Faces.Reset();
		Polyhedra.Reset();
		DirtyFaces.Reset();
	}

	FGraph3DEdge* FGraph3D::FindEdge(FSignedID EdgeID)
	{
		return Edges.Find(FMath::Abs(EdgeID));
	}

	const FGraph3DEdge* FGraph3D::FindEdge(FSignedID EdgeID) const
	{
		return Edges.Find(FMath::Abs(EdgeID));
	}

	FGraph3DEdge *FGraph3D::FindEdgeByVertices(int32 VertexIDA, int32 VertexIDB, bool &bOutForward)
	{
		return const_cast<FGraph3DEdge*>(const_cast<const FGraph3D*>(this)->FindEdgeByVertices(VertexIDA, VertexIDB, bOutForward));
	}

	const FGraph3DEdge *FGraph3D::FindEdgeByVertices(int32 VertexIDA, int32 VertexIDB, bool &bOutForward) const
	{
		FVertexPair vertexPair(
			FMath::Min(VertexIDA, VertexIDB),
			FMath::Max(VertexIDA, VertexIDB)
		);

		if (const int32 *edgeIDPtr = EdgeIDsByVertexPair.Find(vertexPair))
		{
			if (const FGraph3DEdge *edge = Edges.Find(*edgeIDPtr))
			{
				bOutForward = (edge->StartVertexID == VertexIDA);
				return edge;
			}
		}

		return nullptr;
	}

	const void FGraph3D::FindEdges(const FVector &Position, int32 ExistingID, TArray<int32>& OutEdgeIDs) const
	{
		OutEdgeIDs.Reset();

		auto vertex = FindVertex(Position);
		if (vertex != nullptr && vertex->ID != ExistingID)
		{
			return;
		}

		for (auto &kvp : Edges)
		{
			const FGraph3DEdge &edge = kvp.Value;
			auto startVertex = FindVertex(edge.StartVertexID);
			auto endVertex = FindVertex(edge.EndVertexID);

			if (startVertex == nullptr || endVertex == nullptr)
			{
				continue;
			}
			if (startVertex->ID == ExistingID || endVertex->ID == ExistingID)
			{
				continue;
			}

			if (FMath::PointDistToSegment(Position, startVertex->Position, endVertex->Position) < Epsilon)
			{
				OutEdgeIDs.Add(kvp.Key);
			}
		}
	}

	FGraph3DVertex* FGraph3D::FindVertex(int32 VertexID) 
	{ 
		return Vertices.Find(VertexID); 
	}

	const FGraph3DVertex* FGraph3D::FindVertex(int32 VertexID) const 
	{ 
		return Vertices.Find(VertexID); 
	}

	FGraph3DVertex *FGraph3D::FindVertex(const FVector &Position)
	{
		return const_cast<FGraph3DVertex*>(const_cast<const FGraph3D*>(this)->FindVertex(Position));
	}

	const FGraph3DVertex *FGraph3D::FindVertex(const FVector &Position) const
	{
		// TODO: use spatial data structure to speed this up
		for (auto &kvp : Vertices)
		{
			const FGraph3DVertex &otherVertex = kvp.Value;
			if (Position.Equals(otherVertex.Position, Epsilon))
			{
				return &kvp.Value;
			}
		}

		return nullptr;
	}

	FGraph3DFace* FGraph3D::FindFace(FSignedID FaceID) 
	{ 
		return Faces.Find(FMath::Abs(FaceID)); 
	}

	const FGraph3DFace* FGraph3D::FindFace(FSignedID FaceID) const 
	{ 
		return Faces.Find(FMath::Abs(FaceID)); 
	}

	FGraph3DPolyhedron* FGraph3D::FindPolyhedron(int32 PolyhedronID) 
	{ 
		return Polyhedra.Find(PolyhedronID); 
	}

	const FGraph3DPolyhedron* FGraph3D::FindPolyhedron(int32 PolyhedronID) const 
	{ 
		return Polyhedra.Find(PolyhedronID); 
	}

	bool FGraph3D::ContainsObject(int32 ID, EGraph3DObjectType GraphObjectType) const
	{
		switch (GraphObjectType)
		{
		case EGraph3DObjectType::Vertex:
			return Vertices.Contains(ID);
		case EGraph3DObjectType::Edge:
			return Edges.Contains(ID);
		case EGraph3DObjectType::Face:
			return Faces.Contains(ID);
		case EGraph3DObjectType::Polyhedron:
			return Polyhedra.Contains(ID);
		default:
			return false;
		}
	}

	FGraph3DVertex *FGraph3D::AddVertex(const FVector &Position, int32 InID)
	{
		int32 newID = InID;
		if (newID == MOD_ID_NONE)
		{
			return nullptr;
		}

		if (Vertices.Contains(newID))
		{
			return nullptr;
		}

		if (bDebugCheck)
		{
			if (!ensureAlways(FindVertex(Position) == nullptr))
			{
				return nullptr;
			}
		}

		FGraph3DVertex newVertex = FGraph3DVertex(newID, this, Position);
		if (!ensureAlways(newVertex.bValid))
		{
			return nullptr;
		}

		newVertex.Dirty();
		bDirty = true;

		return &Vertices.Add(newID, MoveTemp(newVertex));
	}

	FGraph3DEdge *FGraph3D::AddEdge(int32 StartVertexID, int32 EndVertexID, int32 InID)
	{
		int32 newID = InID;
		if (newID == MOD_ID_NONE)
		{
			return nullptr;
		}

		if (Edges.Contains(newID))
		{
			return nullptr;
		}

		FGraph3DEdge newEdge = FGraph3DEdge(newID, this, StartVertexID, EndVertexID);
		if (!ensureAlways(newEdge.bValid))
		{
			return nullptr;
		}

		newEdge.Dirty();
		bDirty = true;

		FVertexPair vertexPair(
			FMath::Min(StartVertexID, EndVertexID),
			FMath::Max(StartVertexID, EndVertexID)
		);
		EdgeIDsByVertexPair.Add(vertexPair, newID);

		return &Edges.Add(newID, MoveTemp(newEdge));
	}

	FGraph3DFace *FGraph3D::AddFace(const TArray<FVector> &VertexPositions, int32 InID)
	{
		int32 newID = InID;
		if (newID == MOD_ID_NONE)
		{
			return nullptr;
		}

		if (Faces.Contains(newID))
		{
			return nullptr;
		}

		FGraph3DFace newFace = FGraph3DFace(newID, this, VertexPositions);
		if (!ensureAlways(newFace.bValid))
		{
			return nullptr;
		}

		newFace.Dirty();
		bDirty = true;

		return &Faces.Add(newID, MoveTemp(newFace));
	}

	FGraph3DFace *FGraph3D::AddFace(const TArray<int32> &VertexIDs, int32 InID)
	{
		int32 newID = InID;
		if (newID == MOD_ID_NONE)
		{
			return nullptr;
		}

		if (Faces.Contains(newID))
		{
			return nullptr;
		}

		FGraph3DFace newFace = FGraph3DFace(newID, this, VertexIDs);
		if (!ensureAlways(newFace.bValid))
		{
			return nullptr;
		}

		newFace.Dirty();
		bDirty = true;

		return &Faces.Add(newID, MoveTemp(newFace));
	}

	bool FGraph3D::RemoveVertex(int32 VertexID)
	{
		FGraph3DVertex *vertexToRemove = FindVertex(VertexID);
		if (!ensureAlways(vertexToRemove))
		{
			return false;
		}

		vertexToRemove->Dirty();

		for (FSignedID connectedEdgeID : vertexToRemove->ConnectedEdgeIDs)
		{
			bool bEdgeStartsFromVertex = (connectedEdgeID > 0);
			FGraph3DEdge *connectedEdge = FindEdge(connectedEdgeID);
			if (ensureAlways(connectedEdge))
			{
				int32 &vertexIDRef = bEdgeStartsFromVertex ? connectedEdge->StartVertexID : connectedEdge->EndVertexID;
				vertexIDRef = 0;
				bDirty = true;
			}
		}

		return Vertices.Remove(VertexID) > 0;
	}

	bool FGraph3D::RemoveEdge(int32 EdgeID)
	{
		EdgeID = FMath::Abs(EdgeID);
		FGraph3DEdge *edgeToRemove = FindEdge(EdgeID);
		if (!ensureAlways(edgeToRemove))
		{
			return false;
		}

		edgeToRemove->Dirty();

		if (edgeToRemove->StartVertexID != 0)
		{
			FGraph3DVertex *startVertex = FindVertex(edgeToRemove->StartVertexID);
			ensureAlways(startVertex && startVertex->RemoveEdge(EdgeID));
		}

		if (edgeToRemove->EndVertexID != 0)
		{
			FGraph3DVertex *endVertex = FindVertex(edgeToRemove->EndVertexID);
			ensureAlways(endVertex && endVertex->RemoveEdge(-EdgeID));
		}

		FVertexPair vertexPair(
			FMath::Min(edgeToRemove->StartVertexID, edgeToRemove->EndVertexID),
			FMath::Max(edgeToRemove->StartVertexID, edgeToRemove->EndVertexID)
		);
		EdgeIDsByVertexPair.Remove(vertexPair);

		return Edges.Remove(EdgeID) > 0;
	}

	bool FGraph3D::RemoveFace(int32 FaceID)
	{
		FaceID = FMath::Abs(FaceID);
		FGraph3DFace *faceToRemove = FindFace(FaceID);
		if (!ensureAlways(faceToRemove))
		{
			return false;
		}

		faceToRemove->Dirty();

		for (FSignedID edgeID : faceToRemove->EdgeIDs)
		{
			if (FGraph3DEdge *faceEdge = FindEdge(edgeID))
			{
				faceEdge->RemoveFace(FaceID, false);
			}
		}

		return Faces.Remove(FaceID) > 0;
	}

	bool FGraph3D::RemoveObject(int32 ID, EGraph3DObjectType GraphObjectType)
	{
		switch (GraphObjectType)
		{
		case EGraph3DObjectType::Vertex:
			return RemoveVertex(ID);
		case EGraph3DObjectType::Edge:
			return RemoveEdge(ID);
		case EGraph3DObjectType::Face:
			return RemoveFace(ID);
		default:
			ensureAlwaysMsgf(false, TEXT("Attempted to remove a non-vertex/edge/face from the graph, ID: %d"), ID);
			return false;
		}
	}

	bool FGraph3D::CleanObject(int32 ID, EGraph3DObjectType type)
	{
		switch (type)
		{
		case EGraph3DObjectType::Vertex: {
			auto vertex = FindVertex(ID);
			if (vertex == nullptr)
			{
				return false;
			}

			vertex->bDirty = false;
		} break;
		case EGraph3DObjectType::Edge: {
			auto edge = FindEdge(ID);
			if (edge == nullptr)
			{
				return false;
			}
			edge->SetVertices(edge->StartVertexID, edge->EndVertexID);
			edge->bDirty = false;
			
		} break;
		case EGraph3DObjectType::Face: {
			auto face = FindFace(ID);
			if (face == nullptr)
			{
				return false;
			}
			face->UpdateVerticesAndEdges(face->VertexIDs);
			face->bDirty = false;

		} break;
		default:
			ensureAlwaysMsgf(false, TEXT("Attempted to clean a non-vertex/edge/face from the graph, ID: %d"), ID);
			return false;
		}

		return true;
	}

	const TMap<int32, FGraph3DEdge> &FGraph3D::GetEdges() const 
	{ 
		return Edges; 
	}

	const TMap<int32, FGraph3DVertex> &FGraph3D::GetVertices() const 
	{ 
		return Vertices; 
	}

	const TMap<int32, FGraph3DFace> &FGraph3D::GetFaces() const 
	{ 
		return Faces; 
	}

	const TMap<int32, FGraph3DPolyhedron> &FGraph3D::GetPolyhedra() const 
	{ 
		return Polyhedra; 
	}

	bool FGraph3D::ApplyDelta(const FGraph3DDelta &Delta)
	{
		// TODO: updating planes could be a part of Dirty instead of 
		// part of applying the deltas

		for (auto &kvp : Delta.VertexMovements)
		{
			int32 vertexID = kvp.Key;
			const TPair<FVector, FVector> &vertexDelta = kvp.Value;
			FGraph3DVertex *vertex = FindVertex(vertexID);
			if (ensureAlways(vertex))
			{
				vertex->Position = vertexDelta.Value;
			}

			TSet<int32> connectedFaces, connectedEdges;
			vertex->GetConnectedFacesAndEdges(connectedFaces, connectedEdges);

			// this also sets all connected edges and faces dirty
			vertex->Dirty(true);
		}

		for (auto &kvp : Delta.VertexAdditions)
		{
			FGraph3DVertex *newVertex = AddVertex(kvp.Value, kvp.Key);
		}

		for (auto &kvp : Delta.VertexDeletions)
		{
			bool bRemovedVertex = RemoveVertex(kvp.Key);
			ensureAlways(bRemovedVertex);
		}

		for (auto &kvp : Delta.EdgeAdditions)
		{
			int32 edgeID = kvp.Key;
			const TArray<int32> &edgeVertexIDs = kvp.Value.Vertices;
			ensureAlways(edgeVertexIDs.Num() == 2);
			FGraph3DEdge *newEdge = AddEdge(edgeVertexIDs[0], edgeVertexIDs[1], edgeID);
		}

		for (auto &kvp : Delta.EdgeDeletions)
		{
			bool bRemovedEdge = RemoveEdge(kvp.Key);
			ensureAlways(bRemovedEdge);
		}

		for (auto &kvp : Delta.FaceAdditions)
		{
			int32 faceID = kvp.Key;
			const TArray<int32> &faceVertexIDs = kvp.Value.Vertices;
			FGraph3DFace *newFace = AddFace(faceVertexIDs, faceID);
		}

		for (auto &kvp : Delta.FaceDeletions)
		{
			bool bRemovedFace = RemoveFace(kvp.Key);
			ensureAlways(bRemovedFace);
		}

		for (auto &kvp : Delta.FaceVertexAdditions)
		{
			auto face = FindFace(kvp.Key);
			auto addIDs = kvp.Value;

			// TODO: sometimes the face is removed as well by this point
			if (face == nullptr) continue;

			int32 numVertices = face->VertexIDs.Num();
			int32 addIdx = 0;

			TArray<int32> newVertexIDs;

			// -1 should be provided as the index for a vertex being added to the front of the 
			// VertexIDs list, because in the following loop the ID is added after the existing value at that index
			int32 addToFrontIdx = -1;
			if (addIDs.Contains(addToFrontIdx))
			{
				newVertexIDs.Add(addIDs[addToFrontIdx]);
			}

			for (int32 vertexIdx = 0; vertexIdx < numVertices; vertexIdx++)
			{
				newVertexIDs.Add(face->VertexIDs[vertexIdx]);
				if (addIDs.Contains(vertexIdx))
				{
					// this prevents double-adding the vertex 
					if (face->VertexIDs.Contains(addIDs[vertexIdx])) continue;

					newVertexIDs.Add(addIDs[vertexIdx]);
				}
			}

			face->VertexIDs = newVertexIDs;
			face->Dirty(false);
		}

		for (auto &kvp : Delta.FaceVertexRemovals)
		{
			auto face = FindFace(kvp.Key);
			if (ensureAlways(face != nullptr))
			{
				TArray<int32> removeIDs;
				kvp.Value.GenerateValueArray(removeIDs);

				TArray<int32> newVertexIDs;
				for (int32 vertexIdx = 0; vertexIdx < face->VertexIDs.Num(); vertexIdx++)
				{
					if (!removeIDs.Contains(face->VertexIDs[vertexIdx]))
					{
						newVertexIDs.Add(face->VertexIDs[vertexIdx]);
					}
				}
				face->VertexIDs = newVertexIDs;
				face->Dirty(false);
			}
		}

		for (auto &kvp : Faces)
		{
			FGraph3DFace &face = kvp.Value;

			if (face.bDirty)
			{
				bool bUpdatePlanes = face.ShouldUpdatePlane();
				if (bUpdatePlanes)
				{
					if (!face.UpdatePlane(face.VertexIDs))
					{
						return false;
					}
				}

				// Not ensuring here in favor of ensuring if the delta is expected to be correct
				// If UpdatePlanes is called, it will assign the vertices correctly, if it isn't called
				// it needs to be updated through UpdateVerticesAndEdges
				if (!face.UpdateVerticesAndEdges(face.VertexIDs, !bUpdatePlanes))
				{
					return false;
				}
			}
		}
		return true;
	}

	bool FGraph3D::CalculateVerticesOnLine(const FVertexPair &VertexPair, const FVector& StartPos, const FVector& EndPos, TArray<int32> &OutVertexIDs, TPair<int32, int32> &OutSplitEdgeIDs) const
	{
		TArray<TPair<float, int32>> verticesAlongLine;
		FVector direction = (EndPos - StartPos).GetSafeNormal();

		for (auto& vertexkvp : Vertices)
		{
			int32 vertexID = vertexkvp.Key;
			auto vertex = vertexkvp.Value;

			if (FMath::PointDistToLine(vertex.Position, direction, StartPos) < Epsilon)
			{
				float distanceAlongLine = (vertex.Position - StartPos) | direction;
				distanceAlongLine /= FVector::Dist(EndPos, StartPos);
				verticesAlongLine.Add(TPair<float, int32>(distanceAlongLine, vertexID));
			}
		}

		if (FindVertex(VertexPair.Key) == nullptr)
		{
			verticesAlongLine.Add(TPair<float, int32>(0.0f, VertexPair.Key));
		}
		if (FindVertex(VertexPair.Value) == nullptr)
		{
			verticesAlongLine.Add(TPair<float, int32>(1.0f, VertexPair.Value));
		}

		verticesAlongLine.Sort();

		// TODO: populate this field with the edgeIDs that are split by the vertices in the VertexPair
		OutSplitEdgeIDs = TPair<int32, int32>(MOD_ID_NONE, MOD_ID_NONE);

		// return vertices in order along line and determine whether the new vertices split any existing edges
		bool bAddVertex = false;
		int32 vertexIdx = 0;
		for (auto& kvp : verticesAlongLine)
		{
			int32 vertexId = kvp.Value;

			if (vertexId == VertexPair.Key)
			{
				bAddVertex = true;
				if (vertexIdx > 0)
				{
					int32 edgeStart = verticesAlongLine[vertexIdx - 1].Value;
					int32 edgeEnd = verticesAlongLine[vertexIdx + 1].Value;
					bool bForward;
					auto edge = FindEdgeByVertices(edgeStart, edgeEnd, bForward);
					if (edge != nullptr)
					{
						OutSplitEdgeIDs.Key = edge->ID;
					}
				}
			}

			if (bAddVertex)
			{
				OutVertexIDs.Add(vertexId);
			}

			if (vertexId == VertexPair.Value)
			{
				if (vertexIdx < verticesAlongLine.Num() - 1)
				{
					int32 edgeStart = verticesAlongLine[vertexIdx - 1].Value;
					int32 edgeEnd = verticesAlongLine[vertexIdx + 1].Value;
					bool bForward;
					auto edge = FindEdgeByVertices(edgeStart, edgeEnd, bForward);
					if (edge != nullptr)
					{
						OutSplitEdgeIDs.Value = edge->ID;
					}
				}
				break;
			}

			vertexIdx++;
		}

		return true;
	}

	void FGraph3D::FindOverlappingFaces(int32 AddedFaceID, TSet<int32> &OutOverlappingFaces) const
	{
		auto newFace = FindFace(AddedFaceID);
		if (newFace == nullptr)
		{
			return;
		}

		// check for duplicate edge normals
		int32 edgeIdx = 0;
		for (int32 edgeID : newFace->EdgeIDs)
		{
			FVector edgeNormal = newFace->CachedEdgeNormals[edgeIdx];
			auto edge = FindEdge(edgeID);

			for (auto connection : edge->ConnectedFaces)
			{
				if (FMath::Abs(connection.FaceID) != FMath::Abs(AddedFaceID) && FVector::Coincident(connection.EdgeFaceDir, edgeNormal))
				{
					OutOverlappingFaces.Add(connection.FaceID);
				}
			}
			edgeIdx++;
		}
	}

	int32 FGraph3D::FindOverlappingFace(int32 AddedFaceID) const
	{
		auto newFace = FindFace(AddedFaceID);
		if (newFace == nullptr)
		{
			return AddedFaceID;
		}
		// check for duplicate edge normals
		int32 edgeIdx = 0;
		for (int32 edgeID : newFace->EdgeIDs)
		{
			FVector edgeNormal = newFace->CachedEdgeNormals[edgeIdx];
			auto edge = FindEdge(edgeID);
			if (!ensure(edge != nullptr))
			{
				continue;
			}

			for (auto connection : edge->ConnectedFaces)
			{
				if (FMath::Abs(connection.FaceID) != FMath::Abs(AddedFaceID) && FVector::Coincident(connection.EdgeFaceDir, edgeNormal))
				{
					return connection.FaceID;
				}
			}
			edgeIdx++;
		}

		return MOD_ID_NONE;
	}

	bool FGraph3D::TraverseFacesFromEdge(int32 OriginalEdgeID, TArray<TArray<int32>> &OutVertexIDs, FPlane InPlane) const
	{
		// A plane can be provided to force the search to only be on that plane
		TArray<FPlane> planes;
		if (InPlane.Equals(FPlane(ForceInitToZero)))
		{
			GetPlanesForEdge(OriginalEdgeID, planes);
		}
		else
		{
			planes = { InPlane };
		}

		auto edge = FindEdge(OriginalEdgeID);
		if (edge == nullptr)
		{
			return false;
		}

		for (auto& plane : planes)
		{
			FGraph graph2D;

			if (!Create2DGraph(edge->StartVertexID, plane, graph2D))
			{
				continue;
			}

			auto originalEdge2D = graph2D.FindEdge(OriginalEdgeID);
			if (originalEdge2D == nullptr)
			{
				continue;
			}

			for (int32 polyID : { originalEdge2D->LeftPolyID, originalEdge2D->RightPolyID })
			{
				FGraphPolygon *face = graph2D.FindPolygon(polyID);
				if (face == nullptr)
				{
					continue;
				}

				if (!face->bInterior)
				{
					continue;
				}

				TArray<int32> faceVertexIDs;
				for (int32 edgeID : face->Edges)
				{
					auto edge2D = graph2D.FindEdge(edgeID);
					int32 vertexID = edgeID > 0 ? edge2D->StartVertexID : edge2D->EndVertexID;
					faceVertexIDs.Add(vertexID);
				}

				OutVertexIDs.Add(faceVertexIDs);
			}
		}
		return true;
	}

	void FGraph3D::TraverseFacesGeneric(const TSet<FSignedID> &StartingFaceIDs, TArray<FGraph3DTraversal> &OutTraversals,
		const FGraphObjPredicate &EdgePredicate, const FGraphObjPredicate &FacePredicate) const
	{
		TQueue<FSignedID> faceQueue;
		TSet<FSignedID> visitedFaceIDs;
		TMap<FSignedID, TSet<int32>> visitedEdgesPerFace;
		TArray<FSignedID> curTraversalFaceIDs;
		TArray<FVector> curTraversalPoints;

		for (FSignedID startingFaceID : StartingFaceIDs)
		{
			if (!FacePredicate(startingFaceID))
			{
				continue;
			}

			curTraversalFaceIDs.Reset();
			curTraversalPoints.Reset();
			faceQueue.Empty();
			faceQueue.Enqueue(startingFaceID);

			FSignedID curFaceID = MOD_ID_NONE;
			while (faceQueue.Dequeue(curFaceID))
			{
				const FGraph3DFace *curFace = FindFace(curFaceID);

				if (!visitedFaceIDs.Contains(curFaceID) && curFace)
				{
					visitedFaceIDs.Add(curFaceID);
					curTraversalFaceIDs.Add(curFaceID);
					curTraversalPoints.Append(curFace->CachedPositions);

					TSet<int32> &visitedEdgesForFace = visitedEdgesPerFace.FindOrAdd(curFaceID);
					for (FSignedID faceEdgeSignedID : curFace->EdgeIDs)
					{
						if (!EdgePredicate(faceEdgeSignedID))
						{
							continue;
						}

						int32 faceEdgeID = FMath::Abs(faceEdgeSignedID);
						if (visitedEdgesForFace.Contains(faceEdgeID))
						{
							continue;
						}

						FSignedID nextFaceID = MOD_ID_NONE;
						float dihedralAngle = 0.0f;
						int32 nextFaceIndex = INDEX_NONE;

						const FGraph3DEdge *faceEdge = FindEdge(faceEdgeID);
						if (faceEdge && faceEdge->GetNextFace(curFaceID, nextFaceID, dihedralAngle, nextFaceIndex) &&
							FacePredicate(nextFaceID))
						{
							faceQueue.Enqueue(nextFaceID);
							visitedEdgesForFace.Add(faceEdgeID);
						}
					}
				}
			}

			if (curTraversalFaceIDs.Num() > 0)
			{
				OutTraversals.Add(FGraph3DTraversal(curTraversalFaceIDs, curTraversalPoints));
			}
		}
	}

	bool FGraph3D::CleanGraph(TArray<int32> &OutCleanedVertices, TArray<int32> &OutCleanedEdges, TArray<int32> &OutCleanedFaces)
	{
		OutCleanedVertices.Reset();
		OutCleanedEdges.Reset();
		OutCleanedFaces.Reset();

		for (auto& kvp : Vertices)
		{
			auto& vertex = kvp.Value;
			if (vertex.bDirty && CleanObject(kvp.Key, EGraph3DObjectType::Vertex))
			{
				OutCleanedVertices.Add(kvp.Key);
			}
		}

		for (auto& kvp : Edges)
		{
			auto& edge = kvp.Value;
			if (edge.bDirty && CleanObject(kvp.Key, EGraph3DObjectType::Edge))
			{
				OutCleanedEdges.Add(kvp.Key);
			}
		}

		for (auto& kvp : Faces)
		{
			auto& face = kvp.Value;
			if (face.bDirty && CleanObject(kvp.Key, EGraph3DObjectType::Face))
			{
				OutCleanedFaces.Add(kvp.Key);
			}
		}

		for (int32 edgeID : OutCleanedEdges)
		{
			auto *edge = FindEdge(edgeID);
			edge->SortFaces();
		}

		bool bAnyChanged = (OutCleanedVertices.Num() > 0) || (OutCleanedEdges.Num() > 0) || (OutCleanedFaces.Num() > 0);
		if (bAnyChanged)
		{
			CalculatePolyhedra();
		}

		return bAnyChanged;
	}

	int32 FGraph3D::CalculatePolyhedra()
	{
		// clear the existing polygon data before computing new ones
		ClearPolyhedra();
		DirtyFaces.Empty();

		// TODO: ensure this is unnecessary so that the computation can be iterative
		for (auto &kvp : Faces)
		{
			int32 faceID = kvp.Key;
			FSignedID frontID = faceID;
			FSignedID backID = -faceID;
			DirtyFaces.Add(faceID);
			DirtyFaces.Add(backID);
		}

		TArray<FGraph3DTraversal> polyhedralTraversals;

		// traverse through all planes to get the polyhedral traversals
		TraverseFacesGeneric(DirtyFaces, polyhedralTraversals);
		for (const FGraph3DTraversal &traversal : polyhedralTraversals)
		{
			int32 newID = NextPolyhedronID++;
			FGraph3DPolyhedron newPolyhedron(newID, this);
			newPolyhedron.FaceIDs = traversal.FaceIDs;
			newPolyhedron.AABB = FBox(traversal.FacePoints);

			for (FSignedID faceID : newPolyhedron.FaceIDs)
			{
				if (FGraph3DFace *face = FindFace(faceID))
				{
					if (faceID > 0)
					{
						face->FrontPolyhedronID = newID;
					}
					else
					{
						face->BackPolyhedronID = newID;
					}
				}
			}

			Polyhedra.Add(newID, MoveTemp(newPolyhedron));
		}

		// Search through the faces to find which ones have different polyhedra on either side;
		// for such pairs of polyhedra, one must be interior and the other exterior.
		TSet<int32> closedPolyhedronIDs;
		for (const auto &kvp : Faces)
		{
			const FGraph3DFace &face = kvp.Value;
			if (face.FrontPolyhedronID != face.BackPolyhedronID)
			{
				closedPolyhedronIDs.Add(face.FrontPolyhedronID);
				closedPolyhedronIDs.Add(face.BackPolyhedronID);
			}
		}

		for (auto &kvp : Polyhedra)
		{
			int32 polyhedronID = kvp.Key;
			FGraph3DPolyhedron &polyhedron = kvp.Value;

			if (closedPolyhedronIDs.Contains(polyhedronID))
			{
				polyhedron.bClosed = true;
				polyhedron.DetermineInterior();
				polyhedron.DetermineConvex();
			}
			else
			{
				polyhedron.bClosed = false;
				polyhedron.bInterior = false;
			}
		}

		bDirty = false;
		return Polyhedra.Num();
	}

	void FGraph3D::ClearPolyhedra()
	{
		Polyhedra.Reset();
		NextPolyhedronID = 1;
		bDirty = true;
	}

	void FGraph3D::CloneFromGraph(FGraph3D &tempGraph, const FGraph3D &graph)
	{
		tempGraph.Reset();
		tempGraph = graph;

		for (auto& edgekvp : tempGraph.Edges)
		{
			edgekvp.Value.Graph = &tempGraph;
		}
		for (auto& vertexkvp : tempGraph.Vertices)
		{
			vertexkvp.Value.Graph = &tempGraph;
		}
		for (auto& facekvp : tempGraph.Faces)
		{
			facekvp.Value.Graph = &tempGraph;
		}
		for (auto& polykvp : tempGraph.Polyhedra)
		{
			polykvp.Value.Graph = &tempGraph;
		}
	}

	bool FGraph3D::Validate()
	{
		for (auto &kvp : Vertices)
		{
			if (!kvp.Value.ValidateForTests())
			{
				return false;
			}
		}

		for (auto &kvp : Edges)
		{
			if (!kvp.Value.ValidateForTests())
			{
				return false;
			}
		}

		for (auto &kvp : Faces)
		{
			if (!kvp.Value.ValidateForTests())
			{
				return false;
			}
		}

		return true;
	}

	bool FGraph3D::GetPlanesForEdge(int32 OriginalEdgeID, TArray<FPlane> &OutPlanes) const
	{
		OutPlanes.Reset();

		auto originalEdge = FindEdge(OriginalEdgeID);
		if (originalEdge == nullptr)
		{
			return false;
		}

		TQueue<int32> frontierVertexIDs;
		frontierVertexIDs.Enqueue(originalEdge->StartVertexID);
		frontierVertexIDs.Enqueue(originalEdge->EndVertexID);

		FVector originalDirection = originalEdge->CachedDir;

		TSet<int32> visitedVertexIDs;

		while (!frontierVertexIDs.IsEmpty())
		{
			int32 currentVertexID;
			frontierVertexIDs.Dequeue(currentVertexID);

			visitedVertexIDs.Add(currentVertexID);

			auto currentVertex = FindVertex(currentVertexID);

			for (int32 edgeID : currentVertex->ConnectedEdgeIDs)
			{
				auto edge = FindEdge(edgeID);
				if (edge == nullptr)
				{
					continue;
				}

				int32 nextVertexID = (edge->StartVertexID == currentVertexID) ? edge->EndVertexID : edge->StartVertexID;
				if (visitedVertexIDs.Contains(nextVertexID))
				{
					continue;
				}

				FVector currentDirection = edge->CachedDir;

				if (FVector::Parallel(currentDirection, originalDirection))
				{
					frontierVertexIDs.Enqueue(nextVertexID);
				}
				else
				{
					FVector currentPosition = currentVertex->Position;
					FPlane newPlane = FPlane(currentPosition, currentPosition + currentDirection, currentPosition + originalDirection);
					if (newPlane.W < 0)
					{
						newPlane *= -1.0f;
					}

					// TArray::AddUnique uses operator==, which for FPlane does not allow for a tolerance
					bool bAddPlane = true;
					for (auto& plane : OutPlanes)
					{
						if (plane.Equals(newPlane, Epsilon))
						{
							bAddPlane = false;
							break;
						}
					}
					if (bAddPlane)
					{
						OutPlanes.Add(newPlane);
					}
				}
			}

		}

		return true;
	}

	bool FGraph3D::Create2DGraph(int32 startVertexID, const FPlane &CutPlane, FGraph &OutGraph) const
	{
		OutGraph.Reset();

		TQueue<int32> frontierVertexIDs;
		frontierVertexIDs.Enqueue(startVertexID);

		TSet<int32> visitedVertexIDs;

		FVector Cached2DX, Cached2DY;
		UModumateGeometryStatics::FindBasisVectors(Cached2DX, Cached2DY, CutPlane);

		auto startVertex = FindVertex(startVertexID);
		if (startVertex == nullptr)
		{
			return false;
		}

		FVector planeOrigin = startVertex->Position;

		OutGraph.AddVertex(FVector2D(EForceInit::ForceInitToZero), startVertex->ID);

		// BFS
		while (!frontierVertexIDs.IsEmpty())
		{
			int32 currentVertexID;
			frontierVertexIDs.Dequeue(currentVertexID);

			visitedVertexIDs.Add(currentVertexID);

			auto currentVertex = FindVertex(currentVertexID);
			if (currentVertex == nullptr)
			{
				continue;
			}

			for (int32 edgeID : currentVertex->ConnectedEdgeIDs)
			{
				auto edge = FindEdge(edgeID);
				int32 nextVertexID = (edge->StartVertexID == currentVertexID) ? edge->EndVertexID : edge->StartVertexID;

				if (visitedVertexIDs.Contains(nextVertexID))
				{
					continue;
				}

				auto nextVertex = FindVertex(nextVertexID);

				if (FMath::Abs(CutPlane.PlaneDot(nextVertex->Position)) > Epsilon)
				{
					continue;
				}

				FVector difference = nextVertex->Position - planeOrigin;
				FVector2D position2D = FVector2D(difference | Cached2DX, difference | Cached2DY);

				OutGraph.AddVertex(position2D, nextVertex->ID);

				frontierVertexIDs.Enqueue(nextVertexID);

				if (edgeID < 0)
				{
					OutGraph.AddEdge(nextVertexID, currentVertexID, -edgeID);
				}
				else
				{
					OutGraph.AddEdge(currentVertexID, nextVertexID, edgeID);
				}
			}
		}

		OutGraph.CalculatePolygons();

		return true;
	}

	bool FGraph3D::Create2DGraph(const FPlane &CutPlane, const FVector &AxisX, const FVector &AxisY, const FVector &Origin, const FBox2D &BoundingBox, FGraph &OutGraph, TMap<int32, int32> &OutGraphIDToObjID) const
	{
		OutGraph.Reset();

		int32 NextID = 0;
		// TODO: unclear whether clipping needs to be done here or not.
		// Currently, clipping is done here meaning that the graph returned fits inside of the view provided.
		// However, the edges/vertices created may not map exactly to the objects in the scene.
		// Potentially all of the clipping could be handled by drafting later on, but it is unclear what parts 
		// of drafting will be used to help get the drafting lines into the 3D scene

		// TODO: given a floorplan view, calculate polygons for the rooms
		for (auto& facekvp : Faces)
		{
			auto face = facekvp.Value;

			FVector intersectingEdgeOrigin, intersectingEdgeDirection;
			TArray<TPair<FVector, FVector>> intersectingSegments;
			if (face.IntersectsPlane(CutPlane, intersectingEdgeOrigin, intersectingEdgeDirection, intersectingSegments))
			{
				for (auto segment : intersectingSegments)
				{
					TArray<int32> edgeVertexIDs;
					TArray<FVector2D> points2D;
					points2D.Add(UModumateGeometryStatics::ProjectPoint2D(segment.Key, AxisX, AxisY, Origin));
					points2D.Add(UModumateGeometryStatics::ProjectPoint2D(segment.Value, AxisX, AxisY, Origin));

					FVector2D clippedStart, clippedEnd;
					if (UModumateFunctionLibrary::ClipLine2DToRectangle(
						points2D[0],
						points2D[1],
						BoundingBox,
						clippedStart,
						clippedEnd))
					{
						for (auto& point : { clippedStart, clippedEnd })
						{

							// TODO: FindVertex is linear
							FVector2D p = point;
							auto existingVertex = OutGraph.FindVertex(p);
							if (existingVertex == nullptr)
							{
								NextID++;
								OutGraph.AddVertex(point, NextID);
								edgeVertexIDs.Add(NextID);
							}
							else
							{
								edgeVertexIDs.Add(existingVertex->ID);
							}
						}
						NextID++;
						ensureAlways(edgeVertexIDs.Num() == 2);
						OutGraph.AddEdge(edgeVertexIDs[0], edgeVertexIDs[1], NextID);

						OutGraphIDToObjID.Add(NextID, face.ID);
					}
				}
			}
		}
		
		return false;
	}
}
