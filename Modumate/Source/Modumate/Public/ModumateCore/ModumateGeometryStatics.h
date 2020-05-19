// Copyright 2019 Modumate, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ProceduralMeshComponent.h"
#include "Database/ModumateSimpleMesh.h"
#include "ModumateCore/ModumateTypes.h"
#include "Graph/ModumateGraph3DTypes.h"

#include "ModumateGeometryStatics.generated.h"


#define RAY_INTERSECT_TOLERANCE	(1.e-2f)
#define PLANAR_DOT_EPSILON	(1.e-3f)

USTRUCT()
struct MODUMATE_API FTessellationPolygon
{
	GENERATED_USTRUCT_BODY();

	FTessellationPolygon() {};

	FTessellationPolygon(const FVector &InBaseUp, const FVector &InPolyNormal,
		const FVector &InStartPoint, const FVector &InStartDir,
		const FVector &InEndPoint, const FVector &InEndDir);

	FVector BaseUp, PolyNormal, StartPoint, StartDir, EndPoint, EndDir, CachedEdgeDir, CachedEdgeIntersection;

	FPlane CachedPlane;
	float CachedStartRayDist, CachedEndRayDist;
	bool bCachedEndsConverge;
	TArray<FVector> ClippingPoints, PolygonVerts;

	bool ClipWithPolygon(const FTessellationPolygon &ClippingPolygon);
	bool UpdatePolygonVerts();

	void DrawDebug(const FColor &Color, class UWorld* World = nullptr, const FTransform &Transform = FTransform::Identity);
};

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

	FLayerGeomDef() { }

	// A simplified constructor, for non-mitered, hole-less layers
	FLayerGeomDef(const TArray<FVector> &Points, float InThickness, const FVector &InNormal);

	FLayerGeomDef(const TArray<FVector> &InPointsA, const TArray<FVector> &InPointsB, const FVector &InNormal,
		const FVector &InAxisX = FVector::ZeroVector, const TArray<FPolyHole3D> *InHoles = nullptr);

	void Init(const TArray<FVector> &InPointsA, const TArray<FVector> &InPointsB, const FVector &InNormal,
		const FVector &InAxisX = FVector::ZeroVector, const TArray<FPolyHole3D> *InHoles = nullptr);
	FVector2D ProjectPoint2D(const FVector &Point3D) const;
	FVector ProjectPoint3D(const FVector2D &Point2D, bool bSideA) const;
	void CachePoints2D();

	static void AppendTriangles(const TArray<FVector> &Verts, const TArray<int32> &SourceTriIndices,
		TArray<int32> &OutTris, bool bReverseTris);
	bool TriangulateSideFace(const FVector2D &Point2DA1, const FVector2D &Point2DA2,
		const FVector2D &Point2DB1, const FVector2D &Point2DB2, bool bReverseTris,
		TArray<FVector> &OutVerts, TArray<int32> &OutTris,
		TArray<FVector> &OutNormals, TArray<FVector2D> &OutUVs, TArray<FProcMeshTangent> &OutTangents,
		const FVector2D &UVScale, const FVector &UVAnchor = FVector::ZeroVector, float UVRotOffset = 0.0f) const;
	bool TriangulateMesh(TArray<FVector> &OutVerts, TArray<int32> &OutTris,
		TArray<FVector> &OutNormals, TArray<FVector2D> &OutUVs, TArray<FProcMeshTangent> &OutTangents,
		const FVector &UVAnchor = FVector::ZeroVector, float UVRotOffset = 0.0f) const;
};

