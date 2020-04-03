// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "Graph3D.h"

#include "ModumateGeometryStatics.h"

#include "Graph3DDelta.h"

namespace Modumate
{
	bool FGraph3D::GetDeltaForVertexAddition(FGraph3D *Graph, const FVector &VertexPos, FGraph3DDelta &OutDelta, int32 &NextID, int32 &ExistingID)
	{
		const FGraph3DVertex *existingVertex = Graph->FindVertex(VertexPos);
		int32 existingDeltaVertexID = OutDelta.FindAddedVertex(VertexPos);

		if (existingVertex)
		{
			ExistingID = existingVertex->ID;
			return false;
		}
		else if (existingDeltaVertexID != MOD_ID_NONE)
		{
			ExistingID = existingDeltaVertexID;
			return false;
		}
		else
		{
			int32 addedVertexID = NextID++;
			OutDelta.VertexAdditions.Add(addedVertexID, VertexPos);
			return true;
		}
	}

	bool FGraph3D::GetDeltaForVertexMovements(FGraph3D *OldGraph, FGraph3D *Graph, const TArray<int32> &VertexIDs, const TArray<FVector> &NewVertexPositions, TArray<FGraph3DDelta> &OutDeltas, int32 &NextID)
	{
		// First, move all of the vertices
		FGraph3DDelta OutDelta;

		int32 numVertices = VertexIDs.Num();
		if (numVertices != NewVertexPositions.Num())
		{
			return false;
		}

		TSet<int32> oldConnectedFaceIDs;

		bool bJoined = false;
		bool bValidDelta = true;
		for (int32 i = 0; i < numVertices; ++i)
		{
			int32 vertexID = VertexIDs[i];
			FVector newPos = NewVertexPositions[i];
			const FGraph3DVertex *vertex = Graph->FindVertex(vertexID);
			if (vertex == nullptr)
			{
				return false;
			}

			FVector oldPos = vertex->Position;

			vertex->GetConnectedFaces(oldConnectedFaceIDs);

			if (!newPos.Equals(oldPos))
			{
				// if there exists a vertex at the move location
				const FGraph3DVertex *currentVertex = Graph->FindVertex(newPos);
				if (currentVertex != nullptr)
				{
					// TODO: implement vertex and edge joining
					if (!GetDeltaForVertexJoin(Graph, OutDelta, NextID, currentVertex->ID, vertexID))
					{
						return false;
					}
					else
					{
						// TODO: super redundant work here, will need to stay until there is proper merging of deltas
						FGraph3D::CloneFromGraph(*Graph, *OldGraph);
						Graph->ApplyDelta(OutDelta);

						bJoined = true;
					}
				}
				else
				{
					OutDelta.VertexMovements.Add(vertexID, TPair<FVector, FVector>(oldPos, newPos));
				}
			}
		}

		// TODO: this function also needs to check for valid intersections / face additions as a result, regardless
		// of what kinds of intersection behavior is supported
		FGraph3D::CloneFromGraph(*Graph, *OldGraph);
		bValidDelta = Graph->ApplyDelta(OutDelta);

		if (!bValidDelta)
		{
			return bValidDelta;
		}

		OutDeltas.Add(OutDelta);

		// after the vertices have been moved, check if any of the old faces that have been modified
		// have been invalidated
		FGraph3DDelta removeFaceDelta;
		for (int32 faceID : oldConnectedFaceIDs)
		{
			auto face = Graph->FindFace(faceID);
			if (face == nullptr)
			{
				continue;
			}

			if (!Graph->CheckFaceNormals(faceID))
			{
				removeFaceDelta.FaceDeletions.Add(faceID, FGraph3DObjDelta(face->VertexIDs));
			}
		}

		OutDeltas.Add(removeFaceDelta);

		TSet<int32> connectedFaceIDs;
		TSet<int32> connectedEdgeIDs;
		for(FVector pos : NewVertexPositions)
		{
			auto vertex = Graph->FindVertex(pos);
			if (vertex == nullptr)
			{
				return false;
			}

			vertex->GetConnectedFacesAndEdges(connectedFaceIDs, connectedEdgeIDs);
		}

		if (!GetDeltasForEdgeSplits(Graph, OutDeltas, NextID))
		{
			return false;
		}


		for (int32 faceID : connectedFaceIDs)
		{
			// Intersection checks
			FGraph3DDelta checkFaceDelta;
			int32 ExistingID = MOD_ID_NONE;
			bool bFoundSplit = false;

			if (!GetDeltaForFaceSplit(Graph, checkFaceDelta, NextID, ExistingID, faceID, bFoundSplit))
			{
				return false;
			}

			if (!checkFaceDelta.IsEmpty())
			{
				OutDeltas.Add(checkFaceDelta);
			}

			Graph->ApplyDelta(checkFaceDelta);
		}

		return bJoined || (OutDelta.VertexMovements.Num() > 0);
	}

	bool FGraph3D::GetDeltaForEdgeAddition(FGraph3D *Graph, const FVertexPair &VertexPair, FGraph3DDelta &OutDelta, int32 &NextID, int32 &ExistingID, const TArray<int32> ParentIDs)
	{
		bool bExistingEdgeForward = true;
		const FGraph3DEdge *existingEdge = Graph->FindEdgeByVertices(VertexPair.Key, VertexPair.Value, bExistingEdgeForward);
		int32 existingDeltaEdgeID = OutDelta.FindAddedEdge(VertexPair);

		if (VertexPair.Key == VertexPair.Value)
		{
			return false;
		}

		if (existingEdge)
		{
			ExistingID = existingEdge->ID;
			return false;
		}
		else if (existingDeltaEdgeID != MOD_ID_NONE)
		{
			ExistingID = existingDeltaEdgeID;
			return false;
		}
		else
		{
			int32 addedEdgeID = NextID++;
			ExistingID = addedEdgeID;
			OutDelta.EdgeAdditions.Add(addedEdgeID, FGraph3DObjDelta(VertexPair, ParentIDs));
			return true;
		}
	}

