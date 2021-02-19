// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Graph2D.h"
#include "Graph3DTypes.h"
#include "Graph3DVertex.h"
#include "Graph/Graph3DEdge.h"
#include "Graph/Graph3DFace.h"
#include "Graph/Graph3DPolyhedron.h"

struct FGraph3DRecordV1;
typedef FGraph3DRecordV1 FGraph3DRecord;
struct FGraph3DDelta;
struct FGraph3DGroupIDsDelta;

namespace Modumate
{
	class MODUMATE_API FGraph3D
	{
	public:
		FGraph3D(float InEpsilon = DEFAULT_GRAPH3D_EPSILON, bool bInDebugCheck = !UE_BUILD_SHIPPING);

		void Reset();
		bool Equals(const FGraph3D& Other, float EqualityEpsilon = DEFAULT_GRAPH3D_EPSILON) const;

		FGraph3DEdge* FindEdge(FGraphSignedID EdgeID);
		const FGraph3DEdge* FindEdge(FGraphSignedID EdgeID) const;
		FGraph3DEdge *FindEdgeByVertices(int32 VertexIDA, int32 VertexIDB, bool &bOutForward);
		const FGraph3DEdge *FindEdgeByVertices(int32 VertexIDA, int32 VertexIDB, bool &bOutForward) const;

		FGraph3DVertex* FindVertex(int32 VertexID);
		const FGraph3DVertex* FindVertex(int32 VertexID) const;
		FGraph3DVertex *FindVertex(const FVector &Position);
		const FGraph3DVertex *FindVertex(const FVector &Position) const;

		FGraph3DFace* FindFace(FGraphSignedID FaceID);
		const FGraph3DFace* FindFace(FGraphSignedID FaceID) const;
		const FGraph3DFace* FindFaceByVertexIDs(const TArray<int32> &VertexIDs) const;
		void FindFacesContainingPosition(const FVector &Position, TSet<int32> &ContainingFaces, bool bAllowOverlaps) const;

		// Test if one face contains another, using the rules of UModumateGeometryStatics::GetPolygonContainment after projecting the faces in 2D.
		bool GetFaceContainment(int32 ContainingFaceID, int32 ContainedFaceID, bool& bOutFullyContained, bool& bOutPartiallyContained) const;

		FGraph3DPolyhedron* FindPolyhedron(int32 PolyhedronID);
		const FGraph3DPolyhedron* FindPolyhedron(int32 PolyhedronID) const;

		IGraph3DObject* FindObject(int32 ID);
		const IGraph3DObject* FindObject(int32 ID) const;
		bool ContainsObject(int32 ID) const;
		bool ContainsGroup(int32 GroupID) const;
		EGraph3DObjectType GetObjectType(int32 ID) const;

		const TMap<int32, FGraph3DEdge> &GetEdges() const;
		const TMap<int32, FGraph3DVertex> &GetVertices() const;
		const TMap<int32, FGraph3DFace> &GetFaces() const;
		const TMap<int32, FGraph3DPolyhedron> &GetPolyhedra() const;
		const TMap<int32, EGraph3DObjectType>& GetAllObjects() const;
		bool GetGroup(int32 GroupID, TSet<int32> &OutGroupMembers) const;
		const TMap<int32, TSet<int32>> &GetGroups() const;


	// graph analysis tools
	public:
		static bool AlwaysPassPredicate(FGraphSignedID GraphObjID) { return true; }
		void TraverseFacesGeneric(const TSet<FGraphSignedID> &StartingFaceIDs, TArray<FGraph3DTraversal> &OutTraversals,
			const FGraphObjPredicate &EdgePredicate = AlwaysPassPredicate,
			const FGraphObjPredicate &FacePredicate = AlwaysPassPredicate) const;

		int32 CalculatePolyhedra();
		void ClearPolyhedra();

		bool GetPlanesForEdge(int32 OriginalEdgeID, TArray<FPlane> &OutPlanes) const;

