// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "DocumentManagement/ModumateSerialization.h"
#include "ModumateCore/ModumateTypes.h"
#include "Graph/Graph2DDelta.h"
#include "Graph/Graph2DEdge.h"
#include "Graph/Graph2DPolygon.h"
#include "Graph/Graph2DTypes.h"
#include "Graph/Graph2DVertex.h"

namespace Modumate
{
	class FGraph2D;

	class MODUMATE_API FGraph2D
	{
	public:
		FGraph2D(int32 InID = MOD_ID_NONE, float InEpsilon = DEFAULT_GRAPH_EPSILON, bool bInDebugCheck = !UE_BUILD_SHIPPING);

		void Reset();

		FGraph2DVertex* FindVertex(int32 ID);
		const FGraph2DVertex* FindVertex(int32 ID) const;
		FGraph2DVertex* FindVertex(const FVector2D &Position);

		FGraph2DEdge* FindEdge(FEdgeID EdgeID);
		const FGraph2DEdge* FindEdge(FEdgeID EdgeID) const;
		const FGraph2DEdge* FindEdgeByVertices(int32 VertexIDA, int32 VertexIDB, bool &bOutForward);

		FGraph2DPolygon* FindPolygon(int32 ID);
		const FGraph2DPolygon* FindPolygon(int32 ID) const;

		bool ContainsObject(const FTypedGraphObjID &GraphObjID) const;
		bool ContainsObject(int32 ID, EGraphObjectType GraphObjectType) const;
		bool GetEdgeAngle(FEdgeID EdgeID, float &outEdgeAngle);

		FGraph2DVertex *AddVertex(const FVector2D &Position, int32 InID = 0);
		FGraph2DEdge *AddEdge(int32 StartVertexID, int32 EndVertexID, int32 InID = 0);
		bool RemoveVertex(int32 VertexID);
		bool RemoveEdge(int32 EdgeID);
		bool RemoveObject(int32 ID, EGraphObjectType GraphObjectType);
		int32 CalculatePolygons();
		void ClearPolygons();

		int32 GetID() const;
		int32 GetExteriorPolygonID() const;
		FGraph2DPolygon *GetExteriorPolygon();
		const FGraph2DPolygon *GetExteriorPolygon() const;

		const TMap<int32, FGraph2DEdge> &GetEdges() const { return Edges; }
		const TMap<int32, FGraph2DVertex> &GetVertices() const { return Vertices; }
		const TMap<int32, FGraph2DPolygon> &GetPolygons() const { return Polygons; }

		bool ToDataRecord(FGraph2DRecord &OutRecord, bool bSaveOpenPolygons = false, bool bSaveExteriorPolygons = false) const;
		bool FromDataRecord(const FGraph2DRecord &InRecord);

	private:
		FVertexPair MakeVertexPair(int32 VertexIDA, int32 VertexIDB);

	public:
		float Epsilon;
		bool bDebugCheck;

	private:
		int32 ID = MOD_ID_NONE;
		int32 NextEdgeID = 1;
		int32 NextVertexID = 1;
		int32 NextPolyID = 1;
		bool bDirty = false;

		TMap<int32, FGraph2DEdge> Edges;
		TMap<int32, FGraph2DVertex> Vertices;
		TMap<int32, FGraph2DPolygon> Polygons;

		TSet<FEdgeID> DirtyEdges;

		TMap<FVertexPair, int32> EdgeIDsByVertexPair;

		// 2D Graph Deltas
		// The graph operations create a list of deltas that represent instructions for applying the operation.
		// Creating a list of deltas is undo/redo-able, and the public graph operations will leave the graph in the same state
		// that it entered.
	public:
		bool ApplyDelta(const FGraph2DDelta &Delta);

		// Clean all objects that are dirtied (by applying deltas) and output their IDs
		bool CleanGraph(TArray<int32> &OutCleanedVertices, TArray<int32> &OutCleanedEdges, TArray<int32> &OutCleanedPolygons);
	private:
		// The graph operation functions test out individual deltas by applying them as they are generated.
		// A common flow is to make basic deltas for adding objects, apply them, and then create more deltas
		// for handling the side-effects.  Graph operation functions should keep track of the deltas that have been applied,
		// so that they can use ApplyInverseDeltas to reset the graph to its initial state before failing out.
		bool ApplyDeltas(const TArray<FGraph2DDelta> &Deltas);
		void ApplyInverseDeltas(const TArray<FGraph2DDelta> &Deltas);

	public:
		// outputs objects that would be added by the list of deltas.
		// TODO: this returns all added objects - it will be useful to figure out which edges were added 
		// constrained by a line segment to figure out which edges would need to host an object
		void AggregateAddedObjects(const TArray<FGraph2DDelta> &Deltas, TSet<int32> &OutVertices, TSet<int32> &OutEdges);

