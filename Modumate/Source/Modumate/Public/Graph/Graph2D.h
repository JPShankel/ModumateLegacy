// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "DocumentManagement/ModumateSerialization.h"
#include "ModumateCore/ModumateTypes.h"
#include "Graph/Graph2DDelta.h"

namespace Modumate
{
	class FGraph2D;

	// Use this typedef to designate edge IDs, which are only valid if non-0, and whose sign indicates a direction
	typedef int32 FEdgeID;

#define DEFAULT_GRAPH_EPSILON 0.05f

	enum EGraphObjectType
	{
		None,
		Vertex,
		Edge,
		Polygon
	};

	struct FGraphEdge
	{
		int32 ID = MOD_ID_NONE;							// The ID of the edge
		FGraph2D *Graph = nullptr;					// The graph that owns this edge
		int32 StartVertexID = MOD_ID_NONE;				// The ID of the vertex that the start of this edge is connected to
		int32 EndVertexID = MOD_ID_NONE;				// The ID of the vertex that the end of this edge is connected to
		int32 LeftPolyID = MOD_ID_NONE;					// The ID of the polygon that the left side of this edge is a part of
		int32 RightPolyID = MOD_ID_NONE;				// The ID of the polygon that the right side of this edge is a part of
		float Angle = 0.0f;							// The angle of the line from start to end vertex
		FVector2D EdgeDir = FVector2D::ZeroVector;	// The direction of the line from start to end vertex
		bool bValid = false;						// Whether an edge can be used in the graph

		FGraphEdge(int32 InID, FGraph2D* InGraph, int32 InStart, int32 InEnd);
		void SetVertices(int32 InStart, int32 InEnd);
		bool CacheAngle();
	};

	struct FGraphVertex
	{
		int32 ID = MOD_ID_NONE;							// The ID of the vertex
		FGraph2D *Graph = nullptr;					// The graph that owns this vertex
		FVector2D Position = FVector2D::ZeroVector;	// The position of the vertex
		TArray<FEdgeID> Edges;						// The list of edges (sorted, clockwise, from +X) connected to this vertex
		bool bDirty = false;						// Whether the edge list is dirty, and needs to be re-sorted

		FGraphVertex(int32 InID, FGraph2D* InGraph, const FVector2D &InPos)
			: ID(InID)
			, Graph(InGraph)
			, Position(InPos)
		{ }

		void AddEdge(FEdgeID EdgeID);
		bool RemoveEdge(FEdgeID EdgeID);
		void SortEdges();
		bool GetNextEdge(FEdgeID curEdgeID, FEdgeID &outNextEdgeID, float &outAngleDelta, int32 indexDelta = 1) const;
	};

	struct FGraphPolygon
	{
		int32 ID = MOD_ID_NONE;						// The ID of the polygon
		FGraph2D *Graph = nullptr;				// The graph that owns this polygon
		int32 ParentID = MOD_ID_NONE;				// The ID of the polygon that contains this one, if any
		TArray<int32> InteriorPolygons;			// The IDs of polygons that this polygon contains
		TArray<FEdgeID> Edges;					// The list of edges that make up this polygon

		// Cached derived data
		bool bHasDuplicateEdge = false;			// Whether this polygon has any edges that double back on themselves
		bool bInterior = false;					// Whether this is an interior (simple, solid) polygon; otherwise it is a perimeter
		FBox2D AABB = FBox2D(ForceInitToZero);	// The axis-aligned bounding box for the polygon
		TArray<FVector2D> Points;				// The list of vertex positions in this polygon

		FGraphPolygon(int32 InID, FGraph2D* InGraph) : ID(InID), Graph(InGraph) { }
		FGraphPolygon() { };

		bool IsInside(const FGraphPolygon &otherPoly) const;
		void SetParent(int32 inParentID);
	};

	class MODUMATE_API FGraph2D
	{
	public:
		FGraph2D(float InEpsilon = DEFAULT_GRAPH_EPSILON, bool bInDebugCheck = !UE_BUILD_SHIPPING);

		void Reset();

		FGraphVertex* FindVertex(int32 ID);
		const FGraphVertex* FindVertex(int32 ID) const;
		FGraphVertex* FindVertex(const FVector2D &Position);

