// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "Graph/Graph3D.h"

#include "Graph/Graph3DDelta.h"
#include "Graph/Graph2D.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "ModumateCore/ModumateGeometryStatics.h"

bool FGraph3D::GetDeltaForVertexAddition(const FVector &VertexPos, FGraph3DDelta &OutDelta, int32 &NextID, int32 &OutVertexID)
{
	const FGraph3DVertex *existingVertex = FindVertex(VertexPos);
	int32 existingDeltaVertexID = OutDelta.FindAddedVertex(VertexPos);

	if (existingVertex)
	{
		OutVertexID = existingVertex->ID;
		return false;
	}
	else if (existingDeltaVertexID != MOD_ID_NONE)
	{
		OutVertexID = existingDeltaVertexID;
		return false;
	}
	else
	{
		OutVertexID = NextID++;
		OutDelta.VertexAdditions.Add(OutVertexID, VertexPos);
		return true;
	}
}

bool FGraph3D::MoveVerticesDirect(const TArray<int32>& VertexIDs, const TArray<FVector>& NewVertexPositions, TArray<FGraph3DDelta>& OutDeltas, int32 &NextID)
{
	// First, move all of the vertices
	FGraph3DDelta moveVertexDelta(GraphID);

	int32 numVertices = VertexIDs.Num();
	if (numVertices != NewVertexPositions.Num())
	{
		return false;
	}

	for (int32 i = 0; i < numVertices; ++i)
	{
		int32 vertexID = VertexIDs[i];
		const FGraph3DVertex *vertex = FindVertex(vertexID);

		FVector oldPos = vertex->Position;
		FVector newPos = NewVertexPositions[i];

		// ensure that the delta is meaningful
		if (!newPos.Equals(oldPos))
		{
			moveVertexDelta.VertexMovements.Add(vertexID, { oldPos, newPos });
		}

	}

	// the main reason for checking the output of applying the delta is to 
	// check the planarity of the faces
	bool bValidDelta = ApplyDelta(moveVertexDelta);
	if (!bValidDelta)
	{
		return false;
	}
	OutDeltas.Add(moveVertexDelta);

	return true;
}

bool FGraph3D::GetDeltaForVertexMovements(const TArray<int32> &VertexIDs, const TArray<FVector> &NewVertexPositions, TArray<FGraph3DDelta> &OutDeltas, int32 &NextID)
{
	int32 numVertices = VertexIDs.Num();
	TSet<int32> oldConnectedFaceIDs;
	TMap<int32, int32> joinableVertexIDs;
	for (int32 i = 0; i < numVertices; ++i)
	{
		int32 vertexID = VertexIDs[i];
		const FGraph3DVertex *vertex = FindVertex(vertexID);

		vertex->GetConnectedFaces(oldConnectedFaceIDs);

		FVector newPos = NewVertexPositions[i];

		// save vertices that are at the same position - after the moveVertexDelta
		// has been applied, FindVertex will not reliably find the existingVertex
		const FGraph3DVertex *existingVertex = FindVertex(newPos);
		if (existingVertex != nullptr && existingVertex->ID != vertexID && !VertexIDs.Contains(existingVertex->ID))
		{
			joinableVertexIDs.Add(vertexID, existingVertex->ID);
		}
	}

	if (!MoveVerticesDirect(VertexIDs, NewVertexPositions, OutDeltas, NextID))
	{
		return false;
	}

	// join vertices
	// create one delta per vertex join to safely update the vertex lists of the faces,
	// and remove objects that have become redundant	
	FGraph3DDelta joinDelta(GraphID);
	for (auto& kvp : joinableVertexIDs)
	{
		if (!GetDeltaForVertexJoin(joinDelta, NextID, kvp.Value, kvp.Key))
		{
			return false;
		}
		ApplyDelta(joinDelta);
		OutDeltas.Add(joinDelta);
		joinDelta.Reset();
	}

	// after the vertices have been moved, check if any of the old faces that have been modified
	// have been invalidated
	FGraph3DDelta removeFaceDelta(GraphID);
	for (int32 faceID : oldConnectedFaceIDs)
	{
		auto face = FindFace(faceID);
		if (face == nullptr)
		{
			continue;
		}

		if (FindOverlappingFace(faceID) != MOD_ID_NONE)
		{
			removeFaceDelta.FaceDeletions.Add(faceID, FGraph3DObjDelta(face->VertexIDs));
		}
	}

	if (!removeFaceDelta.IsEmpty())
	{
		OutDeltas.Add(removeFaceDelta);
	}

	TSet<int32> connectedFaceIDs;
	TSet<int32> connectedEdgeIDs;
	for(FVector pos : NewVertexPositions)
	{
		auto vertex = FindVertex(pos);
		if (vertex == nullptr)
		{
			return false;
		}

		vertex->GetConnectedFacesAndEdges(connectedFaceIDs, connectedEdgeIDs);
	}

	TArray<int32> connectedEdgeIDsArray = connectedEdgeIDs.Array();
	if (!GetDeltasForEdgeSplits(OutDeltas, connectedEdgeIDsArray, NextID))
	{
		return false;
	}


	TSet<int32> newEdges(connectedEdgeIDsArray);
	for (int32 faceID : connectedFaceIDs)
	{
		// Intersection checks
		int32 ExistingID = MOD_ID_NONE;
		if (!GetDeltasForEdgeAtSplit(OutDeltas, NextID, faceID, newEdges))
		{
			return false;
		}
	}

	TArray<FGraph3DDelta> updateFaceDeltas;
	TArray<int32> addedFaceIDs;
	if (GetDeltasForUpdateFaces(updateFaceDeltas, addedFaceIDs, NextID, newEdges.Array(), connectedFaceIDs.Array(), {}, false))
	{
		OutDeltas.Append(updateFaceDeltas);
	}

	return true;
}