	// Checks against the current graph and makes edges to connect the provided vertices.  OutVertexIDs is a list of vertices
	// along the edges that connect the two vertices in VertexPair
	bool FGraph3D::GetDeltaForMultipleEdgeAdditions(FGraph3D *Graph, const FVertexPair &VertexPair, FGraph3DDelta &OutDelta, int32 &NextID, int32 &ExistingID, TArray<int32> &OutVertexIDs, TArray<int32> ParentIDs)
	{
		bool bAddedEdge = false;

		bool bExistingEdgeForward = true;
		const FGraph3DEdge *existingEdge = Graph->FindEdgeByVertices(VertexPair.Key, VertexPair.Value, bExistingEdgeForward);

		if (VertexPair.Key == VertexPair.Value)
		{
			return false;
		}

		if (existingEdge)
		{
			OutVertexIDs = { VertexPair.Key, VertexPair.Value };
			ExistingID = existingEdge->ID;
			return false;
		}

		FVector startPos;
		if (OutDelta.VertexAdditions.Contains(VertexPair.Key))
		{
			startPos = OutDelta.VertexAdditions[VertexPair.Key];
		}
		else if (const FGraph3DVertex *vertex = Graph->FindVertex(VertexPair.Key))
		{
			startPos = vertex->Position;
		}
		else
		{
			return false;
		}

		FVector endPos;
		if (OutDelta.VertexAdditions.Contains(VertexPair.Value))
		{
			endPos = OutDelta.VertexAdditions[VertexPair.Value];
		}
		else if (const FGraph3DVertex *vertex = Graph->FindVertex(VertexPair.Value))
		{
			endPos = vertex->Position;
		}
		else
		{
			return false;
		}

		// find all of the vertices that are on the line that connects startPos and endPos, and determine
		// the vertices that are between them
		TPair<int32, int32> splitEdgeIDs;
		Graph->CalculateVerticesOnLine(VertexPair, startPos, endPos, OutVertexIDs, splitEdgeIDs);

		// TODO: given edges that are split, delete the split edge and update connected faces
		if (splitEdgeIDs.Key == MOD_ID_NONE && splitEdgeIDs.Value == MOD_ID_NONE)
		{
			for (int32 vertexIdx = 0; vertexIdx < OutVertexIDs.Num() - 1; vertexIdx++)
			{
				FVertexPair vertexPair = FVertexPair(OutVertexIDs[vertexIdx], OutVertexIDs[vertexIdx + 1]);
				// ExistingID should not be used if there are several edges
				int32 existingID;
				bAddedEdge = GetDeltaForEdgeAddition(Graph, vertexPair, OutDelta, NextID, existingID, ParentIDs) || bAddedEdge;
			}
		}
		else
		{
			OutVertexIDs = { VertexPair.Key, VertexPair.Value };

			bAddedEdge = GetDeltaForEdgeAddition(Graph, VertexPair, OutDelta, NextID, ExistingID, ParentIDs) || bAddedEdge;
		}

		return bAddedEdge;
	}

	bool FGraph3D::GetDeltaForEdgeAdditionWithSplit(FGraph3D *OldGraph, FGraph3D *Graph, const FVertexPair &VertexPair, TArray<FGraph3DDelta> &OutDeltas, int32 &NextID, int32 &ExistingID)
	{
		FGraph3DDelta OutDelta;
		if (!GetDeltaForEdgeAddition(Graph, VertexPair, OutDelta, NextID, ExistingID))
		{
			return false;
		}

		Graph->ApplyDelta(OutDelta);
		OutDeltas.Add(OutDelta);

		int32 edgeID = MOD_ID_NONE;
		if (OutDelta.EdgeAdditions.Num() == 0)
		{
			return false;
		}
		else if (OutDelta.EdgeAdditions.Num() > 1)
		{
			return true;
		}
		else
		{
			for (auto& kvp : OutDelta.EdgeAdditions)
			{
				edgeID = kvp.Key;
				break;
			}
		}

		if (!GetDeltasForEdgeSplits(Graph, OutDeltas, NextID))
		{
			return false;
		}

		int32 existingID;
		bool bFoundSplit = false;
		OutDelta.Reset();

		// TODO: split by set of edges? potentially, rewind faces based off the new edge
		if (Graph->FindEdge(edgeID) != nullptr)
		{
			if (!GetDeltaForFaceSplitByEdge(Graph, OutDelta, NextID, existingID, edgeID, bFoundSplit))
			{
				return false;
			}
		}

		OutDeltas.Add(OutDelta);

		return true;
	}

	bool FGraph3D::GetDeltaForEdgeAdditionWithSplit(FGraph3D *OldGraph, FGraph3D *Graph, const FVector &EdgeStartPos, const FVector &EdgeEndPos, TArray<FGraph3DDelta> &OutDeltas, int32 &NextID, int32 &ExistingID)
	{
		FVector vertexPositions[2] = { EdgeStartPos, EdgeEndPos };
		int32 vertexIDs[2] = { MOD_ID_NONE, MOD_ID_NONE };

		for (int32 i = 0; i < 2; ++i)
		{
			const FVector &vertexPosition = vertexPositions[i];

			FGraph3DDelta addVertexDelta;
			int32 vertexID = MOD_ID_NONE;
			if (GetDeltaForVertexAddition(Graph, vertexPosition, addVertexDelta, NextID, vertexID))
			{
				if (!ensureAlways(addVertexDelta.VertexAdditions.Num() == 1))
				{
					return false;
				}

				for (auto &kvp : addVertexDelta.VertexAdditions)
				{
					vertexID = kvp.Key;
					break;
				}

				OutDeltas.Add(addVertexDelta);
				Graph->ApplyDelta(addVertexDelta);
			}

			if (!ensureAlways(vertexID != MOD_ID_NONE))
			{
				return false;
			}

			vertexIDs[i] = vertexID;
		}

		FVertexPair vertexPair(vertexIDs[0], vertexIDs[1]);
		return GetDeltaForEdgeAdditionWithSplit(OldGraph, Graph, vertexPair, OutDeltas, NextID, ExistingID);
	}

	bool FGraph3D::GetDeltaForFaceAddition(FGraph3D *Graph, const TArray<int32> &VertexIDs, FGraph3DDelta &OutDelta, int32 &NextID, int32 &ExistingID, TArray<int32> &ParentFaceIDs, TMap<int32, int32> &ParentEdgeIdxToID, int32& AddedFaceID)
	{
		TArray<int32> faceVertices;

		int32 numVertices = VertexIDs.Num();
		TArray<FSignedID> newFaceEdgeIDs;
		for (int32 idxA = 0; idxA < numVertices; ++idxA)
		{
			int32 idxB = (idxA + 1) % numVertices;
			int32 vertexIDA = VertexIDs[idxA];
			int32 vertexIDB = VertexIDs[idxB];
			bool bEdgeForward = true;

			FVertexPair currentPair = TPair<int32, int32>(vertexIDA, vertexIDB);
			TArray<int32> vertices;
			int32 existingID;

			if (ParentEdgeIdxToID.Contains(idxA))
			{
				GetDeltaForMultipleEdgeAdditions(Graph, currentPair, OutDelta, NextID, existingID, vertices, { ParentEdgeIdxToID[idxA] });
			}
			else
			{
				GetDeltaForMultipleEdgeAdditions(Graph, currentPair, OutDelta, NextID, existingID, vertices);
			}

			// the last value in the vertex list is not needed because it will also be the first value for the next edge
			for (int32 idx = 0; idx < vertices.Num() - 1; idx++)
			{
				faceVertices.Add(vertices[idx]);
			}
		}

		AddedFaceID = NextID++;
		OutDelta.FaceAdditions.Add(AddedFaceID, FGraph3DObjDelta(faceVertices, ParentFaceIDs));

		return true;
	}

