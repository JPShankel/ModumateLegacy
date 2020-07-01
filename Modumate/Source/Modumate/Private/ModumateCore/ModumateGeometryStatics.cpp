// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "ModumateCore/ModumateGeometryStatics.h"

#include "Kismet/KismetMathLibrary.h"
#include "KismetProceduralMeshLibrary.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "Algo/Accumulate.h"
#include "DrawDebugHelpers.h"

#include <algorithm>
#include <queue>
#include <iostream>
using namespace std;

#include "../../poly2tri/poly2tri.h"
#include "Algo/Reverse.h"

#define DEBUG_CHECK_LAYERS (!UE_BUILD_SHIPPING)

FLayerGeomDef::FLayerGeomDef()
{

}

FLayerGeomDef::FLayerGeomDef(const TArray<FVector> &Points, float InThickness, const FVector &InNormal)
	: PointsA(Points)
	, Thickness(InThickness)
	, Normal(InNormal)
{
	int32 numPoints = Points.Num();

	// First, make sure we have a polygon that has a chance of being triangulated
	if ((numPoints < 3) || UModumateGeometryStatics::AreConsecutivePointsRepeated(Points))
	{
		return;
	}

	FPlane sourcePlane;
	bool bSuccess = UModumateGeometryStatics::GetPlaneFromPoints(Points, sourcePlane);
	if (!ensure(bSuccess && FVector::Parallel(Normal, sourcePlane)))
	{
		return;
	}

	bCoincident = FVector::Coincident(sourcePlane, Normal);

	FVector pointsBDelta = Thickness * Normal;

	for (const FVector &pointA : PointsA)
	{
		PointsB.Add(pointA + pointsBDelta);
	}

	Origin = PointsA[0];
	AxisX = (Points[1] - Points[0]).GetSafeNormal();
	AxisY = (AxisX ^ Normal).GetSafeNormal();
	bValid = CachePoints2D();
}

FLayerGeomDef::FLayerGeomDef(const TArray<FVector> &InPointsA, const TArray<FVector> &InPointsB,
	const FVector &InNormal, const FVector &InAxisX, const TArray<FPolyHole3D> *InHoles)
{
	Init(InPointsA, InPointsB, InNormal, InAxisX, InHoles);
}

void FLayerGeomDef::Init(const TArray<FVector> &InPointsA, const TArray<FVector> &InPointsB,
	const FVector &InNormal, const FVector &InAxisX, const TArray<FPolyHole3D> *InHoles)
{
	bValid = false;
	Thickness = 0.0f;

	PointsA = InPointsA;
	PointsB = InPointsB;
	Normal = InNormal;

	int32 numPoints = PointsA.Num();
	if ((numPoints != PointsB.Num()) || (numPoints < 3))
	{
		return;
	}

	if (Normal.IsNearlyZero(PLANAR_DOT_EPSILON))
	{
		return;
	}

	FVector pointsDelta = PointsB[0] - PointsA[0];
	Thickness = pointsDelta | Normal;

	Origin = PointsA[0];

#if DEBUG_CHECK_LAYERS
	for (int32 i = 1; i < numPoints; ++i)
	{
		float otherPointsDist = (PointsB[i] - PointsA[i]) | Normal;
		if (!ensure(FMath::IsNearlyEqual(otherPointsDist, Thickness, PLANAR_DOT_EPSILON)))
		{
			return;
		}
	}
#endif

	// Make sure that we have polygon points that we expect to be able to triangulate
	if (UModumateGeometryStatics::AreConsecutivePointsRepeated(PointsA) ||
		UModumateGeometryStatics::AreConsecutivePointsRepeated(PointsB))
	{
		return;
	}

	// Now, we expect to be able to find a plane from the given points
	FPlane planeA, planeB;
	bool bSuccessA = UModumateGeometryStatics::GetPlaneFromPoints(PointsA, planeA);
	bool bSuccessB = UModumateGeometryStatics::GetPlaneFromPoints(PointsB, planeB);

	if (!ensure(bSuccessA && bSuccessB && FVector::Coincident(planeA, planeB) && FVector::Parallel(planeA, Normal)))
	{
		return;
	}

	bCoincident = FVector::Coincident(planeA, Normal);

	if (InAxisX.IsNormalized() && ensure(FVector::Orthogonal(InAxisX, Normal)))
	{
		AxisX = InAxisX;
		AxisY = Normal ^ AxisX;
	}
	else
	{
		UModumateGeometryStatics::FindBasisVectors(AxisX, AxisY, Normal);
	}

	if (InHoles)
	{
		Holes3D = *InHoles;
	}

	bValid = CachePoints2D();
}

FVector2D FLayerGeomDef::ProjectPoint2D(const FVector &Point3D) const
{
	return UModumateGeometryStatics::ProjectPoint2D(Point3D, AxisX, AxisY, Origin);
}

FVector FLayerGeomDef::ProjectPoint3D(const FVector2D &Point2D, bool bSideA) const
{
	return Origin + (AxisX * Point2D.X) + (AxisY * Point2D.Y) +
		(bSideA ? FVector::ZeroVector : (Thickness * Normal));
}

bool FLayerGeomDef::CachePoints2D()
{
	// Simply project the 3D layer pairs into 2D for triangulation
	CachedPointsA2D.Reset(PointsA.Num());
	for (const FVector &pointA : PointsA)
	{
		CachedPointsA2D.Add(ProjectPoint2D(pointA));
	}

	CachedPointsB2D.Reset(PointsB.Num());
	for (const FVector &pointB : PointsB)
	{
		CachedPointsB2D.Add(ProjectPoint2D(pointB));
	}

	// Make sure the polygons' edges are valid
	if (!UModumateGeometryStatics::IsPolygon2DValid(CachedPointsA2D) ||
		!UModumateGeometryStatics::IsPolygon2DValid(CachedPointsB2D))
	{
		return false;
	}

	// Project the 3D holes into 2D, and make sure that they don't overlap with the layer points or each other.
	int32 numHoles = Holes3D.Num();
	CachedHoles2D.Reset(numHoles);
	ValidHoles2D.Reset(numHoles);
	HolesValid.Reset(numHoles);
	for (int32 holeIdx = 0; holeIdx < numHoles; ++holeIdx)
	{
		const FPolyHole3D &hole3D = Holes3D[holeIdx];
		FPolyHole2D &hole2D = CachedHoles2D.AddDefaulted_GetRef();
		bool &bHoleValid = HolesValid.Add_GetRef(true);
		for (const FVector &holePoint3D : hole3D.Points)
		{
			hole2D.Points.Add(ProjectPoint2D(holePoint3D));
		}

		bool bHoleInA, bHoleOverlapsA, bHoleTouchesA;
		UModumateGeometryStatics::PolyIntersection(hole2D.Points, CachedPointsA2D, bHoleInA, bHoleOverlapsA, bHoleTouchesA);

		bool bHoleInB, bHoleOverlapsB, bHoleTouchesB;
		UModumateGeometryStatics::PolyIntersection(hole2D.Points, CachedPointsB2D, bHoleInB, bHoleOverlapsB, bHoleTouchesB);

		if (!bHoleInA || bHoleOverlapsA || !bHoleInB || bHoleOverlapsB)
		{
			bHoleValid = false;
			continue;
		}

		for (const FPolyHole2D &otherHole2D : ValidHoles2D)
		{
			bool bHoleInOther, bHoleOverlapsOther, bHoleTouchesOther;
			UModumateGeometryStatics::PolyIntersection(hole2D.Points, otherHole2D.Points, bHoleInOther, bHoleOverlapsOther, bHoleTouchesOther);

			if (bHoleInOther || bHoleOverlapsOther || bHoleTouchesOther)
			{
				bHoleValid = false;
				break;
			}
		}

		if (bHoleValid)
		{
			ValidHoles2D.Add(hole2D);
		}
	}

	return true;
}

void FLayerGeomDef::AppendTriangles(const TArray<FVector> &Verts, const TArray<int32> &SourceTriIndices,
	TArray<int32> &OutTris, bool bReverseTris)
{
	int32 numVertices = Verts.Num();
	int32 numTriIndices = SourceTriIndices.Num();
	for (int32 i = 0; i < numTriIndices; ++i)
	{
		int32 triIndex = bReverseTris ? SourceTriIndices[numTriIndices - 1 - i] : SourceTriIndices[i];
		OutTris.Add(triIndex + numVertices);
	}
}

bool FLayerGeomDef::TriangulateSideFace(const FVector2D &Point2DA1, const FVector2D &Point2DA2,
	const FVector2D &Point2DB1, const FVector2D &Point2DB2, bool bReverseTris,
	TArray<FVector> &OutVerts, TArray<int32> &OutTris,
	TArray<FVector> &OutNormals, TArray<FVector2D> &OutUVs, TArray<FProcMeshTangent> &OutTangents,
	const FVector2D &UVScale, const FVector &UVAnchor, float UVRotOffset) const
{
	TempSidePoints.SetNum(4);
	TempSidePoints[0] = ProjectPoint3D(Point2DA1, true);
	TempSidePoints[1] = ProjectPoint3D(Point2DA2, true);
	TempSidePoints[2] = ProjectPoint3D(Point2DB1, false);
	TempSidePoints[3] = ProjectPoint3D(Point2DB2, false);

	FPlane sidePlane(ForceInitToZero);
	bool bSidePointsPlanar = UModumateGeometryStatics::GetPlaneFromPoints(TempSidePoints, sidePlane);
	if (!bSidePointsPlanar)
	{
		UE_LOG(LogTemp, Warning, TEXT("Side of layer is not planar!"));
		// TODO: we could exit here with failure, but until we can prevent this from happening we might as well try our best.
		// This currently happens due to differing number of naive miter participants on different vertices of a given edge.
		//return false;
	}

	FVector edgeADir = (TempSidePoints[1] - TempSidePoints[0]).GetSafeNormal();
	FVector edgeAB1Dir = (TempSidePoints[2] - TempSidePoints[0]).GetSafeNormal();
	FVector sideNormal = (edgeADir ^ edgeAB1Dir).GetSafeNormal();

	// Whether to treat UVs as if the wall was laid on its front face,
	// as opposed to using the best projection on the wall's up vector as the vertical UV component.
	// TODO: when tile materials can reinterpret their pattern data to understand how to cut modules in 3D,
	// make UVs consistent on all sides so that they can all evaluate the same material.
	bool bUseFlatUVs = FVector::Orthogonal(edgeADir, FVector::UpVector);
	bool bSideAlignsWithUp = (edgeADir | FVector::UpVector) > 0.0f;

	if (bReverseTris)
	{
		sideNormal *= -1.0f;
	}

	FVector sideAxisY = bUseFlatUVs ? -Normal : (bSideAlignsWithUp ? -edgeADir : edgeADir);
	FVector sideAxisX = (-sideNormal ^ sideAxisY).GetSafeNormal();

	FVector2D sideUVs[4];
	FVector sideNormals[4];
	FProcMeshTangent sideTangents[4];
	for (int32 i = 0; i < 4; ++i)
	{
		sideUVs[i] = UVScale * UModumateGeometryStatics::ProjectPoint2D(TempSidePoints[i], sideAxisX, sideAxisY, UVAnchor);
		sideNormals[i] = sideNormal;
		sideTangents[i] = FProcMeshTangent(Normal, false);
	}

	static TArray<int32> sideTris({ 3, 1, 2, 2, 1, 0 });
	AppendTriangles(OutVerts, sideTris, OutTris, bReverseTris);
	OutVerts.Append(TempSidePoints);
	OutNormals.Append(sideNormals, 4);
	OutUVs.Append(sideUVs, 4);
	OutTangents.Append(sideTangents, 4);

	return true;
}