bool FGraph3D::GetDeltaForEdgeAddition(const FGraphVertexPair &VertexPair, FGraph3DDelta &OutDelta, int32 &NextID, int32 &ExistingID, const TArray<int32> &ParentIDs)
{
	bool bExistingEdgeForward = true;
	const FGraph3DEdge *existingEdge = FindEdgeByVertices(VertexPair.Key, VertexPair.Value, bExistingEdgeForward);
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
bool FGraph3D::GetDeltaForMultipleEdgeAdditions(const FGraphVertexPair &VertexPair, FGraph3DDelta &OutDelta, int32 &NextID, int32 &ExistingID, TArray<int32> &OutVertexIDs, const TArray<int32> &ParentIDs)
{
	bool bAddedEdge = false;

	bool bExistingEdgeForward = true;
	const FGraph3DEdge *existingEdge = FindEdgeByVertices(VertexPair.Key, VertexPair.Value, bExistingEdgeForward);

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
	else if (const FGraph3DVertex *vertex = FindVertex(VertexPair.Key))
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
	else if (const FGraph3DVertex *vertex = FindVertex(VertexPair.Value))
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
	CalculateVerticesOnLine(VertexPair, startPos, endPos, OutVertexIDs, splitEdgeIDs);

	// TODO: given edges that are split, delete the split edge and update connected faces
	if (splitEdgeIDs.Key == MOD_ID_NONE && splitEdgeIDs.Value == MOD_ID_NONE)
	{
		for (int32 vertexIdx = 0; vertexIdx < OutVertexIDs.Num() - 1; vertexIdx++)
		{
			FGraphVertexPair vertexPair(OutVertexIDs[vertexIdx], OutVertexIDs[vertexIdx + 1]);
			// ExistingID should not be used if there are several edges
			int32 existingID;
			bAddedEdge = GetDeltaForEdgeAddition(vertexPair, OutDelta, NextID, existingID, ParentIDs) || bAddedEdge;
		}
	}
	else
	{
		OutVertexIDs = { VertexPair.Key, VertexPair.Value };

		bAddedEdge = GetDeltaForEdgeAddition(VertexPair, OutDelta, NextID, ExistingID, ParentIDs) || bAddedEdge;
	}

	return bAddedEdge;
}

bool FGraph3D::GetDeltaForEdgeAdditionWithSplit(const FGraphVertexPair &VertexPair, TArray<FGraph3DDelta> &OutDeltas, int32 &NextID, TArray<int32> &OutEdgeIDs)
{
	int32 ExistingID;

	// all deltas created by the graph operations within this function will be added to 
	// splitEdgeDeltas, and if this function failed these deltas can be safely reverted.
	// if the function will succeed, splitEdgeDeltas is appended to the output deltas
	TArray<FGraph3DDelta> splitEdgeDeltas;

	// add edge
	FGraph3DDelta addEdgeDelta(GraphID);
	if (!GetDeltaForEdgeAddition(VertexPair, addEdgeDelta, NextID, ExistingID))
	{
		OutEdgeIDs = { ExistingID };
		return false;
	}


	int32 edgeID = MOD_ID_NONE;
	if (addEdgeDelta.EdgeAdditions.Num() != 1)
	{
		return false;
	}
	else
	{
		for (auto& kvp : addEdgeDelta.EdgeAdditions)
		{
			edgeID = kvp.Key;
			break;
		}
	}

	ApplyDelta(addEdgeDelta);
	splitEdgeDeltas.Add(addEdgeDelta);

	// check for splits with added edge
	OutEdgeIDs = { edgeID };
	if (!GetDeltasForEdgeSplits(splitEdgeDeltas, OutEdgeIDs, NextID))
	{
		ApplyInverseDeltas(splitEdgeDeltas);
		return false;
	}
	OutDeltas.Append(splitEdgeDeltas);

	return true;
}

bool FGraph3D::GetDeltasForUpdateFaces(TArray<FGraph3DDelta> &OutDeltas, TArray<int32> &OutAddedFaceIDs, int32 &NextID, const TArray<int32>& EdgeIDs, const TArray<int32>& FaceIDs, const TArray<FPlane>& InPlanes, bool bAddNewFaces)
{
	int32 existingID;

	// this function is intended to be called once per graph operation, so it makes containers instead of having mutable members
	TSet<int32> oldFaces;
	TSet<int32> newFaces;

	TSet<int32> currentFaces;
	for (auto& kvp : GetFaces())
	{
		currentFaces.Add(kvp.Key);
	}

	TMap<int32, TArray<int32>> oldToNewFaceIDs;
	TArray <TArray<int32>> outFaceVertices;

	// variables for face addition
	FGraph3DDelta faceDelta(GraphID);
	TArray<int32> parentIds = { MOD_ID_NONE };
	TMap<int32, int32> edgeMap;

	for (int32 edgeIdx = 0; edgeIdx < EdgeIDs.Num(); edgeIdx++)
	{
		int32 edgeID = EdgeIDs[edgeIdx];
		FPlane currentPlane = InPlanes.Num() == 0 ? FPlane(EForceInit::ForceInitToZero) : InPlanes[edgeIdx];
		bool bHasPlaneConstraint = !currentPlane.Equals(FPlane(EForceInit::ForceInitToZero));

		// subdivide faces
		outFaceVertices.Reset();
		TraverseFacesFromEdge(edgeID, outFaceVertices);

		for (auto& faceVertices : outFaceVertices)
		{
			faceDelta.Reset();
			parentIds = { MOD_ID_NONE };
			int32 addedFaceID;
			edgeMap.Reset();
			int32 faceNextID = NextID;
			if (!GetDeltaForFaceAddition(faceVertices, faceDelta, faceNextID, existingID, parentIds, edgeMap, addedFaceID))
			{
				continue;
			}

			ApplyDelta(faceDelta);
			bool bAddedToDeltas = false;
			auto newFace = FindFace(addedFaceID);
			if (!ensure(newFace))
			{
				continue;
			}

			// Find out whether the potential added face overlaps with any existing faces
			TSet<int32> coincidentFaceIDs;
			FindOverlappingFaces(addedFaceID, currentFaces, coincidentFaceIDs);

			// There are two overlapping faces when the potential face is between a 
			// face from before the graph operation and a face added during the graph operation.
			// Currently, user-facing graph operations only support adding one face at a time,
			// so there shouldn't be more than two overlapping faces.
			if (!ensureAlways(coincidentFaceIDs.Num() <= 2))
			{
				continue;
			}

			// If there are two faces, choose one arbitrarily.  Potentially, there could be some
			// logic here to attempt to prefer the pre-operation face or the during-operation face
			// to transfer that face's assembly to the new face.
			int32 coincidentFaceID = MOD_ID_NONE;
			if (coincidentFaceIDs.Num() > 0)
			{
				coincidentFaceID = *coincidentFaceIDs.CreateIterator();
			}

			// If the plane is constrained (used when a face is added) only add the new faces that are found if they are subdividing another face
			bool bParallel = FVector::Parallel(FVector(newFace->CachedPlane), FVector(currentPlane));
			// bAddNewFaces exists because because certain graph operations only want to make faces that are created from overlapping faces
			if (bAddNewFaces && coincidentFaceID == MOD_ID_NONE && (!bHasPlaneConstraint || bParallel))
			{
				OutDeltas.Add(faceDelta);
				bAddedToDeltas = true;
			}
			else
			{
				addedFaceID = FMath::Abs(addedFaceID);
				coincidentFaceID = FMath::Abs(coincidentFaceID);

				auto oldFace = FindFace(coincidentFaceID);

				if (newFace && oldFace)
				{
					bool bMatchingNormalsParallel = FVector::Parallel(
						FVector(oldFace->CachedPlane), FVector(newFace->CachedPlane));
					bool bMatchingNormalsCoincident = FVector::Coincident(
						FVector(oldFace->CachedPlane), FVector(newFace->CachedPlane));

					// Manufacture the new face normals matching the old face
					if (bMatchingNormalsParallel && !bMatchingNormalsCoincident)
					{
						Algo::Reverse(faceDelta.FaceAdditions[addedFaceID].Vertices);
					}

					bool bFullyContained, bPartiallyContained;
					if (bMatchingNormalsParallel && 
						GetFaceContainment(oldFace->ID, newFace->ID, bFullyContained, bPartiallyContained) &&
						!bFullyContained && bPartiallyContained)
					{
						faceDelta.FaceAdditions[addedFaceID].ParentObjIDs = { coincidentFaceID };
						OutDeltas.Add(faceDelta);
						oldFaces.Add(coincidentFaceID);
						TArray<int32> &faceIDs = oldToNewFaceIDs.FindOrAdd(coincidentFaceID);
						faceIDs.Add(addedFaceID);
						bAddedToDeltas = true;
					}
				}
			}
			if (!bAddedToDeltas)
			{
				ApplyDelta(*faceDelta.MakeGraphInverse());
			}
			else
			{
				NextID = faceNextID;
				newFaces.Add(addedFaceID);
			}
		}
	}

	FGraph3DDelta deleteDelta(GraphID);
	GetDeltaForDeletions({}, {}, oldFaces.Array(), deleteDelta);
	for (int32 oldFaceID : oldFaces)
	{
		deleteDelta.FaceDeletions[oldFaceID].ParentObjIDs = oldToNewFaceIDs[oldFaceID];
	}

	if (!deleteDelta.IsEmpty())
	{
		OutDeltas.Add(deleteDelta);
		ApplyDelta(deleteDelta);
	}

	// The initial group of faces that need to be updated contains
	// the faces provided as an argument (FaceIDs) by the graph operation
	// that were not deleted through the update (oldFaces), and faces created through the update (newFaces)
	TSet<int32> updateContainmentFaceIDs;
	updateContainmentFaceIDs.Append(newFaces);
	for (int32 faceID : FaceIDs)
	{
		if (!oldFaces.Contains(faceID))
		{
			updateContainmentFaceIDs.Add(faceID);
		}
		else
		{
			OutAddedFaceIDs.Remove(faceID);
			OutAddedFaceIDs.Append(oldToNewFaceIDs[faceID]);
		}
	}

	FGraph3DDelta containmentDelta(GraphID);

	// the side effect faces are the faces that either contain or are contained by the initial faces
	TSet<int32> sideEffectContainedFaceIDs;
	TSet<int32> sideEffectContainingFaceIDs;

	// faces are contained by at most one face.  If several faces mathematically contain a face,
	// the containing face with the minimum area is used.  The relationships in this map are 
	// sufficient for populating the delta.
	TMap<int32, int32> facesContainedByFace;

	for (int32 updateFaceID : updateContainmentFaceIDs)
	{
		auto face = FindFace(updateFaceID);
		if (!ensure(face != nullptr))
		{
			continue;
		}

		int32 containingFaceID = FindMinFaceContainingFace(face->ID, true);
		FindFacesContainedByFace(face->ID, sideEffectContainedFaceIDs, true);

		sideEffectContainedFaceIDs.Append(face->ContainedFaceIDs);

		if (auto oldContainingFace = FindFace(face->ContainingFaceID))
		{
			sideEffectContainingFaceIDs.Add(face->ContainingFaceID);
		}
		if (auto containingFace = FindFace(containingFaceID))
		{
			sideEffectContainingFaceIDs.Add(containingFaceID);
		}

		facesContainedByFace.Add(updateFaceID, containingFaceID);
	}
	sideEffectContainedFaceIDs = sideEffectContainedFaceIDs.Difference(updateContainmentFaceIDs);

	// add default deltas for all involved faces
	for (auto& setInDelta : { updateContainmentFaceIDs, sideEffectContainedFaceIDs, sideEffectContainingFaceIDs })
	{
		for (int32 id : setInDelta)
		{
			if (auto face = FindFace(id))
			{
				containmentDelta.FaceContainmentUpdates.Add(id, FGraph3DFaceContainmentDelta(
					face->ContainingFaceID, face->ContainingFaceID
				));
			}
		}
	}

	// update new containment relationship for side effect contained faces
	for (int32 id : sideEffectContainedFaceIDs)
	{
		if (auto face = FindFace(id))
		{
			int32 containingFaceID = FindMinFaceContainingFace(face->ID, true);
			facesContainedByFace.Add(id, containingFaceID);
		}
	}

	for (auto& kvp : facesContainedByFace)
	{
		int32 faceID = kvp.Key;
		int32 containingFaceID = kvp.Value;
		auto face = FindFace(faceID);

		auto& faceContainmentDelta  = containmentDelta.FaceContainmentUpdates[faceID];
		faceContainmentDelta.PrevContainingFaceID = face->ContainingFaceID;
		faceContainmentDelta.NextContainingFaceID = containingFaceID;
		if (face->ContainingFaceID != containingFaceID)
		{
			auto oldContainingFace = FindFace(face->ContainingFaceID);
			if (oldContainingFace != nullptr)
			{
				auto& oldContainingFaceDelta = containmentDelta.FaceContainmentUpdates[face->ContainingFaceID];
				oldContainingFaceDelta.ContainedFaceIDsToRemove.Add(faceID);
			}
		}

		auto containingFace = FindFace(containingFaceID);
		if (containingFace && !containingFace->ContainedFaceIDs.Contains(faceID) &&
			ensure(containmentDelta.FaceContainmentUpdates.Contains(containingFaceID)))
		{
			auto& containDelta = containmentDelta.FaceContainmentUpdates[containingFaceID];
			containDelta.ContainedFaceIDsToAdd.Add(faceID);
		}
	}

	if (!containmentDelta.IsEmpty())
	{
		OutDeltas.Add(containmentDelta);
		ApplyDelta(containmentDelta);
	}

	return true;
}

bool FGraph3D::GetDeltaForEdgeAdditionWithSplit(const FVector& EdgeStartPos, const FVector& EdgeEndPos, TArray<FGraph3DDelta>& OutDeltas, int32& NextID, TArray<int32>& OutEdgeIDs, bool bCheckFaces, bool bSplitAndUpdateEdges)
{
	OutEdgeIDs.Reset();

	if (EdgeStartPos.Equals(EdgeEndPos, Epsilon))
	{
		return false;
	}

	FVector vertexPositions[2] = { EdgeStartPos, EdgeEndPos };
	int32 vertexIDs[2] = { MOD_ID_NONE, MOD_ID_NONE };

	for (int32 i = 0; i < 2; ++i)
	{
		const FVector& vertexPosition = vertexPositions[i];

		FGraph3DDelta addVertexDelta(GraphID);
		int32 vertexID = MOD_ID_NONE;
		if (GetDeltaForVertexAddition(vertexPosition, addVertexDelta, NextID, vertexID))
		{
			if (!ensureAlways(addVertexDelta.VertexAdditions.Num() == 1))
			{
				return false;
			}

			for (auto& kvp : addVertexDelta.VertexAdditions)
			{
				vertexID = kvp.Key;
				break;
			}

			OutDeltas.Add(addVertexDelta);
			ApplyDelta(addVertexDelta);
		}

		if (!ensureAlways(vertexID != MOD_ID_NONE))
		{
			return false;
		}

		vertexIDs[i] = vertexID;
	}

	FGraphVertexPair vertexPair(vertexIDs[0], vertexIDs[1]);
	if (!bSplitAndUpdateEdges)
	{
		FGraph3DDelta edgeDelta(GraphID);
		int32 existingID;
		TArray<int32> parentIDs;
		if (!GetDeltaForEdgeAddition(vertexPair, edgeDelta, NextID, existingID, parentIDs))
		{
			if (existingID != MOD_ID_NONE)
			{
				OutDeltas.Reset();
				return true;
			}
			return false;
		}
		else if (existingID != MOD_ID_NONE)
		{
			OutEdgeIDs = { existingID };
		}
		OutDeltas.Add(edgeDelta);
	}
	else
	{
		TArray<FGraph3DDelta> splitEdgeDeltas;
		if (GetDeltaForEdgeAdditionWithSplit(vertexPair, splitEdgeDeltas, NextID, OutEdgeIDs))
		{
			OutDeltas.Append(splitEdgeDeltas);
		}
		else
		{
			OutDeltas.Reset();
		}

		if (bCheckFaces)
		{
			TArray<FGraph3DDelta> updateFaceDeltas;
			TArray<int32> addedFaceIDs;
			if (GetDeltasForUpdateFaces(updateFaceDeltas, addedFaceIDs, NextID, OutEdgeIDs, {}))
			{
				OutDeltas.Append(updateFaceDeltas);
			}
			else
			{
				return false;
			}
		}
	}

	return true;
}

bool FGraph3D::GetDeltaForFaceAddition(const TArray<int32> &VertexIDs, FGraph3DDelta &OutDelta, int32 &NextID, int32 &ExistingID,
	TArray<int32> &ParentFaceIDs, TMap<int32, int32> &ParentEdgeIdxToID, int32& AddedFaceID, const TSet<int32> &InGroupIDs)
{
	TArray<int32> faceVertices;

	int32 numVertices = VertexIDs.Num();
	TArray<FGraphSignedID> newFaceEdgeIDs;
	for (int32 idxA = 0; idxA < numVertices; ++idxA)
	{
		int32 idxB = (idxA + 1) % numVertices;
		int32 vertexIDA = VertexIDs[idxA];
		int32 vertexIDB = VertexIDs[idxB];
		bool bEdgeForward = true;

		FGraphVertexPair currentPair(vertexIDA, vertexIDB);
		TArray<int32> vertices;
		int32 existingID;

		if (ParentEdgeIdxToID.Contains(idxA))
		{
			GetDeltaForMultipleEdgeAdditions(currentPair, OutDelta, NextID, existingID, vertices, { ParentEdgeIdxToID[idxA] });
		}
		else
		{
			GetDeltaForMultipleEdgeAdditions(currentPair, OutDelta, NextID, existingID, vertices);
		}

		// the last value in the vertex list is not needed because it will also be the first value for the next edge
		for (int32 idx = 0; idx < vertices.Num() - 1; idx++)
		{
			faceVertices.Add(vertices[idx]);
		}
	}

	if (!ensureAlways(UModumateFunctionLibrary::NormalizeIDs(faceVertices)))
	{
		return false;
	}

	TArray<int32> otherFaceVertexIDs;
	for (auto& kvp : Faces)
	{
		otherFaceVertexIDs = kvp.Value.VertexIDs;
		if (UModumateFunctionLibrary::AreNormalizedIDListsEqual(faceVertices, otherFaceVertexIDs))
		{
			return false;
		}
	}

	AddedFaceID = NextID++;
	FGraph3DObjDelta &faceAdditionDelta = OutDelta.FaceAdditions.Add(AddedFaceID, FGraph3DObjDelta(faceVertices, ParentFaceIDs, InGroupIDs));

	return true;
}

bool FGraph3D::GetDeltaForFaceAddition(const TArray<FVector> &VertexPositions, TArray<FGraph3DDelta> &OutDeltas, int32 &NextID, TArray<int32> &OutFaceIDs, const TSet<int32> &InGroupIDs, bool bSplitAndUpdateFaces)
{
	TArray<int32> newVertices;
	FGraph3DDelta OutDelta(GraphID);
	GetDeltaForVertexList(newVertices, VertexPositions, OutDelta, NextID);

	TArray<int32> sharedEdgeIDs;

	int32 ExistingID;
	int32 addedFaceID;
	TArray<int32> parentIds = { MOD_ID_NONE };
	TMap<int32, int32> edgeMap;

	if (!GetDeltaForFaceAddition(newVertices, OutDelta, NextID, ExistingID, parentIds, edgeMap, addedFaceID, InGroupIDs))
	{
		return false;
	}

	// If we failed to apply a low-level face addition delta for the desired face vertices,
	// then do our best to undo whatever delta we may have gotten and bail out now.
	if (!ApplyDelta(OutDelta))
	{
		ApplyDelta(*OutDelta.MakeGraphInverse());
		return false;
	}

	OutDeltas.Add(OutDelta);
	OutFaceIDs = { addedFaceID };

	if (!bSplitAndUpdateFaces)
	{
		return true;
	}

	// Edges that are added by the face search to add new faces on the same plane as the added face
	TArray<int32> faceEdgeIDs;
	OutDelta.EdgeAdditions.GenerateKeyArray(faceEdgeIDs);

	OutDelta.Reset();

	if (!GetDeltasForEdgeSplits(OutDeltas, faceEdgeIDs, NextID))
	{
		return false;
	}

	TSet<int32> newFaceEdges;
	for (int32 edgeID : faceEdgeIDs)
	{
		newFaceEdges.Add(edgeID);
	}

	TSet<int32> splitEdges;
	if (!GetDeltasForEdgeAtSplit(OutDeltas, NextID, addedFaceID, splitEdges))
	{
		return false;
	}

	// Edges that are added by intersections search to add new faces on all planes that include the edge (and at least one other edge)
	newFaceEdges.Append(splitEdges);

	auto addedFace = FindFace(addedFaceID);
	if (!ensureAlways(addedFace != nullptr))
	{
		return false;
	}
	FPlane addedPlane = addedFace->CachedPlane;

	TArray<int32> newFaceEdgesArray;
	TArray<FPlane> planeConstraints;
	for (int32 newFaceEdgeID : newFaceEdges)
	{
		// If an edge is only added from the face, provide a plane to constrain finding new faces
		FPlane planeConstraint = FPlane(ForceInitToZero);
		if (faceEdgeIDs.Contains(newFaceEdgeID) && !splitEdges.Contains(newFaceEdgeID))
		{
			planeConstraint = addedPlane;
		}

		newFaceEdgesArray.Add(newFaceEdgeID);
		planeConstraints.Add(planeConstraint);
	}
	TArray<FGraph3DDelta> updateFaceDeltas;
	if (GetDeltasForUpdateFaces(updateFaceDeltas, OutFaceIDs, NextID, newFaceEdgesArray, { addedFaceID }, planeConstraints, false))
	{
		OutDeltas.Append(updateFaceDeltas);
	}

	return true;
}

bool FGraph3D::GetDeltasForEdgeSplits(TArray<FGraph3DDelta> &OutDeltas, TArray<int32> &AddedEdgeIDs, int32 &NextID)
{
	for (int32 edgeID : AddedEdgeIDs)
	{
		auto edge = FindEdge(edgeID);
		if (!ensure(edge != nullptr))
		{
			continue;
		}

		int32 startVertexID = edge->StartVertexID;
		int32 endVertexID = edge->EndVertexID;
		auto startVertex = FindVertex(startVertexID);
		auto endVertex = FindVertex(endVertexID);
		if (!ensure(startVertex != nullptr && endVertex != nullptr))
		{
			continue;
		}

		for (auto& edgekvp : Edges)
		{
			int32 otherEdgeID = edgekvp.Key;
			auto otherEdge = FindEdge(otherEdgeID);
			if (!ensure(otherEdge != nullptr))
			{
				continue;
			}

			int32 otherStartVertexID = otherEdge->StartVertexID;
			int32 otherEndVertexID = otherEdge->EndVertexID;
			auto otherStartVertex = FindVertex(otherStartVertexID);
			auto otherEndVertex = FindVertex(otherEndVertexID);
			if (!ensure(otherStartVertex != nullptr && otherEndVertex != nullptr))
			{
				continue;
			}
				
			FVector currentPoint, otherPoint;
			FMath::SegmentDistToSegmentSafe(
				startVertex->Position,
				endVertex->Position,
				otherStartVertex->Position,
				otherEndVertex->Position,
				currentPoint,
				otherPoint
			);

			if (currentPoint.Equals(otherPoint, Epsilon))
			{
				int32 existingID;
				FGraph3DDelta OutDelta(GraphID);
				if (GetDeltaForVertexAddition(currentPoint, OutDelta, NextID, existingID))
				{
					ApplyDelta(OutDelta);
					OutDeltas.Add(OutDelta);
				}
			}
		}
	}

	for (auto& kvp : GetVertices())
	{
		auto vertex = &kvp.Value;
		FVector pos = vertex->Position;
		if (vertex == nullptr)
		{
			return false;
		}

		TArray<int32> edgeIDsToSplit;
		FindEdges(pos, vertex->ID, edgeIDsToSplit);

		for (int32 edgeID : edgeIDsToSplit)
		{
			FGraph3DDelta splitEdgeDelta(GraphID);
			int32 ExistingID = MOD_ID_NONE;

			if (!GetDeltaForEdgeSplit(splitEdgeDelta, edgeID, vertex->ID, NextID, ExistingID))
			{
				return false;
			}

			if (!splitEdgeDelta.IsEmpty())
			{
				OutDeltas.Add(splitEdgeDelta);

				if (AddedEdgeIDs.Contains(edgeID))
				{
					AddedEdgeIDs.Remove(edgeID);

					TArray<int32> edgesAddedFromSplit;
					splitEdgeDelta.EdgeAdditions.GenerateKeyArray(edgesAddedFromSplit);
					AddedEdgeIDs.Append(edgesAddedFromSplit);
				}
			}

			if (!ApplyDelta(splitEdgeDelta))
			{
				return false;
			}
		}
	}

	return true;
}

bool FGraph3D::GetDeltaForFaceVertexAddition(int32 EdgeIDToRemove, int32 FaceID, int32 VertexIDToAdd, FGraph3DDelta &OutDelta)
{
	auto edge = FindEdge(EdgeIDToRemove);
	if (edge == nullptr)
	{
		return false;
	}

	for (auto connection : edge->ConnectedFaces)
	{
		if (connection.FaceID != FaceID && !connection.bContained)
		{
			auto face = FindFace(connection.FaceID);
			auto& faceVertexIDUpdate = FindOrAddVertexUpdates(OutDelta, connection.FaceID);
			bool bForward;
			int32 edgeIdx = face->FindEdgeIndex(edge->ID, bForward);
			if (ensureAlways(edgeIdx != -1))
			{
				faceVertexIDUpdate.NextVertexIDs.Insert(VertexIDToAdd, edgeIdx+1);
			}
		}
	}

	return true;
}

bool FGraph3D::GetDeltaForFaceVertexRemoval(int32 VertexIDToRemove, FGraph3DDelta &OutDelta)
{
	auto vertex = FindVertex(VertexIDToRemove);
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
		auto face = FindFace(faceID);
		if (face == nullptr)
		{
			continue;
		}

		int32 vertexIdx = face->VertexIDs.Find(VertexIDToRemove);
		auto& faceVertexIDUpdate = FindOrAddVertexUpdates(OutDelta, face->ID);
		if (ensureAlways(vertexIdx != -1))
		{
			faceVertexIDUpdate.NextVertexIDs.RemoveAt(vertexIdx);
		}
	}

	return true;
}

