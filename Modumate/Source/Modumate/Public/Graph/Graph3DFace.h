// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "ModumateGraph3DTypes.h"

namespace Modumate
{
	class FGraph3DFace : public IGraph3DObject
	{
	public:
		TArray<int32> VertexIDs;						// The IDs of vertices that define this face's polygon's vertices
		TArray<FSignedID> EdgeIDs;						// The IDs of edges that define this face's polygon's edges
		int32 FrontPolyhedronID = MOD_ID_NONE;			// The ID of the polyhedron that contains the front of this face, if any
		int32 BackPolyhedronID = MOD_ID_NONE;			// The ID of the polyhedron that contains the back of this face, if any

		TArray<FVector> CachedPositions;				// The positions of this face's vertices
		TArray<FVector> CachedEdgeNormals;				// The normals of this face's edges, pointing inward
		FPlane CachedPlane = FPlane(ForceInitToZero);	// The plane of this face; its normal assumes the points are defined in clockwise winding
		FVector Cached2DX = FVector::ZeroVector;		// The cached basis normal vectors for 2D projections
		FVector Cached2DY = FVector::ZeroVector;
		FVector CachedCenter = FVector::ZeroVector;		// The central position to use for casting rays from the plane; intended to lie inside the polygon
		TArray<FVector2D> Cached2DPositions;			// The positions of this face's vertices, projected in 2D

		FGraph3DFace(int32 InID, FGraph3D* InGraph, const TArray<FVector> &VertexPositions);
		FGraph3DFace(int32 InID, FGraph3D* InGraph, const TArray<int32> &InVertexIDs);

		FVector2D ProjectPosition2D(const FVector &Position) const;
		FVector DeprojectPosition(const FVector2D &ProjectedPos) const;
		bool SetVertices(const TArray<int32> &InVertexIDs);
		bool NormalHitsFace(const FGraph3DFace *OtherFace, float Sign) const;
		bool IntersectsFace(const FGraph3DFace *OtherFace, FVector &IntersectingEdgeOrigin, FVector &IntersectingEdgeDir, TArray<FEdgeIntersection> &SourceIntersections, TArray<FEdgeIntersection> &DestIntersections, TPair<bool, bool> &bOutOnFaceEdge) const;
		bool IntersectsPlane(const FPlane OtherPlane, FVector &IntersectingEdgeOrigin, FVector &IntersectingEdgeDir, TArray<TPair<FVector, FVector>> &IntersectingSegments) const;

		// TODO: potentially make this static, and also cache this value during SetVertices
		float CalculateArea();

		void GetAdjacentFaceIDs(TSet<int32>& adjFaceIDs) const;

		const bool FindVertex(FVector &Position) const;
		const int32 FindEdgeIndex(FSignedID edgeID, bool &bOutSameDirection) const;
		const bool SplitFace(TArray<int32> EdgeIdxs, TArray<int32> NewVertexIDs, TArray<TArray<int32>>& OutVertexIDs) const;

		virtual void Dirty(bool bConnected = true) override;
		virtual bool ValidateForTests() const override;
	};
}