	bool FGraph3D::GetDeltaForFaceAddition(FGraph3D *OldGraph, FGraph3D *Graph, const TArray<FVector> &VertexPositions, TArray<FGraph3DDelta> &OutDeltas, int32 &NextID, int32 &ExistingID)
	{
		TArray<int32> newVertices;
		FGraph3DDelta OutDelta;
		GetDeltaForVertexList(Graph, newVertices, VertexPositions, OutDelta, NextID);

		bool bFoundIntersection = false;
		bool bDeltaCreationSuccess = true;

		TArray<int32> sharedEdgeIDs;

		// TODO: currently only supports the added face being split once
		bool bFoundSplit = false;

		int32 addedFaceID;
		TArray<int32> parentIds = { MOD_ID_NONE };
		TMap<int32, int32> edgeMap;
		GetDeltaForFaceAddition(Graph, newVertices, OutDelta, NextID, ExistingID, parentIds, edgeMap, addedFaceID);
		Graph->ApplyDelta(OutDelta);
		OutDeltas.Add(OutDelta);
		OutDelta.Reset();

		if (!GetDeltasForEdgeSplits(Graph, OutDeltas, NextID))
		{
			return false;
		}

		bDeltaCreationSuccess = GetDeltaForFaceSplit(Graph, OutDelta, NextID, ExistingID, addedFaceID, bFoundSplit);
		if (!bDeltaCreationSuccess)
		{
			FGraph3D::CloneFromGraph(*Graph, *OldGraph);
			return false;
		}

		OutDeltas.Add(OutDelta);

		return bDeltaCreationSuccess;
	}

	bool FGraph3D::GetDeltasForEdgeSplits(FGraph3D *Graph, TArray<FGraph3DDelta> &OutDeltas, int32 &NextID)
	{
		for (auto& kvp : Graph->GetVertices())
		{
			auto vertex = &kvp.Value;
			FVector pos = vertex->Position;
			if (vertex == nullptr)
			{
				return false;
			}

			auto edge = Graph->FindEdge(pos, vertex->ID);
			if (edge == nullptr)
			{
				continue;
			}

			FGraph3DDelta splitEdgeDelta;
			int32 ExistingID = MOD_ID_NONE;

			if (!GetDeltaForEdgeSplit(Graph, splitEdgeDelta, edge->ID, vertex->ID, NextID, ExistingID))
			{
				return false;
			}

			if (!splitEdgeDelta.IsEmpty())
			{
				OutDeltas.Add(splitEdgeDelta);
			}

			if (!Graph->ApplyDelta(splitEdgeDelta))
			{
				return false;
			}
		}

		return true;
	}

	bool FGraph3D::GetDeltaForFaceVertexAddition(FGraph3D *Graph, int32 EdgeIDToRemove, int32 FaceID, int32 VertexIDToAdd, FGraph3DDelta &OutDelta)
	{
		auto edge = Graph->FindEdge(EdgeIDToRemove);
		if (edge == nullptr)
		{
			return false;
		}

		for (auto connection : edge->ConnectedFaces)
		{
			if (connection.FaceID != FaceID)
			{
				bool bForward;
				int32 edgeIdx = Graph->FindFace(connection.FaceID)->FindEdgeIndex(edge->ID, bForward);
				if (!OutDelta.FaceVertexAdditions.Contains(connection.FaceID))
				{
					OutDelta.FaceVertexAdditions.Add(connection.FaceID, TMap<int32, int32>());
				}
				if (ensureAlways(edgeIdx != -1))
				{
					if (OutDelta.FaceVertexAdditions[connection.FaceID].Contains(edgeIdx))
					{
						return false;
					}
					OutDelta.FaceVertexAdditions[connection.FaceID].Add(edgeIdx, VertexIDToAdd);
				}
			}
		}

		return true;
	}

	bool FGraph3D::GetDeltaForFaceVertexRemoval(FGraph3D *Graph, int32 VertexIDToRemove, FGraph3DDelta &OutDelta)
	{
		auto vertex = Graph->FindVertex(VertexIDToRemove);
		if (vertex == nullptr)
		{
			return false;
		}

		TSet<int32> connectedEdges, connectedFaces;
		vertex->GetConnectedFacesAndEdges(connectedFaces, connectedEdges);

		// TODO: in future versions of this, this constraint may be relaxed here.
		// this function does not update the adjacent edges, so edges will need 
		// to be updated to match the vertex being removed
		if (connectedEdges.Num() != 2)
		{
			return false;
		}

		for (int32 faceID : connectedFaces)
		{
			auto face = Graph->FindFace(faceID);
			if (face == nullptr)
			{
				continue;
			}

			int32 vertexIdx = face->VertexIDs.Find(VertexIDToRemove);
			if (!OutDelta.FaceVertexRemovals.Contains(faceID))
			{
				OutDelta.FaceVertexRemovals.Add(faceID, TMap<int32, int32>());
			}

			// when removing vertices, ApplyDelta only looks for the ID of the vertex to remove
			// the index is only important for undo - and adding in vertices will add the vertex after the index provided.
			// since removing the vertex reduces the size of the list by 1, adding it back in the same place with undo
			// requires providing the index before its current index
			if (ensureAlways(vertexIdx != -1))
			{
				if (OutDelta.FaceVertexRemovals[faceID].Contains(vertexIdx))
				{
					return false;
				}
				OutDelta.FaceVertexRemovals[faceID].Add(vertexIdx-1, VertexIDToRemove);
			}
		}

		return true;
	}

	bool FGraph3D::GetDeltaForVertexList(FGraph3D *Graph, TArray<int32> &OutVertexIDs, const TArray<FVector> &InVertexPositions, FGraph3DDelta& OutDelta, int32 &NextID)
	{
		bool bHasNewVertex = false;

		for (const FVector &vertexPos : InVertexPositions)
		{
			int32 vertexID = MOD_ID_NONE;
			const FGraph3DVertex *existingVertex = Graph->FindVertex(vertexPos);
			int32 existingDeltaVertexID = OutDelta.FindAddedVertex(vertexPos);
			if (existingVertex)
			{
				vertexID = existingVertex->ID;
			}
			else if (existingDeltaVertexID != MOD_ID_NONE)
			{
				vertexID = existingDeltaVertexID;
			}
			else
			{
				vertexID = NextID++;
				OutDelta.VertexAdditions.Add(vertexID, vertexPos);
				bHasNewVertex = true;
			}
			OutVertexIDs.Add(vertexID);
		}
		return bHasNewVertex;
	}

