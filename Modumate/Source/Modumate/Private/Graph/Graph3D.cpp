// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "Graph/Graph3D.h"

#include "Algo/Transform.h"
#include "Graph/Graph3DDelta.h"

#include "Graph/Graph2D.h"

#include "ModumateCore/ModumateFunctionLibrary.h"
#include "ModumateCore/ModumateGeometryStatics.h"
#include "DocumentManagement/ModumateSerialization.h"

FGraph3D::FGraph3D(float InEpsilon, bool bInDebugCheck)
	: Epsilon(InEpsilon)
	, bDebugCheck(bInDebugCheck)
{
	TraversalGraph2D = MakeShared<FGraph2D>();
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
	AllObjects.Reset();
	DirtyFaces.Reset();
	CachedGroups.Reset();
	TraversalGraph2D->Reset();
}

bool FGraph3D::Equals(const FGraph3D& Other, float EqualityEpsilon) const
{
	if ((Vertices.Num() != Other.Vertices.Num()) ||
		(Edges.Num() != Other.Edges.Num()) ||
		(Faces.Num() != Other.Faces.Num()) ||
		(CachedGroups.Num() != Other.CachedGroups.Num()))
	{
		return false;
	}

	for (auto& kvp : Vertices)
	{
		auto* otherVertex = Other.FindVertex(kvp.Key);
		if ((otherVertex == nullptr) ||
			!kvp.Value.Position.Equals(otherVertex->Position, EqualityEpsilon))
		{
			return false;
		}
	}

	for (auto& kvp : Edges)
	{
		auto* otherEdge = Other.FindEdge(kvp.Key);
		if ((otherEdge == nullptr) ||
			(otherEdge->StartVertexID != kvp.Value.StartVertexID) ||
			(otherEdge->EndVertexID != kvp.Value.EndVertexID))
		{
			return false;
		}
	}

	for (auto& kvp : Faces)
	{
		auto* otherFace = Other.FindFace(kvp.Key);
		if ((otherFace == nullptr) ||
			(otherFace->VertexIDs != kvp.Value.VertexIDs))
		{
			return false;
		}
	}

	for (auto& kvp : CachedGroups)
	{
		auto* otherGroup = Other.CachedGroups.Find(kvp.Key);
		if ((otherGroup == nullptr) ||
			(otherGroup->Num() != kvp.Value.Num()))
		{
			return false;
		}

		for (int32 groupMemberID : kvp.Value)
		{
			if (!otherGroup->Contains(groupMemberID))
			{
				return false;
			}
		}
	}

	return true;
}

FGraph3DEdge* FGraph3D::FindEdge(FGraphSignedID EdgeID)
{
	return Edges.Find(FMath::Abs(EdgeID));
}

const FGraph3DEdge* FGraph3D::FindEdge(FGraphSignedID EdgeID) const
{
	return Edges.Find(FMath::Abs(EdgeID));
}

FGraph3DEdge *FGraph3D::FindEdgeByVertices(int32 VertexIDA, int32 VertexIDB, bool &bOutForward)
{
	return const_cast<FGraph3DEdge*>(const_cast<const FGraph3D*>(this)->FindEdgeByVertices(VertexIDA, VertexIDB, bOutForward));
}