bool FLayerGeomDef::TriangulateMesh(TArray<FVector> &OutVerts, TArray<int32> &OutTris, TArray<FVector> &OutNormals,
	TArray<FVector2D> &OutUVs, TArray<FProcMeshTangent> &OutTangents, const FVector &UVAnchor, float UVRotOffset) const
{
	const FVector2D uvScale = FVector2D(0.01f, 0.01f);

	if (!bValid)
	{
		return false;
	}

	TArray<FVector2D> verticesA2D, perimeterA2D, verticesB2D, perimeterB2D;
	TArray<int32> trisA2D, trisB2D, perimeterVertexHoleIndicesA, perimeterVertexHoleIndicesB;
	TArray<bool> mergedHolesA, mergedHolesB;
	bool bTriangulatedA = UModumateGeometryStatics::TriangulateVerticesPoly2Tri(CachedPointsA2D, ValidHoles2D,
		verticesA2D, trisA2D, perimeterA2D, mergedHolesA, perimeterVertexHoleIndicesA);

	if (!bTriangulatedA)
	{
		return false;
	}

	int32 numHoles = ValidHoles2D.Num();

	if (Thickness > PLANAR_DOT_EPSILON)
	{
		bool bTriangulatedB = UModumateGeometryStatics::TriangulateVerticesPoly2Tri(CachedPointsB2D, ValidHoles2D,
			verticesB2D, trisB2D, perimeterB2D, mergedHolesB, perimeterVertexHoleIndicesB);

		if (!bTriangulatedB)
		{
			return false;
		}

		// The perimeter and hole merging calculations should match; otherwise, we can't fix any differences between them.
		bool bSamePerimeters = (perimeterA2D.Num() == perimeterB2D.Num());
		bool bSameMergedHoles = (mergedHolesA == mergedHolesB);
		if (!ensure(bSamePerimeters == bSameMergedHoles))
		{
			return false;
		}

		// If the two sides differ in hole-merging, then remove inconsistent holes from the perimeters.
		if (!bSameMergedHoles)
		{
			for (int32 holeIdx = 0; holeIdx < numHoles; ++holeIdx)
			{
				if (mergedHolesA[holeIdx] != mergedHolesB[holeIdx])
				{
					TArray<FVector2D> &perimeterRef = mergedHolesA[holeIdx] ? perimeterA2D : perimeterB2D;
					TArray<int32> &perimeterVertexHoleIndicesRef = mergedHolesA[holeIdx] ?
						perimeterVertexHoleIndicesA : perimeterVertexHoleIndicesB;
					TArray<bool> &mergedHolesRef = mergedHolesA[holeIdx] ? mergedHolesA : mergedHolesB;

					for (int32 pointIdx = perimeterRef.Num() - 1; pointIdx >= 0; --pointIdx)
					{
						if (perimeterVertexHoleIndicesRef[pointIdx] == holeIdx)
						{
							perimeterRef.RemoveAt(pointIdx);
							perimeterVertexHoleIndicesRef.RemoveAt(pointIdx);
						}
					}

					mergedHolesRef[holeIdx] = false;
				}
			}
		}

		// Make sure that we were able to fix the perimeters to be the same,
		// after removing inconsistently-merged holes.
		bSamePerimeters = (perimeterA2D.Num() == perimeterB2D.Num());
		if (!ensure(bSamePerimeters))
		{
			return false;
		}
	}

	int32 numPerimeterPoints = perimeterA2D.Num();

	// Side A
	AppendTriangles(OutVerts, trisA2D, OutTris, false);
	for (const FVector2D &vertexA2D : verticesA2D)
	{
		FVector vertexA = ProjectPoint3D(vertexA2D, true);
		OutVerts.Add(vertexA);
		FVector2D uvA = UModumateGeometryStatics::ProjectPoint2D(vertexA, -AxisX, AxisY, UVAnchor);
		OutUVs.Add(uvA * uvScale);
		OutNormals.Add(-Normal);
		OutTangents.Add(FProcMeshTangent(-AxisX, false));
	}

	// If the layer is infinitely thin, then exit before we bother creating triangles for the other sides.
	if (Thickness <= PLANAR_DOT_EPSILON)
	{
		return true;
	}

	// Side B
	AppendTriangles(OutVerts, trisB2D, OutTris, true);
	for (const FVector2D &vertexB2D : verticesB2D)
	{
		FVector vertexB = ProjectPoint3D(vertexB2D, false);
		OutVerts.Add(vertexB);
		FVector2D uvB = UModumateGeometryStatics::ProjectPoint2D(vertexB, AxisX, AxisY, UVAnchor);
		OutUVs.Add(uvB * uvScale);
		OutNormals.Add(Normal);
		OutTangents.Add(FProcMeshTangent(AxisX, false));
	}

	// Perimeter sides
	for (int32 perimIdx1 = 0; perimIdx1 < numPerimeterPoints; ++perimIdx1)
	{
		int32 perimIdx2 = (perimIdx1 + 1) % numPerimeterPoints;

		TriangulateSideFace(perimeterA2D[perimIdx1], perimeterA2D[perimIdx2],
			perimeterB2D[perimIdx1], perimeterB2D[perimIdx2], !bCoincident,
			OutVerts, OutTris, OutNormals, OutUVs, OutTangents, uvScale, UVAnchor, UVRotOffset);
	}

	// Add side faces for insides of holes
	for (int32 holeIdx = 0; holeIdx < numHoles; ++holeIdx)
	{
		ensureMsgf(mergedHolesA[holeIdx] == mergedHolesB[holeIdx],
			TEXT("Hole %d was inconsistently merged by the different layer sides!"), holeIdx);

		if (mergedHolesA[holeIdx] && mergedHolesB[holeIdx])
		{
			continue;
		}

		const FPolyHole2D &hole = ValidHoles2D[holeIdx];
		int32 numHolePoints = hole.Points.Num();
		bool bHolePointsCW = UModumateGeometryStatics::AreLocationsClockwise2D(hole.Points);
		for (int32 holeSideIdx1 = 0; holeSideIdx1 < numHolePoints; ++holeSideIdx1)
		{
			int32 holeSideIdx2 = (holeSideIdx1 + 1) % numHolePoints;
			const FVector2D &holeSidePoint1 = hole.Points[holeSideIdx1];
			const FVector2D &holeSidePoint2 = hole.Points[holeSideIdx2];
			bool bReverseHoleSideTris = bHolePointsCW;
			if (bCoincident)
			{
				bReverseHoleSideTris = !bReverseHoleSideTris;
			}

			TriangulateSideFace(holeSidePoint1, holeSidePoint2, holeSidePoint1, holeSidePoint2, bReverseHoleSideTris,
				OutVerts, OutTris, OutNormals, OutUVs, OutTangents, uvScale, UVAnchor, UVRotOffset);
		}
	}

	return true;
}


// Helper functions for determining a point inside a polygon that is far from its edges and close to its centroid.
// Adapted from https://github.com/mapbox/polylabel and
// https://github.com/mapycz/polylabel/commit/a51fe7acb5f13cda92dff01290faf79efcb38378
namespace PolyDist
{
	// get the distance from a point to a segment
	float GetSegDistSq(const FVector2D& p, const FVector2D& a, const FVector2D& b)
	{
		auto x = a.X;
		auto y = a.Y;
		auto dx = b.X - x;
		auto dy = b.Y - y;

		if (dx != 0 || dy != 0)
		{
			auto t = ((p.X - x) * dx + (p.Y - y) * dy) / (dx * dx + dy * dy);

			if (t > 1)
			{
				x = b.X;
				y = b.Y;
			}
			else if (t > 0)
			{
				x += dx * t;
				y += dy * t;
			}
		}

		dx = p.X - x;
		dy = p.Y - y;

		return dx * dx + dy * dy;
	}

	// signed distance from point to polygon outline (negative if point is outside)
	float PointToPolyDist(const FVector2D& point, const TArray<FVector2D>& polygon)
	{
		bool inside = false;
		float minDistSq = FLT_MAX;

		for (int32 i = 0, len = polygon.Num(), j = len - 1; i < len; j = i++)
		{
			const auto& a = polygon[i];
			const auto& b = polygon[j];

			if ((a.Y > point.Y) != (b.Y > point.Y) &&
				(point.X < (b.X - a.X) * (point.Y - a.Y) / (b.Y - a.Y) + a.X))
			{
				inside = !inside;
			}

			minDistSq = FMath::Min(minDistSq, GetSegDistSq(point, a, b));
		}

		return (inside ? 1 : -1) * FMath::Sqrt(minDistSq);
	}

	struct FitnessFtor
	{
		FitnessFtor(const FVector2D& centroid_, const FVector2D& polygonSize)
			: centroid(centroid_)
			, maxSize(polygonSize.GetMax())
		{}

		float operator()(const FVector2D& cellCenter, float distancePolygon) const {
			if (distancePolygon <= 0) {
				return distancePolygon;
			}
			FVector2D d = cellCenter - centroid;
			double distanceCentroid = d.Size();
			return distancePolygon * (1 - distanceCentroid / maxSize);
		}

		FVector2D centroid;
		float maxSize;
	};

