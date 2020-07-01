// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "Graph/ModumateGraph3DTypes.h"
#include "ModumateCore/ModumateGeometryStatics.h"

namespace Modumate
{
	class FGraph3DFace : public IGraph3DObject
	{
	public:
		TArray<int32> VertexIDs;						// The IDs of vertices that define this face's polygon's vertices
		TArray<FSignedID> EdgeIDs;						// The IDs of edges that define this face's polygon's edges
		int32 FrontPolyhedronID = MOD_ID_NONE;			// The ID of the polyhedron that contains the front of this face, if any
		int32 BackPolyhedronID = MOD_ID_NONE;			// The ID of the polyhedron that contains the back of this face, if any
		int32 ContainingFaceID = MOD_ID_NONE;			// The ID of the face that contains this one, if any
		TSet<int32> ContainedFaceIDs;					// The IDs of faces that are fully contained within this face, such as holes

		TArray<FVector> CachedPositions;				// The positions of this face's vertices
		TArray<FVector> CachedEdgeNormals;				// The normals of this face's edges, pointing inward
		FPlane CachedPlane = FPlane(ForceInitToZero);	// The plane of this face; its normal assumes the points are defined in clockwise winding
		FVector Cached2DX = FVector::ZeroVector;		// The cached basis normal vectors for 2D projections
		FVector Cached2DY = FVector::ZeroVector;
		FVector CachedCenter = FVector::ZeroVector;		// The central position to use for casting rays from the plane; intended to lie inside the polygon
		TArray<FVector2D> Cached2DPositions;			// The positions of this face's vertices, projected in 2D

		TArray<FVector2D> CachedPerimeter;
		TArray<FPolyHole2D> CachedHoles;
		TArray<FPolyHole3D> CachedHoles3D;
		float CachedArea;

		FGraph3DFace(int32 InID, FGraph3D* InGraph, const TArray<int32> &InVertexIDs,
			const TSet<int32> &InGroupIDs, int32 InContainingFaceID, const TSet<int32> &InContainedFaceIDs);

		FVector2D ProjectPosition2D(const FVector &Position) const;
		FVector DeprojectPosition(const FVector2D &ProjectedPos) const;
		bool ContainsPosition(const FVector &Position) const;
		bool UpdatePlane(const TArray<int32> &InVertexIDs);
		bool ShouldUpdatePlane() const;
		bool UpdateVerticesAndEdges(const TArray<int32> &InVertexIDs, bool bAssignVertices = true);
		bool NormalHitsFace(const FGraph3DFace *OtherFace, float Sign) const;
		bool IntersectsFace(const FGraph3DFace *OtherFace, TArray<TPair<FVector, FVector>> &OutEdges) const;
		bool IntersectsPlane(const FPlane OtherPlane, FVector &IntersectingEdgeOrigin, FVector &IntersectingEdgeDir, TArray<TPair<FVector, FVector>> &IntersectingSegments) const;

		void UpdateHoles();
		// TODO: potentially make this static
		bool CalculateArea();

		void GetAdjacentFaceIDs(TSet<int32>& adjFaceIDs) const;

		const bool FindVertex(FVector &Position) const;
		const int32 FindEdgeIndex(FSignedID edgeID, bool &bOutSameDirection) const;

		virtual void Dirty(bool bConnected = true) override;
		virtual bool ValidateForTests() const override;
		virtual EGraph3DObjectType GetType() const override { return EGraph3DObjectType::Face; }

	private:
		bool AssignVertices(const TArray<int32> &InVertexIDs);
	};
}
