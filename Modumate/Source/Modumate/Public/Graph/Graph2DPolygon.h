// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Graph/Graph2DTypes.h"

#pragma once

namespace Modumate
{
	class FGraph2DPolygon : public IGraph2DObject
	{
	public:
		// Definitional data
		TArray<int32> VertexIDs;						// The list of vertices that make up this polygon

		// Derived data
		TArray<FGraphSignedID> Edges;					// The list of edges that make up this polygon
		int32 ParentID = MOD_ID_NONE;					// The ID of the polygon that contains this one, if any
		TArray<int32> InteriorPolygons;					// The IDs of polygons that this polygon contains
		bool bHasDuplicateVertex = false;				// Whether this polygon has any vertices that repeat (peninsulas/islands)
		bool bInterior = false;							// Whether this is an interior (simple, solid) polygon; otherwise it is a perimeter
		FBox2D AABB = FBox2D(ForceInitToZero);			// The axis-aligned bounding box for the polygon
		TArray<FVector2D> CachedPoints;					// The list of vertex positions in this polygon
		TArray<int32> CachedPerimeterVertexIDs;			// For interior polygons, the IDs of vertices that make up the perimeter traversal
		TArray<FGraphSignedID> CachedPerimeterEdgeIDs;	// For interior polygons, the directed IDs of edges that make up the perimeter traversal
		TArray<FVector2D> CachedPerimeterPoints;		// For interior polygons, the positions of vertices that make up the perimeter traversal

		FGraph2DPolygon(int32 InID, FGraph2D* InGraph) : IGraph2DObject(InID, InGraph) { }
		FGraph2DPolygon() : IGraph2DObject(MOD_ID_NONE, nullptr) { };

		FGraph2DPolygon(int32 InID, FGraph2D* InGraph, TArray<int32> &Vertices, bool bInInterior);

		bool IsInside(const FGraph2DPolygon &otherPoly) const;
		void SetParent(int32 inParentID);

		void SetVertices(const TArray<int32> &Vertices);

		// Double-check that the polygon's definition matches the minimal traversal in the graph.
		bool ValidateTraversal();

		virtual void Dirty(bool bConnected = true) override;
		virtual bool Clean() override;

		virtual EGraphObjectType GetType() const override { return EGraphObjectType::Polygon; };
		virtual void GetVertexIDs(TArray<int32> &OutVertexIDs) const override;

	protected:
		TSet<int32> TempVertexSet;
		TSet<FGraphSignedID> TempVisitedEdges;
		TSet<FGraphSignedID> TempAllowedPerimeterEdges;

		// Update the cached perimeter data from the polygon's definitional data; returns whether it was able to complete, if necessary.
		bool CalculatePerimeter();
	};
}
