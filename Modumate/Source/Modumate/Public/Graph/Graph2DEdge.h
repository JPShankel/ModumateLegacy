// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Graph/Graph2DTypes.h"

#pragma once

class FGraph2DEdge : public IGraph2DObject
{
public:
	// Definitional data
	int32 StartVertexID = MOD_ID_NONE;					// The ID of the vertex that the start of this edge is connected to
	int32 EndVertexID = MOD_ID_NONE;					// The ID of the vertex that the end of this edge is connected to

	// Derived data
	int32 LeftPolyID = MOD_ID_NONE;						// The ID of the polygon that the left side of this edge is a part of
	int32 RightPolyID = MOD_ID_NONE;					// The ID of the polygon that the right side of this edge is a part of
	float CachedAngle = 0.0f;							// The angle of the line from start to end vertex
	FVector2D CachedEdgeDir = FVector2D::ZeroVector;	// The direction of the line from start to end vertex

	bool bPolygonDirty = false;							// An extra dirty flag for this edge to be used by the graph to re-calculate polygons

	FGraph2DEdge(int32 InID, TWeakPtr<FGraph2D> InGraph, int32 InStart, int32 InEnd);
	void SetVertices(int32 InStart, int32 InEnd);
	bool GetAdjacentInteriorPolygons(const class FGraph2DPolygon*& OutPolyLeft, const class FGraph2DPolygon*& OutPolyRight) const;

	virtual void Dirty(bool bConnected = true) override;
	virtual bool Clean() override;

	virtual EGraphObjectType GetType() const override { return EGraphObjectType::Edge; };
	virtual void GetVertexIDs(TArray<int32> &OutVertexIDs) const override;

protected:
	bool CacheAngle();
};