	struct Cell
	{
		template <class FitnessFunc>
		Cell(const FVector2D& c_, float h_, const TArray<FVector2D>& polygon, const FitnessFunc& ff)
			: c(c_)
			, h(h_)
			, d(PointToPolyDist(c, polygon))
			, fitness(ff(c, d))
			, maxFitness(ff(c, d + h * FMath::Sqrt(2)))
		{}

		FVector2D c; // cell center
		float h; // half the cell size
		float d; // distance from cell center to polygon
		float fitness; // fitness of the cell center
		float maxFitness; // a "potential" of the cell calculated from max distance to polygon within the cell
	};

	// get polygon centroid
	FVector2D GetCentroid(const TArray<FVector2D>& polygon) {
		float area = 0;
		FVector2D c = FVector2D::ZeroVector;

		for (int32 i = 0, len = polygon.Num(), j = len - 1; i < len; j = i++) {
			const FVector2D& a = polygon[i];
			const FVector2D& b = polygon[j];
			auto f = a.X * b.Y - b.X * a.Y;
			c.X += (a.X + b.X) * f;
			c.Y += (a.Y + b.Y) * f;
			area += f * 3;
		}

		return (area == 0) ? polygon[0] : c / area;
	}

} // namespace PolyDist

FVector2D UModumateGeometryStatics::ProjectPoint2D(const FVector &Point3D, const FVector &AxisX, const FVector &AxisY, const FVector &Origin)
{
	if (!ensure(AxisX.IsNormalized() && AxisY.IsNormalized() &&
		FVector::Orthogonal(AxisX, AxisY)))
	{
		return FVector2D::ZeroVector;
	}

	FVector originDelta = Point3D - Origin;
	return FVector2D(
		originDelta | AxisX,
		originDelta | AxisY
	);
}

FVector2D UModumateGeometryStatics::ProjectVector2D(const FVector &Vector3D, const FVector &AxisX, const FVector &AxisY)
{
	return ProjectPoint2D(Vector3D, AxisX, AxisY, FVector::ZeroVector);
}

bool UModumateGeometryStatics::FindPointFurthestFromPolyEdge(const TArray<FVector2D> &polygon, FVector2D &furthestPoint)
{
	using namespace PolyDist;

	static const float precision = 1.0f;

	FBox2D envelope(ForceInitToZero);
	for (const FVector2D &point : polygon)
	{
		envelope += point;
	}

	FVector2D size = envelope.GetSize();

	const float cellSize = size.GetMin();
	float h = cellSize / 2;

	// a priority queue of cells in order of their "potential" (max distance to polygon)
	auto compareMax = [](const PolyDist::Cell& a, const PolyDist::Cell& b) {
		return a.maxFitness < b.maxFitness;
	};
	using Queue = std::priority_queue<PolyDist::Cell, std::vector<PolyDist::Cell>, decltype(compareMax)>;
	Queue cellQueue(compareMax);

	if (cellSize == 0) {
		furthestPoint = envelope.Min;
		return false;
	}

	FVector2D centroid = GetCentroid(polygon);
	FitnessFtor fitnessFunc(centroid, size);

	// cover polygon with initial cells
	for (float x = envelope.Min.X; x < envelope.Max.X; x += cellSize) {
		for (float y = envelope.Min.Y; y < envelope.Max.Y; y += cellSize) {
			cellQueue.push(PolyDist::Cell({ x + h, y + h }, h, polygon, fitnessFunc));
		}
	}

	// take centroid as the first best guess
	auto bestCell = PolyDist::Cell(centroid, 0, polygon, fitnessFunc);

	auto numProbes = cellQueue.size();
	while (!cellQueue.empty()) {
		// pick the most promising cell from the queue
		auto cell = cellQueue.top();
		cellQueue.pop();

		// update the best cell if we found a better one
		if (cell.fitness > bestCell.fitness) {
			bestCell = cell;
		}

		// do not drill down further if there's no chance of a better solution
		if (cell.maxFitness - bestCell.fitness <= precision) continue;

		// split the cell into four cells
		h = cell.h / 2;
		cellQueue.push(PolyDist::Cell({ cell.c.X - h, cell.c.Y - h }, h, polygon, fitnessFunc));
		cellQueue.push(PolyDist::Cell({ cell.c.X + h, cell.c.Y - h }, h, polygon, fitnessFunc));
		cellQueue.push(PolyDist::Cell({ cell.c.X - h, cell.c.Y + h }, h, polygon, fitnessFunc));
		cellQueue.push(PolyDist::Cell({ cell.c.X + h, cell.c.Y + h }, h, polygon, fitnessFunc));
		numProbes += 4;
	}

	furthestPoint = bestCell.c;
	return true;
}

// For cleaning poly2tri
template <class C> void FreeClear(C & cntr) {
	for (typename C::iterator it = cntr.begin();
		it != cntr.end(); ++it) {
		delete * it;
	}
	cntr.clear();
}

bool UModumateGeometryStatics::TriangulateVerticesPoly2Tri(const TArray<FVector2D> &Vertices, const TArray<FPolyHole2D> &Holes,
	TArray<FVector2D> &OutVertices, TArray<int32> &OutTriangles, TArray<FVector2D> &OutPerimeter, TArray<bool> &OutMergedHoles,
	TArray<int32> &OutPerimeterVertexHoleIndices)
{
	if (!ensure(Vertices.Num() >= 3))
	{
		UE_LOG(LogTemp, Error, TEXT("Input polygon only has %d vertices!"), Vertices.Num());
		return false;
	}

	if (!ensure(!UModumateGeometryStatics::AreConsecutivePoints2DRepeated(Vertices)))
	{
		UE_LOG(LogTemp, Error, TEXT("Input polygon has consecutive repeating points!"));
		return false;
	}

	OutVertices.Reset();
	OutTriangles.Reset();
	OutPerimeter.Reset();
	OutPerimeterVertexHoleIndices.Reset();

	int32 numInputHoles = Holes.Num();
	OutMergedHoles.Reset();
	OutMergedHoles.SetNumZeroed(numInputHoles);

	// Step 1: potentially merge holes with the input segments so that it doesn't fail triangulation
	int32 numVertices = Vertices.Num();
	if (numInputHoles > 0)
	{
		TArray<int32> allMergedHoleIndices, segmentMergedHoleIndices, segmentPointsHoleIndices;
		TArray<FVector2D> segmentPoints;
		TArray<bool> segmentMergedHoles;
		TArray<FVector2D> splitSegments;

		for (int32 polyPointIdx1 = 0; polyPointIdx1 < numVertices; ++polyPointIdx1)
		{
			int32 polyPointIdx2 = (polyPointIdx1 + 1) % numVertices;
			const FVector2D &segmentStart = Vertices[polyPointIdx1];
			const FVector2D &segmentEnd = Vertices[polyPointIdx2];

			bool bSegmentAnalysisSuccess = GetSegmentPolygonIntersections(segmentStart, segmentEnd, Holes, segmentPoints,
				segmentMergedHoleIndices, segmentMergedHoles, splitSegments, segmentPointsHoleIndices);
			int32 numSegmentPoints = segmentPoints.Num();

			if (bSegmentAnalysisSuccess && (numSegmentPoints > 1))
			{
				OutPerimeter.Append(segmentPoints.GetData(), numSegmentPoints - 1);
				OutPerimeterVertexHoleIndices.Append(segmentPointsHoleIndices.GetData(), numSegmentPoints - 1);
				allMergedHoleIndices.Append(segmentMergedHoleIndices);
			}
			else
			{
				return false;
			}
		}

		for (int32 mergedHoleIdx : allMergedHoleIndices)
		{
			OutMergedHoles[mergedHoleIdx] = true;
		}
	}
	else
	{
		OutPerimeter.Append(Vertices);
		for (int32 vertIdx = 0; vertIdx < numVertices; ++vertIdx)
		{
			OutPerimeterVertexHoleIndices.Add(INDEX_NONE);
		}
	}

	// Step 2: Gather vertices to vector, then create CDT and add primary polyline
	//NOTE: polyline must be a simple polygon.The polyline's points constitute constrained edges.No repeat points!!!

	vector<p2t::Point*> polyline;
	vector<p2t::Point*> vector_hole;
	for (int32 i = 0; i < OutPerimeter.Num(); i++)
	{
		// checking for duplicate points in this way is n^2
		for (int32 j = i+1; j < OutPerimeter.Num(); j++)
		{
			if (OutPerimeter[i].Equals(OutPerimeter[j]))
			{
				// skip all vertices contained between equal vertices
				i = j;
			}
		}

		polyline.push_back(new p2t::Point(OutPerimeter[i].X, OutPerimeter[i].Y));
	}

	if (!ensure(polyline.size() >= 3))
	{
		UE_LOG(LogTemp, Error, TEXT("Input polygon only has %d vertices after duplicates were removed!"), polyline.size());
		return false;
	}

	p2t::CDT* cdt = new p2t::CDT(polyline);

	// Step 3: Add holes or Steiner points if necessary

	for (int32 i = 0; i < numInputHoles; i++)
	{
		if (OutMergedHoles[i])
		{
			continue;
		}

		bool bValidHole = true;
		TArray<FVector2D> holeVertices = Holes[i].Points;
		for (int32 j = 0; j < holeVertices.Num(); j++)
		{
			p2t::Point *holePoint = new p2t::Point(holeVertices[j].X, holeVertices[j].Y);
			vector_hole.push_back(holePoint);
		}

		if (bValidHole)
		{
			cdt->AddHole(vector_hole);
		}
		vector_hole.clear();
	}

	// Step 4: Triangulate!
	cdt->Triangulate();
	vector<p2t::Triangle*> triangles = cdt->GetTriangles();
	int32 numTriangles = triangles.size();

	TMap<p2t::Point*, int32> pointIndices;
	auto addTrianglePoint = [&pointIndices, &OutVertices, &OutTriangles](p2t::Triangle* tri, int32 triIdx)
	{
		p2t::Point *point = tri->GetPoint(triIdx);

		if (int32 *vertIdxPtr = pointIndices.Find(point))
		{
			OutTriangles.Add(*vertIdxPtr);
		}
		else
		{
			int32 &polyIdx = pointIndices.Add(point, OutVertices.Num());
			OutTriangles.Add(polyIdx);
			OutVertices.Add(FVector2D(point->x, point->y));
		}
	};

	for (int32 i = 0; i < numTriangles; i++)
	{
		p2t::Triangle* tri = triangles[i];
		addTrianglePoint(tri, 0);
		addTrianglePoint(tri, 1);
		addTrianglePoint(tri, 2);
	}

	// Step 5: Free p2t memory
	FreeClear(polyline);
	FreeClear(vector_hole);
	triangles.clear();
	delete cdt;

	return (OutVertices.Num() > 0);
}

