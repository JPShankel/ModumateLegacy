// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "ModumateGraph3DTypes.h"
#include "Graph3DVertex.h"
#include "Graph/Graph3DEdge.h"
#include "Graph/Graph3DFace.h"
#include "Graph/Graph3DPolyhedron.h"

struct FGraph3DRecordV1;
typedef FGraph3DRecordV1 FGraph3DRecord;

namespace Modumate
{
	class FGraph;
	class FGraph3DDelta;
	struct FGraph3DGroupIDsDelta;

	class MODUMATE_API FGraph3D
	{
	public:
		FGraph3D(float InEpsilon = DEFAULT_GRAPH3D_EPSILON, bool bInDebugCheck = !UE_BUILD_SHIPPING);

		void Reset();

		FGraph3DEdge* FindEdge(FSignedID EdgeID);
		const FGraph3DEdge* FindEdge(FSignedID EdgeID) const;
		FGraph3DEdge *FindEdgeByVertices(int32 VertexIDA, int32 VertexIDB, bool &bOutForward);
		const FGraph3DEdge *FindEdgeByVertices(int32 VertexIDA, int32 VertexIDB, bool &bOutForward) const;

		FGraph3DVertex* FindVertex(int32 VertexID);
		const FGraph3DVertex* FindVertex(int32 VertexID) const;
		FGraph3DVertex *FindVertex(const FVector &Position);
		const FGraph3DVertex *FindVertex(const FVector &Position) const;

		FGraph3DFace* FindFace(FSignedID FaceID);
		const FGraph3DFace* FindFace(FSignedID FaceID) const;

		FGraph3DPolyhedron* FindPolyhedron(int32 PolyhedronID);
		const FGraph3DPolyhedron* FindPolyhedron(int32 PolyhedronID) const;

		IGraph3DObject* FindObject(FTypedGraphObjID TypedObjID);
		const IGraph3DObject* FindObject(FTypedGraphObjID TypedObjID) const;
		bool ContainsObject(FTypedGraphObjID TypedObjID) const;

		FGraph3DVertex *AddVertex(const FVector &Position, int32 InID, const TSet<int32> &InGroupIDs);

		FGraph3DEdge *AddEdge(int32 StartVertexID, int32 EndVertexID, int32 InID, const TSet<int32> &InGroupIDs);

		FGraph3DFace *AddFace(const TArray<int32> &VertexIDs, int32 InID, const TSet<int32> &InGroupIDs);

		bool RemoveVertex(int32 VertexID);
		bool RemoveEdge(int32 EdgeID);
		bool RemoveFace(int32 FaceID);
		bool RemoveObject(int32 ID, EGraph3DObjectType GraphObjectType);
		bool CleanObject(int32 ID, EGraph3DObjectType type);

		const TMap<int32, FGraph3DEdge> &GetEdges() const;
		const TMap<int32, FGraph3DVertex> &GetVertices() const;
		const TMap<int32, FGraph3DFace> &GetFaces() const;
		const TMap<int32, FGraph3DPolyhedron> &GetPolyhedra() const;
		bool GetGroup(int32 GroupID, TSet<FTypedGraphObjID> &OutGroupMembers) const;

		bool ApplyDelta(const FGraph3DDelta &Delta);

		// calculate a list of vertices that are on the line between the two vertices provided
		bool CalculateVerticesOnLine(const FVertexPair &VertexPair, const FVector& StartPos, const FVector& EndPos, TArray<int32> &OutVertexIDs, TPair<int32, int32> &OutSplitEdgeIDs) const;
		int32 FindOverlappingFace(int32 AddedFaceID) const;
		void FindOverlappingFaces(int32 AddedFaceID, TSet<int32> &OutOverlappingFaces) const;

		bool TraverseFacesFromEdge(int32 OriginalEdgeID, TArray<TArray<int32>> &OutVertexIDs) const;

		static bool AlwaysPassPredicate(FSignedID GraphObjID) { return true; }
		void TraverseFacesGeneric(const TSet<FSignedID> &StartingFaceIDs, TArray<FGraph3DTraversal> &OutTraversals,
			const FGraphObjPredicate &EdgePredicate = AlwaysPassPredicate,
			const FGraphObjPredicate &FacePredicate = AlwaysPassPredicate) const;