		// Create 2D graph representing the connecting set of vertices and edges that are part of the given selection of 3D graph object IDs, and allows some requirements:
		// bRequireConnected - that the graph is fully connected (only one exterior polygonal perimeter)
		// bRequireComplete - there are no extra 3D graph objects supplied that don't correspond to a 2D graph object
		bool Create2DGraph(const TSet<int32> &InitialGraphObjIDs, TSet<int32> &OutContainedGraphObjIDs,
			TSharedPtr<FGraph2D> OutGraph, FPlane &OutPlane, bool bRequireConnected, bool bRequireComplete) const;

		// Create 2D graph representing the connecting set of vertices and edges that are on the cut plane and contain the starting vertex,
		// optionally only traversing the whitelisted IDs, if a set is provided, and optionally creating a mapping from 3D Face IDs to 2D Poly IDs
		bool Create2DGraph(int32 StartVertexID, const FPlane &Plane, TSharedPtr<FGraph2D> OutGraph, const TSet<int32> *WhitelistIDs = nullptr, TMap<int32, int32> *OutFace3DToPoly2D = nullptr) const;

		// Create 2D graph representing faces, edges, and vertices that are sliced by the cut plane
		bool Create2DGraph(const FPlane &CutPlane, const FVector &AxisX, const FVector &AxisY, const FVector &Origin, const FBox2D &BoundingBox, TSharedPtr<FGraph2D> OutGraph, TMap<int32, int32> &OutGraphIDToObjID) const;

		// Gather IDs of all objects sliced by cut plane
		bool FindObjectsForPlane(const FVector AxisX, const FVector AxisY, const FVector Origin, const FBox2D& BoundingBox, TSet<int32>& outObjectIDs) const;

		void CheckTranslationValidity(const TArray<int32> &InVertexIDs, TMap<int32, bool> &OutEdgeIDToValidity) const;

	private:
		// Find a mapping between this 3D graph's faces and the given 2D graph's polygons, assuming the underlying vertex IDs are the same.
		bool Find2DGraphFaceMapping(TSet<int32> FaceIDsToSearch, const TSharedPtr<FGraph2D> Graph, TMap<int32, int32> &OutFace3DToPoly2D) const;

	public:
		static void CloneFromGraph(FGraph3D &tempGraph, const FGraph3D &graph);

		void Load(const FGraph3DRecord* InGraph3DRecord);
		void Save(FGraph3DRecord* OutGraph3DRecord);

		void SaveSubset(const TArray<int32> InObjectIDs, FGraph3DRecord* OutGraph3DRecord) const;
		void GetDeltasForPaste(const FGraph3DRecord* InGraph3DRecord, const FVector& InOffset, int32 &NextID, TMap<int32, int32> CopiedToPastedIDs, TArray<FGraph3DDelta>& OutDeltas, bool bIsPreview);

		bool Validate();

		bool CleanGraph(TArray<int32> &OutCleanedVertices, TArray<int32> &OutCleanedEdges, TArray<int32> &OutCleanedFaces);

	public:
		float Epsilon;
		bool bDebugCheck;

	private:
		int32 NextPolyhedronID = 1;
		bool bDirty = false;

		TMap<FGraphVertexPair, int32> EdgeIDsByVertexPair;
		TMap<int32, FGraph3DVertex> Vertices;
		TMap<int32, FGraph3DEdge> Edges;
		TMap<int32, FGraph3DFace> Faces;
		TMap<int32, FGraph3DPolyhedron> Polyhedra;
		TMap<int32, EGraph3DObjectType> AllObjects;
		TSet<FGraphSignedID> DirtyFaces;
		TMap<int32, TSet<int32>> CachedGroups;

		mutable TSet<int32> TempInheritedGroupIDs;
		mutable TArray<FVector2D> TempProjectedPoints;
		TSet<int32> TempDeletedIDs;

		TSharedPtr<FGraph2D> TraversalGraph2D;