const FGraph3DEdge *FGraph3D::FindEdgeByVertices(int32 VertexIDA, int32 VertexIDB, bool &bOutForward) const
{
	FGraphVertexPair vertexPair = FGraphVertexPair::MakeEdgeKey(VertexIDA, VertexIDB);
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

void FGraph3D::FindEdges(const FVector &Position, int32 ExistingID, TArray<int32>& OutEdgeIDs) const
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

int32 FGraph3D::FindMinFaceContainingFace(int32 FaceID, bool bAllowPartialContainment) const
{
	int32 minFaceIDContainingPoly = MOD_ID_NONE;
	bool bFullyContained = false;
	bool bPartiallyContained = false;

	auto face = FindFace(FaceID);
	int32 numPolyPoints = face->VertexIDs.Num();
	if (!ensureAlwaysMsgf(numPolyPoints >= 3, TEXT("Can only test for faces containing a valid polygon with >= 3 points!")))
	{
		return MOD_ID_NONE;
	}

	// First, for every other face in the graph, see if they contain the input face;
	// full containment (no shared vertices / edges) is allowed, and partial containment is only allowed if specified by bAllowPartialContainment.
	// Partial containment can not share edges for a graph whose vertex-edge intersections have already been resolved,
	// so only a single shared vertex is possible here (i.e. an inner triangle touching an outer square at its corner, rather than a small square in the corner of a larger square).
	TArray<int32> faceIDsContainingPolygon;
	for (auto& kvp : Faces)
	{
		if (GetFaceContainment(kvp.Key, FaceID, bFullyContained, bPartiallyContained) &&
			(bFullyContained || (bAllowPartialContainment && bPartiallyContained)))
		{
			faceIDsContainingPolygon.Add(kvp.Key);
		}
	}

	// Next, find the face that "minimally" contains the input face, as in it cannot contain a faces that contains the input face.
	// Use the same partial containment critera as before.
	for (int32 faceIDContainingPoly : faceIDsContainingPolygon)
	{
		if ((minFaceIDContainingPoly == MOD_ID_NONE) ||
			(GetFaceContainment(minFaceIDContainingPoly, faceIDContainingPoly, bFullyContained, bPartiallyContained) &&
			(bFullyContained || (bAllowPartialContainment && bPartiallyContained))))
		{
			minFaceIDContainingPoly = faceIDContainingPoly;
		}
	}

	return minFaceIDContainingPoly;
}

void FGraph3D::FindFacesContainedByFace(int32 ContainingFaceID, TSet<int32> &OutContainedFaces, bool bAllowPartialContainment) const
{
	auto containingFace = FindFace(ContainingFaceID);
	if (containingFace == nullptr)
	{
		return;
	}

	for (const auto &kvp : Faces)
	{
		int32 containedFaceID = kvp.Key;
		bool bFullyContained, bPartiallyContained;
		if (GetFaceContainment(ContainingFaceID, containedFaceID, bFullyContained, bPartiallyContained) &&
			(bFullyContained || (bAllowPartialContainment && bPartiallyContained)))
		{
			OutContainedFaces.Add(containedFaceID);
		}
	}
}

void FGraph3D::AddObjectToGroups(const IGraph3DObject *GraphObject)
{
	int32 objectID = GraphObject->ID;
	for (int32 groupID : GraphObject->GroupIDs)
	{
		auto &groupMembers = CachedGroups.FindOrAdd(groupID);
		groupMembers.Add(GraphObject->ID);
	}
}

void FGraph3D::RemoveObjectFromGroups(const IGraph3DObject *GraphObject)
{
	int32 objectID = GraphObject->ID;
	for (int32 groupID : GraphObject->GroupIDs)
	{
		auto &groupMembers = CachedGroups.FindOrAdd(groupID);
		groupMembers.Remove(GraphObject->ID);

		// Remove empty groups now to keep the map clean whenever possible
		if (groupMembers.Num() == 0)
		{
			CachedGroups.Remove(groupID);
		}
	}
}

void FGraph3D::ApplyGroupIDsDelta(int32 ID, const FGraph3DGroupIDsDelta &GroupDelta)
{
	IGraph3DObject *GraphObject = FindObject(ID);

	if (!ensure(GraphObject))
	{
		return;
	}

	// Remove this element from the membership of groups that it used to belong to
	RemoveObjectFromGroups(GraphObject);

	// Apply the delta to this object's own GroupIDs list
	GraphObject->GroupIDs.Append(GroupDelta.GroupIDsToAdd);
	for (int32 groupIDToRemove : GroupDelta.GroupIDsToRemove)
	{
		GraphObject->GroupIDs.Remove(groupIDToRemove);
	}

	// Add this element as a member of groups that it now belongs to
	AddObjectToGroups(GraphObject);

	// Mark the element as dirty, so that it can be cleaned and alert connected group objects
	GraphObject->Dirty(false);
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

FGraph3DFace* FGraph3D::FindFace(FGraphSignedID FaceID) 
{ 
	return Faces.Find(FMath::Abs(FaceID)); 
}

const FGraph3DFace* FGraph3D::FindFace(FGraphSignedID FaceID) const 
{ 
	return Faces.Find(FMath::Abs(FaceID)); 
}

const FGraph3DFace* FGraph3D::FindFaceByVertexIDs(const TArray<int32> &VertexIDs) const
{
	for (auto& facekvp : Faces)
	{
		if (UModumateFunctionLibrary::AreNormalizedIDListsEqual(VertexIDs, facekvp.Value.VertexIDs))
		{
			return &facekvp.Value;
		}
	}

	return nullptr;
}

void FGraph3D::FindFacesContainingPosition(const FVector &Position, TSet<int32> &ContainingFaces, bool bAllowOverlaps) const
{
	for (auto &kvp : Faces)
	{
		bool bOverlaps;
		if (kvp.Value.ContainsPosition(Position, bOverlaps) || (bAllowOverlaps && bOverlaps))
		{
			ContainingFaces.Add(kvp.Key);
		}
	}
}

bool FGraph3D::GetFaceContainment(int32 ContainingFaceID, int32 ContainedFaceID, bool& bOutFullyContained, bool& bOutPartiallyContained) const
{
	FPointInPolyResult pointInPolyResult;
	bOutFullyContained = false;
	bOutPartiallyContained = false;

	const FGraph3DFace* containingFace = FindFace(ContainingFaceID);
	const FGraph3DFace* containedFace = FindFace(ContainedFaceID);
	if (!ensure(containingFace && containedFace))
	{
		return false;
	}

	if (!UModumateGeometryStatics::ArePlanesCoplanar(containingFace->CachedPlane, containedFace->CachedPlane))
	{
		return false;
	}

	// First, project the potentially contained face's points to the space of the potentially containing face
	int32 numContainedPoints = containedFace->CachedPositions.Num();
	TempProjectedPoints.Reset(numContainedPoints);
	for (const FVector &containedVertex : containedFace->CachedPositions)
	{
		TempProjectedPoints.Add(containingFace->ProjectPosition2D(containedVertex));
	}

	return UModumateGeometryStatics::GetPolygonContainment(containingFace->Cached2DPositions, TempProjectedPoints, bOutFullyContained, bOutPartiallyContained);
}

FGraph3DPolyhedron* FGraph3D::FindPolyhedron(int32 PolyhedronID) 
{ 
	return Polyhedra.Find(PolyhedronID); 
}

const FGraph3DPolyhedron* FGraph3D::FindPolyhedron(int32 PolyhedronID) const 
{ 
	return Polyhedra.Find(PolyhedronID); 
}

	
IGraph3DObject* FGraph3D::FindObject(int32 ID)
{
	if (AllObjects.Contains(ID))
	{
		switch (AllObjects[ID])
		{
			case EGraph3DObjectType::Vertex:
				return FindVertex(ID);
			case EGraph3DObjectType::Edge:
				return FindEdge(ID);
			case EGraph3DObjectType::Face:
				return FindFace(ID);
			case EGraph3DObjectType::Polyhedron:
				return FindPolyhedron(ID);
		}
	}
	return nullptr;
}

const IGraph3DObject* FGraph3D::FindObject(int32 ID) const
{
	return const_cast<IGraph3DObject*>(const_cast<FGraph3D*>(this)->FindObject(ID));
}

bool FGraph3D::ContainsObject(int32 ID) const
{
	return AllObjects.Contains(ID);
}

bool FGraph3D::ContainsGroup(int32 GroupID) const
{
	return CachedGroups.Contains(GroupID);
}

EGraph3DObjectType FGraph3D::GetObjectType(int32 ID) const
{
	if (AllObjects.Contains(ID))
	{
		return AllObjects[ID];
	}
	return EGraph3DObjectType::None;
}

FGraph3DVertex *FGraph3D::AddVertex(const FVector &Position, int32 InID, const TSet<int32> &InGroupIDs)
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

	if (AllObjects.Contains(newID))
	{
		return nullptr;
	}

	FGraph3DVertex newVertex = FGraph3DVertex(newID, this, Position, InGroupIDs);
	if (!ensureAlways(newVertex.bValid))
	{
		return nullptr;
	}

	newVertex.Dirty();
	bDirty = true;

	FGraph3DVertex *vertexPtr = &Vertices.Add(newID, MoveTemp(newVertex));
	AllObjects.Add(newID, vertexPtr->GetType());
	AddObjectToGroups(vertexPtr);

	return vertexPtr;
}

FGraph3DEdge *FGraph3D::AddEdge(int32 StartVertexID, int32 EndVertexID, int32 InID, const TSet<int32> &InGroupIDs)
{
	int32 newID = InID;
	if (newID == MOD_ID_NONE)
	{
		return nullptr;
	}

	if (Edges.Contains(newID))
	{
		return FindEdge(newID);
	}

	if (AllObjects.Contains(newID))
	{
		return nullptr;
	}

	FGraph3DEdge newEdge = FGraph3DEdge(newID, this, StartVertexID, EndVertexID, InGroupIDs);
	if (!ensureAlways(newEdge.bValid))
	{
		return nullptr;
	}

	newEdge.Dirty();
	bDirty = true;

	FGraphVertexPair vertexPair = FGraphVertexPair::MakeEdgeKey(StartVertexID, EndVertexID);
	EdgeIDsByVertexPair.Add(vertexPair, newID);

	FGraph3DEdge *edgePtr = &Edges.Add(newID, MoveTemp(newEdge));
	AllObjects.Add(newID, edgePtr->GetType());
	AddObjectToGroups(edgePtr);

	return edgePtr;
}

FGraph3DFace *FGraph3D::AddFace(const TArray<int32> &VertexIDs, int32 InID,
	const TSet<int32> &InGroupIDs, int32 InContainingFaceID, const TSet<int32> &InContainedFaceIDs)
{
	int32 newID = InID;
	if (newID == MOD_ID_NONE)
	{
		return nullptr;
	}

	if (Faces.Contains(newID))
	{
		return FindFace(newID);
	}

	if (AllObjects.Contains(newID))
	{
		return nullptr;
	}

	FGraph3DFace newFace = FGraph3DFace(newID, this, VertexIDs, InGroupIDs, InContainingFaceID, InContainedFaceIDs);
	if (!ensure(newFace.bValid))
	{
		return nullptr;
	}

	newFace.Dirty();
	bDirty = true;

	FGraph3DFace *facePtr = &Faces.Add(newID, MoveTemp(newFace));
	AddObjectToGroups(facePtr);
	AllObjects.Add(newID, facePtr->GetType());

	return facePtr;
}

bool FGraph3D::RemoveVertex(int32 VertexID)
{
	FGraph3DVertex *vertexToRemove = FindVertex(VertexID);
	if (!ensureAlways(vertexToRemove))
	{
		return false;
	}

	vertexToRemove->Dirty();
	RemoveObjectFromGroups(vertexToRemove);

	for (FGraphSignedID connectedEdgeID : vertexToRemove->ConnectedEdgeIDs)
	{
		bool bEdgeStartsFromVertex = (connectedEdgeID > 0);
		FGraph3DEdge *connectedEdge = FindEdge(connectedEdgeID);
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

	AllObjects.Remove(VertexID);
	return Vertices.Remove(VertexID) > 0;
}

bool FGraph3D::RemoveEdge(int32 EdgeID)
{
	EdgeID = FMath::Abs(EdgeID);
	FGraph3DEdge *edgeToRemove = FindEdge(EdgeID);
	if (!ensure(edgeToRemove))
	{
		return false;
	}

	edgeToRemove->Dirty();
	RemoveObjectFromGroups(edgeToRemove);

	if (edgeToRemove->StartVertexID != MOD_ID_NONE)
	{
		FGraph3DVertex *startVertex = FindVertex(edgeToRemove->StartVertexID);
		ensure(startVertex && startVertex->RemoveEdge(EdgeID));
	}

	if (edgeToRemove->EndVertexID != MOD_ID_NONE)
	{
		FGraph3DVertex *endVertex = FindVertex(edgeToRemove->EndVertexID);
		ensure(endVertex && endVertex->RemoveEdge(-EdgeID));
	}

	// Remove the edge from the vertex pair mapping if it's still in there
	if ((edgeToRemove->StartVertexID != MOD_ID_NONE) && (edgeToRemove->EndVertexID != MOD_ID_NONE))
	{
		FGraphVertexPair vertexPair = FGraphVertexPair::MakeEdgeKey(edgeToRemove->StartVertexID, edgeToRemove->EndVertexID);
		EdgeIDsByVertexPair.Remove(vertexPair);
	}

	AllObjects.Remove(EdgeID);
	return Edges.Remove(EdgeID) > 0;
}

bool FGraph3D::RemoveFace(int32 FaceID)
{
	FaceID = FMath::Abs(FaceID);
	FGraph3DFace *faceToRemove = FindFace(FaceID);
	if (!ensure(faceToRemove))
	{
		return false;
	}

	faceToRemove->Dirty();
	RemoveObjectFromGroups(faceToRemove);

	for (FGraphSignedID edgeID : faceToRemove->EdgeIDs)
	{
		if (FGraph3DEdge *faceEdge = FindEdge(edgeID))
		{
			faceEdge->RemoveFace(FaceID, false, true);
		}
	}

	AllObjects.Remove(FaceID);
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
		face->UpdateHoles();
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

const TMap<int32, EGraph3DObjectType>& FGraph3D::GetAllObjects() const
{
	return AllObjects;
}

bool FGraph3D::GetGroup(int32 GroupID, TSet<int32> &OutGroupMembers) const
{
	OutGroupMembers.Reset();

	auto *groupMemberIDs = CachedGroups.Find(GroupID);
	if (groupMemberIDs && (groupMemberIDs->Num() > 0))
	{
		OutGroupMembers.Append(*groupMemberIDs);
		return true;
	}

	return false;
}

const TMap<int32, TSet<int32>> &FGraph3D::GetGroups() const
{
	return CachedGroups;
}

bool FGraph3D::ApplyDelta(const FGraph3DDelta &Delta)
{
	// TODO: updating planes could be a part of Dirty instead of 
	// part of applying the deltas
	TempDeletedIDs.Reset();

	for (auto &kvp : Delta.VertexMovements)
	{
		int32 vertexID = kvp.Key;
		const auto &vertexDelta = kvp.Value;
		FGraph3DVertex *vertex = FindVertex(vertexID);
		if (!ensure(vertex))
		{
			return false;
		}

		vertex->Position = vertexDelta.Value;

		// this also sets all connected edges and faces dirty
		vertex->Dirty(true);
	}

	// Use a persistent cached set for keeping track of group IDs that are being inherited from parent IDs
	// e.g. splitting edges that belong to one or more groups
	TempInheritedGroupIDs.Reset();

	for (auto &kvp : Delta.VertexAdditions)
	{
		// GroupIDs are currently unused with graph vertices
		TSet<int32> GroupIDs;
		auto newVertex = AddVertex(kvp.Value, kvp.Key, GroupIDs);
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
		TempDeletedIDs.Add(kvp.Key);
	}

	for (auto &kvp : Delta.EdgeAdditions)
	{
		int32 edgeID = kvp.Key;
		const TArray<int32> &edgeVertexIDs = kvp.Value.Vertices;
		if (!ensureAlways(edgeVertexIDs.Num() == 2))
		{
			return false;
		}

		TempInheritedGroupIDs = kvp.Value.GroupIDs;
		for (int32 parentID : kvp.Value.ParentObjIDs)
		{
			auto edge = FindEdge(parentID);
			if (edge != nullptr)
			{
				TempInheritedGroupIDs.Append(edge->GroupIDs);
			}
		}

		auto newEdge = AddEdge(edgeVertexIDs[0], edgeVertexIDs[1], edgeID, TempInheritedGroupIDs);
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
		TempDeletedIDs.Add(kvp.Key);
	}

	for (auto &kvp : Delta.FaceAdditions)
	{
		int32 faceID = kvp.Key;
		const FGraph3DObjDelta &objDelta = kvp.Value;
		const TArray<int32> &faceVertexIDs = objDelta.Vertices;

		TempInheritedGroupIDs = objDelta.GroupIDs;
		for(int32 parentID : objDelta.ParentObjIDs)
		{
			auto face = FindFace(parentID);
			if (face != nullptr)
			{
				TempInheritedGroupIDs.Append(face->GroupIDs);
			}
		}

		auto newFace = AddFace(faceVertexIDs, faceID, TempInheritedGroupIDs, objDelta.ContainingObjID, objDelta.ContainedObjIDs);
		if (!ensure(newFace))
		{
			return false;
		}
	}

	for (auto &kvp : Delta.FaceDeletions)
	{
		bool bRemovedFace = RemoveFace(kvp.Key);
		if (!ensure(bRemovedFace))
		{
			return false;
		}
		TempDeletedIDs.Add(kvp.Key);
	}

	for (auto &kvp : Delta.FaceContainmentUpdates)
	{
		auto *face = FindFace(kvp.Key);
		auto &delta = kvp.Value;
		if (ensureAlways(face && (face->ID != delta.NextContainingFaceID)))
		{
			face->UpdateContainingFace(delta.NextContainingFaceID);

			if (bDebugCheck)
			{
				auto duplicateAdds = face->ContainedFaceIDs.Intersect(delta.ContainedFaceIDsToAdd);
				int32 numDuplicateAdds = duplicateAdds.Num();
				ensureMsgf(numDuplicateAdds == 0,
					TEXT("Face ID #%d already contains %d proposed new faces!"), face->ID, numDuplicateAdds);
			}

			face->ContainedFaceIDs.Append(delta.ContainedFaceIDsToAdd);
			for (int32 faceIDToRemove : delta.ContainedFaceIDsToRemove)
			{
				if (bDebugCheck)
				{
					ensureMsgf(face->ContainedFaceIDs.Contains(faceIDToRemove),
						TEXT("Face ID #%d doesn't contain proposed face to remove, ID #%d!"), face->ID, faceIDToRemove);
				}

				face->ContainedFaceIDs.Remove(faceIDToRemove);
			}
			face->Dirty(false);
		}
	}

	for (auto& kvp : Delta.FaceVertexIDUpdates)
	{
		auto face = FindFace(kvp.Key);

		// TODO: sometimes the face is removed as well by this point
		if (face == nullptr) continue;

		face->VertexIDs = kvp.Value.NextVertexIDs;
		face->Dirty(false);
	}

	// Apply changes to the GroupIDs field of vertices, edges, and faces
	for (auto &kvp : Delta.GroupIDsUpdates)
	{
		// We can safely skip GroupIDs updates for graph members that were just deleted
		if (TempDeletedIDs.Contains(kvp.Key))
		{
			continue;
		}

		ApplyGroupIDsDelta(kvp.Key, kvp.Value);
	}

	bool bValidFaces = true;
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
					bValidFaces = false;
				}
			}

			// Not ensuring here in favor of ensuring if the delta is expected to be correct
			// If UpdatePlanes is called, it will assign the vertices correctly, if it isn't called
			// it needs to be updated through UpdateVerticesAndEdges
			if (!face.UpdateVerticesAndEdges(face.VertexIDs, !bUpdatePlanes))
			{
				bValidFaces = false;
			}
		}
	}

	TSet<int32> updatedHoles;
	for (auto &kvp : Delta.FaceContainmentUpdates)
	{
		auto *face = FindFace(kvp.Key);
		if (!ensure(face))
		{
			return false;
		}

		if (!updatedHoles.Contains(face->ID))
		{
			face->UpdateHoles();
			updatedHoles.Add(face->ID);
		}
		if (auto containingFace = FindFace(face->ContainingFaceID))
		{
			if (!updatedHoles.Contains(containingFace->ID))
			{
				containingFace->UpdateHoles();
				updatedHoles.Add(containingFace->ID);
			}
		}
		for (int32 containedFaceID : face->ContainedFaceIDs)
		{
			if (auto containedFace = FindFace(containedFaceID))
			{
				if (!updatedHoles.Contains(containedFace->ID))
				{
					containedFace->UpdateHoles();
					updatedHoles.Add(containedFace->ID);
				}
			}
		}
	}

	return bValidFaces;
}