		FGraphEdge* FindEdge(FEdgeID EdgeID);
		const FGraphEdge* FindEdge(FEdgeID EdgeID) const;
		const FGraphEdge* FindEdgeByVertices(int32 VertexIDA, int32 VertexIDB, bool &bOutForward);

		FGraphPolygon* FindPolygon(int32 ID);
		const FGraphPolygon* FindPolygon(int32 ID) const;


		bool ContainsObject(const FTypedGraphObjID &GraphObjID) const;
		bool ContainsObject(int32 ID, EGraphObjectType GraphObjectType) const;
		bool GetEdgeAngle(FEdgeID EdgeID, float &outEdgeAngle);

		FGraphVertex *AddVertex(const FVector2D &Position, int32 InID = 0);
		FGraphEdge *AddEdge(int32 StartVertexID, int32 EndVertexID, int32 InID = 0);
		bool RemoveVertex(int32 VertexID);
		bool RemoveEdge(int32 EdgeID);
		bool RemoveObject(int32 ID, EGraphObjectType GraphObjectType);
		int32 CalculatePolygons();
		void ClearPolygons();
		int32 GetExteriorPolygonID() const;
		FGraphPolygon *GetExteriorPolygon();
		const FGraphPolygon *GetExteriorPolygon() const;

		const TMap<int32, FGraphEdge> &GetEdges() const { return Edges; }
		const TMap<int32, FGraphVertex> &GetVertices() const { return Vertices; }
		const TMap<int32, FGraphPolygon> &GetPolygons() const { return Polygons; }

		bool ToDataRecord(FGraph2DRecord &OutRecord, bool bSaveOpenPolygons = false, bool bSaveExteriorPolygons = false) const;
		bool FromDataRecord(const FGraph2DRecord &InRecord);

	private:
		FVertexPair MakeVertexPair(int32 VertexIDA, int32 VertexIDB);

	public:
		float Epsilon;
		bool bDebugCheck;

	private:
		int32 NextEdgeID = 1;
		int32 NextVertexID = 1;
		int32 NextPolyID = 1;
		bool bDirty = false;

		TMap<int32, FGraphEdge> Edges;
		TMap<int32, FGraphVertex> Vertices;
		TMap<int32, FGraphPolygon> Polygons;

		TSet<FEdgeID> DirtyEdges;

		TMap<FVertexPair, int32> EdgeIDsByVertexPair;

		// 2D Graph Deltas
		// The graph operations create a list of deltas that represent instructions for applying the operation.
		// Creating a list of deltas is undo/redo-able, and the public graph operations will leave the graph in the same state
		// that it entered.
	public:
		bool ApplyDelta(const FGraph2DDelta &Delta);

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
		// crosses any existing edges, (TODO) several edges will be created 
		bool AddEdge(TArray<FGraph2DDelta> &OutDeltas, int32 &NextID, const FVector2D StartPosition, const FVector2D EndPosition);

		// Create Deltas that delete all objects provided and also all objects that are invalidated by the deletions -
		// For example, vertices that are no longer connected to any edges are also deleted
		bool DeleteObjects(TArray<FGraph2DDelta> &OutDeltas, const TArray<int32> &VertexIDs, const TArray<int32> &EdgeIDs);

	private:
		// Create Delta resulting in a new vertex added to the graph at the input position
		bool AddVertexDirect(FGraph2DDelta &OutDelta, int32 &NextID, const FVector2D Position);

		// Create Delta resulting in a new edge connecting two existing vertices
		bool AddEdgeDirect(FGraph2DDelta &OutDelta, int32 &NextID, const int32 StartVertexID, const int32 EndVertexID);

		// Create Delta that delete precisely the provided objects
		bool DeleteObjectsDirect(FGraph2DDelta &OutDelta, const TSet<int32> &VertexIDs, const TSet<int32> &EdgeIDs);

		// Create Delta that replaces the edge with two edges - an edge from the start vertex to the split vertex, 
		// and an edge from the split vertex to the end vertex.
		bool SplitEdge(FGraph2DDelta &OutDelta, int32 &NextID, int32 EdgeID, int32 SplittingVertexID);
	};
}