		// aggregates only the added vertices, helpful for determining the result of AddVertex
		void AggregateAddedVertices(const TArray<FGraph2DDelta> &Deltas, TSet<int32> &OutVertices);

		// 2D Graph Operations
		// All graph operations should leave the graph in the same state that it entered if the function failed or if the function
		// is public, even though it needs to be modified to set up all of the deltas.

		/* Common arguments in 2D Graph Operations
			@param[in] NextID: next available ID to assign to created objects.  NextID will be incremented 
			as needed when new graph objects are created, and NextID is shared with the document so that 
			the graph objects can match a backing object instance.
			@param[out] OutDeltas: list of deltas that result in the desired operation applied to the graph
		*/
		
		// Graph operations return true when they are able to create applicable deltas.  This can be vacuously true when 
		// no deltas are needed, for example when the graph object already exists.

		// The public functions are what should be used externally by tools (and by unit tests) because they 
		// properly handle the side effects that are required to maintain assumptions about the graph.
	public:
		// Create Deltas resulting in a new vertex added to the graph at the input position
		bool AddVertex(TArray<FGraph2DDelta> &OutDeltas, int32 &NextID, const FVector2D Position);

		// Create Deltas resulting in new edge(s) added to the graph connecting vertices at the two input positions
		// the 2D graph must not have edges intersecting, so if the line segment connecting the two input positions
		// crosses any existing edges, several edges will be created 
		bool AddEdge(TArray<FGraph2DDelta> &OutDeltas, int32 &NextID, const FVector2D StartPosition, const FVector2D EndPosition);

		// Create Deltas that delete all objects provided and also all objects that are invalidated by the deletions -
		// For example, vertices that are no longer connected to any edges are also deleted.  Note that this function
		// does not use NextID because no new objects should be created.
		bool DeleteObjects(TArray<FGraph2DDelta> &OutDeltas, const TArray<int32> &VertexIDs, const TArray<int32> &EdgeIDs);

		// Create Deltas that move the provided vertices by the provided offset vector.  Moving vertices handles the 
		// same kind of side effects that occur when you add objects (splitting edges, (TODO) handling new polygons).
		bool MoveVertices(TArray<FGraph2DDelta> &OutDeltas, int32 &NextID, const TArray<int32> &VertexIDs, const FVector2D &Offset);


		// The "Direct" versions of helper functions are meant to be combined together to make deltas that result in
		// more complicated operations, and do not apply the delta that they create.
	private:
		// Create Delta resulting in a new vertex added to the graph at the input position
		bool AddVertexDirect(FGraph2DDelta &OutDelta, int32 &NextID, const FVector2D Position);

		// Create Delta resulting in a new edge connecting two existing vertices
		bool AddEdgeDirect(FGraph2DDelta &OutDelta, int32 &NextID, const int32 StartVertexID, const int32 EndVertexID);

		// Create Delta that delete precisely the provided objects
		bool DeleteObjectsDirect(FGraph2DDelta &OutDelta, const TSet<int32> &VertexIDs, const TSet<int32> &EdgeIDs);


		// These helper functions account for side-effects caused by the public functions and apply the deltas that they create,
		// often because deltas that are created afterwards for the same operation rely on the result.
	private:
		// Create and apply a Delta that replaces the edge with two edges - an edge from the start vertex to the split vertex, 
		// and an edge from the split vertex to the end vertex.
		bool SplitEdge(FGraph2DDelta &OutDelta, int32 &NextID, int32 EdgeID, int32 SplittingVertexID);

		// Creates and applies Deltas that split edges if the provided vertices are on them.  This helper function should be called
		// after adding vertices directly to maintain the graph's assumption that nothing overlaps.
		bool SplitEdgesByVertices(TArray<FGraph2DDelta> &OutDeltas, int32 &NextID, TArray<int32> &VertexIDs);

		// Creates and applies Deltas that add edges connecting the two provided vertices.  Several edges may be added instead of one
		// to account for intersections with other edges.  When there is an intersection, the existing edge splits there
		// and edges are added ending at the intersection and starting from the intersection.
		bool AddEdgesBetweenVertices(TArray<FGraph2DDelta> &OutDeltas, int32 &NextID, int32 StartVertexID, int32 EndVertexID);

		// Create and apply a delta that replaces the removed vertex with the saved vertex.  When two vertices are put in the 
		// same position, one is saved and one is removed, representing the join operation.  The edges that 
		// were connected to the removed vertex are replaced to connect to the saved vertex.
		bool JoinVertices(FGraph2DDelta &OutDelta, int32 &NextID, int32 SavedVertexID, int32 RemovedVertexID);
	};
}
