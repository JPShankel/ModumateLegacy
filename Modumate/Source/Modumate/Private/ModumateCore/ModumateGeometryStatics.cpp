// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "ModumateGeometryStatics.h"

#include "Kismet/KismetMathLibrary.h"
#include "KismetProceduralMeshLibrary.h"
#include "ModumateFunctionLibrary.h"
#include "Algo/Accumulate.h"
#include "DrawDebugHelpers.h"

#include <algorithm>
#include <queue>
#include <iostream>
using namespace std;

#include "poly2tri/poly2tri.h"
#include "Reverse.h"

#define DEBUG_CHECK_LAYERS (!UE_BUILD_SHIPPING)


FLayerGeomDef::FLayerGeomDef(const TArray<FVector> &Points, float InThickness, const FVector &InNormal)
	: PointsA(Points)
	, Thickness(InThickness)
	, Normal(InNormal)
{
	int32 numPoints = Points.Num();

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
	CachePoints2D();
	bValid = true;
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

	CachePoints2D();
	bValid = true;
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

void FLayerGeomDef::CachePoints2D()
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

#define CHECK_REPEAT_POINTS (UE_BUILD_DEBUG)

bool UModumateGeometryStatics::TriangulateVerticesPoly2Tri(const TArray<FVector2D> &Vertices, const TArray<FPolyHole2D> &Holes,
	TArray<FVector2D> &OutVertices, TArray<int32> &OutTriangles, TArray<FVector2D> &OutPerimeter, TArray<bool> &OutMergedHoles,
	TArray<int32> &OutPerimeterVertexHoleIndices)
{
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
#if CHECK_REPEAT_POINTS
			// Debug check for repeating points in holes
			for (int32 k = 0; k < holeVertices.Num(); k++)
			{
				if (!ensureAlwaysMsgf((j == k) || !holeVertices[j].Equals(holeVertices[k], SMALL_NUMBER),
					TEXT("Error: tried to create a hole with identical vertices #%d and #%d: %s"),
					j, k, *holeVertices[j].ToString()))
				{
					bValidHole = false;
				}
			}
#endif
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
bool UModumateGeometryStatics::IsPointInPolygon(const FVector2D &Point, const TArray<FVector2D> &Polygon, float Tolerance)
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

		// Since this is inclusive, early return true if the test point is close to a polygon point
		if (FVector2D::Distance(Point, edgePoint1) <= Tolerance)
		{
			return true;
		}

		// Since this is inclusive, early return true if the test point is close to a polygon edge
		FVector2D projectedPoint = edgePoint1 + edgeDir * ((Point - edgePoint1) | edgeDir);
		if (FVector2D::Distance(Point, projectedPoint) <= Tolerance)
		{
			return true;
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

FTessellationPolygon::FTessellationPolygon(const FVector &InBaseUp, const FVector &InPolyNormal,
	const FVector &InStartPoint, const FVector &InStartDir,
	const FVector &InEndPoint, const FVector &InEndDir)
	: BaseUp(InBaseUp)
	, PolyNormal(InPolyNormal)
	, StartPoint(InStartPoint)
	, StartDir(InStartDir)
	, EndPoint(InEndPoint)
	, EndDir(InEndDir)
	, CachedEdgeDir((EndPoint - StartPoint).GetSafeNormal())
	, CachedEdgeIntersection(ForceInitToZero)
	, CachedPlane(StartPoint, PolyNormal)
	, CachedStartRayDist(0.0f)
	, CachedEndRayDist(0.0f)
	, bCachedEndsConverge(false)
{
	bCachedEndsConverge = UModumateGeometryStatics::RayIntersection3D(StartPoint, StartDir, EndPoint, EndDir,
		CachedEdgeIntersection, CachedStartRayDist, CachedEndRayDist, true);

	UpdatePolygonVerts();
}

bool FTessellationPolygon::ClipWithPolygon(const FTessellationPolygon &ClippingPolygon)
{
	// Make sure that this polygon's plane at least intersects with the other polygon's plane, before clipping
	FVector planeIntersectPoint, planeIntersectDir;
	if (!FMath::IntersectPlanes2(planeIntersectPoint, planeIntersectDir, CachedPlane, ClippingPolygon.CachedPlane))
	{
		return false;
	}

	// Make sure the intersection between the planes is not vertical (relative to the base)
	// TODO: remove this restriction when adjacent vertical roof faces are allowed
	if (FVector::Parallel(planeIntersectDir, BaseUp))
	{
		return false;
	}

	float lineStartLength = -FLT_MAX, lineEndLength = FLT_MAX;

	// Helper function to trim the line shared by both polygons by the start/end rays of each polygon
	// TODO: This could be more general if it supported arbitrary coplanar ray / polygon intersections
	auto trimLineByRay = [&lineStartLength, &lineEndLength, planeIntersectPoint, planeIntersectDir]
	(const FVector &RayOrigin, const FVector &RayDir, const FVector &BaseEdgeDir, bool bIsStartRay, const FPlane &OtherPlane) -> bool
	{
		if (FVector::Orthogonal(RayDir, OtherPlane) || FVector::Orthogonal(planeIntersectDir, BaseEdgeDir))
		{
			return false;
		}

		FVector rayOnOtherPlane = FMath::RayPlaneIntersection(RayOrigin, RayDir, OtherPlane);
		float hitDistOnSharedLine = (rayOnOtherPlane - planeIntersectPoint) | planeIntersectDir;

		bool bSharedLineAlignsWithEdge = ((planeIntersectDir | BaseEdgeDir) > 0.0f);

		if (bSharedLineAlignsWithEdge == bIsStartRay)
		{
			if (hitDistOnSharedLine > lineStartLength)
			{
				lineStartLength = hitDistOnSharedLine;
				return true;
			}
		}
		else
		{
			if (hitDistOnSharedLine < lineEndLength)
			{
				lineEndLength = hitDistOnSharedLine;
				return true;
			}
		}

		return false;
	};

	// Trim the shared line to try to produce a valid clipping line
	trimLineByRay(StartPoint, StartDir, CachedEdgeDir, true, ClippingPolygon.CachedPlane);
	trimLineByRay(EndPoint, EndDir, CachedEdgeDir, false, ClippingPolygon.CachedPlane);
	trimLineByRay(ClippingPolygon.StartPoint, ClippingPolygon.StartDir, ClippingPolygon.CachedEdgeDir, true, CachedPlane);
	trimLineByRay(ClippingPolygon.EndPoint, ClippingPolygon.EndDir, ClippingPolygon.CachedEdgeDir, false, CachedPlane);

	if ((lineStartLength > -FLT_MAX) && (lineEndLength < FLT_MAX) &&
		!FMath::IsNearlyEqual(lineStartLength, lineEndLength))
	{
		FVector clipStartPoint = planeIntersectPoint + lineStartLength * planeIntersectDir;
		FVector clipEndPoint = planeIntersectPoint + lineEndLength * planeIntersectDir;

		float edgeHeight = StartPoint | BaseUp;
		if (((clipStartPoint | BaseUp) < edgeHeight) || ((clipEndPoint | BaseUp) < edgeHeight))
		{
			return false;
		}

		// If this polygon is a triangle, make sure the clip points are inside the triangle.
		if (bCachedEndsConverge)
		{
			FVector clipStartBary = FMath::ComputeBaryCentric2D(clipStartPoint, StartPoint, EndPoint, CachedEdgeIntersection);
			FVector clipEndBary = FMath::ComputeBaryCentric2D(clipEndPoint, StartPoint, EndPoint, CachedEdgeIntersection);
			if ((clipStartBary.GetMin() < -KINDA_SMALL_NUMBER) || clipEndBary.GetMin() < -KINDA_SMALL_NUMBER)
			{
				return false;
			}
		}

		ClippingPoints.Add(clipStartPoint);
		ClippingPoints.Add(clipEndPoint);
		return true;
	}

	return false;
}

bool FTessellationPolygon::UpdatePolygonVerts()
{
	PolygonVerts.Reset();

	if (ClippingPoints.Num() == 0)
	{
		if (bCachedEndsConverge)
		{
			PolygonVerts.Add(StartPoint);
			PolygonVerts.Add(EndPoint);
			PolygonVerts.Add(CachedEdgeIntersection);
		}
		else
		{
			static float trapezoidWidth = 500.0f;

			PolygonVerts.Add(StartPoint);
			PolygonVerts.Add(EndPoint);
			PolygonVerts.Add(EndPoint + trapezoidWidth * StartDir);
			PolygonVerts.Add(StartPoint + trapezoidWidth * StartDir);
		}
	}
	else
	{
		PolygonVerts.Add(StartPoint);
		PolygonVerts.Add(EndPoint);

		auto testClipPointOnRay = [](const FVector &RayOrigin, const FVector &RayDir, const FVector &ClipPoint, float &LengthOnRay) -> bool
		{
			FVector clipPointFromOrigin = (ClipPoint - RayOrigin);
			LengthOnRay = clipPointFromOrigin | RayDir;
			FVector clipPointOnRay = RayOrigin + (LengthOnRay * RayDir);
			return ClipPoint.Equals(clipPointOnRay, RAY_INTERSECT_TOLERANCE);
		};

		// Naively consume clipping segments, assuming that they are attached to one another.
		// TODO: use generic polygon clipping if they are always convex, or make stronger guarantees
		// about clipping segment connectivity if the polygon may be clipped into a convex polygon.
		bool bReachedStartRay = false;
		bool bConsumeClipSegments = true;
		while (bConsumeClipSegments)
		{
			bConsumeClipSegments = false;

			// If we're searching for the clipping segment that connects to the end ray,
			// find the clipping segment that clips it the most.
			bool bConnectingEndRay = (PolygonVerts.Num() == 2);
			float shortestLengthOnEndRay = FLT_MAX, curLengthOnEndRay = FLT_MAX;
			int32 endRaySegStartIdx = INDEX_NONE;
			int32 endRaySegEndIdx = INDEX_NONE;

			int32 numClipSegments = ClippingPoints.Num() / 2;
			for (int32 clipSegIdx = numClipSegments - 1; clipSegIdx >= 0; clipSegIdx--)
			{
				int32 clipStartIdx = (2 * clipSegIdx) + 0;
				int32 clipEndIdx = (2 * clipSegIdx) + 1;
				const FVector &clipStartPoint = ClippingPoints[clipStartIdx];
				const FVector &clipEndPoint = ClippingPoints[clipEndIdx];
				bool bConsumed = false;

				// Search for the point on the end ray, so we have points against which other clipping segments can connect
				if (bConnectingEndRay)
				{
					if (testClipPointOnRay(EndPoint, EndDir, clipStartPoint, curLengthOnEndRay))
					{
						if (curLengthOnEndRay < shortestLengthOnEndRay)
						{
							shortestLengthOnEndRay = curLengthOnEndRay;
							endRaySegStartIdx = clipStartIdx;
							endRaySegEndIdx = clipEndIdx;
						}
					}
					else if (testClipPointOnRay(EndPoint, EndDir, clipEndPoint, curLengthOnEndRay))
					{
						if (curLengthOnEndRay < shortestLengthOnEndRay)
						{
							shortestLengthOnEndRay = curLengthOnEndRay;
							endRaySegStartIdx = clipEndIdx;
							endRaySegEndIdx = clipStartIdx;
						}
					}
				}
				// Search for connected points
				else
				{
					const FVector &lastPolyVert = PolygonVerts.Last();
					if (clipStartPoint.Equals(lastPolyVert, RAY_INTERSECT_TOLERANCE))
					{
						PolygonVerts.Add(clipEndPoint);
						bConsumed = true;
						bReachedStartRay = testClipPointOnRay(StartPoint, StartDir, clipEndPoint, curLengthOnEndRay);
					}
					else if (clipEndPoint.Equals(lastPolyVert, RAY_INTERSECT_TOLERANCE))
					{
						PolygonVerts.Add(clipStartPoint);
						bConsumed = true;
						bReachedStartRay = testClipPointOnRay(StartPoint, StartDir, clipStartPoint, curLengthOnEndRay);
					}
				}

				if (bConsumed)
				{
					ClippingPoints.RemoveAt(clipStartIdx, 2, false);
					bConsumeClipSegments = true;
					break;
				}
			}

			// If we found a clipping segment connected to the end ray that produces the shortest end line,
			// then consume its points and keep looking for connected clip segments.
			if (bConnectingEndRay && (endRaySegStartIdx != INDEX_NONE) && (endRaySegEndIdx != INDEX_NONE))
			{
				const FVector &clipPointA = ClippingPoints[endRaySegStartIdx];
				const FVector &clipPointB = ClippingPoints[endRaySegEndIdx];

				PolygonVerts.Add(clipPointA);
				PolygonVerts.Add(clipPointB);
				int32 minIndex = FMath::Min(endRaySegStartIdx, endRaySegEndIdx);
				ClippingPoints.RemoveAt(minIndex, 2, false);
				bConsumeClipSegments = true;
				bReachedStartRay = testClipPointOnRay(StartPoint, StartDir, clipPointB, curLengthOnEndRay);
			}
		}

		if (!bReachedStartRay)
		{
			UE_LOG(LogTemp, Warning, TEXT("Tessellation polygon (%s -> %s) was not fully clipped!"),
				*StartPoint.ToString(), *EndPoint.ToString());
		}
	}

	return (PolygonVerts.Num() >= 3);
}

void FTessellationPolygon::DrawDebug(const FColor &Color, UWorld* World, const FTransform &Transform)
{
	if (World == nullptr)
	{
		World = GWorld;
	}

	static bool bPersistent = true;
	static float lifetime = 1.0f;
	static uint8 depth = 0xFF;
	static float edgeThickness = 0.5f;
	static float clipThickness = 1.0f;
	static float pointSize = 1.5f;
	static float nonConvergingLineLength = 500.0f;

	DrawDebugLine(World, StartPoint, EndPoint, Color, bPersistent, lifetime, depth, edgeThickness);

	float startEdgeLength = bCachedEndsConverge ? CachedStartRayDist : nonConvergingLineLength;
	DrawDebugLine(World, StartPoint, StartPoint + startEdgeLength * StartDir, Color, bPersistent, lifetime, depth, edgeThickness);

	float endEdgeLength = bCachedEndsConverge ? CachedEndRayDist : nonConvergingLineLength;
	DrawDebugLine(World, EndPoint, EndPoint + endEdgeLength * EndDir, Color, bPersistent, lifetime, depth, edgeThickness);

	int32 numClippingLines = ClippingPoints.Num() / 2;
	for (int32 i = 0; i < numClippingLines; ++i)
	{
		const FVector &clippingLineStart = ClippingPoints[i * 2 + 0];
		const FVector &clippingLineEnd = ClippingPoints[i * 2 + 1];
		DrawDebugPoint(World, clippingLineStart, pointSize, Color, bPersistent, lifetime, depth);
		DrawDebugPoint(World, clippingLineEnd, pointSize, Color, bPersistent, lifetime, depth);
		DrawDebugLine(World, clippingLineStart, clippingLineEnd, Color, bPersistent, lifetime, depth, clipThickness);
	}
}

bool UModumateGeometryStatics::TessellateSlopedEdges(const TArray<FVector> &EdgePoints, const TArray<float> &EdgeSlopes, const TArray<bool> &EdgesHaveFaces,
	TArray<FVector> &OutCombinedPolyVerts, TArray<int32> &OutPolyVertIndices, const FVector &NormalHint)
{
	OutCombinedPolyVerts.Reset();
	OutPolyVertIndices.Reset();

	int32 numEdges = EdgePoints.Num();
	bool bFlip = false;

	if (!ensureAlways((numEdges >= 3) && (numEdges == EdgeSlopes.Num())))
	{
		return false;
	}

	FPlane basePlane;
	if (!UModumateGeometryStatics::GetPlaneFromPoints(EdgePoints, basePlane))
	{
		return false;
	}

	// Make sure that the plane normal aligns with the hint, if possible
	if (NormalHint.IsUnit() && !FVector::Orthogonal(basePlane, NormalHint) && ((FVector(basePlane) | NormalHint) < 0.0f))
	{
		basePlane *= -1.0f;
		bFlip = true;
	}
	FVector baseNormal(basePlane);

	// Verify that the slopes are valid and points are coplanar before attempting to tessellate
	for (int32 i = 0; i < numEdges; ++i)
	{
		float edgeSlope = EdgeSlopes[i];
		static const float maxSlope = 10000.0f;
		if (EdgesHaveFaces[i] && !ensureAlwaysMsgf((edgeSlope > 0.0f) && (edgeSlope <= maxSlope),
			TEXT("Edge #%d slope (%.2f) is outside of the allowed bounds!"), i + 1, edgeSlope))
		{
			return false;
		}

		const FVector &edgePoint = EdgePoints[i];
		float pointOnNormal = edgePoint | baseNormal;
		if (!ensureAlwaysMsgf(FMath::IsNearlyEqual(pointOnNormal, basePlane.W, RAY_INTERSECT_TOLERANCE),
			TEXT("Edge #%d point (%s) is not coplanar with the rest of the points!"), i + 1, *edgePoint.ToString()))
		{
			return false;
		}
	}

	// Perform actual tessellation
	TArray<FVector> edgeDirs, edgeInsideDirs, edgeTangents, edgeNormals, ridgeDirs;

	// Compute basic per-edge directions
	for (int32 edgeIdx = 0; edgeIdx < numEdges; ++edgeIdx)
	{
		int32 edgeStartIdx = edgeIdx;
		int32 edgeEndIdx = ((edgeIdx + 1) % numEdges);

		const FVector &edgeStartPoint = EdgePoints[edgeStartIdx];
		const FVector &edgeEndPoint = EdgePoints[edgeEndIdx];

		FVector edgeDir = (edgeEndPoint - edgeStartPoint).GetSafeNormal();
		edgeDirs.Add(edgeDir);

		FVector edgeInsideDir = (baseNormal ^ edgeDir) * (bFlip ? -1.0f : 1.0f);
		edgeInsideDirs.Add(edgeInsideDir);

		float edgeSlope = EdgeSlopes[edgeIdx];
		FVector edgeTangent, edgeNormal;

		if (EdgesHaveFaces[edgeIdx])
		{
			edgeTangent = ((edgeSlope * baseNormal) + edgeInsideDir).GetSafeNormal();
			edgeNormal = edgeDir ^ edgeTangent;
		}
		else
		{
			edgeTangent = baseNormal;
			edgeNormal = edgeDir ^ edgeTangent;
		}

		edgeTangents.Add(edgeTangent);
		edgeNormals.Add(edgeNormal);
	}

	// Compute ridge directions for each edge point, at the intersection between their planes
	for (int32 edgeIdx = 0; edgeIdx < numEdges; ++edgeIdx)
	{
		int32 prevEdgeIdx = (edgeIdx + numEdges - 1) % numEdges;

		const FVector &edgeNormal = edgeNormals[edgeIdx];
		const FVector &prevEdgeNormal = edgeNormals[prevEdgeIdx];

		FVector ridgeDir = (prevEdgeNormal ^ edgeNormal).GetSafeNormal();
		if ((ridgeDir | baseNormal) < 0.0f)
		{
			ridgeDir *= -1.0f;
		}
		ridgeDirs.Add(ridgeDir);
	}

	// Create tessellation polygons for each edge of the base polygon
	TArray<FTessellationPolygon> edgePolygons;
	for (int32 edgeIdx = 0; edgeIdx < numEdges; ++edgeIdx)
	{
		int32 edgeStartIdx = edgeIdx;
		int32 edgeEndIdx = ((edgeIdx + 1) % numEdges);

		const FVector &edgeStartPoint = EdgePoints[edgeStartIdx];
		const FVector &edgeEndPoint = EdgePoints[edgeEndIdx];

		const FVector &edgeStartRidgeDir = ridgeDirs[edgeStartIdx];
		const FVector &edgeEndRidgeDir = ridgeDirs[edgeEndIdx];

		const FVector &edgeNormal = edgeNormals[edgeIdx];

		FTessellationPolygon edgePoly(baseNormal, edgeNormal, edgeStartPoint, edgeStartRidgeDir, edgeEndPoint, edgeEndRidgeDir);

		edgePolygons.Add(MoveTemp(edgePoly));
	}

	// Clip edge polygons by non-adjacent polygons
	int32 debugNumClips = 0;
	for (int32 edgeIdx = 0; edgeIdx < numEdges; ++edgeIdx)
	{
		int32 prevEdgeIdx = (edgeIdx + numEdges - 1) % numEdges;
		int32 nextEdgeIdx = (edgeIdx + 1) % numEdges;
		FTessellationPolygon &edgePoly = edgePolygons[edgeIdx];

		for (int32 otherEdgeIdx = 0; otherEdgeIdx < numEdges; ++otherEdgeIdx)
		{
			if ((otherEdgeIdx != edgeIdx) && (otherEdgeIdx != prevEdgeIdx) && (otherEdgeIdx != nextEdgeIdx))
			{
				const FTessellationPolygon &otherEdgePoly = edgePolygons[otherEdgeIdx];
				bool bClip = edgePoly.ClipWithPolygon(otherEdgePoly);

				if (bClip)
				{
					++debugNumClips;
				}
			}
		}

		/* Draw debug lines to diagnose tessellation issues
		float debugHue = FMath::Frac(edgeIdx * UE_GOLDEN_RATIO);
		FLinearColor debugColor = FLinearColor::MakeFromHSV8(debugHue * 0xFF, 0xFF, 0xFF);
		edgePoly.DrawDebug(debugColor.ToFColor(false));
		//*/

		edgePoly.UpdatePolygonVerts();
	}

	// Collect the vertices for each edge tessellation polygon
	for (int32 edgeIdx = 0; edgeIdx < numEdges; ++edgeIdx)
	{
		const FTessellationPolygon &edgePoly = edgePolygons[edgeIdx];

		OutCombinedPolyVerts.Append(edgePoly.PolygonVerts);
		OutPolyVertIndices.Add(OutCombinedPolyVerts.Num() - 1);
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

bool UModumateGeometryStatics::AnalyzeCachedPositions(const TArray<FVector> &InPositions, FPlane &OutPlane, FVector &OutAxis2DX, FVector &OutAxis2DY, TArray<FVector2D> &Out2DPositions, FVector &OutCenter)
{
	// Find the plane that defines this face's points, in which they are oriented clockwise
	bool bPlanar = UModumateGeometryStatics::GetPlaneFromPoints(InPositions, OutPlane);

	if (!(bPlanar && OutPlane.IsNormalized()))
	{
		return false;
	}

	// Find the 2D projection of our points, for convenient intersection/triangulation computation later
	UModumateGeometryStatics::FindBasisVectors(OutAxis2DX, OutAxis2DY, OutPlane);

	Out2DPositions.Reset(InPositions.Num());
	for (const FVector &position : InPositions)
	{
		Out2DPositions.Add(ProjectPoint2D(position, OutAxis2DX, OutAxis2DY,InPositions[0]));
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
