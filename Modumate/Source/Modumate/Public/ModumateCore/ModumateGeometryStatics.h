// Copyright 2019 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ProceduralMeshComponent.h"
#include "Database/ModumateSimpleMesh.h"
#include "ModumateCore/ModumateTypes.h"
#include "Graph/Graph3DTypes.h"

#include "ModumateGeometryStatics.generated.h"


#define RAY_INTERSECT_TOLERANCE	(0.2f)
#define PLANAR_DOT_EPSILON	(0.1f)

USTRUCT()
struct MODUMATE_API FPolyHoleIndices
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY()
	TArray<int32> Indices;
};

USTRUCT()
struct MODUMATE_API FPolyHole2D
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY()
	TArray<FVector2D> Points;

	FPolyHole2D() { }
	FPolyHole2D(const TArray<FVector2D> &InPoints) : Points(InPoints) { }
};

USTRUCT()
struct MODUMATE_API FPolyHole3D
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY()
	TArray<FVector> Points;

	FPolyHole3D() { }
	FPolyHole3D(const TArray<FVector> &InPoints) : Points(InPoints) { }
};

// Define a struct that can fully define the geometry necessary to triangulate a layer in a layered assembly.
USTRUCT()
struct MODUMATE_API FLayerGeomDef
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY()
	TArray<FVector> PointsA;

	UPROPERTY()
	TArray<FVector> PointsB;

	UPROPERTY()
	TArray<FPolyHole3D> Holes3D;

	UPROPERTY()
	float Thickness = 0.0f;

	UPROPERTY()
	FVector Normal = FVector::ZeroVector;

	UPROPERTY()
	FVector Origin = FVector::ZeroVector;

	UPROPERTY()
	FVector AxisX = FVector::ZeroVector;

	UPROPERTY()
	FVector AxisY = FVector::ZeroVector;

	UPROPERTY()
	bool bValid = false;

	UPROPERTY()
	bool bCoincident = false;

	UPROPERTY()
	TArray<FVector2D> CachedPointsA2D;

	UPROPERTY()
	TArray<FVector2D> CachedPointsB2D;

	UPROPERTY()
	TArray<FPolyHole2D> CachedHoles2D;

	UPROPERTY()
	TArray<FPolyHole2D> ValidHoles2D;

	UPROPERTY()
	TArray<bool> HolesValid;

	mutable TArray<FVector> TempSidePoints;

	FLayerGeomDef();

	// A simplified constructor, for non-mitered, hole-less layers
	FLayerGeomDef(const TArray<FVector>& Points, float InThickness, const FVector& InNormal);

	FLayerGeomDef(const TArray<FVector>& InPointsA, const TArray<FVector>& InPointsB, const FVector& InNormal,
		const FVector& InAxisX = FVector::ZeroVector, const TArray<FPolyHole3D> *InHoles = nullptr);

	void Init(const TArray<FVector>& InPointsA, const TArray<FVector>& InPointsB, const FVector& InNormal,
		const FVector& InAxisX = FVector::ZeroVector, const TArray<FPolyHole3D> *InHoles = nullptr);
	FVector2D ProjectPoint2D(const FVector& Point3D) const;
	FVector2D ProjectPoint2DSnapped(const FVector& Point3D, float Tolerance = KINDA_SMALL_NUMBER) const;
	FVector Deproject2DPoint(const FVector2D& Point2D, bool bSideA) const;
	bool CachePoints2D();

	static void AppendTriangles(const TArray<FVector>& Verts, const TArray<int32>& SourceTriIndices,
		TArray<int32>& OutTris, bool bReverseTris);
	bool TriangulateSideFace(const FVector2D& Point2DA1, const FVector2D& Point2DA2,
		const FVector2D& Point2DB1, const FVector2D& Point2DB2, bool bReverseTris,
		TArray<FVector>& OutVerts, TArray<int32>& OutTris,
		TArray<FVector>& OutNormals, TArray<FVector2D>& OutUVs, TArray<FProcMeshTangent>& OutTangents,
		const FVector2D& UVScale, const FVector& UVAnchor = FVector::ZeroVector, float UVRotOffset = 0.0f) const;
	bool TriangulateMesh(TArray<FVector>& OutVerts, TArray<int32>& OutTris,
		TArray<FVector>& OutNormals, TArray<FVector2D>& OutUVs, TArray<FProcMeshTangent>& OutTangents,
		const FVector& UVAnchor = FVector::ZeroVector, float UVRotOffset = 0.0f) const;
};