bool UModumateGeometryStatics::AreLocationsClockwise2D(const TArray<FVector2D> &Locations)
{
	int32 numLocations = Locations.Num();
	if (numLocations < 3)
	{
		UE_LOG(LogTemp, Warning, TEXT("Can't determine winding of < 3 locations!"));
		return true;
	}

	float totalWindingAngle = 0.0f;

	for (int32 i1 = 0; i1 < numLocations; ++i1)
	{
		int32 i2 = (i1 + 1) % numLocations;
		int32 i3 = (i2 + 1) % numLocations;

		const FVector2D &p1 = Locations[i1];
		const FVector2D &p2 = Locations[i2];
		const FVector2D &p3 = Locations[i3];

		if (p1.Equals(p2) || p2.Equals(p3))
		{
			UE_LOG(LogTemp, Warning, TEXT("Can't determine winding of redundant locations!"));
			return true;
		}

		FVector2D n1 = (p2 - p1).GetSafeNormal();
		FVector2D n2 = (p3 - p2).GetSafeNormal();

		float unsignedAngle = FMath::Acos(n1 | n2);
		float angleSign = -FMath::Sign(n1 ^ n2);
		float signedAngle = angleSign * unsignedAngle;
		totalWindingAngle += signedAngle;
	}

	return (totalWindingAngle >= 0.0f);
}

bool UModumateGeometryStatics::GetPlaneFromPoints(const TArray<FVector> &Points, FPlane &outPlane, float Tolerance)
{
	if (Points.Num() < 3)
	{
		UE_LOG(LogTemp, Warning, TEXT("Can't construct plane from less than three points."));
		return false;
	}

	int32 planeThirdIdx = INDEX_NONE;
	int32 numLocations = Points.Num();
	FVector v1 = (Points[1] - Points[0]).GetSafeNormal();

	if (v1.IsNearlyZero())
	{
		UE_LOG(LogTemp, Warning, TEXT("Won't construct plane when first points are duplicated."));
		return false;
	}

	for (int32 vertexIdx = 2; vertexIdx < numLocations; vertexIdx++)
	{
		FVector v2 = (Points[vertexIdx] - Points[1]).GetSafeNormal();

		if (v2.IsNormalized() && !FVector::Parallel(v1, v2))
		{
			planeThirdIdx = vertexIdx;
			break;
		}
	}

	if (planeThirdIdx == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("Can't construct plane from a set of colinear points."));
		return false;
	}

	outPlane = FPlane(Points[0], Points[1], Points[planeThirdIdx]);

	float totalWindingAngle = 0.0f;

	for (int32 vertexIdx = 0; vertexIdx < numLocations; vertexIdx++)
	{
		float planeDot = outPlane.PlaneDot(Points[vertexIdx]);
		if (FMath::Abs(planeDot) > Tolerance)
		{
			// Provided points are not planar
			return false;
		}

		int32 i2 = (vertexIdx + 1) % numLocations;
		int32 i3 = (i2 + 1) % numLocations;

		const FVector &p1 = Points[vertexIdx];
		const FVector &p2 = Points[i2];
		const FVector &p3 = Points[i3];

		if (p1.Equals(p2) || p2.Equals(p3))
		{
			continue;
		}

		FVector n1 = (p2 - p1).GetUnsafeNormal();
		FVector n2 = (p3 - p2).GetUnsafeNormal();

		FVector crossResult = (n1 ^ n2).GetSafeNormal();
		float crossDotUp = crossResult | outPlane;
		if (FMath::Abs(crossDotUp) < THRESH_NORMALS_ARE_PARALLEL)
		{
			continue;
		}

		float unsignedAngle = FMath::Acos(n1 | n2);
		float angleSign = FMath::Sign(crossDotUp);
		float signedAngle = angleSign * unsignedAngle;
		totalWindingAngle += signedAngle;
	}

	bool bClockwise = (totalWindingAngle >= 0.0f);

	if (!bClockwise)
	{
		outPlane *= -1.0f;
	}


	return true;
}

// Copy of FMath::SegmentIntersection2D, but with an optional tolerance.
bool UModumateGeometryStatics::SegmentIntersection2D(const FVector2D& SegmentStartA, const FVector2D& SegmentEndA, const FVector2D& SegmentStartB, const FVector2D& SegmentEndB, FVector2D& outIntersectionPoint, float Tolerance)
{
	const FVector2D VectorA = SegmentEndA - SegmentStartA;
	const FVector2D VectorB = SegmentEndB - SegmentStartB;

	const float S = (-VectorA.Y * (SegmentStartA.X - SegmentStartB.X) + VectorA.X * (SegmentStartA.Y - SegmentStartB.Y)) / (-VectorB.X * VectorA.Y + VectorA.X * VectorB.Y);
	const float T = (VectorB.X * (SegmentStartA.Y - SegmentStartB.Y) - VectorB.Y * (SegmentStartA.X - SegmentStartB.X)) / (-VectorB.X * VectorA.Y + VectorA.X * VectorB.Y);

	const bool bIntersects = (S >= -Tolerance && S <= 1 + Tolerance && T >= -Tolerance && T <= 1 + Tolerance);

	if (bIntersects)
	{
		outIntersectionPoint.X = SegmentStartA.X + (T * VectorA.X);
		outIntersectionPoint.Y = SegmentStartA.Y + (T * VectorA.Y);
	}

	return bIntersects;
}

bool UModumateGeometryStatics::RayIntersection2D(const FVector2D& RayOriginA, const FVector2D& RayDirectionA, const FVector2D& RayOriginB, const FVector2D& RayDirectionB,
	FVector2D& OutIntersectionPoint, float &OutRayADist, float &OutRayBDist, bool &bOutColinear, bool bRequirePositive, float Tolerance)
{
	OutRayADist = OutRayBDist = 0.0f;
	bOutColinear = false;

	// First, check if the rays start at the same origin
	FVector2D originDelta = RayOriginB - RayOriginA;
	float originDist = originDelta.Size();

	float rayADotB = RayDirectionA | RayDirectionB;
	bool bParallel = (FMath::Abs(rayADotB) > THRESH_NORMALS_ARE_PARALLEL);

	if (FMath::IsNearlyZero(originDist, Tolerance))
	{
		OutIntersectionPoint = RayOriginA;
		bOutColinear = bParallel;
		return true;
	}

	// Next, check for parallel rays; they can only intersect if they're overlapping
	if (bParallel)
	{
		// Check if the rays are colinear; if not, then they can't intersect
		float originBOnRayADist = originDelta | RayDirectionA;
		FVector2D originBOnRayAPoint = RayOriginA + (originBOnRayADist * RayDirectionA);

		float originAOnRayBDist = -originDelta | RayDirectionB;
		FVector2D originAOnRayBPoint = RayOriginB + (originAOnRayBDist * RayDirectionB);

		float rayDist = FVector2D::Distance(RayOriginB, originBOnRayAPoint);
		if (rayDist > Tolerance)
		{
			return false;
		}

		bOutColinear = true;

		// Coincident colinear rays
		if (rayADotB > 0.0f)
		{
			ensureAlways(FMath::Sign(originBOnRayADist) != FMath::Sign(originAOnRayBDist));

			OutRayADist = FMath::Max(originBOnRayADist, 0.0f);
			OutRayBDist = FMath::Max(originAOnRayBDist, 0.0f);
			if (originBOnRayADist > 0.0f)
			{
				OutIntersectionPoint = RayOriginB;
			}
			else
			{
				OutIntersectionPoint = RayOriginA;
			}

			return true;
		}
		// Anti-parallel colinear rays
		else
		{
			ensureAlways(FMath::IsNearlyEqual(originBOnRayADist, originAOnRayBDist, Tolerance));

			OutIntersectionPoint = 0.5f * (RayOriginA + RayOriginB);
			OutRayADist = 0.5f * originBOnRayADist;
			OutRayBDist = 0.5f * originAOnRayBDist;

			return (originBOnRayADist > 0.0f) || !bRequirePositive;
		}
	}

	// Determine ray perpendicular vectors, for intersection
	FVector2D rayNormalA(-RayDirectionA.Y, RayDirectionA.X);
	FVector2D rayNormalB(-RayDirectionB.Y, RayDirectionB.X);

	// Compute intersection between rays, using FMath::RayPlaneIntersection as a basis
	// float Distance = (( PlaneOrigin - RayOrigin ) | PlaneNormal) / (RayDirection | PlaneNormal);
	OutRayADist = (originDelta | rayNormalB) / (RayDirectionA | rayNormalB);
	OutRayBDist = (-originDelta | rayNormalA) / (RayDirectionB | rayNormalA);

	// Potentially throw out results that are behind the origins of the rays
	if (bRequirePositive && ((OutRayADist < 0.0f) || (OutRayBDist < 0.0f)))
	{
		return false;
	}

	// Ensure that intersection points are consistent
	FVector2D rayAOnBPoint = RayOriginA + (OutRayADist * RayDirectionA);
	FVector2D rayBOnAPoint = RayOriginB + (OutRayBDist * RayDirectionB);
	if (!rayAOnBPoint.Equals(rayBOnAPoint, Tolerance))
	{
		return false;
	}

	OutIntersectionPoint = rayAOnBPoint;
	return true;
}