	// objects are added and removed from the graph through the use of deltas.  
	// Deltas are created by the (public) 
	public:
		bool ApplyDelta(const FGraph3DDelta &Delta);
		bool ApplyInverseDeltas(const TArray<FGraph3DDelta>& Deltas);
	private:
		FGraph3DVertex *AddVertex(const FVector &Position, int32 InID, const TSet<int32> &InGroupIDs);
		FGraph3DEdge *AddEdge(int32 StartVertexID, int32 EndVertexID, int32 InID, const TSet<int32> &InGroupIDs);
		FGraph3DFace *AddFace(const TArray<int32> &VertexIDs, int32 InID, const TSet<int32> &InGroupIDs, int32 InContainingFaceID, const TSet<int32> &InContainedFaceIDs);

		bool RemoveVertex(int32 VertexID);
		bool RemoveEdge(int32 EdgeID);
		bool RemoveFace(int32 FaceID);
		bool RemoveObject(int32 ID, EGraph3DObjectType GraphObjectType);
		bool CleanObject(int32 ID, EGraph3DObjectType type);

	// object addition
	public:
		bool GetDeltaForVertexAddition(const FVector &VertexPos, FGraph3DDelta &OutDelta, int32 &NextID, int32 &ExistingID);
		bool GetDeltaForEdgeAdditionWithSplit(const FVector &EdgeStartPos, const FVector &EdgeEndPos, TArray<FGraph3DDelta> &OutDeltas, int32 &NextID, TArray<int32> &OutEdgeIDs, bool bCheckFaces = false, bool bSplitAndUpdateEdges = true);
		bool GetDeltaForFaceAddition(const TArray<FVector>& VertexPositions, TArray<FGraph3DDelta>& OutDeltas, int32& NextID, int32& ExistingID, const TSet<int32>& InGroupIDs = TSet<int32>(), bool bSplitAndUpdateFaces = true);

	private:
		bool GetDeltaForEdgeAddition(const FGraphVertexPair &VertexPair, FGraph3DDelta &OutDelta, int32 &NextID, int32 &ExistingID, const TArray<int32> &ParentIDs = TArray<int32>());
		bool GetDeltaForFaceAddition(const TArray<int32> &VertexIDs, FGraph3DDelta &OutDelta, int32 &NextID, int32 &ExistingID, TArray<int32> &ParentFaceIDs, TMap<int32, int32> &ParentEdgeIdxToID, int32& AddedFaceID, const TSet<int32> &InGroupIDs = TSet<int32>());

	// object deletion
	public:
		// Propagates deletion to connected objects
		bool GetDeltaForDeleteObjects(const TArray<int32>& ObjectIDsToDelete, FGraph3DDelta& OutDelta, bool bGatherEdgesFromFaces);
	private:
		// Deletes only the provided objects, used as in intermediate stage in other delta functions
		bool GetDeltaForDeletions(const TArray<int32> &VertexIDs, const TArray<int32> &EdgeIDs, const TArray<int32> &FaceIDs, FGraph3DDelta &OutDelta);
		// Creates Delta based on pending deleted objects
		bool GetDeltaForDeleteObjects(const TSet<const FGraph3DVertex *> &VerticesToDelete, const TSet<const FGraph3DEdge *> &EdgesToDelete, const TSet<const FGraph3DFace *> &FacesToDelete, FGraph3DDelta &OutDelta);

	// joining
	public:
		bool GetDeltasForObjectJoin(TArray<FGraph3DDelta> &OutDeltas, const TArray<int32> &ObjectIDs, int32 &NextID, EGraph3DObjectType ObjectType);
	private:
		bool GetDeltasForReduceEdges(TArray<FGraph3DDelta> &OutDeltas, int32 FaceID, int32 &NextID);

		bool GetDeltaForVertexJoin(FGraph3DDelta &OutDelta, int32 &NextID, int32 SavedVertexID, int32 RemovedVertexID);
		bool GetDeltaForEdgeJoin(FGraph3DDelta &OutDelta, int32 &NextID, const TPair<int32, int32>& EdgeIDs);
		bool GetDeltasForFaceJoin(TArray<FGraph3DDelta> &OutDeltas, const TArray<int32> &FaceIDs, int32 &NextID);
		bool GetSharedSeamForFaces(const int32 FaceID, const int32 OtherFaceID, const int32 SharedIdx, int32 &SeamStartIdx, int32 &SeamEndIdx, int32 &SeamLength);