		bool CleanGraph(TArray<int32> &OutCleanedVertices, TArray<int32> &OutCleanedEdges, TArray<int32> &OutCleanedFaces);

		int32 CalculatePolyhedra();
		void ClearPolyhedra();

		bool GetPlanesForEdge(int32 OriginalEdgeID, TArray<FPlane> &OutPlanes) const;

		// Create 2D graph representing the connecting set of vertices and edges that are part of the given selection of 3D graph object IDs, and allows some requirements:
		// bRequireConnected - that the graph is fully connected (only one exterior polygonal perimeter)
		// bRequireComplete - there are no extra 3D graph objects supplied that don't correspond to a 2D graph object
		bool Create2DGraph(const TSet<FTypedGraphObjID> &InitialGraphObjIDs, TSet<FTypedGraphObjID> &OutContainedGraphObjIDs,
			FGraph &OutGraph, FPlane &OutPlane, bool bRequireConnected, bool bRequireComplete) const;

		// Create 2D graph representing the connecting set of vertices and edges that are on the cut plane and contain the starting vertex,
		// optionally only traversing the whitelisted IDs, if a set is provided, and optionally creating a mapping from 3D Face IDs to 2D Poly IDs
		bool Create2DGraph(int32 StartVertexID, const FPlane &Plane, FGraph &OutGraph, const TSet<FTypedGraphObjID> *WhitelistIDs = nullptr, TMap<int32, int32> *OutFace3DToPoly2D = nullptr) const;

		// Create 2D graph representing faces, edges, and vertices that are sliced by the cut plane
		bool Create2DGraph(const FPlane &CutPlane, const FVector &AxisX, const FVector &AxisY, const FVector &Origin, const FBox2D &BoundingBox, FGraph &OutGraph, TMap<int32, int32> &OutGraphIDToObjID) const;

		// Find a mapping between this 3D graph's faces and the given 2D graph's polygons, assuming the underlying vertex IDs are the same.
		bool Find2DGraphFaceMapping(TSet<int32> FaceIDsToSearch, const FGraph &Graph, TMap<int32, int32> &OutFace3DToPoly2D) const;

		static void CloneFromGraph(FGraph3D &tempGraph, const FGraph3D &graph);

		void Load(const FGraph3DRecord* InGraph3DRecord);
		void Save(FGraph3DRecord* OutGraph3DRecord);

		bool Validate();

	private:
		const void FindEdges(const FVector &Position, int32 ExistingID, TArray<int32>& OutEdgeIDs) const;

		void AddObjectToGroups(const IGraph3DObject *GraphObject);
		void RemoveObjectFromGroups(const IGraph3DObject *GraphObject);
		void ApplyGroupIDsDelta(FTypedGraphObjID TypedObjectID, const FGraph3DGroupIDsDelta &GroupDelta);

	public:
		float Epsilon;
		bool bDebugCheck;

	private:
		int32 NextPolyhedronID = 1;
		bool bDirty = false;

		TMap<FVertexPair, int32> EdgeIDsByVertexPair;
		TMap<int32, FGraph3DVertex> Vertices;
		TMap<int32, FGraph3DEdge> Edges;
		TMap<int32, FGraph3DFace> Faces;
		TMap<int32, FGraph3DPolyhedron> Polyhedra;
		TSet<FSignedID> DirtyFaces;
		TMap<int32, TSet<FTypedGraphObjID>> CachedGroups;
		TSet<int32> TempInheritedGroupIDs;

	public:

		// direct addition functions
		bool GetDeltaForVertexAddition(const FVector &VertexPos, FGraph3DDelta &OutDelta, int32 &NextID, int32 &ExistingID);
		bool GetDeltaForEdgeAddition(const FVertexPair &VertexPair, FGraph3DDelta &OutDelta, int32 &NextID, int32 &ExistingID, const TArray<int32> &ParentIDs = TArray<int32>());
		bool GetDeltaForFaceAddition(const TArray<int32> &VertexIDs, FGraph3DDelta &OutDelta, int32 &NextID, int32 &ExistingID, TArray<int32> &ParentFaceIDs, TMap<int32, int32> &ParentEdgeIdxToID, int32& AddedFaceID);

