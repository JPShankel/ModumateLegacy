// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Graph/Graph2DTypes.h"

#pragma once

namespace Modumate
{
	class FGraph2DPolygon : public IGraph2DObject
	{
	public:
		int32 ParentID = MOD_ID_NONE;				// The ID of the polygon that contains this one, if any
		TArray<int32> InteriorPolygons;			// The IDs of polygons that this polygon contains
		TArray<FEdgeID> Edges;					// The list of edges that make up this polygon

		// Cached derived data
		bool bHasDuplicateEdge = false;			// Whether this polygon has any edges that double back on themselves
		bool bInterior = false;					// Whether this is an interior (simple, solid) polygon; otherwise it is a perimeter
		FBox2D AABB = FBox2D(ForceInitToZero);	// The axis-aligned bounding box for the polygon
		TArray<FVector2D> Points;				// The list of vertex positions in this polygon

		FGraph2DPolygon(int32 InID, FGraph2D* InGraph) : IGraph2DObject(InID, InGraph) { }
		FGraph2DPolygon() : IGraph2DObject(MOD_ID_NONE, nullptr) { };

		bool IsInside(const FGraph2DPolygon &otherPoly) const;
		void SetParent(int32 inParentID);

		virtual void Dirty(bool bConnected = true) override {};

		virtual EGraphObjectType GetType() const override { return EGraphObjectType::Polygon; };
	};
}