	// movement
	public:
		bool MoveVerticesDirect(const TArray<int32>& VertexIDs, const TArray<FVector>& NewVertexPositions, TArray<FGraph3DDelta>& OutDeltas, int32& NextID);
		bool GetDeltaForVertexMovements(const TArray<int32>& VertexIDs, const TArray<FVector>& NewVertexPositions, TArray<FGraph3DDelta>& OutDeltas, int32& NextID);

	// splitting functions, support for other graph operations
	private:
		// calculates splits, etc. then will call the direct versions of the function
		bool GetDeltaForMultipleEdgeAdditions(const FGraphVertexPair &VertexPair, FGraph3DDelta &OutDelta, int32 &NextID, int32 &ExistingID, TArray<int32> &OutVertexIDs, const TArray<int32> &ParentIDs = TArray<int32>());
		bool GetDeltaForEdgeAdditionWithSplit(const FGraphVertexPair &VertexPair, TArray<FGraph3DDelta> &OutDeltas, int32 &NextID, TArray<int32> &OutEdgeIDs);

		bool GetDeltasForUpdateFaces(TArray<FGraph3DDelta> &OutDeltas, int32 &NextID, const TArray<int32>& EdgeIDs, const TArray<int32>& FaceIDs, const TArray<FPlane>& InPlanes = TArray<FPlane>(), bool bAddNewFaces = true);

		// provides deltas for splitting edges and adjusting faces after a graph operation
		bool GetDeltasForEdgeSplits(TArray<FGraph3DDelta> &OutDeltas, TArray<int32> &AddedEdgeIDs, int32 &NextID);

		bool GetDeltaForFaceVertexAddition(int32 EdgeIDToRemove, int32 FaceID, int32 VertexIDToAdd, FGraph3DDelta &OutDelta);
		bool GetDeltaForFaceVertexRemoval(int32 VertexIDToRemove, FGraph3DDelta &OutDelta);

		bool GetDeltaForVertexList(TArray<int32> &OutVertexIDs, const TArray<FVector> &InVertexPositions, FGraph3DDelta &OutDelta, int32 &NextID);

		bool GetDeltasForEdgeAtSplit(TArray<FGraph3DDelta> &OutDeltas, int32 &NextID, int32 SplittingFaceID, TSet<int32> &OutEdges);

		bool GetDeltaForEdgeSplit(FGraph3DDelta &OutDelta, int32 edgeID, int32 vertexID, int32 &NextID, int32 &ExistingID);

		void FindEdges(const FVector &Position, int32 ExistingID, TArray<int32>& OutEdgeIDs) const;

		// Find the face that minimally contains this face
		// NOTE: this assumes that the graph is in a valid enough state that there are no face overlaps
		int32 FindMinFaceContainingFace(int32 FaceID, bool bAllowPartialContainment) const;
		void FindFacesContainedByFace(int32 FaceID, TSet<int32> &OutContainedFaces, bool bAllowPartialContainment) const;

		void AddObjectToGroups(const IGraph3DObject *GraphObject);
		void RemoveObjectFromGroups(const IGraph3DObject *GraphObject);
		void ApplyGroupIDsDelta(int32 ID, const FGraph3DGroupIDsDelta &GroupDelta);

		// calculate a list of vertices that are on the line between the two vertices provided
		bool CalculateVerticesOnLine(const FGraphVertexPair &VertexPair, const FVector& StartPos, const FVector& EndPos, TArray<int32> &OutVertexIDs, TPair<int32, int32> &OutSplitEdgeIDs) const;

		// TODO: this should become private once FinalizeGraphDeltas doesn't check for 
		// overlapping faces (and doesn't trigger the ensure)
	public:
		int32 FindOverlappingFace(int32 AddedFaceID) const;
	private:
		void FindOverlappingFaces(int32 AddedFaceID, const TSet<int32> &ExistingFaces, TSet<int32> &OutOverlappingFaces) const;

		bool TraverseFacesFromEdge(int32 OriginalEdgeID, TArray<TArray<int32>> &OutVertexIDs) const;
	};
}