FGraph3DFaceVertexIDsDelta& FGraph3D::FindOrAddVertexUpdates(FGraph3DDelta& OutDelta, int32 FaceID)
{
	auto face = FindFace(FaceID);
	check(face);

	if (OutDelta.FaceVertexIDUpdates.Contains(FaceID))
	{
		return OutDelta.FaceVertexIDUpdates[FaceID];
	}
	return OutDelta.FaceVertexIDUpdates.Add(face->ID, FGraph3DFaceVertexIDsDelta(face->VertexIDs, face->VertexIDs));
}

bool FGraph3D::GetDeltaForVertexList(TArray<int32> &OutVertexIDs, const TArray<FVector> &InVertexPositions, FGraph3DDelta& OutDelta, int32 &NextID)
{
	bool bHasNewVertex = false;

	for (const FVector &vertexPos : InVertexPositions)
	{
		int32 vertexID = MOD_ID_NONE;
		const FGraph3DVertex *existingVertex = FindVertex(vertexPos);
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

bool FGraph3D::GetDeltasForEdgeAtSplit(TArray<FGraph3DDelta> &OutDeltas, int32 &NextID, int32 SplittingFaceID, TSet<int32> &OutEdges)
{
	FGraph3DFace *splittingFace = FindFace(SplittingFaceID);
	if (splittingFace == nullptr)
	{
		return false;
	}

	for (auto &kvp : GetFaces())
	{
		const FGraph3DFace* otherFace = &kvp.Value;

		if (kvp.Key == SplittingFaceID)
		{
			continue;
		}
		TArray<TPair<FVector, FVector>> OutEdgesFromSplit;

		if (splittingFace->IntersectsFace(otherFace, OutEdgesFromSplit))
		{
			for (auto& edgePoints : OutEdgesFromSplit)
			{
				TArray<FGraph3DDelta> addEdgeDeltas;
				FVector startPosition = edgePoints.Key;
				FVector endPosition = edgePoints.Value;

				TArray<int32> OutEdgeIDs;
				if (!GetDeltaForEdgeAdditionWithSplit(startPosition, endPosition, addEdgeDeltas, NextID, OutEdgeIDs))
				{
					if (OutEdgeIDs.Num() == 0 || OutEdgeIDs[0] == MOD_ID_NONE)
					{
						continue;
					}
				}
				else
				{
					OutDeltas.Append(addEdgeDeltas);
				}
					
				OutEdges.Append(OutEdgeIDs);
			}
		}
	}

	return true;
}

bool FGraph3D::GetDeltaForEdgeSplit(FGraph3DDelta &OutDelta, int32 edgeID, int32 vertexID, int32 &NextID, int32 &ExistingID)
{
	auto edge = FindEdge(edgeID);
	if (edge == nullptr)
	{
		return false;
	}

	auto vertex = FindVertex(vertexID);
	if (vertex == nullptr)
	{
		return false;
	}

	FGraphVertexPair startPair = FGraphVertexPair(edge->StartVertexID, vertexID);
	bool bForward;
	auto existingStartEdge = FindEdgeByVertices(edge->StartVertexID, vertexID, bForward);
	if (existingStartEdge == nullptr)
	{
		if (!GetDeltaForEdgeAddition(startPair, OutDelta, NextID, ExistingID, { edgeID }))
		{
			return false;
		}
	}

	FGraphVertexPair endPair = FGraphVertexPair(vertexID, edge->EndVertexID);
	auto existingEndEdge = FindEdgeByVertices(edge->EndVertexID, vertexID, bForward);
	if (existingEndEdge == nullptr)
	{
		if (!GetDeltaForEdgeAddition(endPair, OutDelta, NextID, ExistingID, { edgeID }))
		{
			return false;
		}
	}

	if (!GetDeltaForFaceVertexAddition(edge->ID, MOD_ID_NONE, vertexID, OutDelta))
	{
		return false;
	}

	if (!GetDeltaForDeletions({}, { edge->ID }, {}, OutDelta))
	{
		return false;
	}

	return true;
}

bool FGraph3D::GetDeltaForVertexJoin(FGraph3DDelta &OutDelta, int32 &NextID, int32 SavedVertexID, int32 RemovedVertexID)
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

	TSet<int32> oldVertexConnectedFaces;
	oldVertex->GetConnectedFaces(oldVertexConnectedFaces);

	for (int32 faceID : oldVertexConnectedFaces)
	{
		auto face = FindFace(faceID);
		if (!ensureAlways(face != nullptr))
		{
			return false;
		}

		int32 oldVertexIdx = face->VertexIDs.Find(RemovedVertexID);
		if (oldVertexIdx == INDEX_NONE)
		{
			return false;
		}

		auto& faceVertexIDUpdate = FindOrAddVertexUpdates(OutDelta, face->ID);
		faceVertexIDUpdate.NextVertexIDs[oldVertexIdx] = SavedVertexID;
	}

	// TODO: check for coincident faces
	for (int32 edgeID : oldVertex->ConnectedEdgeIDs)
	{
		// if new edge is already in graph, delete edge, otherwise modify edge
		auto edge = FindEdge(edgeID);
		if (edge == nullptr)
		{
			return false;
		}

		int32 savedEdgeVertexID = RemovedVertexID == edge->StartVertexID ? edge->EndVertexID : edge->StartVertexID;
		// TODO: catch savedEdgeID == SavedVertexID and then collapse edge and update the connected faces
		bool bOutForward;

		if (!GetDeltaForDeletions({}, { edgeID }, {}, OutDelta))
		{
			return false;
		}

		if (FindEdgeByVertices(savedEdgeVertexID, SavedVertexID, bOutForward) == nullptr)
		{
			// edge should not exist yet, because it was just checked
			int32 ExistingID;

			// TODO: handle edge deletes
			if (!GetDeltaForEdgeAddition(FGraphVertexPair(savedEdgeVertexID, SavedVertexID), OutDelta, NextID, ExistingID))
			{
				return false;
			}
		}
	}

	if (!GetDeltaForDeletions({ RemovedVertexID }, {}, {}, OutDelta))
	{
		return false;
	}

	return true;
}

bool FGraph3D::GetDeltaForEdgeJoin(FGraph3DDelta &OutDelta, int32 &NextID, const TPair<int32, int32>& EdgeIDs)
{
	auto edgeA = FindEdge(EdgeIDs.Key);
	auto edgeB = FindEdge(EdgeIDs.Value);

	if (!edgeA || !edgeB)
	{
		return false;
	}

	// for edges to be joined, one vertex must be shared, and the two vertices unique to one edge will create a new edge
	FGraphVertexPair newEdgeVertexIDs;
	int32 sharedVertexID;

	// find which vertex is shared between the edges
	if (edgeA->StartVertexID == edgeB->StartVertexID)
	{
		sharedVertexID = edgeA->StartVertexID;
		newEdgeVertexIDs = FGraphVertexPair(edgeA->EndVertexID, edgeB->EndVertexID);
	}
	else if (edgeA->EndVertexID == edgeB->EndVertexID)
	{
		sharedVertexID = edgeA->EndVertexID;
		newEdgeVertexIDs = FGraphVertexPair(edgeA->StartVertexID, edgeB->StartVertexID);
	}
	else if (edgeA->StartVertexID == edgeB->EndVertexID)
	{
		sharedVertexID = edgeA->StartVertexID;
		newEdgeVertexIDs = FGraphVertexPair(edgeA->EndVertexID, edgeB->StartVertexID);
	}
	else if (edgeA->EndVertexID == edgeB->StartVertexID)
	{
		sharedVertexID = edgeA->EndVertexID;
		newEdgeVertexIDs = FGraphVertexPair(edgeA->StartVertexID, edgeB->EndVertexID);
	}
	else
	{
		return false;
	}

	auto sharedVertex = FindVertex(sharedVertexID);
	if (sharedVertex->ConnectedEdgeIDs.Num() != 2)
	{
		return false;
	}

	int32 ExistingID;
	TArray<int32> ParentEdgeIDs = { EdgeIDs.Key, EdgeIDs.Value };
	if (!GetDeltaForEdgeAddition(newEdgeVertexIDs, OutDelta, NextID, ExistingID, ParentEdgeIDs))
	{
		return false;
	}

	if (!GetDeltaForDeletions({ sharedVertexID }, ParentEdgeIDs, {}, OutDelta))
	{
		return false;
	}

	// update surrounding faces
	if (!GetDeltaForFaceVertexRemoval(sharedVertexID, OutDelta))
	{
		return false;
	}

	return true;
}

bool FGraph3D::GetSharedSeamForFaces(const int32 FaceID, const int32 OtherFaceID, const int32 SharedIdx, int32 &SeamStartIdx, int32 &SeamEndIdx, int32 &SeamLength)
{
	// attempt to find a shared seam, which is a connected set of edges that are on both faces.

	// because the edgeIDs and vertexIDs are a loop, the first shared edge detected
	// may not be the first shared edge in the seam.  For example, when sharedIdx is 0,
	// the seam can start at the end of the edgeIDs list and wrap back to 0.

	auto face = FindFace(FaceID);
	auto otherFace = FindFace(OtherFaceID);

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

bool FGraph3D::GetDeltasForFaceJoin(TArray<FGraph3DDelta> &OutDeltas, const TArray<int32> &FaceIDs, int32 &NextID)
{
	if (FaceIDs.Num() != 2)
	{
		return false;
	}

	auto face = FindFace(FaceIDs[0]);
	auto otherFace = FindFace(FaceIDs[1]);

	if (face == nullptr || otherFace == nullptr)
	{
		return false;
	}

	// if the faces contain each other, use delete instead
	int32 containedFaceID =
		(face->ContainingFaceID == otherFace->ID ? face->ID :
		(otherFace->ContainingFaceID == face->ID ? otherFace->ID : MOD_ID_NONE));
	if (containedFaceID != MOD_ID_NONE)
	{
		TArray<int32> deletedFaceIDs = { containedFaceID };
		GetDeltaForDeleteObjects(deletedFaceIDs, OutDeltas, NextID, true, false);

		return true;
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
	GetSharedSeamForFaces(face->ID, otherFace->ID, sharedIdx, startIdx, endIdx, seamLength);

	int32 otherStartIdx, otherEndIdx, otherSeamLength;
	GetSharedSeamForFaces(otherFace->ID, face->ID, otherSharedIdx, otherStartIdx, otherEndIdx, otherSeamLength);

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
		auto edge = FindEdge(edgeID);

		// all shared edges are removed, and the vertices that they share are removed
		if (sharedEdgeIDs.Num() > 0)
		{
			int32 sharedVertexID = edgeID > 0 ? edge->StartVertexID : edge->EndVertexID;
			auto sharedVertex = FindVertex(sharedVertexID);
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

	// verify that the shared edges can be removed
	for (int32 edgeID : sharedEdgeIDs)
	{
		auto edge = FindEdge(edgeID);

		// only checks faces that are connected to the edge
		// joins are allowed when the edge is free within the face
		for (auto& connection : edge->ConnectedFaces)
		{
			auto connectedFace = FindFace(connection.FaceID);
			if (!FVector::Parallel(connectedFace->CachedPlane, face->CachedPlane))
			{
				return false;
			}
		}
	}

	// create deltas from adding the new face, deleting the old faces, 
	// and deleting the vertices and edges contained in the shared seam
	//int32 ExistingID, addedID;
	TSet<int32> whitelistIDs, connectedGraphIDs;
	for (int32 edgeID : face->EdgeIDs)
	{
		whitelistIDs.Add(FMath::Abs(edgeID));
	}
	for (int32 edgeID : otherFace->EdgeIDs)
	{
		whitelistIDs.Add(FMath::Abs(edgeID));
	}
	for (int32 edgeID : sharedEdgeIDs)
	{
		whitelistIDs.Remove(FMath::Abs(edgeID));
	}
	if (!ensureAlways(whitelistIDs.Num() >= 3))
	{
		return false;
	}

	int32 seedEdgeID = MOD_ID_NONE;
	for (auto whitelistID : whitelistIDs)
	{
		seedEdgeID = whitelistID;
		break;
	}

	FGraph3DDelta deleteDelta(GraphID);
	TArray<int32> parentFaceIDs = { face->ID, otherFace->ID };
	TSet<int32> groupIDs = face->GroupIDs.Union(otherFace->GroupIDs);
	int32 containingFaceID = MOD_ID_NONE;
	TSet<int32> containedFaceIDs;

	GetDeltaForDeletions(sharedVertexIDs, sharedEdgeIDs, parentFaceIDs, deleteDelta);

	for (int32 parentIdx = 0; parentIdx < parentFaceIDs.Num(); parentIdx++)
	{
		auto parentFace = FindFace(parentFaceIDs[parentIdx]);

		// all of the faces being joined should be contained by the same face
		if (parentIdx == 0)
		{
			containingFaceID = parentFace->ContainingFaceID;
		}
		else
		{
			ensure(containingFaceID == parentFace->ContainingFaceID);
		}

		// all of the faces contained by a face being joined should be contained by the resulting face
		containedFaceIDs.Append(parentFace->ContainedFaceIDs);
	}

	ApplyDelta(deleteDelta);

	// TODO: this kind of code may be useful somewhere else, and could be generalized to be 
	// similar in use to GetDeltaForUpdateFaces
	// only edges need to actually be calculated to use Create2DGraph
	TraversalGraph2D->Reset();
	FPlane graphPlane;
	TArray<int32> newFaceVertexIDs;
	Create2DGraph(whitelistIDs, connectedGraphIDs, TraversalGraph2D, graphPlane, true, true);
	for (auto &kvp : TraversalGraph2D->GetPolygons())
	{
		const FGraph2DPolygon &polygon = kvp.Value;
		if (!polygon.bInterior)
		{
			for (int32 edgeID : polygon.Edges)
			{
				auto edge = TraversalGraph2D->FindEdge(edgeID);
				int32 vertexID = edgeID < 0 ? edge->EndVertexID : edge->StartVertexID;
				newFaceVertexIDs.Add(vertexID);
			}
			break;
		}
	}

	FGraph3DDelta addFaceDelta(GraphID);
	int32 addedFaceID, existingID;
	TMap<int32, int32> edgeMap;
	if (!GetDeltaForFaceAddition(newFaceVertexIDs, addFaceDelta, NextID, existingID, parentFaceIDs, edgeMap, addedFaceID, groupIDs))
	{
		return false;
	}

	TArray<int32> addedFaces;
	addFaceDelta.FaceAdditions.GenerateKeyArray(addedFaces);
	if (addedFaces.Num() != 1)
	{
		return false;
	}

	// Once the new face is added, update the deleted-face delta with the necessary face relationship
	for (auto faceID : parentFaceIDs)
	{
		deleteDelta.FaceDeletions[faceID].ParentObjIDs = { addedFaceID };
	}
	auto &faceAddition = addFaceDelta.FaceAdditions[addedFaceID];
	faceAddition.ParentObjIDs = parentFaceIDs;
	faceAddition.ContainingObjID = containingFaceID;
	faceAddition.ContainedObjIDs = containedFaceIDs;

	// If there's a face that contains the faces that were joined, then make sure it now contains the new joined face,
	// and doesn't contain the faces that are contained by the new joined face.
	if (containingFaceID != MOD_ID_NONE)
	{
		if (auto* containingFace = FindFace(containingFaceID))
		{
			auto& containmentDelta = addFaceDelta.FaceContainmentUpdates.FindOrAdd(containingFaceID);
			containmentDelta.PrevContainingFaceID = containmentDelta.NextContainingFaceID = containingFace->ContainingFaceID;
			containmentDelta.ContainedFaceIDsToAdd.Add(addedFaceID);
			containmentDelta.ContainedFaceIDsToRemove.Append(containedFaceIDs);
		}
	}

	for (int32 faceID : containedFaceIDs)
	{
		auto containedFace = FindFace(faceID);
		if (containedFace != nullptr)
		{
			auto &containmentDelta = addFaceDelta.FaceContainmentUpdates.FindOrAdd(faceID);
			containmentDelta.PrevContainingFaceID = containedFace->ContainingFaceID;
			containmentDelta.NextContainingFaceID = addedFaceID;
		}
	}

	OutDeltas.Add(deleteDelta);

	ApplyDelta(addFaceDelta);
	OutDeltas.Add(addFaceDelta);

	if (!GetDeltasForReduceEdges(OutDeltas, addedFaceID, NextID))
	{
		return false;
	}

	return true;
}

bool FGraph3D::GetDeltasForObjectJoin(TArray<FGraph3DDelta> &OutDeltas, const TArray<int32> &ObjectIDs, int32 &NextID, EGraph3DObjectType ObjectType)
{
	if (ObjectType == EGraph3DObjectType::Face)
	{
		return GetDeltasForFaceJoin(OutDeltas, ObjectIDs, NextID);
	}
	else if (ObjectType == EGraph3DObjectType::Edge)
	{
		FGraph3DDelta OutDelta(GraphID);
		if (!GetDeltaForEdgeJoin(OutDelta, NextID, FGraphVertexPair(ObjectIDs[0], ObjectIDs[1])))
		{
			return false;
		}
		OutDeltas.Add(OutDelta);

		return true;
	}

	return false;
}

bool FGraph3D::GetDeltasForReduceEdges(TArray<FGraph3DDelta> &OutDeltas, int32 FaceID, int32 &NextID)
{
	// TODO: refactor/clean & comment this logic; repeating a for loop within a while loop (without re-initializing the index) is awkward.
	bool bAttemptEdgeJoin = true;
	int32 edgeIdx = 0;
	while (bAttemptEdgeJoin)
	{
		bAttemptEdgeJoin = false;
		auto face = FindFace(FaceID);
		if (face == nullptr)
		{
			return false;
		}

		int32 numEdges = face->EdgeIDs.Num();

		FGraph3DDelta joinEdgeDelta(GraphID);
		for (; edgeIdx < numEdges; edgeIdx++)
		{
			int32 currentEdgeID = face->EdgeIDs[edgeIdx];
			int32 nextEdgeID = face->EdgeIDs[(edgeIdx + 1) % numEdges];

			auto currentEdge = FindEdge(currentEdgeID);
			auto nextEdge = FindEdge(nextEdgeID);
			if (!currentEdge || !nextEdge)
			{
				continue;
			}

			if (!FVector::Parallel(currentEdge->CachedDir, nextEdge->CachedDir))
			{
				continue;
			}

			FGraphVertexPair currentEdgePair(currentEdgeID, nextEdgeID);
			if (GetDeltaForEdgeJoin(joinEdgeDelta, NextID, currentEdgePair))
			{
				bAttemptEdgeJoin = true;

				ApplyDelta(joinEdgeDelta);
				OutDeltas.Add(joinEdgeDelta);
					
				// loop becomes invalidated once an edge is joined, so break out and start again until an edge isn't joined
				break;
			}
		}
	}

	return true;
}

bool FGraph3D::GetDeltaForDeleteObjects(const TArray<int32>& ObjectIDsToDelete, TArray<FGraph3DDelta>& OutDeltas, int32& NextID, bool bGatherEdgesFromFaces, bool bAttemptJoin)
{
	// If we are deleting objects that split graph objects that can be joined, then join them rather than deleting the adjacent objects.
	// TODO: make this logic more sophisticated: each edge and vertex that is being deleted could potentially cause a join,
	// but for now it's easier to have binary logic where joining behavior only happens if that's all that should happen.
	TSet<int32> objIDsToJoin;
	EGraph3DObjectType joinType = EGraph3DObjectType::None;

	if (bAttemptJoin)
	{
		for (int32 objIDToDelete : ObjectIDsToDelete)
		{
			auto* vertexToDelete = FindVertex(objIDToDelete);
			auto* edgeToDelete = FindEdge(objIDToDelete);

			if (vertexToDelete && ((joinType == EGraph3DObjectType::None) || (joinType == EGraph3DObjectType::Edge)) &&
				(vertexToDelete->ConnectedEdgeIDs.Num() == 2))
			{
				auto* joinEdgeA = FindEdge(vertexToDelete->ConnectedEdgeIDs[0]);
				auto* joinEdgeB = FindEdge(vertexToDelete->ConnectedEdgeIDs[1]);
				if (joinEdgeA && joinEdgeB && FVector::Parallel(joinEdgeA->CachedDir, joinEdgeB->CachedDir))
				{
					objIDsToJoin.Add(joinEdgeA->ID);
					objIDsToJoin.Add(joinEdgeB->ID);
					joinType = EGraph3DObjectType::Edge;
				}
			}
			else if (edgeToDelete && ((joinType == EGraph3DObjectType::None) || (joinType == EGraph3DObjectType::Face)) &&
				(edgeToDelete->ConnectedFaces.Num() == 2))
			{
				auto& edgeFaceA = edgeToDelete->ConnectedFaces[0];
				auto& edgeFaceB = edgeToDelete->ConnectedFaces[1];
				if (FVector::Coincident(edgeFaceA.EdgeFaceDir, -edgeFaceB.EdgeFaceDir))
				{
					objIDsToJoin.Add(FMath::Abs(edgeFaceA.FaceID));
					objIDsToJoin.Add(FMath::Abs(edgeFaceB.FaceID));
					joinType = EGraph3DObjectType::Face;
				}
			}
			else
			{
				joinType = EGraph3DObjectType::None;
				break;
			}
		}
	}

	// If the objects to delete can join their neighbors, then only perform that operation.
	if ((joinType != EGraph3DObjectType::None) && (objIDsToJoin.Num() == 2))
	{
		return GetDeltasForObjectJoin(OutDeltas, objIDsToJoin.Array(), NextID, joinType);
	}

	// Otherwise, proceed with a standard deletion of the desired objects (and potentially their neighbors)
	FGraph3DDelta& deleteDelta = OutDeltas.AddDefaulted_GetRef();

	TSet<int32> vertexIDsToDelete, edgeIDsToDelete, faceIDsToDelete;
	TSet<int32> tempGroupMembers, groupIDsToAdd, groupIDsToRemove;

	// The ids that are considered for deletion start with the input arguments, and grow based on object connectivity.
	// Also, for any groups being deleted, remove the group from any objects that mark themselves as belonging to it.
	// TODO: potentially add a flag here to delete objects that belong to a group that's being deleted
	for (int32 signedIDToDelete : ObjectIDsToDelete)
	{
		int32 objectIDToDelete = FMath::Abs(signedIDToDelete);
		switch (GetObjectType(objectIDToDelete))
		{
		case EGraph3DObjectType::None:
			if (GetGroup(objectIDToDelete, tempGroupMembers))
			{
				groupIDsToRemove.Add(objectIDToDelete);
				for (int32 groupMember : tempGroupMembers)
				{
					deleteDelta.GroupIDsUpdates.Add(groupMember, FGraph3DGroupIDsDelta(groupIDsToAdd, groupIDsToRemove));
				}
			}
			break;
		case EGraph3DObjectType::Vertex:
			vertexIDsToDelete.Add(objectIDToDelete);
			break;
		case EGraph3DObjectType::Edge:
			edgeIDsToDelete.Add(objectIDToDelete);
			break;
		case EGraph3DObjectType::Face:
			faceIDsToDelete.Add(objectIDToDelete);
			break;
		default:
			break;
		}
	}

	for (int32 vertexID : vertexIDsToDelete)
	{
		auto vertex = FindVertex(vertexID);
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
		auto edge = FindEdge(edgeID);
		if (edge != nullptr)
		{
			// consider all faces that are connected to the edge for deletion
			for (auto faceConnection : edge->ConnectedFaces)
			{
				if (faceConnection.bContained)
				{
					continue;
				}
				faceIDsToDelete.Add(FMath::Abs(faceConnection.FaceID));
			}

			// delete connected vertices that are not connected to any edge that is not being deleted
			for (auto vertexID : { edge->StartVertexID, edge->EndVertexID })
			{
				auto vertex = FindVertex(vertexID);
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
		auto face = FindFace(faceID);
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

				auto edge = FindEdge(edgeID);
				if (edge != nullptr)
				{
					bool bDeleteEdge = true;
					for (auto connection : edge->ConnectedFaces)
					{
						if (connection.bContained)
						{
							continue;
						}

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

				auto vertex = FindVertex(vertexID);
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
		if (auto *vertex = FindVertex(vertexID))
		{
			allVerticesToDelete.Add(vertex);
		}
	}

	TSet<const FGraph3DEdge *> allEdgesToDelete;
	for (int32 edgeID : edgeIDsToDelete)
	{
		if (auto *edge = FindEdge(edgeID))
		{
			allEdgesToDelete.Add(edge);
		}
	}

	TSet<const FGraph3DFace *> allFacesToDelete;
	for (int32 faceID : faceIDsToDelete)
	{
		if (auto *face = FindFace(faceID))
		{
			allFacesToDelete.Add(face);
		}
	}

	return GetDeltaForDeleteObjects(allVerticesToDelete, allEdgesToDelete, allFacesToDelete, deleteDelta);
}

bool FGraph3D::GetDeltaForDeleteObjects(const TSet<const FGraph3DVertex *> &VerticesToDelete, const TSet<const FGraph3DEdge *> &EdgesToDelete, const TSet<const FGraph3DFace *> &FacesToDelete, FGraph3DDelta &OutDelta)
{
	for (const FGraph3DVertex *vertex : VerticesToDelete)
	{
		if (OutDelta.VertexAdditions.Remove(vertex->ID) == 0)
		{
			// TODO: keep track of GroupIDs in vertex deletions/additions, if they can ever support it.
			// For now, it's easier to leave it as-is and only keep an FVector in the delta.
			ensure(vertex->GroupIDs.Num() == 0);
			OutDelta.VertexDeletions.Add(vertex->ID, vertex->Position);
		}
	}

	for (const FGraph3DEdge *edge : EdgesToDelete)
	{
		if (OutDelta.EdgeAdditions.Remove(edge->ID) == 0)
		{
			OutDelta.EdgeDeletions.Add(edge->ID, FGraph3DObjDelta(FGraphVertexPair(edge->StartVertexID, edge->EndVertexID), {}, edge->GroupIDs));
		}
	}

	for (const FGraph3DFace *face : FacesToDelete)
	{
		if (OutDelta.FaceAdditions.Remove(face->ID) == 0)
		{
			OutDelta.FaceDeletions.Add(face->ID, FGraph3DObjDelta(face->VertexIDs, {}, face->GroupIDs, face->ContainingFaceID, face->ContainedFaceIDs));
		}

		// find next containingID by traversing
		int32 nextContainingFaceID = face->ContainingFaceID;
		while (true)
		{
			auto nextFace = FindFace(nextContainingFaceID);
			if (nextFace == nullptr)
			{
				ensureAlways(nextContainingFaceID == MOD_ID_NONE);
				break;
			}
			else if (!FacesToDelete.Contains(nextFace))
			{
				break;
			}
			nextContainingFaceID = nextFace->ContainingFaceID;
		}

		for (int32 containedFaceID : face->ContainedFaceIDs)
		{
			auto containedFace = FindFace(containedFaceID);
			if (containedFace != nullptr && !FacesToDelete.Contains(containedFace))
			{
				auto& containmentDelta = OutDelta.FaceContainmentUpdates.FindOrAdd(containedFaceID);
				containmentDelta.PrevContainingFaceID = face->ID;
				containmentDelta.NextContainingFaceID = nextContainingFaceID;

				auto nextContainingFace = FindFace(nextContainingFaceID);
				if (nextContainingFace != nullptr)
				{
					auto& nextContainmentDelta = OutDelta.FaceContainmentUpdates.FindOrAdd(nextContainingFaceID);
					nextContainmentDelta.PrevContainingFaceID = nextContainingFace->ContainingFaceID;
					nextContainmentDelta.NextContainingFaceID = nextContainingFace->ContainingFaceID;
					nextContainmentDelta.ContainedFaceIDsToAdd.Add(containedFaceID);
				}
			}
		}
		auto containingFace = FindFace(face->ContainingFaceID);
		if (containingFace != nullptr && !FacesToDelete.Contains(containingFace))
		{
			auto& containmentDelta = OutDelta.FaceContainmentUpdates.FindOrAdd(face->ContainingFaceID);
			containmentDelta.PrevContainingFaceID = containingFace->ContainingFaceID;
			containmentDelta.NextContainingFaceID = containingFace->ContainingFaceID;
			containmentDelta.ContainedFaceIDsToRemove.Add(face->ID);
		}
	}

	OutDelta.GraphID = GraphID;
	return true;
}

bool FGraph3D::GetDeltaForDeletions(const TArray<int32> &VertexIDs, const TArray<int32> &EdgeIDs,
	const TArray<int32> &FaceIDs, FGraph3DDelta &OutDelta)
{
	TSet<const FGraph3DVertex *> allVerticesToDelete;
	TSet<const FGraph3DEdge *> allEdgesToDelete;
	TSet<const FGraph3DFace *> allFacesToDelete;

	for (int32 vertexID : VertexIDs)
	{
		if (const FGraph3DVertex *vertex = FindVertex(vertexID))
		{
			allVerticesToDelete.Add(vertex);
		}
	}

	for (int32 edgeID : EdgeIDs)
	{
		if (const FGraph3DEdge *edge = FindEdge(edgeID))
		{
			allEdgesToDelete.Add(edge);
		}
	}

	for (int32 faceID : FaceIDs)
	{
		if (const FGraph3DFace *face = FindFace(faceID))
		{
			allFacesToDelete.Add(face);
		}
	}

	return GetDeltaForDeleteObjects(allVerticesToDelete, allEdgesToDelete, allFacesToDelete, OutDelta);
}