	bool FGraph3D::GetDeltaForFaceSplit(FGraph3D *Graph, FGraph3DDelta &OutDelta, int32 &NextID, int32 &ExistingID, int32 &FaceID, bool &bOutFoundSplit, TPair<FVector, FVector> &Intersection, TPair<int32, int32> &EdgeIdxs)
	{
		FVector intersectStart = Intersection.Key;
		FVector intersectEnd = Intersection.Value;

		int32 edgeIdxStart = EdgeIdxs.Key;
		int32 edgeIdxEnd = EdgeIdxs.Value;

		auto face = Graph->FindFace(FaceID);

		if (face == nullptr)
		{
			return false;
		}

		int32 numEdges = face->CachedPositions.Num();

		// not handling tangent corners
		if ((intersectStart - intersectEnd).IsNearlyZero())
		{
			return false;
		}

		TArray<int32> facesToRemove;
		TMap<int32, TArray<int32>> newFaceIDs;

		facesToRemove.Add(face->ID);
		newFaceIDs.Add(face->ID, TArray<int32>());

		bOutFoundSplit = true;

		TArray<int32> edgesToRemove;
		TArray<int32> intersectionIDs;

		GetDeltaForVertexList(Graph, intersectionIDs, { intersectStart, intersectEnd }, OutDelta, NextID);

		TArray<TArray<int32>> OutFaceVertices;

		int32 idx = 0;
		TArray<int32> currentEdgesToRemove;
		TMap<int32, int32> edgeMap;
		if (!face->FindVertex(intersectStart))
		{
			currentEdgesToRemove.Add(face->EdgeIDs[edgeIdxStart]);
			edgeMap.Add(edgeIdxStart, face->EdgeIDs[edgeIdxStart]);
		}
		if (!face->FindVertex(intersectEnd))
		{
			// if the edge at the start intersection is not being removed, it shouldn't be
			// considered for edge splitting
			if (currentEdgesToRemove.Num() == 0)
			{
				idx = 1;
			}
			currentEdgesToRemove.Add(face->EdgeIDs[edgeIdxEnd]);
			edgeMap.Add(edgeIdxEnd, face->EdgeIDs[edgeIdxEnd]);
		}

		edgesToRemove.Append(currentEdgesToRemove);

		for (auto edgeID : currentEdgesToRemove)
		{
			GetDeltaForFaceVertexAddition(Graph, edgeID, face->ID, intersectionIDs[idx], OutDelta);

			idx++;
		}

		if (face->SplitFace({ edgeIdxStart, edgeIdxEnd }, intersectionIDs, OutFaceVertices))
		{
			for (auto& vertexIDs : OutFaceVertices)
			{
				int32 addedFaceID;
				TArray<int32> parentIds = { face->ID };

				GetDeltaForFaceAddition(Graph, vertexIDs, OutDelta, NextID, ExistingID, parentIds, edgeMap, addedFaceID);
				// TODO: may be a bug, this face may never actually exist
				newFaceIDs.Add(addedFaceID);
			}
		}

		if (facesToRemove.Num() > 0)
		{
			GetDeltaForDeletions(Graph, {}, edgesToRemove, facesToRemove, OutDelta);
			for (auto newFacekvp : newFaceIDs)
			{
				if (newFacekvp.Value.Num() > 0)
				{
					OutDelta.FaceDeletions[newFacekvp.Key].ParentFaceIDs = newFacekvp.Value;
				}
			}
		}

		return true;
	}

	bool FGraph3D::GetDeltaForFaceSplitByEdge(FGraph3D *Graph, FGraph3DDelta &OutDelta, int32 &NextID, int32 &ExistingID, int32 &EdgeID, bool &bOutFoundSplit)
	{
		FGraph3DEdge *newEdge = Graph->FindEdge(EdgeID);
		if (newEdge == nullptr)
		{
			return false;
		}

		bool bFaceHasSplit = false;

		for (auto &kvp : Graph->GetFaces())
		{
			const FGraph3DFace* otherFace = &kvp.Value;

			FVector intersectingEdgeOrigin, intersectingEdgeDir;
			TArray<FEdgeIntersection> intersections;
			bool bOnFaceEdge;

			if (newEdge->IntersectsFace(otherFace, intersectingEdgeOrigin, intersectingEdgeDir, intersections, bOnFaceEdge))
			{
				int32 currentFaceID = kvp.Key;

				for (int segmentIdx = 0; segmentIdx < intersections.Num() - 1; segmentIdx += 2)
				{
					int32 idx1, idx2;
					if (intersections[segmentIdx].EdgeIdxA < intersections[segmentIdx + 1].EdgeIdxA)
					{
						idx1 = segmentIdx;
						idx2 = segmentIdx + 1;
					}
					else
					{
						idx1 = segmentIdx + 1;
						idx2 = segmentIdx;
					}

					FEdgeIntersection i1 = intersections[idx1];
					FEdgeIntersection i2 = intersections[idx2];

					auto intersection = TPair<FVector, FVector>(i1.Point, i2.Point);
					auto edgeIdx = TPair<int32, int32>(i1.EdgeIdxA, i2.EdgeIdxA);

					bool bFoundSplit = false;
					if (!GetDeltaForFaceSplit(Graph, OutDelta, NextID, ExistingID, currentFaceID, bFoundSplit, intersection, edgeIdx))
					{
						return false;
					}
				}

			}
		}


		return true;
	}