		// Propagates deletion to connected objects
		bool GetDeltaForDeleteObjects(const TArray<int32> &VertexIDs, const TArray<int32> &EdgeIDs, const TArray<int32> &FaceIDs, const TArray<int32> &GroupIDs, FGraph3DDelta &OutDelta, bool bGatherEdgesFromFaces);
		// Deletes only the provided objects, used as in intermediate stage in other delta functions
		bool GetDeltaForDeletions(const TArray<int32> &VertexIDs, const TArray<int32> &EdgeIDs, const TArray<int32> &FaceIDs, FGraph3DDelta &OutDelta);
		// Creates Delta based on pending deleted objects
		bool GetDeltaForDeleteObjects(const TSet<const FGraph3DVertex *> &VerticesToDelete, const TSet<const FGraph3DEdge *> &EdgesToDelete, const TSet<const FGraph3DFace *> &FacesToDelete, FGraph3DDelta &OutDelta);

		bool GetDeltaForVertexMovements(const TArray<int32> &VertexIDs, const TArray<FVector> &NewVertexPositions, TArray<FGraph3DDelta> &OutDeltas, int32 &NextID);

		// calculates splits, etc. then will call the direct versions of the function
		bool GetDeltaForMultipleEdgeAdditions(const FVertexPair &VertexPair, FGraph3DDelta &OutDelta, int32 &NextID, int32 &ExistingID, TArray<int32> &OutVertexIDs, const TArray<int32> &ParentIDs = TArray<int32>());
		bool GetDeltaForEdgeAdditionWithSplit(const FVertexPair &VertexPair, TArray<FGraph3DDelta> &OutDeltas, int32 &NextID, TArray<int32> &OutEdgeIDs);
		bool GetDeltaForEdgeAdditionWithSplit(const FVector &EdgeStartPos, const FVector &EdgeEndPos, TArray<FGraph3DDelta> &OutDeltas, int32 &NextID, TArray<int32> &OutEdgeIDs, bool bCheckFaces = false);
		bool GetDeltaForFaceAddition(const TArray<FVector> &VertexPositions, TArray<FGraph3DDelta> &OutDeltas, int32 &NextID, int32 &ExistingID);

		bool GetDeltasForUpdateFaces(TArray<FGraph3DDelta> &OutDeltas, int32 &NextID, int32 EdgeID, FPlane InPlane = FPlane(EForceInit::ForceInitToZero));

		// provides deltas for splitting edges and adjusting faces after a graph operation
		bool GetDeltasForEdgeSplits(TArray<FGraph3DDelta> &OutDeltas, TArray<int32> &AddedEdgeIDs, int32 &NextID); 

		bool GetDeltaForFaceVertexAddition(int32 EdgeIDToRemove, int32 FaceID, int32 VertexIDToAdd, FGraph3DDelta &OutDelta);
		bool GetDeltaForFaceVertexRemoval(int32 VertexIDToRemove, FGraph3DDelta &OutDelta);

		bool GetDeltaForVertexList(TArray<int32> &OutVertexIDs, const TArray<FVector> &InVertexPositions, FGraph3DDelta &OutDelta, int32 &NextID);

		bool GetDeltasForEdgeAtSplit(TArray<FGraph3DDelta> &OutDeltas, int32 &NextID, int32 &FaceID, TSet<int32> &OutEdges);

		bool GetDeltaForEdgeSplit(FGraph3DDelta &OutDelta, int32 edgeID, int32 vertexID, int32 &NextID, int32 &ExistingID);

		bool GetDeltasForObjectJoin(TArray<FGraph3DDelta> &OutDeltas, const TArray<int32> &ObjectIDs, int32 &NextID, EGraph3DObjectType ObjectType);
		bool GetDeltasForReduceEdges(TArray<FGraph3DDelta> &OutDeltas, int32 FaceID, int32 &NextID);

		bool GetDeltaForVertexJoin(FGraph3DDelta &OutDelta, int32 &NextID, int32 SavedVertexID, int32 RemovedVertexID);
		bool GetDeltaForEdgeJoin(FGraph3DDelta &OutDelta, int32 &NextID, TPair<int32, int32> EdgeIDs);
		bool GetDeltasForFaceJoin(TArray<FGraph3DDelta> &OutDeltas, const TArray<int32> &FaceIDs, int32 &NextID);
		bool GetSharedSeamForFaces(const int32 FaceID, const int32 OtherFaceID, const int32 SharedIdx, int32 &SeamStartIdx, int32 &SeamEndIdx, int32 &SeamLength);
	};
}
