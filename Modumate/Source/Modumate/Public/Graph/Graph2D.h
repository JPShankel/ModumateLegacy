// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Graph/Graph2DDelta.h"
#include "Graph/Graph2DEdge.h"
#include "Graph/Graph2DPolygon.h"
#include "Graph/Graph2DTypes.h"
#include "Graph/Graph2DVertex.h"

struct FGraph2DRecord;

namespace Modumate
{
	class FGraph2D;

	class MODUMATE_API FGraph2D
	{
	public:
		FGraph2D(int32 InID = MOD_ID_NONE, float InEpsilon = DEFAULT_GRAPH_EPSILON, bool bInDebugCheck = !UE_BUILD_SHIPPING);

		void Reset();
		bool IsEmpty() const;

		FGraph2DVertex* FindVertex(int32 ID);
		const FGraph2DVertex* FindVertex(int32 ID) const;
		FGraph2DVertex* FindVertex(const FVector2D &Position);

		FGraph2DEdge* FindEdge(FGraphSignedID EdgeID);
		const FGraph2DEdge* FindEdge(FGraphSignedID EdgeID) const;

		FGraph2DEdge *FindEdgeByVertices(int32 VertexIDA, int32 VertexIDB, bool &bOutForward);
		const FGraph2DEdge* FindEdgeByVertices(int32 VertexIDA, int32 VertexIDB, bool &bOutForward) const;

		FGraph2DPolygon* FindPolygon(int32 ID);
		const FGraph2DPolygon* FindPolygon(int32 ID) const;

		bool ContainsObject(int32 ID, EGraphObjectType GraphObjectType) const;
		bool GetEdgeAngle(FGraphSignedID EdgeID, float &outEdgeAngle);

		FGraph2DVertex *AddVertex(const FVector2D &Position, int32 InID = 0);
		FGraph2DEdge *AddEdge(int32 StartVertexID, int32 EndVertexID, int32 InID = 0);
		FGraph2DPolygon *AddPolygon(TArray<int32> &VertexIDs, int32 InID = 0, bool bInterior = false);

		bool RemoveVertex(int32 VertexID);
		bool RemoveEdge(int32 EdgeID);
		bool RemovePolygon(int32 PolyID);
		bool RemoveObject(int32 ID, EGraphObjectType GraphObjectType);
		int32 CalculatePolygons();
		void ClearPolygons();

		int32 GetID() const;
		int32 GetRootPolygonID() const;
		FGraph2DPolygon *GetRootPolygon();
		const FGraph2DPolygon *GetRootPolygon() const;

		const TMap<int32, FGraph2DEdge> &GetEdges() const { return Edges; }
		const TMap<int32, FGraph2DVertex> &GetVertices() const { return Vertices; }
		const TMap<int32, FGraph2DPolygon> &GetPolygons() const { return Polygons; }

		bool ToDataRecord(FGraph2DRecord* OutRecord, bool bSaveOpenPolygons = false, bool bSaveExteriorPolygons = false) const;
		bool FromDataRecord(const FGraph2DRecord* InRecord);

		float Epsilon;
		bool bDebugCheck;

	private:
		int32 ID = MOD_ID_NONE;
		int32 NextEdgeID = 1;
		int32 NextVertexID = 1;
		int32 NextPolyID = 1;

		TMap<int32, FGraph2DEdge> Edges;
		TMap<int32, FGraph2DVertex> Vertices;
		TMap<int32, FGraph2DPolygon> Polygons;

		TMap<FGraphVertexPair, int32> EdgeIDsByVertexPair;

		// vertices and edges in the graph must be inside the bounding vertices,
		// and outside the bounding contained vertices
		TPair<int32, TArray<int32>> BoundingPolygon;
		TMap<int32, TArray<int32>> BoundingContainedPolygons;

		// During validation, the vertex IDs saved in BoundingVertices and BoundingContainedVertices
		// are converted into this to calculate whether the vertices and edges of the graph are within the bounds
		struct FBoundsInformation
		{
			TArray<FVector2D> Positions;
			TArray<FVector2D> EdgeNormals;
		};

		FBoundsInformation CachedOuterBounds;
		TArray<FBoundsInformation> CachedInnerBounds;

		// 2D Graph Deltas
		// The graph operations create a list of deltas that represent instructions for applying the operation.
		// Creating a list of deltas is undo/redo-able, and the public graph operations will leave the graph in the same state
		// that it entered.
	public:
		bool ApplyDelta(const FGraph2DDelta &Delta);

		// Clear the modified flags from all graph objects, and gather their IDs so that reflected objects can be updated.
		bool ClearModifiedObjects(TArray<int32>& OutModifiedVertices, TArray<int32>& OutModifiedEdges, TArray<int32>& OutModifiedPolygons);

		// Clean all dirty graph objects, in the correct order based on geometric dependencies between vertices, edges, and polygons
		bool CleanDirtyObjects(bool bCleanPolygons);

		// Get whether any graph objects are marked as derived data dirty
		bool IsDirty() const;