	bool FGraph3D::GetDeltaForFaceSplit(FGraph3D *Graph, FGraph3DDelta &OutDelta, int32 &NextID, int32 &ExistingID, int32 &FaceID, bool &bOutFoundSplit)
	{
		FGraph3DFace *newFace = Graph->FindFace(FaceID);
		if (newFace == nullptr)
		{
			return false;
		}

		bool bFaceHasSplit = false;

		for (auto &kvp : Graph->GetFaces())
		{
			const FGraph3DFace* otherFace = &kvp.Value;

			if (kvp.Key == FaceID)
			{
				continue;
			}

			FVector intersectingEdgeOrigin, intersectingEdgeDir;
			TArray<FEdgeIntersection> sourceIntersections;
			TArray<FEdgeIntersection> destIntersections;
			TPair<bool, bool> bOnFaceEdges;

			if (newFace->IntersectsFace(otherFace, intersectingEdgeOrigin, intersectingEdgeDir, sourceIntersections, destIntersections, bOnFaceEdges))
			{
				TArray<TArray<FEdgeIntersection>> allIntersections = { sourceIntersections, destIntersections };
				TArray<int32> faceIDs = { newFace->ID, kvp.Key };

				for (int32 intersectionIdx = 0; intersectionIdx < allIntersections.Num(); intersectionIdx++)
				{
					bool bIntersectionOnEdge = intersectionIdx == 0 ? bOnFaceEdges.Key : bOnFaceEdges.Value;
					if (bIntersectionOnEdge)
					{
						continue;
					}

					auto intersections = allIntersections[intersectionIdx];
					int32 currentFaceID = faceIDs[intersectionIdx];

					for (int segmentIdx = 0; segmentIdx < intersections.Num() - 1; segmentIdx += 2)
					{
						int32 idx1, idx2;
						if (intersections[segmentIdx].EdgeIdxA < intersections[segmentIdx + 1].EdgeIdxA)
						{
							idx1 = segmentIdx;
							idx2 = segmentIdx + 1;
						}
						else
						{
							idx1 = segmentIdx + 1;
							idx2 = segmentIdx;
						}

						FEdgeIntersection i1 = intersections[idx1];
						FEdgeIntersection i2 = intersections[idx2];

						auto intersection = TPair<FVector, FVector>(i1.Point, i2.Point);
						auto edgeIdx = TPair<int32, int32>(i1.EdgeIdxA, i2.EdgeIdxA);

						bool bFoundSplit = false;
						if (!GetDeltaForFaceSplit(Graph, OutDelta, NextID, ExistingID, currentFaceID, bFoundSplit, intersection, edgeIdx))
						{
							return false;
						}

						// TODO: currently restricted to one split per input face
						if (bFoundSplit && currentFaceID == FaceID)
						{
							if (bFaceHasSplit)
							{
								return false;
							}
							else
							{
								bFaceHasSplit = true;
							}
						}
					}
				}
			}
		}
		return true;
	}

	bool FGraph3D::GetDeltaForEdgeSplit(FGraph3D *Graph, FGraph3DDelta &OutDelta, int32 edgeID, int32 vertexID, int32 &NextID, int32 &ExistingID)
	{
		auto edge = Graph->FindEdge(edgeID);
		if (edge == nullptr)
		{
			return false;
		}

		auto vertex = Graph->FindVertex(vertexID);
		if (vertex == nullptr)
		{
			return false;
		}

		FVertexPair startPair = FVertexPair(edge->StartVertexID, vertexID);
		bool bForward;
		auto existingStartEdge = Graph->FindEdgeByVertices(edge->StartVertexID, vertexID, bForward);
		if (existingStartEdge == nullptr)
		{
			if (!GetDeltaForEdgeAddition(Graph, startPair, OutDelta, NextID, ExistingID, { edgeID }))
			{
				return false;
			}
		}

		FVertexPair endPair = FVertexPair(vertexID, edge->EndVertexID);
		auto existingEndEdge = Graph->FindEdgeByVertices(edge->EndVertexID, vertexID, bForward);
		if (existingEndEdge == nullptr)
		{
			if (!GetDeltaForEdgeAddition(Graph, endPair, OutDelta, NextID, ExistingID, { edgeID }))
			{
				return false;
			}
		}

		// no edges added
		if (existingStartEdge != nullptr && existingEndEdge != nullptr)
		{
			return false;
		}

		if (!GetDeltaForFaceVertexAddition(Graph, edge->ID, MOD_ID_NONE, vertexID, OutDelta))
		{
			return false;
		}

		if (!GetDeltaForDeletions(Graph, {}, { edge->ID }, {}, OutDelta))
		{
			return false;
		}

		return true;
	}

	bool FGraph3D::GetDeltaForVertexJoin(FGraph3D *Graph, FGraph3DDelta &OutDelta, int32 &NextID, int32 SavedVertexID, int32 RemovedVertexID)
	{
		auto joinVertex = Graph->FindVertex(SavedVertexID);
		auto oldVertex = Graph->FindVertex(RemovedVertexID);

		if (joinVertex == nullptr || oldVertex == nullptr)
		{
			return false;
		}

		TSet<int32> oldVertexConnectedFaces;
		oldVertex->GetConnectedFaces(oldVertexConnectedFaces);

		for (int32 faceID : oldVertexConnectedFaces)
		{
			auto face = Graph->FindFace(faceID);
			if (!ensureAlways(face != nullptr))
			{
				return false;
			}

			int32 oldVertexIdx = face->VertexIDs.Find(RemovedVertexID);
			if (oldVertexIdx == INDEX_NONE)
			{
				return false;
			}

			if (!OutDelta.FaceVertexAdditions.Contains(faceID))
			{
				OutDelta.FaceVertexAdditions.Add(faceID, TMap<int32, int32>());
			}
			if (!OutDelta.FaceVertexRemovals.Contains(faceID))
			{
				OutDelta.FaceVertexRemovals.Add(faceID, TMap<int32, int32>());
			}

			OutDelta.FaceVertexAdditions[faceID].Add(oldVertexIdx, SavedVertexID);
			OutDelta.FaceVertexRemovals[faceID].Add(oldVertexIdx, RemovedVertexID);
		}

		// TODO: check for coincident faces
		for (int32 edgeID : oldVertex->ConnectedEdgeIDs)
		{
			// if new edge is already in graph, delete edge, otherwise modify edge
			auto edge = Graph->FindEdge(edgeID);
			if (edge == nullptr)
			{
				return false;
			}

			int32 savedEdgeID = RemovedVertexID == edge->StartVertexID ? edge->EndVertexID : edge->StartVertexID;
			// TODO: catch savedEdgeID == SavedVertexID and then collapse edge and update the connected faces
			bool bOutForward;

			if (!GetDeltaForDeletions(Graph, {}, { edgeID }, {}, OutDelta))
			{
				return false;
			}

			if (Graph->FindEdgeByVertices(savedEdgeID, SavedVertexID, bOutForward) == nullptr)
			{
				// edge should not exist yet, because it was just checked
				int32 ExistingID;

				// TODO: handle edge deletes
				if (!GetDeltaForEdgeAddition(Graph, TPair<int32, int32>(savedEdgeID, SavedVertexID), OutDelta, NextID, ExistingID))
				{
					return false;
				}
			}
		}

		if (!GetDeltaForDeletions(Graph, { RemovedVertexID }, {}, {}, OutDelta))
		{
			return false;
		}

		return true;
	}