// Test whether a point is inside a polygon, within Tolerance distance of an edge (inclusive).
// It assumes a valid, simple polygon input (convex or concave, no holes, closed).
bool UModumateGeometryStatics::IsPointInPolygon(const FVector2D &Point, const TArray<FVector2D> &Polygon, float Tolerance, bool bInclusive)
{
	int32 numPolyPoints = Polygon.Num();

	if (!ensure(numPolyPoints >= 3))
	{
		return false;
	}

	// Choose an arbitrary ray direction from which to test if our sample point hits an odd number
	// of polygon edges.
	const FVector2D testRay(1.0f, 0.0f);
	int32 edgeRayHits = 0;

	// Test the ray against every edge, since we aren't using any kind of spatial data structure to rule out edges,
	// and this is intended to work with concave polygons where being inside corners or on the same side of every edge
	// wouldn't be sufficient.
	for (int32 pointIdx = 0; pointIdx < numPolyPoints; ++pointIdx)
	{
		int32 edgeIdx1 = pointIdx;
		int32 edgeIdx2 = (pointIdx + 1) % numPolyPoints;

		const FVector2D &edgePoint1 = Polygon[edgeIdx1];
		const FVector2D &edgePoint2 = Polygon[edgeIdx2];
		FVector2D edgeDelta = edgePoint2 - edgePoint1;
		float edgeLen = edgeDelta.Size();
		if (!ensure(!FMath::IsNearlyZero(edgeLen)))
		{
			return false;
		}
		FVector2D edgeDir = edgeDelta / edgeLen;

		// If the test point is close to a polygon point, return whether we're inclusive
		if (FVector2D::Distance(Point, edgePoint1) <= Tolerance)
		{
			return bInclusive;
		}

		// If the test point is close to a polygon edge, return whether we're inclusive
		FVector2D projectedPoint = edgePoint1 + edgeDir * ((Point - edgePoint1) | edgeDir);
		if (FVector2D::Distance(Point, projectedPoint) <= Tolerance)
		{
			return bInclusive;
		}

		FVector2D edgeIntersection;
		float testRayDist, edgeRayDist;
		bool bRaysColinear;
		if (UModumateGeometryStatics::RayIntersection2D(Point, testRay, edgePoint1, edgeDir, edgeIntersection,
			testRayDist, edgeRayDist, bRaysColinear, true) && !bRaysColinear &&
			FMath::IsWithinInclusive(edgeRayDist, -Tolerance, edgeLen + Tolerance))
		{
			++edgeRayHits;
		}
	}

	// If the ray hit an odd number of edges, then its origin must be inside the polygon.
	return ((edgeRayHits % 2) == 1);
}

void UModumateGeometryStatics::PolyIntersection(const TArray<FVector2D> &PolyA, const TArray<FVector2D> &PolyB,
	bool &bOutAInB, bool &bOutOverlapping, bool &bOutTouching, float Tolerance)
{
	bOutAInB = false;
	bOutOverlapping = false;
	bOutTouching = false;

	int32 numPointsA = PolyA.Num();
	int32 numPointsB = PolyB.Num();

	FBox2D polyBoundsA(PolyA);
	FBox2D polyBoundsB(PolyB);
	FBox2D expandedBoundsA = polyBoundsA.ExpandBy(Tolerance);
	FBox2D expandedBoundsB = polyBoundsB.ExpandBy(Tolerance);

	// First, check for AABBs intersecting; if they don't, then we don't need to check edges.
	if (!expandedBoundsA.Intersect(expandedBoundsB))
	{
		return;
	}

	bool bAOutsideOfB = false;

	// Next, check for intersections between each edge, and if each point in the inner poly is inside the outer poly
	for (int32 edgeAIdx1 = 0; edgeAIdx1 < numPointsA; ++edgeAIdx1)
	{
		int32 edgeAIdx2 = (edgeAIdx1 + 1) % numPointsA;
		const FVector2D &edgeAPoint1 = PolyA[edgeAIdx1];
		const FVector2D &edgeAPoint2 = PolyA[edgeAIdx2];
		FVector2D edgeADelta = edgeAPoint2 - edgeAPoint1;
		float edgeALen = edgeADelta.Size();
		if (!ensure(!FMath::IsNearlyZero(edgeALen, Tolerance)))
		{
			return;
		}
		FVector2D edgeADir = edgeADelta / edgeALen;
		int32 edgeRayHits = 0;
		bool bEdgeAOnPolyB = false;

		for (int32 edgeBIdx1 = 0; edgeBIdx1 < numPointsB; ++edgeBIdx1)
		{
			int32 edgeBIdx2 = (edgeBIdx1 + 1) % numPointsB;
			const FVector2D &edgeBPoint1 = PolyB[edgeBIdx1];
			const FVector2D &edgeBPoint2 = PolyB[edgeBIdx2];
			FVector2D edgeBDelta = edgeBPoint2 - edgeBPoint1;
			float edgeBLen = edgeBDelta.Size();
			if (!ensure(!FMath::IsNearlyZero(edgeBLen, Tolerance)))
			{
				return;
			}
			FVector2D edgeBDir = edgeBDelta / edgeBLen;

			FVector2D edgeIntersection;
			float rayADist, rayBDist;
			bool bEdgesColinear;
			if (RayIntersection2D(edgeAPoint1, edgeADir, edgeBPoint1, edgeBDir,
				edgeIntersection, rayADist, rayBDist, bEdgesColinear, false, Tolerance))
			{
				bool bRayBHitOnSegment = FMath::IsWithinInclusive(rayBDist, -Tolerance, edgeBLen + Tolerance);

				// If the two edges are colinear, and the source edge lines on the target edge,
				// then skip the rest of its intersections with the target polygon
				if (bEdgesColinear && bRayBHitOnSegment)
				{
					bEdgeAOnPolyB = true;
					bOutTouching = true;
					break;
				}

				if ((rayADist > Tolerance) && bRayBHitOnSegment)
				{
					// If the source ray hit a target edge well within the length of the source edge,
					// then the source edge sufficiently crosses over the target edge, and the polygons are overlapping.
					if (rayADist < (edgeALen - Tolerance))
					{
						bOutOverlapping = true;
					}
					// Otherwise the inner ray hit an outer edge, so keep track of it
					// so we can determine whether the origin is inside or outside of the outer poly.
					// The epsilon is interpreted to weight toward this option, so that polygons with edges within
					// Tolerance distance of each other can be more easily interpreted as touching if necessary,
					// and marked as overlapping if they significantly cross over each other.
					else
					{
						++edgeRayHits;
					}
				}
			}
		}

		// If the inner ray hit 0 (or an even number of) outer edges, then its source point must be outside the outer polygon
		if (!bEdgeAOnPolyB && ((edgeRayHits % 2) == 0))
		{
			bAOutsideOfB = true;
		}
	}

	// If we've determined that no edges intersect with each other, and none of PolyA's points are outside of PolyB,
	// then the PolyA must be inside of PolyB
	bOutAInB = !bAOutsideOfB;
}

bool UModumateGeometryStatics::RayIntersection3D(const FVector& RayOriginA, const FVector& RayDirectionA, const FVector& RayOriginB, const FVector& RayDirectionB,
	FVector& OutIntersectionPoint, float &OutRayADist, float &OutRayBDist, bool bRequirePositive, float Tolerance)
{
	// Find the plane shared by the ray directions
	FVector planeNormal = RayDirectionA ^ RayDirectionB;
	if (!planeNormal.Normalize(Tolerance))
	{
		return false;
	}

	float planeDistA = RayOriginA | planeNormal;
	float planeDistB = RayOriginB | planeNormal;

	// Ray origins must be in the same plane as their rays' shared plane
	if (!FMath::IsNearlyEqual(planeDistA, planeDistB, Tolerance))
	{
		return false;
	}

	FVector originDelta = RayOriginB - RayOriginA;
	float originDist = originDelta.Size();

	if (FMath::IsNearlyZero(originDist, Tolerance))
	{
		OutRayADist = OutRayBDist = 0.0f;
		OutIntersectionPoint = RayOriginA;
		return true;
	}

	// Determine ray perpendicular vectors, for intersection
	FVector rayNormalA = (RayDirectionA ^ planeNormal);
	FVector rayNormalB = (RayDirectionB ^ planeNormal);

	if (!rayNormalA.IsUnit(Tolerance) || !rayNormalB.IsUnit(Tolerance))
	{
		return false;
	}

	// Compute intersection between rays, using FMath::RayPlaneIntersection as a basis
	// float Distance = (( PlaneOrigin - RayOrigin ) | PlaneNormal) / (RayDirection | PlaneNormal);
	OutRayADist = (originDelta | rayNormalB) / (RayDirectionA | rayNormalB);
	OutRayBDist = (-originDelta | rayNormalA) / (RayDirectionB | rayNormalA);

	// Potentially throw out results that are behind the origins of the rays
	if (bRequirePositive && ((OutRayADist < 0.0f) || (OutRayBDist < 0.0f)))
	{
		return false;
	}

	// Ensure that intersection points are consistent
	FVector rayAOnBPoint = RayOriginA + (OutRayADist * RayDirectionA);
	FVector rayBOnAPoint = RayOriginB + (OutRayBDist * RayDirectionB);
	if (!FVector::PointsAreNear(rayAOnBPoint, rayBOnAPoint, Tolerance))
	{
		return false;
	}

	OutIntersectionPoint = rayAOnBPoint;
	return true;
}