		void ClearBounds();

		// Walk the edges in the graph from the given edge (and direction), visiting the "next" edge from the perspective of each visited vertex.
		// If called for every edge using the same RefVisitedEdgeIDs, it can generate non-simple polygonal traversals that cover every
		// edge once in each direction.
		// When called for a single non-simple polygonal traversal, it can generate a simple polygonal perimeter traversal,
		// if only allowed to visit the original traversal's edges.
		bool TraverseEdges(FGraphSignedID StartingEdgeID, TArray<FGraphSignedID>& OutEdgeIDs, TArray<int32>& OutVertexIDs,
			bool& bOutInterior, TSet<FGraphSignedID>& RefVisitedEdgeIDs,
			bool bUseDualEdges = true, const TSet<FGraphSignedID>* AllowedEdgeIDs = nullptr) const;

		// The graph operation functions test out individual deltas by applying them as they are generated.
		// A common flow is to make basic deltas for adding objects, apply them, and then create more deltas
		// for handling the side-effects.  Graph operation functions should keep track of the deltas that have been applied,
		// so that they can use ApplyInverseDeltas to reset the graph to its initial state before failing out.
		bool ApplyDeltas(const TArray<FGraph2DDelta> &Deltas, bool bApplyInverseOnFailure = true);
		bool ApplyInverseDeltas(const TArray<FGraph2DDelta> &Deltas);

	private:
		// Checks the values of the graph against the bounds if they are set.  This should be called as the 
		// last side-effect in every public graph operation.  If any vertex is outside the bounding
		// polygon or inside any of the bounding holes, the deltas are invalid and should be inverted.
		bool ValidateAgainstBounds();

		bool UpdateCachedBoundsPositions();
		void UpdateCachedBoundsNormals();

	public:
		// outputs objects that would be added by the list of deltas.
		void AggregateAddedObjects(const TArray<FGraph2DDelta>& Deltas, TSet<int32>& OutVertices, TSet<int32>& OutEdges, TSet<int32>& OutPolygons);

		// aggregates only the added vertices, helpful for determining the result of AddVertex
		void AggregateAddedVertices(const TArray<FGraph2DDelta> &Deltas, TSet<int32> &OutVertices);

		// aggregates only the added edges, constrained by the provided segment
		void AggregateAddedEdges(const TArray<FGraph2DDelta> &Deltas, TSet<int32> &OutEdges, const FVector2D &StartPosition, const FVector2D &EndPosition);

		void GetOuterBoundsIDs(TArray<int32> &OutVertexIDs) const;
		const TMap<int32, TArray<int32>>& GetInnerBounds() const;

		// TODO: aggregate polygons constrained by polygon

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
		// For example, vertices that are no longer connected to any edges are also deleted.  
		bool DeleteObjects(TArray<FGraph2DDelta> &OutDeltas, int32 &NextID, const TArray<int32> &VertexIDs, const TArray<int32> &EdgeIDs);

		// Create Deltas that move the provided vertices by the provided offset vector.  Moving vertices handles the 
		// same kind of side effects that occur when you add objects (splitting edges, (TODO) handling new polygons).
		bool MoveVertices(TArray<FGraph2DDelta> &OutDeltas, int32 &NextID, const TMap<int32, FVector2D>& NewVertexPositions);

		// MoveVertices, but with an identical offset applied to each vertex specified by ID, rather than arbitrary new positions.
		bool MoveVertices(TArray<FGraph2DDelta> &OutDeltas, int32 &NextID, const TArray<int32> &VertexIDs, const FVector2D &Offset);

		// Set lists of vertex IDs to represent the bounds of the surface graph.  All vertices and edges must be inside the 
		// OuterBounds (inclusive) and outside all InnerBounds (inclusive).
		bool SetBounds(TArray<FGraph2DDelta> &OutDeltas, TPair<int32, TArray<int32>> &OuterBounds, TMap<int32, TArray<int32>> &InnerBounds);

		// Create Deltas for an empty graph resulting in adding the specified polygons.
		// If using as bounds, the bounds will be set based on the resulting polygons.
		bool PopulateFromPolygons(TArray<FGraph2DDelta>& OutDeltas, int32& NextID, TArray<TArray<FVector2D>>& InitialPolygons, bool bUseAsBounds);

		// The "Direct" versions of helper functions are meant to be combined together to make deltas that result in
		// more complicated operations, and do not apply the delta that they create.
	private:
		// Create Delta resulting in a new vertex added to the graph at the input position
		bool AddVertexDirect(FGraph2DDelta &OutDelta, int32 &NextID, const FVector2D Position);

		// Create Delta resulting in a new edge connecting two existing vertices
		bool AddEdgeDirect(FGraph2DDelta &OutDelta, int32 &NextID, const int32 StartVertexID, const int32 EndVertexID, const TArray<int32> &ParentIDs = TArray<int32>());

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

		bool CalculatePolygons(TArray<FGraph2DDelta> &OutDeltas, int32 &NextID);
	};
}
