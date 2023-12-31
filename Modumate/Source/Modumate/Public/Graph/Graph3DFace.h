// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "Graph/Graph2D.h"
#include "Graph/Graph3DTypes.h"
#include "ModumateCore/ModumateGeometryStatics.h"

class MODUMATE_API FGraph3DFace : public IGraph3DObject
{
public:
	// Definitional data
	TArray<int32> VertexIDs;						// The IDs of vertices that define this face's polygon's vertices
	int32 ContainingFaceID = MOD_ID_NONE;			// The ID of the face that contains this one, if any
	TSet<int32> ContainedFaceIDs;					// The IDs of faces that are fully contained within this face, such as holes

	// Derived data
	TArray<FGraphSignedID> EdgeIDs;					// The IDs of edges that define this face's polygon's edges
	int32 FrontPolyhedronID = MOD_ID_NONE;			// The ID of the polyhedron that contains the front of this face, if any
	int32 BackPolyhedronID = MOD_ID_NONE;			// The ID of the polyhedron that contains the back of this face, if any

	TArray<FVector> CachedPositions;				// The positions of this face's vertices
	TArray<FVector> CachedEdgeNormals;				// The normals of this face's edges, pointing inward
	FPlane CachedPlane = FPlane(ForceInitToZero);	// The plane of this face; its normal assumes the points are defined in clockwise winding
	FVector Cached2DX = FVector::ZeroVector;		// The cached basis normal vectors for 2D projections
	FVector Cached2DY = FVector::ZeroVector;
	FVector CachedCenter = FVector::ZeroVector;		// The central position to use for casting rays from the plane; intended to lie inside the polygon
	TArray<FPolyHole3D> CachedHoles;				// The cached positions of all holes, from both islands and contained faces

	TArray<FVector2D> Cached2DPositions;			// CachedPositions, projected in the face's 2D coordinate space
	TArray<FVector2D> Cached2DEdgeNormals;			// CachedEdgeNormals, projected in the face's 2D coordinate space
	TArray<FPolyHole2D> Cached2DHoles;				// CachedHoles, projected in the face's 2D coordinate space

	FGraph3DFace(int32 InID, FGraph3D* InGraph, const TArray<int32> &InVertexIDs, int32 InContainingFaceID, const TSet<int32> &InContainedFaceIDs);

	// Default constructor used by MetaPlaneSpan to define perimeter face for mitering
	FGraph3DFace();

	FVector2D ProjectPosition2D(const FVector &Position) const;
	FVector DeprojectPosition(const FVector2D &ProjectedPos) const;
	bool ContainsPosition(const FVector &Position, bool& bOutOverlaps) const;
	bool UpdatePlane(const TArray<int32> &InVertexIDs);
	bool ShouldUpdatePlane() const;
	bool UpdateVerticesAndEdges(const TArray<int32> &InVertexIDs, bool bAssignVertices = true);
	bool NormalHitsFace(const FGraph3DFace *OtherFace, float Sign) const;
	bool IntersectsFace(const FGraph3DFace *OtherFace, TArray<TPair<FVector, FVector>> &OutEdges) const;
	bool IntersectsPlane(const FPlane OtherPlane, FVector &IntersectingEdgeOrigin, FVector &IntersectingEdgeDir, TArray<TPair<FVector, FVector>> &IntersectingSegments) const;

	bool UpdateHoles();
	void UpdateContainingFace(int32 NewContainingFaceID);

	void GetAdjacentFaceIDs(TSet<int32>& adjFaceIDs) const;

	int32 FindVertexID(const FVector& Position) const;
	int32 FindEdgeIndex(FGraphSignedID edgeID, bool &bOutSameDirection) const;

	virtual void Dirty(bool bConnected = true) override;
	virtual bool ValidateForTests() const override;
	virtual EGraph3DObjectType GetType() const override { return EGraph3DObjectType::Face; }
	virtual void GetVertexIDs(TArray<int32>& OutVertexIDs) const override;

	double CalculateArea() const;

private:
	bool AssignVertices(const TArray<int32> &InVertexIDs);
};