bool UModumateGeometryStatics::GetSegmentPolygonIntersections(const FVector2D &SegmentStart, const FVector2D &SegmentEnd, const TArray<FPolyHole2D> &Polygons,
	TArray<FVector2D> &OutPoints, TArray<int32> &OutMergedPolyIndices, TArray<bool> &OutMergedPolygons, TArray<FVector2D> &OutSegments,
	TArray<int32> &OutPointsHoleIndices)
{
	OutPoints.Reset();
	OutMergedPolyIndices.Reset();
	OutMergedPolygons.Reset();
	OutSegments.Reset();
	OutPointsHoleIndices.Reset();

	TMap<int32, FVector2D> polyMergedSegmentDists;
	TMap<int32, int32> polyMergedSegmentIndices;
	TMap<int32, bool> polyMergeCoincident;

	FVector2D segmentDelta = SegmentEnd - SegmentStart;
	float segmentLength = segmentDelta.Size();
	if (FMath::IsNearlyZero(segmentLength))
	{
		return false;
	}
	FVector2D segmentDir = segmentDelta / segmentLength;

	OutPoints.Add(SegmentStart);
	OutPointsHoleIndices.Add(INDEX_NONE);

	for (int32 polyIdx = 0; polyIdx < Polygons.Num(); ++polyIdx)
	{
		const FPolyHole2D &polygon = Polygons[polyIdx];
		bool &bMergedPolygon = OutMergedPolygons.Add_GetRef(false);

		// iterate through the segments of the polygon to see if any touch or cross the initial segment
		int32 numPolyPoints = polygon.Points.Num();
		for (int32 polyPointIdx1 = 0; polyPointIdx1 < numPolyPoints; ++polyPointIdx1)
		{
			int32 polyPointIdx2 = (polyPointIdx1 + 1) % numPolyPoints;
			const FVector2D &polyPoint1 = polygon.Points[polyPointIdx1];
			const FVector2D &polyPoint2 = polygon.Points[polyPointIdx2];
			FVector2D polySegmentDelta = polyPoint2 - polyPoint1;
			float polySegmentLength = polySegmentDelta.Size();
			if (FMath::IsNearlyZero(polySegmentLength))
			{
				return false;
			}
			FVector2D polySegmentDir = polySegmentDelta / polySegmentLength;
			FVector2D startToPoint1 = polyPoint1 - SegmentStart;
			float point1DistOnSegment = startToPoint1 | segmentDir;

			// Make sure the polygon point lies between the start and end points of the input segment
			if (!FMath::IsWithinInclusive(point1DistOnSegment, PLANAR_DOT_EPSILON, segmentLength - PLANAR_DOT_EPSILON))
			{
				continue;
			}

			FVector2D point1OnSegment = SegmentStart + point1DistOnSegment * segmentDir;
			float point1DistFromSegment = FVector2D::Distance(point1OnSegment, polyPoint1);
			bool bPoint1OnSegment = FMath::IsNearlyZero(point1DistFromSegment, PLANAR_DOT_EPSILON);
			float segmentsDot = segmentDir | polySegmentDir;

			// If this poly point and its next point form a segment that lies on the original segment,
			// then note that this segment will be used to merge.
			if ((FMath::Abs(segmentsDot) >= THRESH_NORMALS_ARE_PARALLEL) && bPoint1OnSegment)
			{
				bool bSegmentCoincident = (segmentsDot > 0.0f);
				float point2DistOnSegment = (polyPoint2 - SegmentStart) | segmentDir;
				ensureAlways(bSegmentCoincident == (point1DistOnSegment < point2DistOnSegment));

				bMergedPolygon = true;
				OutMergedPolyIndices.Add(polyIdx);
				float startOnSegment = FMath::Min(point1DistOnSegment, point2DistOnSegment);
				float endOnSegment = FMath::Max(point1DistOnSegment, point2DistOnSegment);
				polyMergedSegmentDists.Add(polyIdx, FVector2D(startOnSegment, endOnSegment));
				polyMergedSegmentIndices.Add(polyIdx, polyPointIdx1);
				polyMergeCoincident.Add(polyIdx, bSegmentCoincident);
				continue;
			}
		}
	}

	int32 numMergedPolys = OutMergedPolyIndices.Num();
	OutMergedPolyIndices.Sort([&polyMergedSegmentDists](const int32 &polyIdxA, const int32 &polyIdxB) {
		return polyMergedSegmentDists[polyIdxA].X < polyMergedSegmentDists[polyIdxB].X;
	});

	float lastPolyEndOnSegment = 0.0f;

	for (const int32 &polyIdx : OutMergedPolyIndices)
	{
		// keep track of the solid segments between merged polygons
		float polyStartOnSegment = polyMergedSegmentDists[polyIdx].X;
		OutSegments.Add(FVector2D(lastPolyEndOnSegment, polyStartOnSegment));
		lastPolyEndOnSegment = polyMergedSegmentDists[polyIdx].Y;

		// add points from each merged polygon
		const FPolyHole2D &polygon = Polygons[polyIdx];
		int32 numPolyPoints = polygon.Points.Num();

		int32 segmentStartIdx = polyMergedSegmentIndices[polyIdx];
		bool bMergeCoincident = polyMergeCoincident[polyIdx];

		for (int32 mergedIdx = 0; mergedIdx < numPolyPoints; ++mergedIdx)
		{
			int32 pointIdx = bMergeCoincident ?
				((segmentStartIdx + numPolyPoints - mergedIdx) % numPolyPoints) :
				((segmentStartIdx + 1 + mergedIdx) % numPolyPoints);

			OutPoints.Add(polygon.Points[pointIdx]);
			OutPointsHoleIndices.Add(polyIdx);
		}
	}

	OutPoints.Add(SegmentEnd);
	OutPointsHoleIndices.Add(INDEX_NONE);
	OutSegments.Add(FVector2D(lastPolyEndOnSegment, segmentLength));

	return true;
}

bool UModumateGeometryStatics::TranslatePolygonEdge(const TArray<FVector> &PolyPoints, const FVector &PolyNormal, int32 EdgeStartIdx, float Translation, FVector &OutStartPoint, FVector &OutEndPoint)
{
	// Make sure we can at least index the polygon
	if ((PolyPoints.Num() < 3) || !PolyPoints.IsValidIndex(EdgeStartIdx))
	{
		return false;
	}

	int32 numPoints = PolyPoints.Num();
	int32 edgeEndIdx = (EdgeStartIdx + 1) % numPoints;
	int32 preEdgeIdx = (EdgeStartIdx + numPoints - 1) % numPoints;
	int32 postEdgeIdx = (edgeEndIdx + 1) % numPoints;

	const FVector &preEdgePoint = PolyPoints[preEdgeIdx];
	const FVector &edgeStartPoint = PolyPoints[EdgeStartIdx];
	const FVector &edgeEndPoint = PolyPoints[edgeEndIdx];
	const FVector &postEdgePoint = PolyPoints[postEdgeIdx];

	OutStartPoint = edgeStartPoint;
	OutEndPoint = edgeEndPoint;

	// Get the target edge, the edge leading up to its start point, and the edge leading up to its end point
	FVector edgeDelta = (edgeEndPoint - edgeStartPoint);
	float edgeLength = edgeDelta.Size();
	FVector preEdgeDelta = (edgeStartPoint - preEdgePoint);
	float preEdgeLength = preEdgeDelta.Size();
	FVector postEdgeDelta = (edgeEndPoint - postEdgePoint);
	float postEdgeLength = postEdgeDelta.Size();

	// First, make sure that the edge before and after the target extrusion edge are not parallel, otherwise the edge between them can't be extruded.
	if (FMath::IsNearlyZero(edgeLength) || FMath::IsNearlyZero(preEdgeLength) || FMath::IsNearlyZero(postEdgeLength))
	{
		return false;
	}

	FVector edgeDir = edgeDelta / edgeLength;
	FVector preEdgeDir = preEdgeDelta / preEdgeLength;
	FVector postEdgeDir = postEdgeDelta / postEdgeLength;
	if (FVector::Parallel(preEdgeDir, edgeDir) || FVector::Parallel(edgeDir, postEdgeDir))
	{
		return false;
	}

	// Make sure the edges involved are all coplanar
	FVector preEdgeCross = (preEdgeDir ^ edgeDir).GetSafeNormal();
	FVector postEdgeCross = (edgeDir ^ postEdgeDir).GetSafeNormal();
	if (!FVector::Parallel(preEdgeCross, PolyNormal) || !FVector::Parallel(postEdgeCross, PolyNormal) || !FVector::Orthogonal(edgeDir, PolyNormal))
	{
		return false;
	}

	// Early-exit if we aren't actually doing the translation, since we've verified planarity
	if (Translation == 0.0f)
	{
		return true;
	}

	// Double check that our edge extension won't result in any divisions by zero, would should have been caught by the earlier checks
	FVector edgeNormal = edgeDir ^ PolyNormal;
	float preEdgeInvExtension = edgeNormal | preEdgeDir;
	float postEdgeInvExtension = edgeNormal | postEdgeDir;
	if (!ensure(!FMath::IsNearlyZero(preEdgeInvExtension) && !FMath::IsNearlyZero(postEdgeInvExtension)))
	{
		return false;
	}

	float preEdgeExtension = Translation / preEdgeInvExtension;
	float postEdgeExtension = Translation / postEdgeInvExtension;
	float newPreEdgeLength = preEdgeLength + preEdgeExtension;
	float newPostEdgeLength = postEdgeLength + postEdgeExtension;

	// Check if either of the pre- or post- edges would have been removed by the translation
	if ((newPreEdgeLength < KINDA_SMALL_NUMBER) || (newPostEdgeLength < KINDA_SMALL_NUMBER))
	{
		return false;
	}

	OutStartPoint = preEdgePoint + newPreEdgeLength * preEdgeDir;
	OutEndPoint = postEdgePoint + newPostEdgeLength * postEdgeDir;

	// Don't allow removing the edge that's being translated, if its points come together to a single point
	FVector newEdgeDelta = OutEndPoint - OutStartPoint;
	float newEdgeLength = newEdgeDelta.Size();
	if (FMath::IsNearlyZero(newEdgeLength))
	{
		return false;
	}

	// Don't allow the points to pass each other, which would result in the edge flipping direction
	FVector newEdgeDir = newEdgeDelta / newEdgeLength;
	if (!FVector::Coincident(newEdgeDir, edgeDir))
	{
		return false;
	}

	return true;
}

void UModumateGeometryStatics::FindBasisVectors(FVector &OutAxisX, FVector &OutAxisY, const FVector &AxisZ)
{
	// If the Z basis is vertical, then we can pick some simple X and Y basis vectors
	if (FVector::Parallel(AxisZ, FVector::UpVector))
	{
		OutAxisX = FVector(1.0f, 0.0f, 0.0f);

		if (AxisZ.Z > 0.0f)
		{
			OutAxisY = FVector(0.0f, 1.0f, 0.0f);
		}
		else
		{
			OutAxisY = FVector(0.0f, -1.0f, 0.0f);
		}
	}
	// Otherwise, we want to calculate X and Y basis vectors such that the Y basis align to -Z (for UV readability)
	else
	{
		OutAxisX = (AxisZ ^ FVector::UpVector).GetSafeNormal();
		OutAxisY = AxisZ ^ OutAxisX;
	}
}

bool UModumateGeometryStatics::FindShortestDistanceBetweenRays(
	const FVector &RayOriginA, 
	const FVector &RayDirectionA, 
	const FVector &RayOriginB, 
	const FVector &RayDirectionB, 
	FVector &OutRayInterceptA, 
	FVector &OutRayInterceptB, 
	float &outDistance)
{
	// The shortest distance between two rays is defined by two points (one on each ray) that define a line segment perpendicular to both (cross product)

	FVector dirCross = FVector::CrossProduct(RayDirectionA, RayDirectionB);
	float dirCrossMag = dirCross.Size();

	// If the cross product magnitude is zero, then the rays are parallel and their distance
	// Distance is still valid but intercepts are not so return false
	if (FMath::IsNearlyZero(dirCrossMag, RAY_INTERSECT_TOLERANCE))
	{
		outDistance = FMath::PointDistToLine(RayOriginA, RayDirectionA, RayOriginB);
		return false;
	}

	// With the common normal between the rays determined, the intercept point for ray A is the line/plane intersection of ray A with the plane that
	// contains both ray B and the common normal .. and of course vice versa for the intercept point for ray B

	OutRayInterceptA = FMath::LinePlaneIntersection(RayOriginA, RayOriginA+RayDirectionA, RayOriginB, FVector::CrossProduct(dirCross, RayDirectionB));
	OutRayInterceptB = FMath::LinePlaneIntersection(RayOriginB, RayOriginB+RayDirectionB, RayOriginA, FVector::CrossProduct(dirCross, RayDirectionA));

	// Now that we've computed the intersections, project them on to the original rays more accurately
	OutRayInterceptA = RayOriginA + (OutRayInterceptA - RayOriginA).ProjectOnToNormal(RayDirectionA);
	OutRayInterceptB = RayOriginB + (OutRayInterceptB - RayOriginB).ProjectOnToNormal(RayDirectionB);

	// Distance between rays = distance between intercepts
	outDistance = (OutRayInterceptA - OutRayInterceptB).Size();

	return true;
}

