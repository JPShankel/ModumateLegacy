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
		int32 ContainingPolyID = MOD_ID_NONE;			// The ID of the polygon that contains this one, if any
		TArray<int32> ContainedPolyIDs;					// The IDs of polygons that this polygon contains
		bool bHasDuplicateVertex = false;				// Whether this polygon has any vertices that repeat (peninsulas/islands)
		bool bInterior = false;							// Whether this is an interior (simple, solid) polygon; otherwise it is a perimeter
		FBox2D AABB = FBox2D(ForceInitToZero);			// The axis-aligned bounding box for the polygon
		TArray<FVector2D> CachedPoints;					// The list of vertex positions in this polygon
		TArray<int32> CachedPerimeterVertexIDs;			// For interior polygons, the IDs of vertices that make up the perimeter traversal
		TArray<FGraphSignedID> CachedPerimeterEdgeIDs;	// For interior polygons, the directed IDs of edges that make up the perimeter traversal
		TArray<FVector2D> CachedPerimeterPoints;		// For interior polygons, the positions of vertices that make up the perimeter traversal

		FGraph2DPolygon();
		FGraph2DPolygon(int32 InID, TWeakPtr<FGraph2D> InGraph);
		FGraph2DPolygon(int32 InID, TWeakPtr<FGraph2D> InGraph, const TArray<int32>& InVertices);

		bool IsInside(int32 OtherPolyID) const;
		void SetContainingPoly(int32 NewContainingPolyID);

		void SetVertices(const TArray<int32> &Vertices);

		// Double-check that the polygon's definition matches the minimal traversal in the graph.
		bool CalculateTraversal(bool& bOutCalculatedInterior);

		virtual void Dirty(bool bConnected = true) override;
		virtual bool Clean() override;

		virtual EGraphObjectType GetType() const override { return EGraphObjectType::Polygon; };
		virtual void GetVertexIDs(TArray<int32> &OutVertexIDs) const override;

	protected:
		static TSet<int32> TempVertexSet;
		static TSet<FGraphSignedID> TempVisitedEdges;

		// Update the cached perimeter data from the polygon's definitional data; returns whether it was able to complete, if necessary.
		bool CalculatePerimeter();
	};
}