bool FGraph3D::ApplyInverseDeltas(const TArray<FGraph3DDelta>& Deltas)
{
	bool bSuccess = true;
	int32 numDeltas = Deltas.Num();
	for (int32 deltaIdx = numDeltas - 1; deltaIdx >= 0; deltaIdx--)
	{
		// TODO: mirror FGraph2D::ApplyInverseDeltas and implement void FGraph3DDelta::Invert() to avoid allocating as many new deltas
		bSuccess = ApplyDelta(*Deltas[deltaIdx].MakeGraphInverse()) && bSuccess;
	}

	return bSuccess;
}

bool FGraph3D::CalculateVerticesOnLine(const FGraphVertexPair &VertexPair, const FVector& StartPos, const FVector& EndPos, TArray<int32> &OutVertexIDs, TPair<int32, int32> &OutSplitEdgeIDs) const
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

void FGraph3D::FindOverlappingFaces(int32 AddedFaceID, const TSet<int32> &ExistingFaces, TSet<int32> &OutOverlappingFaces) const
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
			if (!connection.bContained && ExistingFaces.Contains(FMath::Abs(connection.FaceID)) && FMath::Abs(connection.FaceID) != FMath::Abs(AddedFaceID) && FVector::Coincident(connection.EdgeFaceDir, edgeNormal))
			{
				OutOverlappingFaces.Add(FMath::Abs(connection.FaceID));
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
			if (!connection.bContained && FMath::Abs(connection.FaceID) != FMath::Abs(AddedFaceID) && FVector::Coincident(connection.EdgeFaceDir, edgeNormal))
			{
				return connection.FaceID;
			}
		}
		edgeIdx++;
	}

	return MOD_ID_NONE;
}

