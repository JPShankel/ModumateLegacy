// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "ModumateTypes.h"
#include "ModumateSerialization.h"

namespace Modumate
{
	class FGraph;

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
		FGraph *Graph = nullptr;					// The graph that owns this edge
		int32 StartVertexID = MOD_ID_NONE;				// The ID of the vertex that the start of this edge is connected to
		int32 EndVertexID = MOD_ID_NONE;				// The ID of the vertex that the end of this edge is connected to
		int32 LeftPolyID = MOD_ID_NONE;					// The ID of the polygon that the left side of this edge is a part of
		int32 RightPolyID = MOD_ID_NONE;				// The ID of the polygon that the right side of this edge is a part of
		float Angle = 0.0f;							// The angle of the line from start to end vertex
		FVector2D EdgeDir = FVector2D::ZeroVector;	// The direction of the line from start to end vertex
		bool bValid = false;						// Whether an edge can be used in the graph

		FGraphEdge(int32 InID, FGraph* InGraph, int32 InStart, int32 InEnd);
		void SetVertices(int32 InStart, int32 InEnd);
		bool CacheAngle();
	};

	struct FGraphVertex
	{
		int32 ID = MOD_ID_NONE;							// The ID of the vertex
		FGraph *Graph = nullptr;					// The graph that owns this vertex
		FVector2D Position = FVector2D::ZeroVector;	// The position of the vertex
		TArray<FEdgeID> Edges;						// The list of edges (sorted, clockwise, from +X) connected to this vertex
		bool bDirty = false;						// Whether the edge list is dirty, and needs to be re-sorted

		FGraphVertex(int32 InID, FGraph* InGraph, const FVector2D &InPos)
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
		FGraph *Graph = nullptr;				// The graph that owns this polygon
		int32 ParentID = MOD_ID_NONE;				// The ID of the polygon that contains this one, if any
		TArray<int32> InteriorPolygons;			// The IDs of polygons that this polygon contains
		TArray<FEdgeID> Edges;					// The list of edges that make up this polygon

		// Cached derived data
		bool bHasDuplicateEdge = false;			// Whether this polygon has any edges that double back on themselves
		bool bInterior = false;					// Whether this is an interior (simple, solid) polygon; otherwise it is a perimeter
		FBox2D AABB = FBox2D(ForceInitToZero);	// The axis-aligned bounding box for the polygon
		TArray<FVector2D> Points;				// The list of vertex positions in this polygon

		FGraphPolygon(int32 InID, FGraph* InGraph) : ID(InID), Graph(InGraph) { }
		FGraphPolygon() { };

		bool IsInside(const FGraphPolygon &otherPoly) const;
		void SetParent(int32 inParentID);
	};

	class MODUMATE_API FGraph
	{
	public:
		FGraph(float InEpsilon = DEFAULT_GRAPH_EPSILON, bool bInDebugCheck = !UE_BUILD_SHIPPING);

		void Reset();

		FGraphEdge* FindEdge(FEdgeID EdgeID);
		const FGraphEdge* FindEdge(FEdgeID EdgeID) const;
		FGraphVertex* FindVertex(int32 ID);
		const FGraphVertex* FindVertex(int32 ID) const;
		FGraphPolygon* FindPolygon(int32 ID);
		const FGraphPolygon* FindPolygon(int32 ID) const;

		FGraphVertex* FindVertex(const FVector2D &Position);

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
	};
}