	bool FGraph3D::GetDeltaForEdgeJoin(FGraph3D *Graph, FGraph3DDelta &OutDelta, int32 &NextID, TPair<int32, int32> EdgeIDs)
	{
		auto edgeA = Graph->FindEdge(EdgeIDs.Key);
		auto edgeB = Graph->FindEdge(EdgeIDs.Value);

		if (!edgeA || !edgeB)
		{
			return false;
		}

		// for edges to be joined, one vertex must be shared, and the two vertices unique to one edge will create a new edge
		TPair<int32, int32> newEdgeVertexIDs;
		int32 sharedVertexID;

		// find which vertex is shared between the edges
		if (edgeA->StartVertexID == edgeB->StartVertexID)
		{
			sharedVertexID = edgeA->StartVertexID;
			newEdgeVertexIDs = TPair<int32, int32>(edgeA->EndVertexID, edgeB->EndVertexID);
		}
		else if (edgeA->EndVertexID == edgeB->EndVertexID)
		{
			sharedVertexID = edgeA->EndVertexID;
			newEdgeVertexIDs = TPair<int32, int32>(edgeA->StartVertexID, edgeB->StartVertexID);
		}
		else if (edgeA->StartVertexID == edgeB->EndVertexID)
		{
			sharedVertexID = edgeA->StartVertexID;
			newEdgeVertexIDs = TPair<int32, int32>(edgeA->EndVertexID, edgeB->StartVertexID);
		}
		else if (edgeA->EndVertexID == edgeB->StartVertexID)
		{
			sharedVertexID = edgeA->EndVertexID;
			newEdgeVertexIDs = TPair<int32, int32>(edgeA->StartVertexID, edgeB->EndVertexID);
		}
		else
		{
			return false;
		}

		auto sharedVertex = Graph->FindVertex(sharedVertexID);
		if (sharedVertex->ConnectedEdgeIDs.Num() != 2)
		{
			return false;
		}

		int32 ExistingID;
		TArray<int32> ParentEdgeIDs = { EdgeIDs.Key, EdgeIDs.Value };
		if (!GetDeltaForEdgeAddition(Graph, newEdgeVertexIDs, OutDelta, NextID, ExistingID, ParentEdgeIDs))
		{
			return false;
		}

		if (!GetDeltaForDeletions(Graph, { sharedVertexID }, ParentEdgeIDs, {}, OutDelta))
		{
			return false;
		}

		// update surrounding faces
		if (!GetDeltaForFaceVertexRemoval(Graph, sharedVertexID, OutDelta))
		{
			return false;
		}

		return true;
	}

	bool FGraph3D::GetSharedSeamForFaces(FGraph3D *Graph, const int32 FaceID, const int32 OtherFaceID, const int32 SharedIdx, int32 &SeamStartIdx, int32 &SeamEndIdx, int32 &SeamLength)
	{
		// attempt to find a shared seam, which is a connected set of edges that are on both faces.

		// because the edgeIDs and vertexIDs are a loop, the first shared edge detected
		// may not be the first shared edge in the seam.  For example, when sharedIdx is 0,
		// the seam can start at the end of the edgeIDs list and wrap back to 0.

		auto face = Graph->FindFace(FaceID);
		auto otherFace = Graph->FindFace(OtherFaceID);

		if (face == nullptr || otherFace == nullptr)
		{
			return false;
		}
		if (SharedIdx == INDEX_NONE)
		{
			return false;
		}

		bool bSameDirection;
		// seam length starts at 1 to include the given index, in the loops below
		// it is expanded from the start and end directions
		SeamLength = 1;

		// find start of seam by iterating backwards until the edge isn't shared
		int32 startIdx = SharedIdx;
		while (true)
		{
			int32 tempIdx = startIdx - 1;
			if (tempIdx < 0)
			{
				tempIdx = face->EdgeIDs.Num() - 1;
			}

			int32 otherFaceIdx = otherFace->FindEdgeIndex(face->EdgeIDs[tempIdx], bSameDirection);
			if (otherFaceIdx == INDEX_NONE)
			{
				break;
			}
			SeamLength++;

			startIdx = tempIdx;
		}

		// find end of seam by iterating forwards until the edge isn't shared
		int32 endIdx = SharedIdx;
		while (true)
		{
			int32 tempIdx = endIdx + 1;
			tempIdx %= face->EdgeIDs.Num();

			int32 otherFaceIdx = otherFace->FindEdgeIndex(face->EdgeIDs[tempIdx], bSameDirection);
			if (otherFaceIdx == INDEX_NONE)
			{
				break;
			}
			SeamLength++;

			endIdx = tempIdx;
		}

		SeamStartIdx = startIdx;
		SeamEndIdx = endIdx;

		return true;
	}