bool FGraph3D::TraverseFacesFromEdge(int32 OriginalEdgeID, TArray<TArray<int32>> &OutVertexIDs) const
{
	TraversalGraph2D->Reset();

	TArray<FPlane> planes;
	// TODO: it would be interesting to start profiling this
	GetPlanesForEdge(OriginalEdgeID, planes);

	auto edge = FindEdge(OriginalEdgeID);
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

	for (auto& plane : planes)
	{
		bool startVertexDistance = FMath::IsNearlyZero(plane.PlaneDot(startVertex->Position), Epsilon);
		bool endVertexDistance = FMath::IsNearlyZero(plane.PlaneDot(startVertex->Position), Epsilon);
		if (!startVertexDistance || !endVertexDistance || 
			!Create2DGraph(edge->StartVertexID, plane, TraversalGraph2D))
		{
			continue;
		}

		auto originalEdge2D = TraversalGraph2D->FindEdge(OriginalEdgeID);
		if (originalEdge2D == nullptr)
		{
			continue;
		}

		for (int32 polyID : { originalEdge2D->LeftPolyID, originalEdge2D->RightPolyID })
		{
			FGraph2DPolygon* polygon = TraversalGraph2D->FindPolygon(polyID);
			if ((polygon == nullptr) || !polygon->bInterior)
			{
				continue;
			}

			OutVertexIDs.Add(polygon->CachedPerimeterVertexIDs);
		}
	}
	return true;
}