// Helper functions for geometric operations; anything that doesn't need to know about Modumate-specific object types,
// and/or anything that needs to use Poly2Tri for underlying work.
UCLASS()
class MODUMATE_API UModumateGeometryStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(Category = "Modumate | Geometry")
	static FVector2D ProjectPoint2D(const FVector &Point3D, const FVector &AxisX, const FVector &AxisY, const FVector &Origin);

	UFUNCTION(Category = "Modumate | Geometry")
	static FVector2D ProjectVector2D(const FVector &Vector3D, const FVector &AxisX, const FVector &AxisY);

	UFUNCTION(Category = "Modumate | Geometry")
	static bool FindPointFurthestFromPolyEdge(const TArray<FVector2D> &polygon, FVector2D &furthestPoint);

	UFUNCTION(Category = "Modumate | Geometry")
	static bool AreLocationsClockwise2D(const TArray<FVector2D> &Locations);

	UFUNCTION(Category = "Modumate | Geometry")
	static bool GetPlaneFromPoints(const TArray<FVector> &Points, FPlane &outPlane, float Tolerance = PLANAR_DOT_EPSILON);

	/* Poly2Tri version of triangulation.
	 * @param[in] Vertices: the polygon vertices to triangulate, either in CW or CCW, but must not repeat
	 * @param[in] Holes: the array of 0-n holes that should be bored in the polygon (must not overlap with each other)
	 * @param[out] OutVertices: the vertices that are to be used for triangulation, in the order referenced by OutTriangles
	 * @param[out] OutTriangles: the triplets of indices of OutVertices that represent the triangulation of the input polygon
	 * @param[out] OutPerimeter: the vertices that represent the perimeter of the polygon, which includes holes that were merged by the input polygon
	 * @param[out] OutMergedHoles: the array (same size as Holes) that indicates whether or not a hole was merged by the polygon into the perimeter
	 * @param[out] OutPerimeterVertexHoleIndices: for each point in OutPerimeter, the index of the hole that created the point, otherwise INDEX_NONE if it came from the original polygon
	 * @result: whether we could triangulate the polygon at all; fails on invalid input polygon/holes, or expected poly2tri failures.
	 */
	UFUNCTION(Category = "Modumate | Geometry")
	static bool TriangulateVerticesPoly2Tri(const TArray<FVector2D> &Vertices, const TArray<FPolyHole2D> &Holes,
		TArray<FVector2D> &OutVertices, TArray<int32> &OutTriangles, TArray<FVector2D> &OutPerimeter, TArray<bool> &OutMergedHoles,
		TArray<int32> &OutPerimeterVertexHoleIndices);

	UFUNCTION(Category = "Modumate | Geometry")
	static bool SegmentIntersection2D(const FVector2D& SegmentStartA, const FVector2D& SegmentEndA, const FVector2D& SegmentStartB, const FVector2D& SegmentEndB, FVector2D& outIntersectionPoint, float Tolerance = SMALL_NUMBER);

	UFUNCTION(Category = "Modumate | Geometry")
	static bool RayIntersection2D(const FVector2D& RayOriginA, const FVector2D& RayDirectionA, const FVector2D& RayOriginB, const FVector2D& RayDirectionB,
		FVector2D& OutIntersectionPoint, float &OutRayADist, float &OutRayBDist, bool &bOutColinear, bool bRequirePositive = true, float Tolerance = RAY_INTERSECT_TOLERANCE);

	UFUNCTION(Category = "Modumate | Geometry")
	static bool IsPointInPolygon(const FVector2D &Point, const TArray<FVector2D> &Polygon, float Tolerance = RAY_INTERSECT_TOLERANCE);

	UFUNCTION(Category = "Modumate | Geometry")
	static void PolyIntersection(const TArray<FVector2D> &PolyA, const TArray<FVector2D> &PolyB,
		bool &bOutAInB, bool &bOutOverlapping, bool &bOutTouching, float Tolerance = RAY_INTERSECT_TOLERANCE);

	UFUNCTION(Category = "Modumate | Geometry")
	static bool RayIntersection3D(const FVector& RayOriginA, const FVector& RayDirectionA, const FVector& RayOriginB, const FVector& RayDirectionB,
		FVector& OutIntersectionPoint, float &OutRayADist, float &OutRayBDist, bool bRequirePositive = true, float Tolerance = RAY_INTERSECT_TOLERANCE);

	/* Given a line segment and an array of polygons (holes), find the intersections between the polygons' segments and the input segment,
	 * and return a combined set of segments that merges the polygons, if possible.
	 * @param[in] SegmentStart: the start point of the line segment
	 * @param[in] SegmentEnd: the end point of the line segment
	 * @param[in] Polygons: the array of 0-n polygons that are to be merged with the line segment
	 * @param[out] OutPoints: the full array of merged points, including the SegmentStart/End, as well as potentially points from Polygons
	 * @param[out] OutMergedPolyIndices: the array of polygons, by index, that were merged with the line segment, in the order in which they were encountered from SegmentStart to SegmentEnd
	 * @param[out] OutSplitSegments: the array of solid segments that remain from the original line segment, unmerged by the polygons
	 * @param[out] OutPointsHoleIndices: for each point in OutPoints, the index of the hole that created the point, otherwise INDEX_NONE if it came from the original line segment
	 * @result: whether the analysis could complete, it should only fail if the input segment or one of the polygon segments are 0 in length
	 */
	UFUNCTION(Category = "Modumate | Geometry")
	static bool GetSegmentPolygonIntersections(const FVector2D &SegmentStart, const FVector2D &SegmentEnd, const TArray<FPolyHole2D> &Polygons,
		TArray<FVector2D> &OutPoints, TArray<int32> &OutMergedPolyIndices, TArray<bool> &OutMergedPolygons, TArray<FVector2D> &OutSplitSegments,
		TArray<int32> &OutPointsHoleIndices);

	UFUNCTION(Category = "Modumate | Geometry")
	static bool TessellateSlopedEdges(const TArray<FVector> &EdgePoints, const TArray<float> &EdgeSlopes, const TArray<bool> &EdgesHaveFaces,
		TArray<FVector> &OutCombinedPolyVerts, TArray<int32> &OutPolyVertIndices, const FVector &NormalHint = FVector::UpVector);

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

	UFUNCTION(Category = "Modumate | Geometry")
	static bool SliceBoxWithPlane(const FBox &InBox, const FVector &InOrigin, const FVector &InNormal, FVector &OutAxisX, FVector &OutAxisY, FBox2D &OutSlice);

	static bool ConvertProcMeshToLinesOnPlane(UProceduralMeshComponent* InProcMesh, FVector PlanePosition, FVector PlaneNormal, TArray<TPair<FVector, FVector>> &OutEdges);

	static bool GetAxisForVector(const FVector &Normal, EAxis::Type &OutAxis, float &OutSign);

	static bool GetEdgeIntersections(const TArray<FVector> &Positions, const FVector &IntersectionOrigin, const FVector &IntersectionDir, TArray<Modumate::FEdgeIntersection> &OutEdgeIntersections, float Epsilon = DEFAULT_GRAPH3D_EPSILON);
};