// Given a corner point, its neighboring points, the normals of its neighboring edges, and offsets along those edge normals,
// find the resulting corner position based on the intersection of the new edges.
bool UModumateGeometryStatics::GetExtendedCorner(FVector &RefCorner, const FVector &PrevPoint, const FVector &NextPoint,
	const FVector &PrevEdgeNormal, const FVector &NextEdgeNormal, float PrevEdgeExtension, float NextEdgeExtension)
{
	FVector prevDelta = RefCorner - PrevPoint;
	float prevLen = prevDelta.Size();
	FVector nextDelta = NextPoint - RefCorner;
	float nextLen = nextDelta.Size();

	if (FMath::IsNearlyZero(prevLen) || FMath::IsNearlyZero(nextLen))
	{
		return false;
	}

	FVector prevRayDir = prevDelta / prevLen;
	FVector nextRayDir = -nextDelta / nextLen;

	FVector prevOrigin = PrevPoint - PrevEdgeExtension * PrevEdgeNormal;
	FVector nextOrigin = NextPoint - NextEdgeExtension * NextEdgeNormal;

	FVector2D rayDists;
	return UModumateGeometryStatics::RayIntersection3D(prevOrigin, prevRayDir, nextOrigin, nextRayDir,
		RefCorner, rayDists.X, rayDists.Y);
}

bool UModumateGeometryStatics::CompareVectors(const TArray<FVector2D> &vectorsA, const TArray<FVector2D> &vectorsB, float tolerance)
{
	if (vectorsA.Num() != vectorsB.Num())
	{
		return false;
	}

	int32 numPoints = vectorsA.Num();
	for (int32 i = 0; i < numPoints; ++i)
	{
		if (!vectorsA[i].Equals(vectorsB[i], tolerance))
		{
			return false;
		}
	}

	return true;
}

bool UModumateGeometryStatics::AnalyzeCachedPositions(const TArray<FVector> &InPositions, FPlane &OutPlane, FVector &OutAxis2DX, FVector &OutAxis2DY, TArray<FVector2D> &Out2DPositions, FVector &OutCenter, bool bUpdatePlane)
{
	if (InPositions.Num() < 3)
	{
		return false;
	}

	// Find the plane that defines this face's points, in which they are oriented clockwise
	if (bUpdatePlane)
	{
		bool bPlanar = UModumateGeometryStatics::GetPlaneFromPoints(InPositions, OutPlane);

		if (!(bPlanar && OutPlane.IsNormalized()))
		{
			return false;
		}
	}

	// Find the 2D projection of our points, for convenient intersection/triangulation computation later
	UModumateGeometryStatics::FindBasisVectors(OutAxis2DX, OutAxis2DY, OutPlane);

	Out2DPositions.Reset(InPositions.Num());
	for (const FVector &position : InPositions)
	{
		Out2DPositions.Add(ProjectPoint2D(position, OutAxis2DX, OutAxis2DY, InPositions[0]));
	}

	// Find a central point to use for casting rays
	// (essential for concave polygons, where the centroid would not suffice)
	FVector2D furthestPoint;
	if (UModumateGeometryStatics::FindPointFurthestFromPolyEdge(Out2DPositions, furthestPoint))
	{
		OutCenter = InPositions[0] +
			(furthestPoint.X * OutAxis2DX) +
			(furthestPoint.Y * OutAxis2DY);
	}
	else
	{
		OutCenter = Algo::Accumulate(InPositions, FVector::ZeroVector,[](const FVector &c, const FVector &p) { return c + p; }) / InPositions.Num();
	}

	return true;
}

// Given a 3D box and a plane, attempt to find a slice of the box that is on the plane
bool UModumateGeometryStatics::SliceBoxWithPlane(const FBox &InBox, const FVector &InOrigin, const FVector &InNormal, FVector &OutAxisX, FVector &OutAxisY, FBox2D &OutSlice)
{
	FindBasisVectors(OutAxisX, OutAxisY, InNormal);
	FPlane plane = FPlane(InOrigin, InNormal);

	if (!FMath::PlaneAABBIntersection(plane, InBox))
	{
		return false;
	}
	// 8 points on an FBox
	TArray<FVector> points = {
		FVector(InBox.Min.X, InBox.Min.Y, InBox.Min.Z),
		FVector(InBox.Min.X, InBox.Min.Y, InBox.Max.Z),
		FVector(InBox.Min.X, InBox.Max.Y, InBox.Min.Z),
		FVector(InBox.Min.X, InBox.Max.Y, InBox.Max.Z),
		FVector(InBox.Max.X, InBox.Min.Y, InBox.Min.Z),
		FVector(InBox.Max.X, InBox.Min.Y, InBox.Max.Z),
		FVector(InBox.Max.X, InBox.Max.Y, InBox.Min.Z),
		FVector(InBox.Max.X, InBox.Max.Y, InBox.Max.Z),
	};

	// 12 lines connecting the 8 points
	TArray<TPair<FVector, FVector>> lines = {
		TPair<FVector, FVector>(points[0], points[1]),
		TPair<FVector, FVector>(points[0], points[2]),
		TPair<FVector, FVector>(points[1], points[3]),
		TPair<FVector, FVector>(points[2], points[3]),
		TPair<FVector, FVector>(points[4], points[5]),
		TPair<FVector, FVector>(points[4], points[6]),
		TPair<FVector, FVector>(points[5], points[7]),
		TPair<FVector, FVector>(points[6], points[7]),
		TPair<FVector, FVector>(points[0], points[4]),
		TPair<FVector, FVector>(points[1], points[5]),
		TPair<FVector, FVector>(points[2], points[6]),
		TPair<FVector, FVector>(points[3], points[7])
	};

	// Calculate all intersections with the plane
	TArray<FVector2D> intersections;
	for (auto& line : lines)
	{
		FVector point;
		if (FMath::SegmentPlaneIntersection(line.Key, line.Value, plane, point))
		{
			intersections.Add(UModumateGeometryStatics::ProjectPoint2D(point, OutAxisX, OutAxisY, InOrigin));
		}
	}

	// with three intersections the slice is meaningful
	if (intersections.Num() >= 3)
	{
		OutSlice = FBox2D(intersections);
		return true;
	}

	return false;
}

bool UModumateGeometryStatics::ConvertProcMeshToLinesOnPlane(UProceduralMeshComponent* InProcMesh, FVector PlanePosition, FVector PlaneNormal, TArray<TPair<FVector, FVector>> &OutEdges)
{
	// This code is mostly copied from UKismetProceduralMeshLibrary::SliceProceduralMesh.
	// This function differs because it does not create any meshes, instead it creates and outputs edges that 
	// represent where the triangles intersect with the input plane.

	if (InProcMesh == nullptr)
	{
		return false;
	}

	// Transform plane from world to local space
	FTransform ProcCompToWorld = InProcMesh->GetComponentToWorld();
	FVector LocalPlanePos = ProcCompToWorld.InverseTransformPosition(PlanePosition);
	FVector LocalPlaneNormal = ProcCompToWorld.InverseTransformVectorNoScale(PlaneNormal);
	LocalPlaneNormal = LocalPlaneNormal.GetSafeNormal(); // Ensure normalized

	FPlane SlicePlane(LocalPlanePos, LocalPlaneNormal);

	for (int32 SectionIndex = 0; SectionIndex < InProcMesh->GetNumSections(); SectionIndex++)
	{
		FProcMeshSection* BaseSection = InProcMesh->GetProcMeshSection(SectionIndex);
		// If we have a section, and it has some valid geom
		if (BaseSection != nullptr && BaseSection->ProcIndexBuffer.Num() > 0 && BaseSection->ProcVertexBuffer.Num() > 0)
		{
			// Compare bounding box of section with slicing plane
			//int32 BoxCompare = UKismetProceduralMeshLibrary::BoxPlaneCompare(BaseSection->SectionLocalBox, SlicePlane);

			// Body of BoxPlaneCompare is pasted here
			int32 BoxCompare = 0;
			FVector BoxCenter, BoxExtents;
			BaseSection->SectionLocalBox.GetCenterAndExtents(BoxCenter, BoxExtents);

			// Find distance of box center from plane
			float BoxCenterDist = SlicePlane.PlaneDot(BoxCenter);

			// See size of box in plane normal direction
			float BoxSize = FVector::BoxPushOut(SlicePlane, BoxExtents);

			if (BoxCenterDist > BoxSize)
			{
				BoxCompare = 1;
			}
			else if (BoxCenterDist < -BoxSize)
			{
				BoxCompare = -1;
			}
			else
			{
				BoxCompare = 0;
			}

			// only consider meshes that are intersecting the plane
			if (BoxCompare != 0)
			{
				continue;
			}

			int32 sideIndex = 0;
			TMap<int32, int32> BaseToSlicedVertIndex;
			const int32 NumBaseVerts = BaseSection->ProcVertexBuffer.Num();

			// Distance of each base vert from slice plane
			TArray<float> VertDistance;
			VertDistance.AddUninitialized(NumBaseVerts);

			// Build vertex buffer 
			for (int32 BaseVertIndex = 0; BaseVertIndex < NumBaseVerts; BaseVertIndex++)
			{
				FProcMeshVertex& BaseVert = BaseSection->ProcVertexBuffer[BaseVertIndex];

				// Calc distance from plane
				VertDistance[BaseVertIndex] = SlicePlane.PlaneDot(BaseVert.Position);

				// See if vert is being kept in this section
				if (VertDistance[BaseVertIndex] > 0.f)
				{
					// Copy to sliced v buffer
					//int32 SlicedVertIndex = NewSection.ProcVertexBuffer.Add(BaseVert);
					int32 SlicedVertIndex = sideIndex;
					sideIndex++;

					// Add to map
					BaseToSlicedVertIndex.Add(BaseVertIndex, SlicedVertIndex);
				}
			}

		   	// Iterate over base triangles (ie 3 indices at a time)
			for (int32 BaseIndex = 0; BaseIndex < BaseSection->ProcIndexBuffer.Num(); BaseIndex += 3)
			{
				int32 BaseV[3]; // Triangle vert indices in original mesh
				int32* SlicedV[3]; // Pointers to vert indices in new v buffer

				// For each vertex..
				for (int32 i = 0; i < 3; i++)
				{
					// Get triangle vert index
					BaseV[i] = BaseSection->ProcIndexBuffer[BaseIndex + i];
					// Look up in sliced v buffer
					SlicedV[i] = BaseToSlicedVertIndex.Find(BaseV[i]);
				}

				float PlaneDist[3];
				PlaneDist[0] = VertDistance[BaseV[0]];
				PlaneDist[1] = VertDistance[BaseV[1]];
				PlaneDist[2] = VertDistance[BaseV[2]];

				// only consider triangles that are partially culled by the plane
				if ((SlicedV[0] != nullptr && SlicedV[1] != nullptr && SlicedV[2] != nullptr) ||
					(SlicedV[0] == nullptr && SlicedV[1] == nullptr && SlicedV[2] == nullptr))
				{
					continue;
				}

				TArray<FVector> newPositions;

				// create a line representing the intersection
				// This function is only interested in the positions, SliceProcedural mesh would create a mesh vertex instead
				for (int32 EdgeIdx = 0; EdgeIdx < 3; EdgeIdx++)
				{
					int32 ThisVert = EdgeIdx;
					int32 NextVert = (EdgeIdx + 1) % 3;

					// if the vertices are on different sides of the plane, create a new vertex
					if ((SlicedV[EdgeIdx] == nullptr) != (SlicedV[NextVert] == nullptr))
					{
						FVector ThisPos = BaseSection->ProcVertexBuffer[BaseV[ThisVert]].Position;
						FVector NextPos = BaseSection->ProcVertexBuffer[BaseV[NextVert]].Position;
						float Alpha = -PlaneDist[ThisVert] / (PlaneDist[NextVert] - PlaneDist[ThisVert]);

						newPositions.Add(FMath::Lerp(ThisPos, NextPos, Alpha));
					}

				}

				// A triangle intersects a plane at most twice, and for a meaningful line to be created 
				// the triangle must intersect the plane at least twice
				if (newPositions.Num() == 2)
				{
					// Reset to world coordinates
					FVector edgeStart = ProcCompToWorld.TransformPosition(newPositions[0]);
					FVector edgeEnd = ProcCompToWorld.TransformPosition(newPositions[1]);
					OutEdges.Add(TPair<FVector, FVector>(edgeStart, edgeEnd));
				}
			}
		}
	}

	return true;
}