void FGraph3D::TraverseFacesGeneric(const TSet<FGraphSignedID> &StartingFaceIDs, TArray<FGraph3DTraversal> &OutTraversals,
	const FGraphObjPredicate &EdgePredicate, const FGraphObjPredicate &FacePredicate) const
{
	TQueue<FGraphSignedID> faceQueue;
	TSet<FGraphSignedID> visitedFaceIDs;
	TMap<FGraphSignedID, TSet<int32>> visitedEdgesPerFace;
	TArray<FGraphSignedID> curTraversalFaceIDs;
	TArray<FVector> curTraversalPoints;

	for (FGraphSignedID startingFaceID : StartingFaceIDs)
	{
		if (!FacePredicate(startingFaceID))
		{
			continue;
		}

		curTraversalFaceIDs.Reset();
		curTraversalPoints.Reset();
		faceQueue.Empty();
		faceQueue.Enqueue(startingFaceID);

		FGraphSignedID curFaceID = MOD_ID_NONE;
		while (faceQueue.Dequeue(curFaceID))
		{
			const FGraph3DFace *curFace = FindFace(curFaceID);

			if (!visitedFaceIDs.Contains(curFaceID) && curFace)
			{
				visitedFaceIDs.Add(curFaceID);
				curTraversalFaceIDs.Add(curFaceID);
				curTraversalPoints.Append(curFace->CachedPositions);

				TSet<int32> &visitedEdgesForFace = visitedEdgesPerFace.FindOrAdd(curFaceID);
				for (FGraphSignedID faceEdgeSignedID : curFace->EdgeIDs)
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

					FGraphSignedID nextFaceID = MOD_ID_NONE;
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
		FGraphSignedID frontID = faceID;
		FGraphSignedID backID = -faceID;
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

		for (FGraphSignedID faceID : newPolyhedron.FaceIDs)
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

void FGraph3D::CheckTranslationValidity(const TArray<int32> &InVertexIDs, TMap<int32, bool> &OutEdgeIDToValidity) const
{
	TSet<int32> connectedEdges, connectedFaces;
	for (int32 id : InVertexIDs)
	{
		auto vertex = FindVertex(id);
		if (vertex == nullptr)
		{
			continue;
		}

		vertex->GetConnectedFacesAndEdges(connectedFaces, connectedEdges);
	}

	for (int32 edgeID : connectedEdges)
	{
		auto currentEdge = FindEdge(edgeID);
		bool bHasStartVertexID = InVertexIDs.Find(currentEdge->StartVertexID) != INDEX_NONE;
		bool bHasEndVertexID = InVertexIDs.Find(currentEdge->EndVertexID) != INDEX_NONE;
		if (bHasStartVertexID && bHasEndVertexID)
		{
			OutEdgeIDToValidity.Add(edgeID, false);
			continue;
		}

		FVector offsetDir = bHasStartVertexID ? currentEdge->CachedDir : currentEdge->CachedDir * -1.0f;

		bool bValidDirection = true;

		for (int32 faceID : connectedFaces)
		{
			auto face = FindFace(faceID);
			if (FVector::Orthogonal(offsetDir, face->CachedPlane))
			{
				continue;
			}
			else if (face->VertexIDs.Num() == 3)
			{
				continue;
			}

			TArray<FVector> newVertices;
			for (int32 idx = 0; idx < face->VertexIDs.Num(); idx++)
			{
				if (InVertexIDs.Find(face->VertexIDs[idx]) != INDEX_NONE)
				{
					newVertices.Add(face->CachedPositions[idx] + offsetDir);
				}
				else
				{
					newVertices.Add(face->CachedPositions[idx]);
				}
			}

			FPlane newPlane;
			bValidDirection = UModumateGeometryStatics::GetPlaneFromPoints(newVertices, newPlane);
			if (!bValidDirection)
			{
				break;
			}
		}

		OutEdgeIDToValidity.Add(edgeID, bValidDirection);
	}

}

void FGraph3D::Load(const FGraph3DRecord* InGraph3DRecord)
{
	Reset();

	for (auto &kvp : InGraph3DRecord->Vertices)
	{
		AddVertex(FVector(kvp.Value.Position), kvp.Key, TSet<int32>());
	}

	for (auto &kvp : InGraph3DRecord->Edges)
	{
		AddEdge(kvp.Value.StartVertexID, kvp.Value.EndVertexID, kvp.Key, kvp.Value.GroupIDs);
	}

	for (auto &kvp : InGraph3DRecord->Faces)
	{
		AddFace(kvp.Value.VertexIDs, kvp.Key, kvp.Value.GroupIDs, kvp.Value.ContainingFaceID, kvp.Value.ContainedFaceIDs);
	}
		
	TArray<int32> cleanedVertices, cleanedEdges, cleanedFaces;
	CleanGraph(cleanedVertices, cleanedEdges, cleanedFaces);
}
	
void FGraph3D::Save(FGraph3DRecord* OutGraph3DRecord)
{
	OutGraph3DRecord->Vertices.Reset();
	for (auto &kvp : Vertices)
	{
		OutGraph3DRecord->Vertices.Add(kvp.Key, FGraph3DVertexRecord(kvp.Key, kvp.Value.Position));
	}

	OutGraph3DRecord->Edges.Reset();
	for (auto &kvp : Edges)
	{
		OutGraph3DRecord->Edges.Add(kvp.Key, FGraph3DEdgeRecord(kvp.Key, kvp.Value.StartVertexID, kvp.Value.EndVertexID, kvp.Value.GroupIDs));
	}

	OutGraph3DRecord->Faces.Reset();
	for (auto &kvp : Faces)
	{
		OutGraph3DRecord->Faces.Add(kvp.Key, FGraph3DFaceRecord(kvp.Key, kvp.Value.VertexIDs, kvp.Value.GroupIDs, kvp.Value.ContainingFaceID, kvp.Value.ContainedFaceIDs));
	}
}

void FGraph3D::SaveSubset(const TSet<int32> InObjectIDs, FGraph3DRecord* OutGraph3DRecord) const
{
	OutGraph3DRecord->Vertices.Reset();
	OutGraph3DRecord->Edges.Reset();
	OutGraph3DRecord->Faces.Reset();

	for (int32 id : InObjectIDs)
	{
		if (auto vertex = FindVertex(id))
		{
			OutGraph3DRecord->Vertices.Add(id, FGraph3DVertexRecord(id, vertex->Position));
		}
		else if (auto edge = FindEdge(id))
		{
			OutGraph3DRecord->Edges.Add(id, FGraph3DEdgeRecord(id, edge->StartVertexID, edge->EndVertexID, edge->GroupIDs));

			auto startVertex = FindVertex(edge->StartVertexID);
			auto endVertex = FindVertex(edge->EndVertexID);
			if (!ensure(startVertex && endVertex))
			{
				continue;
			}

			OutGraph3DRecord->Vertices.FindOrAdd(startVertex->ID, FGraph3DVertexRecord(startVertex->ID, startVertex->Position));
			OutGraph3DRecord->Vertices.FindOrAdd(endVertex->ID, FGraph3DVertexRecord(endVertex->ID, endVertex->Position));
		}
		else if (auto face = FindFace(id))
		{
			OutGraph3DRecord->Faces.Add(id, FGraph3DFaceRecord(id, face->VertexIDs, face->GroupIDs, face->ContainingFaceID, face->ContainedFaceIDs));
			for (int32 vertexID : face->VertexIDs)
			{
				auto faceVertex = FindVertex(vertexID);
				if (!ensure(faceVertex))
				{
					continue;
				}
				OutGraph3DRecord->Vertices.FindOrAdd(faceVertex->ID, FGraph3DVertexRecord(faceVertex->ID, faceVertex->Position));
			}

			for (int32 edgeID : face->EdgeIDs)
			{
				auto faceEdge = FindEdge(edgeID);
				if (!ensure(faceEdge))
				{
					continue;
				}
				OutGraph3DRecord->Edges.FindOrAdd(faceEdge->ID, FGraph3DEdgeRecord(faceEdge->ID, faceEdge->StartVertexID, faceEdge->EndVertexID, faceEdge->GroupIDs));
			}
		}
	}
}

void FGraph3D::GetDeltasForPaste(const FGraph3DRecord* InGraph3DRecord, const FVector& InOffset, int32 &NextID, TArray<FGraph3DDelta>& OutDeltas, TMap<int32, TArray<int32>>& OutCopiedToPastedIDs, bool bIsPreview)
{
	if (!ensureAlways(InGraph3DRecord))
	{
		return;
	}

	FGraph3DDelta vertexDelta;
	TSet<int32> addedVertices;
	for (auto& kvp : InGraph3DRecord->Vertices)
	{
		int32 outVertexID;
		GetDeltaForVertexAddition(FVector(kvp.Value.Position) + InOffset, vertexDelta, NextID, outVertexID);
		OutCopiedToPastedIDs.Add(kvp.Key, { outVertexID });
	}
	ApplyDelta(vertexDelta);

	OutDeltas.Add(vertexDelta);

	TArray<FGraph3DDelta> faceDeltas;
	TArray<FVector> positions;
	for (auto& kvp : InGraph3DRecord->Faces)
	{
		faceDeltas.Reset();
		positions.Reset();

		TArray<int32> addedFaceIDs;
		for (int32 id : kvp.Value.VertexIDs)
		{
			int32 originalID = OutCopiedToPastedIDs[id][0];
			positions.Add(FindVertex(originalID)->Position);
		}

		if (GetDeltaForFaceAddition(positions, faceDeltas, NextID, addedFaceIDs, TSet<int32>(), !bIsPreview))
		{
			OutCopiedToPastedIDs.Add(kvp.Key, addedFaceIDs);
			OutDeltas.Append(faceDeltas);
		}
	}

	for (auto& kvp : InGraph3DRecord->Edges)
	{
		if (OutCopiedToPastedIDs.Contains(kvp.Key))
		{
			continue;
		}

		TArray<FGraph3DDelta> edgeDeltas;
		TArray<int32> addedEdgeIDs;
		int32 startVertexID = OutCopiedToPastedIDs[kvp.Value.StartVertexID][0];
		int32 endVertexID = OutCopiedToPastedIDs[kvp.Value.EndVertexID][0];
		FVector startPosition = FindVertex(startVertexID)->Position;
		FVector endPosition = FindVertex(endVertexID)->Position;
		bool bOutSameDirection = false;
		if (auto edge = FindEdgeByVertices(startVertexID, endVertexID, bOutSameDirection))
		{
			OutCopiedToPastedIDs.Add(kvp.Key, { edge->ID });
		}
		else if (GetDeltaForEdgeAdditionWithSplit(startPosition, endPosition,
			edgeDeltas, NextID, addedEdgeIDs, true, !bIsPreview))
		{
			OutCopiedToPastedIDs.Add(kvp.Key, addedEdgeIDs);

			OutDeltas.Append(edgeDeltas);
		}
	}
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
				FPlane newPlane(currentPosition, currentPosition + currentDirection, currentPosition + originalDirection);

				// TArray::AddUnique uses operator==, which isn't the comparison we want for planes
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

bool FGraph3D::Create2DGraph(const TSet<int32> &InitialGraphObjIDs, TSet<int32> &OutContainedGraphObjIDs,
	TSharedPtr<FGraph2D> OutGraph, FPlane &OutPlane, bool bRequireConnected, bool bRequireComplete) const
{
	OutPlane = FPlane(ForceInitToZero);

	OutContainedGraphObjIDs = InitialGraphObjIDs;

	// First, try to get the shared plane by finding the common plane for the given faces, if they are given.
	// Additionally, expand the sets of vertices and edges to include those that are part of the provided faces.
	for (int32 graphObjID : InitialGraphObjIDs)
	{
		auto obj = FindObject(graphObjID);
		if (obj == nullptr)
		{
			continue;
		}

		switch (obj->GetType())
		{
		case EGraph3DObjectType::Face:
		{
			const FGraph3DFace *face = FindFace(graphObjID);
			if (!ensure(face))
			{
				return false;
			}

			if (OutPlane.IsZero())
			{
				OutPlane = face->CachedPlane;
			}
			else if (!face->CachedPlane.Equals(OutPlane, PLANAR_DOT_EPSILON) &&
				!face->CachedPlane.Equals(OutPlane.Flip(), PLANAR_DOT_EPSILON))
			{
				return false;
			}

			OutContainedGraphObjIDs.Append(face->VertexIDs);
			OutContainedGraphObjIDs.Append(face->EdgeIDs);
			break;
		}
		// Expand the sets of vertices to include those that are part of the provided edges.
		case EGraph3DObjectType::Edge:
		{
			const FGraph3DEdge *edge = FindEdge(graphObjID);
			if (!ensure(edge))
			{
				return false;
			}

			OutContainedGraphObjIDs.Add(edge->StartVertexID);
			OutContainedGraphObjIDs.Add(edge->EndVertexID);
			break;
		}
		default:
			break;
		}
	}

	// Find an initial vertex to use as a starting point for traversing a new Graph2D.
	// Also, we may need to derive the shared plane from the provided vertex positions.
	int32 initialVertexID = MOD_ID_NONE;
	TArray<FVector> vertexPositions;
	for (int32 graphObjID : OutContainedGraphObjIDs)
	{
		if (GetObjectType(graphObjID) == EGraph3DObjectType::Vertex)
		{
			const FGraph3DVertex *vertex = FindVertex(graphObjID);
			if (!ensure(vertex))
			{
				return false;
			}

			if (initialVertexID == MOD_ID_NONE)
			{
				initialVertexID = graphObjID;
			}

			vertexPositions.Add(vertex->Position);
		}
	}

	// If we still haven't found a plane, then use the provided vertices to find one. Otherwise, we have no other means of finding a shared plane.
	if (OutPlane.IsZero())
	{
		bool bFoundPlane = UModumateGeometryStatics::GetPlaneFromPoints(vertexPositions, OutPlane);
		if (!bFoundPlane)
		{
			return false;
		}
	}

	// Now that we've found a shared plane and have collected all of the graph objects that are part of the initially provided selection,
	// go ahead and create a graph using these objects as a whitelist.
	// Only populate the 3D Face -> 2D Poly mapping if completeness was required.
	TMap<int32, int32> face3DToPoly2D;
	bool bCreatedGraph = Create2DGraph(initialVertexID, OutPlane, OutGraph, &OutContainedGraphObjIDs,
		bRequireComplete ? &face3DToPoly2D : nullptr);

	if (!bCreatedGraph)
	{
		return false;
	}

	int32 exteriorPolyID = OutGraph->GetRootInteriorPolyID();
	bool bFullyConnected = (exteriorPolyID != MOD_ID_NONE);

	// Check the connected requirement, which detects if there are multiple exterior polygons that indicate disconnected graphs
	if (bRequireConnected && !bFullyConnected)
	{
		UE_LOG(LogTemp, Log, TEXT("Graph2D was expected to be fully connected, but it didn't have a singular exterior polygon!"));
		return false;
	}

	// Check the completeness requirement, which fails if there were any input 3D graph objects that didn't make it into the output 2D graph
	if (bRequireComplete)
	{
		for (int32 graphObjID : OutContainedGraphObjIDs)
		{
			switch (GetObjectType(graphObjID))
			{
			case EGraph3DObjectType::Vertex:
			{
				if (!OutGraph->ContainsObject(graphObjID))
				{
					UE_LOG(LogTemp, Log, TEXT("Graph2D was expected to completely contain the input objects, but Vertex #%d was missing!"), graphObjID);
					return false;
				}
				break;
			}
			case EGraph3DObjectType::Edge:
			{
				if (!OutGraph->ContainsObject(graphObjID))
				{
					UE_LOG(LogTemp, Log, TEXT("Graph2D was expected to completely contain the input objects, but Edge #%d was missing!"), graphObjID);
					return false;
				}
				break;
			}
			case EGraph3DObjectType::Face:
			{
				if (!face3DToPoly2D.Contains(graphObjID))
				{
					UE_LOG(LogTemp, Log, TEXT("Graph2D was expected to completely contain the input objects, but Face #%d was missing!"), graphObjID);
					return false;
				}
				break;
			}
			default:
				break;
			}
		}
	}

	return true;
}

bool FGraph3D::Create2DGraph(int32 StartVertexID, const FPlane &Plane, TSharedPtr<FGraph2D> OutGraph, const TSet<int32> *WhitelistIDs, TMap<int32, int32> *OutFace3DToPoly2D) const
{
	if (!ensure(OutGraph.IsValid()))
	{
		return false;
	}

	OutGraph->Reset();

	TQueue<int32> frontierVertexIDs;
	frontierVertexIDs.Enqueue(StartVertexID);

	TSet<int32> visitedVertexIDs, adjacentFaceIDs;

	FVector Cached2DX, Cached2DY;
	UModumateGeometryStatics::FindBasisVectors(Cached2DX, Cached2DY, Plane);

	auto startVertex = FindVertex(StartVertexID);
	if (startVertex == nullptr)
	{
		return false;
	}

	FVector planeOrigin = startVertex->Position;

	OutGraph->AddVertex(FVector2D(EForceInit::ForceInitToZero), startVertex->ID);

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
			if (WhitelistIDs && !WhitelistIDs->Contains(FMath::Abs(edgeID)))
			{
				continue;
			}

			auto edge = FindEdge(edgeID);
			int32 nextVertexID = (edge->StartVertexID == currentVertexID) ? edge->EndVertexID : edge->StartVertexID;

			if (visitedVertexIDs.Contains(nextVertexID) ||
				(WhitelistIDs && !WhitelistIDs->Contains(nextVertexID)))
			{
				continue;
			}

			auto nextVertex = FindVertex(nextVertexID);

			bool distanceFromPlane = FMath::IsNearlyZero(Plane.PlaneDot(nextVertex->Position), Epsilon);
			if (!distanceFromPlane)
			{
				continue;
			}

			FVector difference = nextVertex->Position - planeOrigin;
			FVector2D position2D = FVector2D(difference | Cached2DX, difference | Cached2DY);

			OutGraph->AddVertex(position2D, nextVertex->ID);

			frontierVertexIDs.Enqueue(nextVertexID);

			if (edgeID < 0)
			{
				OutGraph->AddEdge(nextVertexID, currentVertexID, -edgeID);
			}
			else
			{
				OutGraph->AddEdge(currentVertexID, nextVertexID, edgeID);
			}

			for (auto edgeFace : edge->ConnectedFaces)
			{
				if (edgeFace.bContained)
				{
					continue;
				}
				adjacentFaceIDs.Add(FMath::Abs(edgeFace.FaceID));
			}
		}
	}

	OutGraph->CalculatePolygons();

	if (OutFace3DToPoly2D && !Find2DGraphFaceMapping(adjacentFaceIDs, OutGraph, *OutFace3DToPoly2D))
	{
		return false;
	}

	return true;
}

bool FGraph3D::Create2DGraph(const FPlane &CutPlane, const FVector &AxisX, const FVector &AxisY, const FVector &Origin, const FBox2D &BoundingBox, TSharedPtr<FGraph2D> OutGraph, TMap<int32, int32> &OutGraphIDToObjID) const
{
	OutGraph->Reset();

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
						auto existingVertex = OutGraph->FindVertex(p);
						if (existingVertex == nullptr)
						{
							NextID++;
							OutGraph->AddVertex(point, NextID);
							edgeVertexIDs.Add(NextID);
						}
						else
						{
							edgeVertexIDs.Add(existingVertex->ID);
						}
					}
					NextID++;
					ensureAlways(edgeVertexIDs.Num() == 2);
					OutGraph->AddEdge(edgeVertexIDs[0], edgeVertexIDs[1], NextID);

					OutGraphIDToObjID.Add(NextID, face.ID);
				}
			}
		}
	}
		
	return false;
}