	bool FGraph3D::GetDeltaForFaceJoin(FGraph3D *Graph, FGraph3DDelta &OutDelta, const TArray<int32> &FaceIDs, int32 &NextID)
	{
		if (FaceIDs.Num() != 2)
		{
			return false;
		}

		auto face = Graph->FindFace(FaceIDs[0]);
		auto otherFace = Graph->FindFace(FaceIDs[1]);

		if (face == nullptr || otherFace == nullptr)
		{
			return false;
		}

		int32 faceIdx = INDEX_NONE;
		int32 otherSharedIdx = INDEX_NONE;

		// start by detecting the first shared edge
		bool bSameDirection;
		int32 idx = 0;
		int32 sharedIdx = INDEX_NONE;
		int32 sharedCount = 0;
		for (int32 edgeID : face->EdgeIDs)
		{
			int32 tempOtherFaceIdx = otherFace->FindEdgeIndex(edgeID, bSameDirection);
			if (tempOtherFaceIdx != INDEX_NONE)
			{
				if (sharedIdx == INDEX_NONE)
				{
					sharedIdx = idx;
					otherSharedIdx = tempOtherFaceIdx;
				}
				sharedCount++;
			}
			idx++;
		}

		if (sharedIdx == INDEX_NONE)
		{
			return false;
		}

		int32 startIdx, endIdx, seamLength;
		GetSharedSeamForFaces(Graph, face->ID, otherFace->ID, sharedIdx, startIdx, endIdx, seamLength);

		int32 otherStartIdx, otherEndIdx, otherSeamLength;
		GetSharedSeamForFaces(Graph, otherFace->ID, face->ID, otherSharedIdx, otherStartIdx, otherEndIdx, otherSeamLength);

		// it may not be necessary to reject these cases, 
		// however the resulting faces currently have potentially inconsistent, undefined, or undesired behavior

		// this case will trigger if the calculated shared seam does not contain all shared edges, for example
		// two faces that when combined would make a donut shape (hole in the middle)
		if (seamLength != sharedCount)
		{
			return false;
		}

		// this case will trigger when the faces have edges that wouldn't contribute to the outer perimeter of the face.  
		// These specific edges make the seam appear different based on what face's edge list is being used
		if (seamLength != otherSeamLength)
		{
			return false;
		}

		// iterate through the shared seam on the first face to mark edges and vertices for deletion	
		TArray<int32> sharedEdgeIDs;
		TArray<int32> sharedVertexIDs;
		int32 edgeIdx = startIdx;
		while (true)
		{
			int32 edgeID = face->EdgeIDs[edgeIdx];
			auto edge = Graph->FindEdge(edgeID);

			// all shared edges are removed, and the vertices that they share are removed
			if (sharedEdgeIDs.Num() > 0)
			{
				int32 sharedVertexID = edgeID > 0 ? edge->StartVertexID : edge->EndVertexID;
				auto sharedVertex = Graph->FindVertex(sharedVertexID);
				if (sharedVertex->ConnectedEdgeIDs.Num() == 2)
				{
					sharedVertexIDs.Add(sharedVertexID);
				}
			}
			sharedEdgeIDs.Add(edgeID);

			// object removal is inclusive
			if (edgeIdx == endIdx)
			{
				break;
			}
			edgeIdx++;
			edgeIdx %= face->EdgeIDs.Num();
		}

		// create a vertex ids loop for the new face resulting from the join by iterating through the 
		// parts of each face that are not on the seam

		// the directions of each edge on the seam are arbitrary, so the direction is always used to 
		// choose either the start or end vertex id of an edge
		auto startEdgeID = face->EdgeIDs[startIdx];
		auto startEdge = Graph->FindEdge(startEdgeID);
		int32 sharedStartVertexID = startEdgeID > 0 ? startEdge->StartVertexID : startEdge->EndVertexID;

		auto endEdgeID = face->EdgeIDs[endIdx];
		auto endEdge = Graph->FindEdge(endEdgeID);
		int32 sharedEndVertexID = endEdgeID > 0 ? endEdge->EndVertexID : endEdge->StartVertexID;

		TArray<int32> newVertexIDs;

		// loop through the vertices on the first face not contained in the seam
		int32 currentIdx = face->VertexIDs.Find(sharedEndVertexID);
		while (true)
		{
			int32 currentID = face->VertexIDs[currentIdx];
			if (currentID == sharedStartVertexID)
			{
				break;
			}

			newVertexIDs.Add(currentID);

			currentIdx++;
			currentIdx %= face->VertexIDs.Num();
		}

		// continue from where the previous loop ended, through the second face
		currentIdx = otherFace->VertexIDs.Find(sharedStartVertexID);

		// detect the direction of the iteration by attempting to iterate forward and determining whether
		// the next vertex ID would be on the shared seam, and iterating in the opposite direction if it is
		int32 nextIdx = (currentIdx + 1) % otherFace->VertexIDs.Num();
		int32 nextID = otherFace->VertexIDs[nextIdx];
		int32 secondFaceEndID = sharedEndVertexID;

		int32 secondFaceIncrement = 1;
		if (nextID == sharedEndVertexID || sharedVertexIDs.Find(nextID) != INDEX_NONE)
		{
			secondFaceIncrement = -1;
		}
			
		while (true)
		{
			int32 currentID = otherFace->VertexIDs[currentIdx];
			if (currentID == secondFaceEndID)
			{
				break;
			}

			newVertexIDs.Add(currentID);

			currentIdx += secondFaceIncrement;
			if (currentIdx < 0)
			{
				currentIdx = otherFace->VertexIDs.Num() - 1;
			}
			else
			{
				currentIdx %= otherFace->VertexIDs.Num();
			}
		}

		// create deltas from adding the new face, deleting the old faces, 
		// and deleting the vertices and edges contained in the shared seam
		int32 ExistingID, addedID;
		TArray<int32> parentFaceIDs = { face->ID, otherFace->ID };
		TMap<int32, int32> edgeMap;
		GetDeltaForFaceAddition(Graph, newVertexIDs, OutDelta, NextID, ExistingID, parentFaceIDs, edgeMap, addedID);
		GetDeltaForDeletions(Graph, sharedVertexIDs, sharedEdgeIDs, parentFaceIDs, OutDelta);
		for (auto faceID : parentFaceIDs)
		{
			OutDelta.FaceDeletions[faceID].ParentFaceIDs = { addedID };
		}

		return true;
	}

	bool FGraph3D::GetDeltasForObjectJoin(FGraph3D *Graph, TArray<FGraph3DDelta> &OutDeltas, const TArray<int32> &ObjectIDs, int32 &NextID, EGraph3DObjectType ObjectType)
	{
		if (ObjectType == EGraph3DObjectType::Face)
		{
			FGraph3DDelta OutDelta;
			if (!GetDeltaForFaceJoin(Graph, OutDelta, ObjectIDs, NextID))
			{
				return false;
			}

			Graph->ApplyDelta(OutDelta);
			OutDeltas.Add(OutDelta);

			TArray<int32> addedFaces;
			OutDelta.FaceAdditions.GenerateKeyArray(addedFaces);

			if (addedFaces.Num() != 1)
			{
				return false;
			}

			if (!GetDeltasForReduceEdges(Graph, OutDeltas, addedFaces[0], NextID))
			{
				return false;
			}

			return true;
		}
		else if (ObjectType == EGraph3DObjectType::Edge)
		{
			FGraph3DDelta OutDelta;
			if (!GetDeltaForEdgeJoin(Graph, OutDelta, NextID, TPair<int32, int32>(ObjectIDs[0], ObjectIDs[1])))
			{
				return false;
			}
			OutDeltas.Add(OutDelta);

			return true;
		}

		return false;
	}

	bool FGraph3D::GetDeltasForReduceEdges(FGraph3D *Graph, TArray<FGraph3DDelta> &OutDeltas, int32 FaceID, int32 &NextID)
	{
		bool bAttemptEdgeJoin = true;
		int32 edgeIdx = 0;
		while (bAttemptEdgeJoin)
		{
			bAttemptEdgeJoin = false;
			auto face = Graph->FindFace(FaceID);
			if (face == nullptr)
			{
				return false;
			}

			int32 numEdges = face->EdgeIDs.Num();

			FGraph3DDelta joinEdgeDelta;
			for (edgeIdx; edgeIdx < numEdges; edgeIdx++)
			{
				int32 currentEdgeID = face->EdgeIDs[edgeIdx];
				int32 nextEdgeID = face->EdgeIDs[(edgeIdx + 1) % numEdges];

				auto currentEdge = Graph->FindEdge(currentEdgeID);
				auto nextEdge = Graph->FindEdge(nextEdgeID);
				if (!currentEdge || !nextEdge)
				{
					continue;
				}

				if (!FVector::Parallel(currentEdge->CachedDir, nextEdge->CachedDir))
				{
					continue;
				}

				TPair<int32, int32> currentEdgePair = TPair<int32, int32>(currentEdgeID, nextEdgeID);
				if (GetDeltaForEdgeJoin(Graph, joinEdgeDelta, NextID, currentEdgePair))
				{
					bAttemptEdgeJoin = true;

					Graph->ApplyDelta(joinEdgeDelta);
					OutDeltas.Add(joinEdgeDelta);
					
					// loop becomes invalidated once an edge is joined, so break out and start again until an edge isn't joined
					break;
				}
			}
		}

		return true;
	}

	bool FGraph3D::GetDeltaForDeleteObjects(FGraph3D *Graph, const TArray<int32> &VertexIDs, const TArray<int32> &EdgeIDs, const TArray<int32> &FaceIDs, FGraph3DDelta &OutDelta, bool bGatherEdgesFromFaces)
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

