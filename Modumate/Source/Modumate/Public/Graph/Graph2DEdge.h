// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Graph/Graph2DTypes.h"

#pragma once

namespace Modumate
{
	class FGraph2DEdge : public IGraph2DObject
	{
	public:
		int32 StartVertexID = MOD_ID_NONE;				// The ID of the vertex that the start of this edge is connected to
		int32 EndVertexID = MOD_ID_NONE;				// The ID of the vertex that the end of this edge is connected to
		int32 LeftPolyID = MOD_ID_NONE;					// The ID of the polygon that the left side of this edge is a part of
		int32 RightPolyID = MOD_ID_NONE;				// The ID of the polygon that the right side of this edge is a part of
		float Angle = 0.0f;							// The angle of the line from start to end vertex
		FVector2D EdgeDir = FVector2D::ZeroVector;	// The direction of the line from start to end vertex

		FGraph2DEdge(int32 InID, FGraph2D* InGraph, int32 InStart, int32 InEnd);
		void SetVertices(int32 InStart, int32 InEnd);
		bool CacheAngle();

		virtual void Dirty(bool bConnected = true) override;
		virtual void Clean() override;

		virtual EGraphObjectType GetType() const override { return EGraphObjectType::Edge; };
	};
}