bool UModumateGeometryStatics::GetAxisForVector(const FVector &Normal, EAxis::Type &OutAxis, float &OutSign)
{
	OutAxis = EAxis::Type::None;
	OutSign = 0.0f;

	if (!Normal.IsNormalized())
	{
		return false;
	}

	for (int32 axisIdx = (int32)EAxis::Type::X; axisIdx <= (int32)EAxis::Type::Z; ++axisIdx)
	{
		EAxis::Type axisType = (EAxis::Type)axisIdx;
		float axisValue = Normal.GetComponentForAxis(axisType);
		if (FMath::IsNearlyEqual(FMath::Abs(axisValue), 1.0f))
		{
			OutAxis = axisType;
			OutSign = FMath::Sign(axisValue);
			return true;
		}
	}

	return false;
}

bool UModumateGeometryStatics::GetEdgeIntersections(const TArray<FVector> &Positions, const FVector &IntersectionOrigin, const FVector &IntersectionDir, TArray<Modumate::FEdgeIntersection> &OutEdgeIntersections, float Epsilon)
{
	OutEdgeIntersections.Reset();
	int32 numSourceEdges = Positions.Num();

	// mark vertices that are part of an intersection so that the positions are not considered twice
	TSet<int32> intersectedIdxs;

	for (int32 edgeIdxA = 0; edgeIdxA < numSourceEdges; ++edgeIdxA)
	{
		int32 edgeIdxB = (edgeIdxA + 1) % numSourceEdges;

		if (intersectedIdxs.Contains(edgeIdxA) || intersectedIdxs.Contains(edgeIdxB))
		{
			continue;
		}

		const FVector &edgePosA = Positions[edgeIdxA];
		const FVector &edgePosB = Positions[edgeIdxB];
		FVector edgeDelta = edgePosB - edgePosA;
		float edgeDist = edgeDelta.Size();
		FVector edgeDir = edgeDelta / edgeDist;

		FVector edgeIntersection;
		float distAlongRay, distAlongEdge;

		bool bIntersect = UModumateGeometryStatics::RayIntersection3D(IntersectionOrigin, IntersectionDir,
			edgePosA, edgeDir, edgeIntersection, distAlongRay, distAlongEdge, false);
		bool b1 = distAlongEdge > -Epsilon;
		bool b2 = distAlongEdge < (edgeDist + Epsilon);
		if (bIntersect && b1 && b2)
		{
			bool bNearVertexA = distAlongEdge < Epsilon;
			bool bNearVertexB = distAlongEdge > (edgeDist - Epsilon);

			if (bNearVertexA)
			{
				intersectedIdxs.Add(edgeIdxA);
				OutEdgeIntersections.Add(Modumate::FEdgeIntersection(edgeIdxA, edgeIdxB, distAlongRay, distAlongEdge, edgeIntersection));
			}
			// if the intersection is on a vertex, create the intersection so that the EdgeIdxA field holds the vertexID
			else if (bNearVertexB)
			{
				intersectedIdxs.Add(edgeIdxB);
				OutEdgeIntersections.Add(Modumate::FEdgeIntersection(edgeIdxB, (edgeIdxB + 1) % numSourceEdges, distAlongRay, distAlongEdge, edgeIntersection));
			}
			else
			{
				OutEdgeIntersections.Add(Modumate::FEdgeIntersection(edgeIdxA, edgeIdxB, distAlongRay, distAlongEdge, edgeIntersection));
			}
		}

	}

	OutEdgeIntersections.Sort();
	return (OutEdgeIntersections.Num() > 0);
	
}

bool UModumateGeometryStatics::AreConsecutivePoints2DRepeated(const TArray<FVector2D> &Points, float Tolerance)
{
	int32 numPoints = Points.Num();
	for (int32 pointIdxA = 0; pointIdxA < numPoints; ++pointIdxA)
	{
		int32 pointIdxB = (pointIdxA + 1) % numPoints;
		if (Points[pointIdxA].Equals(Points[pointIdxB], Tolerance))
		{
			return true;
		}
	}

	return false;
}

bool UModumateGeometryStatics::AreConsecutivePointsRepeated(const TArray<FVector> &Points, float Tolerance)
{
	int32 numPoints = Points.Num();
	for (int32 pointIdxA = 0; pointIdxA < numPoints; ++pointIdxA)
	{
		int32 pointIdxB = (pointIdxA + 1) % numPoints;
		if (Points[pointIdxA].Equals(Points[pointIdxB], Tolerance))
		{
			return true;
		}
	}

	return false;
}

bool UModumateGeometryStatics::IsPolygon2DValid(const TArray<FVector2D> &Points, FFeedbackContext* InWarn)
{
	int32 numPoints = Points.Num();

	if (numPoints < 3)
	{
		return false;
	}

	for (int32 segIdxAStart = 0; segIdxAStart < numPoints; ++segIdxAStart)
	{
		int32 segIdxAEnd = (segIdxAStart + 1) % numPoints;
		const FVector2D &segAStart = Points[segIdxAStart];
		const FVector2D &segAEnd = Points[segIdxAEnd];

		if (segAStart.Equals(segAEnd))
		{
			if (InWarn)
			{
				InWarn->Logf(ELogVerbosity::Error, TEXT("Line segment (%s, %s) is zero-length!"), *segAStart.ToString(), *segAEnd.ToString());
			}
			return false;
		}

		for (int32 segIdxBStart = 0; segIdxBStart < numPoints; ++segIdxBStart)
		{
			int32 segIdxBEnd = (segIdxBStart + 1) % numPoints;
			const FVector2D &segBStart = Points[segIdxBStart];
			const FVector2D &segBEnd = Points[segIdxBEnd];
			FVector2D intersectionPoint;

			if ((segIdxAStart != segIdxBStart) &&
				!segAStart.Equals(segBStart) && !segAStart.Equals(segBEnd) &&
				!segAEnd.Equals(segBStart) && !segAEnd.Equals(segBEnd))
			{
				if (UModumateGeometryStatics::SegmentIntersection2D(segAStart, segAEnd, segBStart, segBEnd, intersectionPoint, RAY_INTERSECT_TOLERANCE))
				{
					if (InWarn)
					{
						InWarn->Logf(ELogVerbosity::Error, TEXT("Line segments (%s, %s) and (%s, %s) intersect; invalid simple polygon!"),
							*segAStart.ToString(), *segAEnd.ToString(), *segBStart.ToString(), *segBEnd.ToString());
					}
					return false;
				}
			}
		}
	}

	return true;
}

bool UModumateGeometryStatics::IsPolygonValid(const TArray<FVector> &Points3D, FPlane PolyPlane, FFeedbackContext* InWarn)
{
	if (Points3D.Num() < 3)
	{
		return false;
	}

	FVector polyOrigin = Points3D[0];

	// If a plane is provided, make sure the provided polygon points all lie on it
	if (!PolyPlane.IsZero())
	{
		for (const FVector& point3D : Points3D)
		{
			if (!FMath::IsNearlyZero(PolyPlane.PlaneDot(point3D), PLANAR_DOT_EPSILON))
			{
				return false;
			}
		}
	}
	// Otherwise, make sure we can compute one from the given points
	else if (!UModumateGeometryStatics::GetPlaneFromPoints(Points3D, PolyPlane))
	{
		return false;
	}

	// Now, project the points to a plane so we can more easily compute segment intersection
	FVector axisX, axisY;
	UModumateGeometryStatics::FindBasisVectors(axisX, axisY, FVector(PolyPlane));

	TArray<FVector2D> points2D;
	for (const FVector& point3D : Points3D)
	{
		points2D.Add(UModumateGeometryStatics::ProjectPoint2D(point3D, axisX, axisY, polyOrigin));
	}

	return IsPolygon2DValid(points2D);
}