		TSet<int32> faceIDsToDelete = TSet<int32>(FaceIDs);
		for (int32 faceID : FaceIDs)
		{
			faceIDsToDelete.Add(FMath::Abs(faceID));
		}

		for (int32 vertexID : vertexIDsToDelete)
		{
			auto vertex = Graph->FindVertex(vertexID);
			if (vertex != nullptr)
			{
				// consider all edges that are connected to the vertex for deletion
				for (int32 connectedEdgeID : vertex->ConnectedEdgeIDs)
				{
					edgeIDsToDelete.Add(FMath::Abs(connectedEdgeID));
				}
			}
		}

		for (int32 edgeID : edgeIDsToDelete)
		{
			auto edge = Graph->FindEdge(edgeID);
			if (edge != nullptr)
			{
				// consider all faces that are connected to the edge for deletion
				for (auto faceConnection : edge->ConnectedFaces)
				{
					faceIDsToDelete.Add(FMath::Abs(faceConnection.FaceID));
				}

				// delete connected vertices that are not connected to any edge that is not being deleted
				for (auto vertexID : { edge->StartVertexID, edge->EndVertexID })
				{
					auto vertex = Graph->FindVertex(vertexID);
					if (vertex != nullptr)
					{
						bool bDeleteVertex = true;
						for (int32 connectedEdgeID : vertex->ConnectedEdgeIDs)
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

		for (int32 faceID : faceIDsToDelete)
		{
			auto face = Graph->FindFace(faceID);
			if ((face != nullptr) && bGatherEdgesFromFaces)
			{
				// traverse back to the vertices, and make sure vertices and edges are connected to at least one face
				// after a face is deleted
				for (int32 edgeID : face->EdgeIDs)
				{
					// already deleted
					if (edgeIDsToDelete.Contains(FMath::Abs(edgeID)))
					{
						continue;
					}

					auto edge = Graph->FindEdge(edgeID);
					if (edge != nullptr)
					{
						bool bDeleteEdge = true;
						for (auto connection : edge->ConnectedFaces)
						{
							if (!faceIDsToDelete.Contains(FMath::Abs(connection.FaceID)))
							{
								bDeleteEdge = false;
								break;
							}
						}
						if (bDeleteEdge)
						{
							edgeIDsToDelete.Add(FMath::Abs(edgeID));
						}
					}
				}

				for (int32 vertexID : face->VertexIDs)
				{
					// already deleted
					if (vertexIDsToDelete.Contains(FMath::Abs(vertexID)))
					{
						continue;
					}

					auto vertex = Graph->FindVertex(vertexID);
					if (vertex != nullptr)
					{
						bool bDeleteVertex = true;
						TSet<int32> connectedFaces, connectedEdges;
						vertex->GetConnectedFacesAndEdges(connectedFaces, connectedEdges);

						// Don't delete the target face's vertex if it's connected to another face that isn't being deleted
						for (int32 connectedFaceID : connectedFaces)
						{
							if (!faceIDsToDelete.Contains(FMath::Abs(connectedFaceID)))
							{
								bDeleteVertex = false;
								break;
							}
						}

						// Also, don't delete the target face's edge if it's connected to another face that isn't being deleted
						for (int32 connectedEdgeID : connectedEdges)
						{
							if (!edgeIDsToDelete.Contains(FMath::Abs(connectedEdgeID)))
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
			}
		}

		// collect objects to be deleted
		TSet<const FGraph3DVertex *> allVerticesToDelete;
		for (int32 vertexID : vertexIDsToDelete)
		{
			if (auto *vertex = Graph->FindVertex(vertexID))
			{
				allVerticesToDelete.Add(vertex);
			}
		}

		TSet<const FGraph3DEdge *> allEdgesToDelete;
		for (int32 edgeID : edgeIDsToDelete)
		{
			if (auto *edge = Graph->FindEdge(edgeID))
			{
				allEdgesToDelete.Add(edge);
			}
		}

		TSet<const FGraph3DFace *> allFacesToDelete;
		for (int32 faceID : faceIDsToDelete)
		{
			if (auto *face = Graph->FindFace(faceID))
			{
				allFacesToDelete.Add(face);
			}
		}

		return GetDeltaForDeleteObjects(allVerticesToDelete, allEdgesToDelete, allFacesToDelete, OutDelta);
	}

	bool FGraph3D::GetDeltaForDeleteObjects(const TSet<const FGraph3DVertex *> VerticesToDelete, const TSet<const FGraph3DEdge *> EdgesToDelete, const TSet<const FGraph3DFace *> FacesToDelete, FGraph3DDelta &OutDelta)
	{
		for (const FGraph3DVertex *vertex : VerticesToDelete)
		{
			if (OutDelta.VertexAdditions.Remove(vertex->ID) == 0)
			{
				OutDelta.VertexDeletions.Add(vertex->ID, vertex->Position);
			}
		}

		for (const FGraph3DEdge *edge : EdgesToDelete)
		{
			if (OutDelta.EdgeAdditions.Remove(edge->ID) == 0)
			{
				OutDelta.EdgeDeletions.Add(edge->ID, FVertexPair(edge->StartVertexID, edge->EndVertexID));
			}
		}

		for (const FGraph3DFace *face : FacesToDelete)
		{
			if (OutDelta.FaceAdditions.Remove(face->ID) == 0)
			{
				OutDelta.FaceDeletions.Add(face->ID, FGraph3DObjDelta(face->VertexIDs));
			}
		}
		return true;
	}

	bool FGraph3D::GetDeltaForDeletions(FGraph3D *Graph, const TArray<int32> &VertexIDs, const TArray<int32> &EdgeIDs,
		const TArray<int32> &FaceIDs, FGraph3DDelta &OutDelta)
	{
		TSet<const FGraph3DVertex *> allVerticesToDelete;
		TSet<const FGraph3DEdge *> allEdgesToDelete;
		TSet<const FGraph3DFace *> allFacesToDelete;

		for (int32 vertexID : VertexIDs)
		{
			if (const FGraph3DVertex *vertex = Graph->FindVertex(vertexID))
			{
				allVerticesToDelete.Add(vertex);
			}
		}

		for (int32 edgeID : EdgeIDs)
		{
			if (const FGraph3DEdge *edge = Graph->FindEdge(edgeID))
			{
				allEdgesToDelete.Add(edge);
			}
		}

		for (int32 faceID : FaceIDs)
		{
			if (const FGraph3DFace *face = Graph->FindFace(faceID))
			{
				allFacesToDelete.Add(face);
			}
		}

		return GetDeltaForDeleteObjects(allVerticesToDelete, allEdgesToDelete, allFacesToDelete, OutDelta);
	}
}