bool FGraph3D::FindObjectsForPlane(const FVector AxisX, const FVector AxisY, const FVector Origin, const FBox2D& BoundingBox, TSet<int32>& outObjectIDs) const
{
	outObjectIDs.Reset();
	const FPlane cutPlane(Origin, Origin + AxisX, Origin + AxisY);
	for (const auto& edgeItem: Edges)
	{
		const auto& edge = edgeItem.Value;
		FVector v1(Vertices[edge.StartVertexID].Position);
		FVector v2(Vertices[edge.EndVertexID].Position);
		FVector intersect;

		if (FMath::SegmentPlaneIntersection(v1, v2, cutPlane, intersect)
			&& BoundingBox.IsInside(UModumateGeometryStatics::ProjectPoint2D(intersect, AxisX, AxisY, Origin)) )
		{
			outObjectIDs.Add(edge.ID);
			for (const auto& face: edge.ConnectedFaces)
			{
				outObjectIDs.Add(FMath::Abs(face.FaceID));
			}
		}
	}

	return true;
}

bool FGraph3D::Find2DGraphFaceMapping(TSet<int32> FaceIDsToSearch, const TSharedPtr<FGraph2D> Graph, TMap<int32, int32>& OutFace3DToPoly2D) const
{
	const TMap<int32, FGraph2DPolygon> &graphPolys = Graph->GetPolygons();

	// Compare 2D polygon vertex lists with 3D face vertex lists in order to create a mapping between the face IDs.
	// Only consider interior, closed 2D polygons, since those are the only ones that can exist as 3D faces in the volume graph.
	// TODO: cache faster unique comparison values that correspond to vertex IDs
	TMap<int32, TArray<int32>> sortedVertsBy3DFace, sortedVertsBy2DPoly;
	for (int32 faceID : FaceIDsToSearch)
	{
		auto *face = FindFace(faceID);
		TArray<int32> sortedVerts(face->VertexIDs);
		sortedVerts.Sort();
		sortedVertsBy3DFace.Add(faceID, MoveTemp(sortedVerts));
	}
	for (auto &kvp : graphPolys)
	{
		int32 polyID = kvp.Key;
		const FGraph2DPolygon &polygon = kvp.Value;
		if (polygon.bInterior)
		{
			TArray<int32> sortedVerts;
			Algo::Transform(polygon.Edges, sortedVerts, [&Graph](const FGraphSignedID &EdgeID) {
				const FGraph2DEdge *edge = Graph->FindEdge(EdgeID);
				return (EdgeID > 0) ? edge->StartVertexID : edge->EndVertexID;
			});
			sortedVerts.Sort();
			sortedVertsBy2DPoly.Add(polyID, MoveTemp(sortedVerts));
		}
	}

	for (auto &poly2DKVP : sortedVertsBy2DPoly)
	{
		int32 polyID = poly2DKVP.Key;
		for (auto &face3DKVP : sortedVertsBy3DFace)
		{
			int32 faceID = face3DKVP.Key;
			if (poly2DKVP.Value == face3DKVP.Value)
			{
				// Make sure there's only one 2D poly for a given 3D face
				if (!ensure(!OutFace3DToPoly2D.Contains(faceID)))
				{
					return false;
				}

				OutFace3DToPoly2D.Add(faceID, polyID);
			}
		}
	}

	return (OutFace3DToPoly2D.Num() == sortedVertsBy2DPoly.Num());
}