USTRUCT()
struct FPointInPolyResult
{
	GENERATED_BODY()

	UPROPERTY()
	bool bInside;

	UPROPERTY()
	bool bOverlaps;

	UPROPERTY()
	int32 StartVertexIdx;

	UPROPERTY()
	int32 EndVertexIdx;

	UPROPERTY()
	float EdgeDistance;

	FPointInPolyResult();
	void Reset();
};

// Helper functions for geometric operations; anything that doesn't need to know about Modumate-specific object types
UCLASS()
class MODUMATE_API UModumateGeometryStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(Category = "Modumate | Geometry")
	static FVector2D ProjectPoint2D(const FVector &Point3D, const FVector &AxisX, const FVector &AxisY, const FVector &Origin);

	UFUNCTION(Category = "Modumate | Geometry")
	static FVector2D ProjectPoint2DTransform(const FVector &Point3D, const FTransform& Origin);

	UFUNCTION(Category = "Modumate | Geometry")
	static FVector Deproject2DPoint(const FVector2D &Point2D, const FVector &AxisX, const FVector &AxisY, const FVector &Origin);

	UFUNCTION(Category = "Modumate | Geometry")
	static FVector Deproject2DPointTransform(const FVector2D &Point2D, const FTransform& Origin);

	UFUNCTION(Category = "Modumate | Geometry")
	static FVector2D ProjectVector2D(const FVector &Vector3D, const FVector &AxisX, const FVector &AxisY);

	UFUNCTION(Category = "Modumate | Geometry")
	static bool FindPointFurthestFromPolyEdge(const TArray<FVector2D> &polygon, FVector2D &furthestPoint);

	UFUNCTION(Category = "Modumate | Geometry")
	static void GetPolygonWindingAndConcavity(const TArray<FVector2D> &Locations, bool &bOutClockwise, bool &bOutConcave);

	UFUNCTION(Category = "Modumate | Geometry")
	static bool GetPlaneFromPoints(const TArray<FVector> &Points, FPlane &outPlane, float Tolerance = PLANAR_DOT_EPSILON);

	/* Triangulation helper function, using GeometryAlgorithms / Geometric Tools Engine
	 * @param[in] Vertices: the simple polygon vertices to triangulate; concave/convex or CW/CCW are okay, but no self-intersections or repeating points
	 * @param[in] Holes: the array of holes that should be bored in the polygon; can't overlap with each other, or intersect the polygon at more than 1 point
	 * @param[out] OutTriangles: the triplets of indices of vertices that represent the triangulation of the input polygon
	 * @param[out, optional] OutCombinedVertices: an optional convenience list that combines Vertices and the points in Holes together, as referenced by the indices in OutTriangles
	 * @param[in, optional] bCheckValid: whether to perform validity and intersection checks on the input polygon and holes
	 * @result: whether we could triangulate the polygon at all; fails on invalid input polygon/holes.
	 */
	static bool TriangulateVerticesGTE(const TArray<FVector2D>& Vertices, const TArray<FPolyHole2D>& Holes,
		TArray<int32>& OutTriangles, TArray<FVector2D>* OutCombinedVertices, bool bCheckValid = true);

	UFUNCTION(Category = "Modumate | Geometry")
	static bool SegmentIntersection2D(const FVector2D& SegmentStartA, const FVector2D& SegmentEndA, const FVector2D& SegmentStartB, const FVector2D& SegmentEndB, FVector2D& OutIntersectionPoint, bool& bOutOverlapping, float Tolerance = RAY_INTERSECT_TOLERANCE);

	UFUNCTION(Category = "Modumate | Geometry")
	static bool RayIntersection2D(const FVector2D& RayOriginA, const FVector2D& RayDirectionA, const FVector2D& RayOriginB, const FVector2D& RayDirectionB,
		FVector2D& OutIntersectionPoint, float &OutRayADist, float &OutRayBDist, bool bRequirePositive = true, float Tolerance = RAY_INTERSECT_TOLERANCE);

	// Low-level helper function: for three rays originating at Origin, test if a ray is between (or overlaps with) the other two rays, given their bounding normals.
	// Does not validate normals, because they are generated by the higher-level functions calling this helper.
	// One use is to test whether a ray originating at a corner of a polygon points inside or outside the polygon;
	//   in that case, the directions of the edges relative to the shared vertex are RayDirA and RayDirB,
	//   RayNormalA and RayNormalB would be the internal normals of those edges that point inside the polygon,
	//   and TestRay would be the ray that is being tested for whether it points inside or outside the polygon, or if it overlaps with one of the edges.
	static bool IsRayBoundedByRays(const FVector2D& RayDirA, const FVector2D& RayDirB, const FVector2D& RayNormalA, const FVector2D& RayNormalB,
		const FVector2D& TestRay, bool& bOutOverlaps);

	static bool GetPolyEdgeInfo(const TArray<FVector2D>& Polygon, bool bPolyCW, int32 EdgeStartIdx,
		FVector2D& OutStartPoint, FVector2D& OutEndPoint, float& OutEdgeLength, FVector2D& OutEdgeDir, FVector2D& OutEdgeNormal,
		float Epsilon = THRESH_POINTS_ARE_SAME);

	/* Check whether the given point is inside of or overlaps with the given polygon.
	 * 
	 * @param[in] Point: the point to test for containment
	 * @param[in] Polygon: the polygon to test; it may be concave/convex or CW/CCW, but no repeated points or self-intersections (it's a perimeter, no peninsulas)
	 * @param[out] OutResult: whether the point is strictly inside the polygon, and if not, whether the point overlaps with the polygon (and if so, which vertex/edge it overlaps with and where)
	 * @param[in, optional] Tolerance: the epsilon value to use for comparing equal points and overlaps
	 * @result: whether the test could be completed successfully and the inputs are valid
	 */
	static bool TestPointInPolygon(const FVector2D& Point, const TArray<FVector2D>& Polygon, FPointInPolyResult& OutResult, float Tolerance = RAY_INTERSECT_TOLERANCE);

	// Test if a simple polygon contains a (simple or non-simple) polygon; either fully (no touching vertices/edges) or partially (overlapping edges and/or touching vertices).
	// NOTE: this requires that the polygons have edges that are already fully split, and do not intersect with one another except at vertices, and no vertices lie inside other edges.
	// [param] bOutFullyContained: All vertices and edges of ContainedPolygon are inside of ContainingPolygon, and do not touch.
	// [param] bOutPartiallyContained: All vertices and edges of ContainedPolygon are either inside of ContainingPolygon, or overlap with vertices and edges of ContainingPolygon, AND
	//                                 at least one vertex and/or edge of ContainingPolygon is outside of ContainedPolygon, so they can not be identical.
	// If neither are true, then the faces may or may not overlap geometrically; we could add out parameters to detect this if needed.
	static bool GetPolygonContainment(const TArray<FVector2D> &ContainingPolygon, const TArray<FVector2D> &ContainedPolygon, bool& bOutFullyContained, bool& bOutPartiallyContained);

	// The same as GetPolygonContainment, except we first check for edge intersections between polygons and will early-exit if they intersect.
	static bool GetPolygonIntersection(const TArray<FVector2D> &ContainingPolygon, const TArray<FVector2D> &ContainedPolygon, bool& bOverlapping, bool& bOutFullyContained, bool& bOutPartiallyContained);

	UFUNCTION(Category = "Modumate | Geometry")
	static bool RayIntersection3D(const FVector& RayOriginA, const FVector& RayDirectionA, const FVector& RayOriginB, const FVector& RayDirectionB,
		FVector& OutIntersectionPoint, float &OutRayADist, float &OutRayBDist, bool bRequirePositive = true, float IntersectionTolerance = RAY_INTERSECT_TOLERANCE, float RayNormalTolerance = KINDA_SMALL_NUMBER);

	UFUNCTION(Category = "Modumate | Geometry")
	static bool TranslatePolygonEdge(const TArray<FVector> &PolyPoints, const FVector &PolyNormal, int32 EdgeStartIdx, float Translation, FVector &OutStartPoint, FVector &OutEndPoint);

	UFUNCTION(Category = "Modumate | Geometry")
	static void FindBasisVectors(FVector &OutAxisX, FVector &OutAxisY, const FVector &AxisZ);

	UFUNCTION(Category = "Modumate | Geometry")
	static bool FindShortestDistanceBetweenRays(const FVector &RayOriginA, const FVector &RayDirectionA, const FVector &RayOriginB, const FVector &RayDirectionB, FVector &OutRayInterceptA, FVector &OutRayInterceptB, float &outDistance);

	UFUNCTION(Category = "Modumate | Geometry")
	static bool GetExtendedCorner(FVector &RefCorner, const FVector &PrevPoint, const FVector &NextPoint,
		const FVector &PrevEdgeNormal, const FVector &NextEdgeNormal, float PrevEdgeExtension, float NextEdgeExtension);

	UFUNCTION(Category = "Modumate | Geometry")
	static bool CompareVectors(const TArray<FVector2D> &vectorsA, const TArray<FVector2D> &vectorsB, float tolerance = KINDA_SMALL_NUMBER);

	UFUNCTION(Category = "Modumate | Geometry")
	static bool AnalyzeCachedPositions(const TArray<FVector> &InPositions, FPlane &OutPlane, FVector &OutAxis2DX, FVector &OutAxis2DY, TArray<FVector2D> &Out2DPositions, FVector &OutCenter, bool bUpdatePlane = true);

	static bool ConvertProcMeshToLinesOnPlane(UProceduralMeshComponent* InProcMesh, FVector PlanePosition, FVector PlaneNormal, TArray<TPair<FVector, FVector>> &OutEdges);
	static bool ConvertStaticMeshToLinesOnPlane(UStaticMeshComponent* InStaticMesh, FVector PlanePosition, FVector PlaneNormal, TArray<TPair<FVector, FVector>> &OutEdges);

	static bool GetAxisForVector(const FVector &Normal, EAxis::Type &OutAxis, float &OutSign);

	static bool GetEdgeIntersections(const TArray<FVector> &Positions, const FVector &IntersectionOrigin, const FVector &IntersectionDir, TArray<Modumate::FEdgeIntersection> &OutEdgeIntersections, float Epsilon = DEFAULT_GRAPH3D_EPSILON);

	static bool AreConsecutivePoints2DRepeated(const TArray<FVector2D> &Points, float Tolerance = KINDA_SMALL_NUMBER);
	static bool AreConsecutivePointsRepeated(const TArray<FVector> &Points, float Tolerance = KINDA_SMALL_NUMBER);

	static bool IsPolygon2DValid(const TArray<FVector2D> &Points2D, class FFeedbackContext* InWarn = nullptr);
	static bool IsPolygonValid(const TArray<FVector> &Points, FPlane PolyPlane = FPlane(ForceInitToZero), class FFeedbackContext* InWarn = nullptr);

	static bool IsLineSegmentWithin2D(const FEdge& OuterLine, const FEdge& InnerLine, float epsilon = THRESH_POINTS_ARE_NEAR);
	static bool IsLineSegmentWithin2D(const FVector2D& OuterLineStart, const FVector2D& OuterLineEnd,
		const FVector2D& InnerLineStart, const FVector2D& InnerLineEnd, float epsilon = THRESH_POINTS_ARE_NEAR);

	static bool IsLineSegmentBoundedByPoints2D(const FVector2D &StartPosition, const FVector2D &EndPosition, const TArray<FVector2D> &Positions, const TArray<FVector2D> &BoundingNormals, float Epsilon = THRESH_POINTS_ARE_NEAR);

	// Compare that the given planes lie in the same plane, even if they have opposite normals.
	static bool ArePlanesCoplanar(const FPlane& PlaneA, const FPlane& PlaneB, float Epsilon = THRESH_POINTS_ARE_NEAR);

	static void GetPlaneIntersections(TArray<FVector>& OutIntersections, const TArray<FVector>& InPoints, const FPlane& InPlane, const FVector InLocation = FVector::ZeroVector);

	// Divide Intersection by any projected layer-holes; return as parametric ranges.
	static void GetRangesForHolesOnPlane(TArray<TPair<float, float>>& OutRanges, TPair<FVector, FVector>& Intersection, const FLayerGeomDef& Layer, const FVector& HoleOffset, const FPlane& Plane, const FVector& AxisX, const FVector& AxisY, const FVector& Origin);

	// Sort helper for points that lie on an arbitrary line.
	static bool Points3dSorter(const FVector& rhs, const FVector& lhs);
};
