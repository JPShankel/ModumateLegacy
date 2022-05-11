// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "ModumateCore/ModumateGeometryStatics.h"

#include "Algo/ForEach.h"
#include "Kismet/KismetMathLibrary.h"
#include "KismetProceduralMeshLibrary.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "Algo/Accumulate.h"
#include "DrawDebugHelpers.h"
#include "ModumateCore/ModumateStats.h"
#include "DrawingDesigner//DrawingDesignerLine.h"


#include <algorithm>
#include <queue>
#include <iostream>
using namespace std;

#include "Algo/Reverse.h"

#include "ConstrainedDelaunay2.h"
#include "GeomTools.h"


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

FPointInPolyResult::FPointInPolyResult()
{
	Reset();
}

void FPointInPolyResult::Reset()
{
	bInside = false;
	bOverlaps = false;
	StartVertexIdx = INDEX_NONE;
	EndVertexIdx = INDEX_NONE;
	EdgeDistance = 0.0f;
}

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

FVector2D UModumateGeometryStatics::ProjectPoint2DTransform(const FVector &Point3D, const FTransform& Origin)
{
	return ProjectPoint2D(Point3D, Origin.GetUnitAxis(EAxis::X), Origin.GetUnitAxis(EAxis::Y), Origin.GetLocation());
}

FVector UModumateGeometryStatics::Deproject2DPoint(const FVector2D &Point2D, const FVector &AxisX, const FVector &AxisY, const FVector &Origin)
{
	return Origin + (AxisX * Point2D.X) + (AxisY * Point2D.Y);
}

FVector UModumateGeometryStatics::Deproject2DPointTransform(const FVector2D &Point2D, const FTransform& Origin)
{
	return Deproject2DPoint(Point2D, Origin.GetUnitAxis(EAxis::X), Origin.GetUnitAxis(EAxis::Y), Origin.GetLocation());
}

FVector2D UModumateGeometryStatics::ProjectVector2D(const FVector &Vector3D, const FVector &AxisX, const FVector &AxisY)
{
	return ProjectPoint2D(Vector3D, AxisX, AxisY, FVector::ZeroVector);
}

bool UModumateGeometryStatics::FindPointFurthestFromPolyEdge(const TArray<FVector2D> &polygon, FVector2D &furthestPoint)
{
	using namespace PolyDist;

	static constexpr float precision = 1.0f;

	FBox2D envelope(ForceInitToZero);
	for (const FVector2D &point : polygon)
	{
		envelope += point;
	}

	FVector2D size = envelope.GetSize();

	const float cellSize = FMath::Max(size.GetMin(), precision / 2.0f);
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

bool UModumateGeometryStatics::TriangulateVerticesGTE(const TArray<FVector2D>& Vertices, const TArray<FPolyHole2D>& Holes,
	TArray<int32>& OutTriangles, TArray<FVector2D>* OutCombinedVertices, bool bCheckValid /*= true*/, TArray<FVector2D>* OutOptionalCDTVertices /*= nullptr*/,
	FDynamicGraph2<float>* OptionalGraphPoints /*= nullptr*/, bool bFastmode /*= false*/)
{
	OutTriangles.Reset();
	if (OutCombinedVertices)
	{
		OutCombinedVertices->Reset();
	}

	TArray<FVector2f> vertices;
	for (auto& vertex : Vertices)
	{
		vertices.Add(vertex);
	}
	if (OutCombinedVertices)
	{
		OutCombinedVertices->Append(Vertices);
	}

	FPolygon2f perimeter(vertices);
	FGeneralPolygon2f polygon;
	polygon.SetOuter(perimeter);

	for (auto& hole : Holes)
	{
		vertices.Reset();
		for (auto& vertex : hole.Points)
		{
			vertices.Add(vertex);
		}
		if (OutCombinedVertices)
		{
			OutCombinedVertices->Append(hole.Points);
		}

		FPolygon2f holePoly(vertices);
		polygon.AddHole(holePoly, false, false);
	}

	FConstrainedDelaunay2f constrainedDelaunay;
	constrainedDelaunay.bOutputCCW = true;
	constrainedDelaunay.Add(polygon);

	if (OptionalGraphPoints)
	{
		constrainedDelaunay.bOutputCCW = false; // TODO: No idea why. Maybe is the order of the gridpoint added
		constrainedDelaunay.Add(*OptionalGraphPoints);
	}

	if (bFastmode)
	{	// 'Fast' mode uses double as the internal compute type, rather than software floating-point.
		if (!constrainedDelaunay.TriangulateFast())
		{
			return false;
		}
	}
	else
	{
		if (!constrainedDelaunay.Triangulate())
		{
			return false;
		}
	}

	for (auto& triangle : constrainedDelaunay.Triangles)
	{
		OutTriangles.Add(triangle.A);
		OutTriangles.Add(triangle.B);
		OutTriangles.Add(triangle.C);
	}

	if (OutOptionalCDTVertices)
	{
		OutOptionalCDTVertices->Reset();
		for (auto& curVert : constrainedDelaunay.Vertices)
		{
			OutOptionalCDTVertices->Add(FVector2D(curVert.X, curVert.Y));
		}
	}

	return true;
}

void UModumateGeometryStatics::GetPolygonWindingAndConcavity(const TArray<FVector2D> &Locations, bool &bOutClockwise, bool &bOutConcave)
{
	int32 numLocations = Locations.Num();
	if (numLocations < 3)
	{
		UE_LOG(LogTemp, Warning, TEXT("Can't determine winding of < 3 locations!"));
		return;
	}

	// a polygon is concave if any two edges have opposite signs,
	// so save the first non-zero sign and compare it to the rest
	bOutConcave = false;
	float savedSign = 0.0f;
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
			return;
		}

		FVector2D n1 = (p2 - p1).GetSafeNormal();
		FVector2D n2 = (p3 - p2).GetSafeNormal();

		float unsignedAngle = FMath::Acos(n1 | n2);
		float angleSign = -FMath::Sign(n1 ^ n2);
		if (savedSign == 0.0f && angleSign != 0.0f)
		{
			savedSign = angleSign;
		}
		else if (savedSign * angleSign < 0.0f)
		{	// if the signs are opposite, their product is less than 0
			bOutConcave = true;
		}

		float signedAngle = angleSign * unsignedAngle;
		totalWindingAngle += signedAngle;
	}

	bOutClockwise = (totalWindingAngle >= 0.0f);
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

// It would be nice to use FMath::SegmentIntersection2D directly, but we need to handle zero-length segments, variable epsilon values, and parallel segments
bool UModumateGeometryStatics::SegmentIntersection2D(
	const FVector2D& SegmentStartAFloat, const FVector2D& SegmentEndAFloat,
	const FVector2D& SegmentStartBFloat, const FVector2D& SegmentEndBFloat,
	FVector2D& OutIntersectionPoint, bool& bOutOverlapping, float FloatTolerance)
{
	// TODO: remove these argument conversions and *float names once the arguments are double-precision, when every client is operating in doubles
	// Alternative TODO: replace this with IntrLine2Line2/IntrSegment2Segment2 if it handles our same required degenerate edge cases
	FVec2d
		SegmentStartA(SegmentStartAFloat),
		SegmentEndA(SegmentEndAFloat),
		SegmentStartB(SegmentStartBFloat),
		SegmentEndB(SegmentEndBFloat);

	FSegment2d segmentA(SegmentStartA, SegmentEndA);
	FSegment2d segmentB(SegmentStartB, SegmentEndB);

	bOutOverlapping = false;
	double signedTolerance = (double)FloatTolerance;
	double Tolerance = FMath::Abs(signedTolerance);

	FVec2d segmentADelta = SegmentEndA - SegmentStartA;
	double segmentALength = segmentADelta.Length();
	FVec2d segmentADir = (FMath::Abs(segmentALength) <= Tolerance) ? FVec2d::Zero() : (segmentADelta / segmentALength);
	FVec2d segmentBDelta = SegmentEndB - SegmentStartB;
	double segmentBLength = segmentBDelta.Length();
	FVec2d segmentBDir = (FMath::Abs(segmentBLength) <= Tolerance) ? FVec2d::Zero() : (segmentBDelta / segmentBLength);

	// First, check degenerate zero-length segments by seeing if points are equal, or if one point lies on another segment
	if (!segmentADir.IsNormalized() && !segmentBDir.IsNormalized())
	{
		if (VectorUtil::EpsilonEqual(SegmentStartA, SegmentStartB, Tolerance))
		{
			OutIntersectionPoint = FVector2D(SegmentStartA);
			return true;
		}
		return false;
	}
	else if (!segmentADir.IsNormalized())
	{
		FVec2d pointAOnSegmentB = segmentB.NearestPoint(SegmentStartA);
		if (VectorUtil::EpsilonEqual(pointAOnSegmentB, SegmentStartA, Tolerance))
		{
			OutIntersectionPoint = FVector2D(SegmentStartA);
			return true;
		}
		return false;
	}
	else if (!segmentBDir.IsNormalized())
	{
		FVec2d pointBOnSegmentA = segmentA.NearestPoint(SegmentStartB);
		if (VectorUtil::EpsilonEqual(pointBOnSegmentA, SegmentStartB, Tolerance))
		{
			OutIntersectionPoint = FVector2D(SegmentStartB);
			return true;
		}
		return false;
	}

	// If the segments are parallel, then they may be overlapping in ways in which ray intersection can't handle, so handle it here
	double segmentDirsDot = segmentADir.Dot(segmentBDir);
	FVec2d startsDelta = (SegmentStartB - SegmentStartA);

	if (FMath::Abs(segmentDirsDot) >= THRESH_NORMALS_ARE_PARALLEL)
	{
		double segBStartDistOnA = startsDelta.Dot(segmentADir);
		double segBEndDistOnA = (SegmentEndB - SegmentStartA).Dot(segmentADir);
		double segBMinDistOnA = FMath::Min(segBStartDistOnA, segBEndDistOnA);
		double segBMaxDistOnA = FMath::Max(segBStartDistOnA, segBEndDistOnA);

		// If the segment points don't lie on the same line, then they can't overlap
		FVec2d segBStartOnA = SegmentStartA + (segBStartDistOnA * segmentADir);
		if (!VectorUtil::EpsilonEqual(SegmentStartB, segBStartOnA, Tolerance))
		{
			return false;
		}

		if ((segBMinDistOnA >= (segmentALength + signedTolerance)) || (segBMaxDistOnA <= -signedTolerance))
		{
			return false;
		}

		bOutOverlapping = true;
		double overlapMinDistOnA = FMath::Max(segBMinDistOnA, 0.0);
		double overlapMaxDistOnA = FMath::Min(segBMaxDistOnA, segmentALength);
		double overlapCenterDistOnA = 0.5 * (overlapMinDistOnA + overlapMaxDistOnA);
		OutIntersectionPoint = FVector2D(SegmentStartA + (overlapCenterDistOnA * segmentADir));
		return true;
	}
	// Otherwise, calculate the intersection directly since we know we won't divide by zero
	else
	{
		FVec2d segmentANormal = segmentADir.Perp();
		FVec2d segmentBNormal = segmentBDir.Perp();

		// Compute intersection between rays, using FMath::RayPlaneIntersection as a basis
		// Distance = (( PlaneOrigin - RayOrigin ) | PlaneNormal) / (RayDirection | PlaneNormal);
		double intersectionADist = startsDelta.Dot(segmentBNormal) / segmentADir.Dot(segmentBNormal);
		double intersectionBDist = -startsDelta.Dot(segmentANormal) / segmentBDir.Dot(segmentANormal);

		FVec2d intersectionAPoint = SegmentStartA + (intersectionADist * segmentADir);
		FVec2d intersectionBPoint = SegmentStartB + (intersectionBDist * segmentBDir);
		if (!ensure(VectorUtil::EpsilonEqual(intersectionAPoint, intersectionBPoint, Tolerance)))
		{
			return false;
		}

		OutIntersectionPoint = FVector2D(intersectionAPoint);
		return FMath::IsWithinInclusive(intersectionADist, -signedTolerance, segmentALength + signedTolerance) &&
			FMath::IsWithinInclusive(intersectionBDist, -signedTolerance, segmentBLength + signedTolerance);
	}
}

bool UModumateGeometryStatics::RayIntersection2D(
	const FVector2D& RayOriginAFloat, const FVector2D& RayDirectionAFloat,
	const FVector2D& RayOriginBFloat, const FVector2D& RayDirectionBFloat,
	FVector2D& OutIntersectionPoint, float &OutRayADist, float &OutRayBDist, bool bRequirePositive, float ToleranceFloat)
{
	// TODO: remove these argument conversions and *float names once the arguments are double-precision, when every client is operating in doubles
	FVec2d
		RayOriginA(RayOriginAFloat),
		RayDirectionA(RayDirectionAFloat),
		RayOriginB(RayOriginBFloat),
		RayDirectionB(RayDirectionBFloat);

	double Tolerance = (double)ToleranceFloat;
	OutRayADist = OutRayBDist = 0.0f;

	// First, check if the rays start at the same origin
	FVec2d originDelta = RayOriginB - RayOriginA;
	double originDist = originDelta.Length();

	double rayADotB = RayDirectionA.Dot(RayDirectionB);
	bool bParallel = (FMath::Abs(rayADotB) > THRESH_NORMALS_ARE_PARALLEL);

	if (FMath::Abs(originDist) <= Tolerance)
	{
		OutIntersectionPoint = FVector2D(RayOriginA);
		return true;
	}

	// Next, check for parallel rays; they can only intersect if they're overlapping
	if (bParallel)
	{
		// Check if the rays are colinear; if not, then they can't intersect
		double originBOnRayADist = originDelta.Dot(RayDirectionA);
		FVec2d originBOnRayAPoint = RayOriginA + (originBOnRayADist * RayDirectionA);

		double originAOnRayBDist = -originDelta.Dot(RayDirectionB);
		FVec2d originAOnRayBPoint = RayOriginB + (originAOnRayBDist * RayDirectionB);

		double rayDist = RayOriginB.Distance(originBOnRayAPoint);
		if (rayDist > Tolerance)
		{
			return false;
		}

		// Coincident colinear rays
		if (rayADotB > 0.0)
		{
			ensureAlways(FMath::Sign(originBOnRayADist) != FMath::Sign(originAOnRayBDist));

			OutRayADist = FMath::Max(originBOnRayADist, 0.0);
			OutRayBDist = FMath::Max(originAOnRayBDist, 0.0);
			if (originBOnRayADist > -Tolerance)
			{
				OutIntersectionPoint = FVector2D(RayOriginB);
			}
			else
			{
				OutIntersectionPoint = FVector2D(RayOriginA);
			}

			return true;
		}
		// Anti-parallel colinear rays
		else
		{
			ensureAlways(VectorUtil::EpsilonEqual(originBOnRayADist, originAOnRayBDist, Tolerance));

			OutIntersectionPoint = FVector2D(0.5 * (RayOriginA + RayOriginB));
			OutRayADist = 0.5 * originBOnRayADist;
			OutRayBDist = 0.5 * originAOnRayBDist;

			return (originBOnRayADist > -Tolerance) || !bRequirePositive;
		}
	}

	// Determine ray perpendicular vectors, for intersection
	FVec2d rayNormalA = RayDirectionA.Perp();
	FVec2d rayNormalB = RayDirectionB.Perp();

	// Compute intersection between rays, using FMath::RayPlaneIntersection as a basis
	// Distance = (( PlaneOrigin - RayOrigin ) | PlaneNormal) / (RayDirection | PlaneNormal);
	OutRayADist = originDelta.Dot(rayNormalB) / RayDirectionA.Dot(rayNormalB);
	OutRayBDist = -originDelta.Dot(rayNormalA) / RayDirectionB.Dot(rayNormalA);

	// Potentially throw out results that are behind the origins of the rays
	if (bRequirePositive && ((OutRayADist < -Tolerance) || (OutRayBDist < -Tolerance)))
	{
		return false;
	}

	// Ensure that intersection points are consistent
	FVec2d rayAOnBPoint = RayOriginA + (OutRayADist * RayDirectionA);
	FVec2d rayBOnAPoint = RayOriginB + (OutRayBDist * RayDirectionB);
	if (!VectorUtil::EpsilonEqual(rayAOnBPoint, rayBOnAPoint, Tolerance))
	{
		return false;
	}

	OutIntersectionPoint = FVector2D(rayAOnBPoint);
	return true;
}

bool UModumateGeometryStatics::IsRayBoundedByRays(const FVector2D& RayDirA, const FVector2D& RayDirB, const FVector2D& RayNormalA, const FVector2D& RayNormalB,
	const FVector2D& TestRay, bool& bOutOverlaps)
{
	bOutOverlaps = false;

#if !UE_BUILD_SHIPPING
	// Debug validation that all input ray directions are normalized, and that the bounding ray normals are indeed normal to their rays.
	if (!ensure(
		FMath::IsNearlyEqual(RayDirA.SizeSquared(), 1.0f, THRESH_VECTOR_NORMALIZED) &&
		FMath::IsNearlyEqual(RayDirB.SizeSquared(), 1.0f, THRESH_VECTOR_NORMALIZED) &&
		FMath::IsNearlyEqual(RayNormalA.SizeSquared(), 1.0f, THRESH_VECTOR_NORMALIZED) &&
		FMath::IsNearlyEqual(RayNormalB.SizeSquared(), 1.0f, THRESH_VECTOR_NORMALIZED) &&
		FMath::IsNearlyEqual(TestRay.SizeSquared(), 1.0f, THRESH_VECTOR_NORMALIZED) &&
		(FMath::Abs(RayDirA | RayNormalA) <= THRESH_NORMALS_ARE_ORTHOGONAL) &&
		(FMath::Abs(RayDirB | RayNormalB) <= THRESH_NORMALS_ARE_ORTHOGONAL)))
	{
		return false;
	}
#endif

	// If the test ray is coincident to one of the bounding rays, early exit and report overlapping.
	bool bTestOverlapsA = (TestRay | RayDirA) >= THRESH_NORMALS_ARE_PARALLEL;
	bool bTestOverlapsB = (TestRay | RayDirB) >= THRESH_NORMALS_ARE_PARALLEL;
	if (bTestOverlapsA || bTestOverlapsB)
	{
		bOutOverlaps = true;
		return false;
	}

	// If the bounding rays are parallel, then we can handle that here without worrying about precision later
	float boundingRaysDot = (RayDirA | RayDirB);
	if (FMath::Abs(boundingRaysDot) >= THRESH_NORMALS_ARE_PARALLEL)
	{
		// The bounding rays can't be coincident, and if they're parallel then their normals must be coincident
		if (!ensure((boundingRaysDot < 0) && ((RayNormalA | RayNormalB) >= THRESH_NORMALS_ARE_PARALLEL)))
		{
			return false;
		}

		float testNormalDot = (TestRay | RayNormalA);
		return (testNormalDot > THRESH_NORMALS_ARE_ORTHOGONAL);
	}

	// Otherwise, use the convexity of the corner (based on the ray normals) to test the ray
	bool bIsCornerConvex = ((RayDirA | RayNormalB) > 0.0f) && ((RayDirB | RayNormalA) > 0.0f);

	float prevToNextCrossAmount = (RayDirA ^ RayDirB);
	float prevToTestCrossAmount = (RayDirA ^ TestRay);
	float nextToPrevCrossAmount = (RayDirB ^ RayDirA);
	float nextToTestCrossAmount = (RayDirB ^ TestRay);

	bool bTestDirBetweenEdgesAcute =
		((prevToNextCrossAmount * prevToTestCrossAmount) >= 0) &&
		((nextToPrevCrossAmount * nextToTestCrossAmount) >= 0);

	return (bIsCornerConvex == bTestDirBetweenEdgesAcute);
}

bool UModumateGeometryStatics::GetPolyEdgeInfo(const TArray<FVector2D>& Polygon, bool bPolyCW, int32 EdgeStartIdx,
	FVector2D& OutStartPoint, FVector2D& OutEndPoint, float& OutEdgeLength, FVector2D& OutEdgeDir, FVector2D& OutEdgeNormal, float Epsilon)
{
	if (!Polygon.IsValidIndex(EdgeStartIdx))
	{
		return false;
	}

	int32 edgeEndIdx = (EdgeStartIdx + 1) % Polygon.Num();
	OutStartPoint = Polygon[EdgeStartIdx];
	OutEndPoint = Polygon[edgeEndIdx];
	if (OutStartPoint.Equals(OutEndPoint, Epsilon))
	{
		return false;
	}

	FVector2D edgeDelta = OutEndPoint - OutStartPoint;
	OutEdgeLength = edgeDelta.Size();
	OutEdgeDir = edgeDelta / OutEdgeLength;

	// Rotate the edge direction clockwise (in our left-handed coordinate system) to get the normal for a clockwise polygon
	OutEdgeNormal = FVector2D(OutEdgeDir.Y, -OutEdgeDir.X);

	if (!bPolyCW)
	{
		OutEdgeNormal *= -1.0f;
	}

	return true;
}

bool UModumateGeometryStatics::TestPointInPolygon(const FVector2D& Point, const TArray<FVector2D>& Polygon, FPointInPolyResult& OutResult, float Tolerance)
{
	int32 numPolyPoints = Polygon.Num();
	OutResult.Reset();

	if (!ensure(numPolyPoints >= 3))
	{
		return false;
	}

	// First, test if the input point lies on any of the input polygon's vertices or edges,
	// since any other ray intersection tests would be invalidated by the point overlapping with the polygon.
	for (int32 pointIdx = 0; pointIdx < numPolyPoints; ++pointIdx)
	{
		int32 edgeIdx1 = pointIdx;
		int32 edgeIdx2 = (pointIdx + 1) % numPolyPoints;

		const FVector2D &edgePoint1 = Polygon[edgeIdx1];
		const FVector2D &edgePoint2 = Polygon[edgeIdx2];
		if (!ensure(!edgePoint1.Equals(edgePoint2, Tolerance)))
		{
			return false;
		}

		// If the test point is the same as one of the polygon vertices, then report the vertex overlap
		if (Point.Equals(edgePoint1, Tolerance))
		{
			OutResult.bOverlaps = true;
			OutResult.StartVertexIdx = pointIdx;
			return true;
		}

		// If the test point lies inside one of the polygon edges, then report the edge overlap
		FVector2D edgeDelta = (edgePoint2 - edgePoint1);
		float edgeLen = edgeDelta.Size();
		FVector2D edgeDir = edgeDelta / edgeLen;
		FVector2D pointDelta = Point - edgePoint1;
		float distOnEdge = pointDelta | edgeDir;
		FVector2D projectedPoint = edgePoint1 + (distOnEdge * edgeDir);
		if (Point.Equals(projectedPoint, Tolerance) && FMath::IsWithinInclusive(distOnEdge, -Tolerance, edgeLen + Tolerance))
		{
			OutResult.bOverlaps = true;
			OutResult.StartVertexIdx = edgeIdx1;
			OutResult.StartVertexIdx = edgeIdx2;
			OutResult.EdgeDistance = distOnEdge;
			return true;
		}
	}

	// Get the winding for the input polygon, so we can calculate internal edge normals to perform ray tests
	bool bPolyCW, bPolyConcave;
	GetPolygonWindingAndConcavity(Polygon, bPolyCW, bPolyConcave);

	// Choose an arbitrary ray direction from which to test if our sample point hits an odd number
	// of polygon edges.
	const FVector2D testRay(1.0f, 0.0f);

	// Keep track of the shortest hit against either a vertex or edge of the polygon,
	// so we can test whether the hit normal is inside the polygon.
	float minRayHitDist = FLT_MAX;
	int32 minHitEdgeStartIdx = INDEX_NONE;
	bool bMinHitVertex = false;

	// The edge info for the edge that we have a min hit against
	float hitEdgeLength;
	FVector2D hitEdgePoint1, hitEdgePoint2, hitEdgeDir, hitEdgeNormal;

	// Test the ray against every edge, since we aren't using any kind of spatial data structure to rule out edges,
	// and this is intended to work with concave polygons where being inside corners or on the same side of every edge
	// wouldn't be sufficient.
	for (int32 pointIdx = 0; pointIdx < numPolyPoints; ++pointIdx)
	{
		int32 edgeStartIdx = pointIdx;

		float edgeLength;
		FVector2D edgePoint1, edgePoint2, edgeDir, edgeNormal;
		GetPolyEdgeInfo(Polygon, bPolyCW, edgeStartIdx, edgePoint1, edgePoint2, edgeLength, edgeDir, edgeNormal, Tolerance);

		// Now, try to intersect with the actual edge
		FVector2D edgeIntersection;
		float testRayDist, edgeRayDist;
		bool bRaysOpposite = (testRay | edgeDir) <= -THRESH_NORMALS_ARE_PARALLEL;
		bool bHitEdge = UModumateGeometryStatics::RayIntersection2D(Point, testRay, edgePoint1, edgeDir,
			edgeIntersection, testRayDist, edgeRayDist, true);

		// - Require an intersection of some kind
		// - Require a shorter intersection than we've already had
		// - Require an intersection against only either the start of the target edge or its inside;
		//   the target edge's end point that will be considered with the next edge
		// - Require that the rays are not pointing opposite directions; otherwise, an intersection against a connected
		//   edge's start vertex would've been or will be detected. Coincident edges are allowed, since the target
		//   coincident edge is the one whose start vertex will be used to detect collision.
		if (bHitEdge && !bRaysOpposite && (testRayDist < minRayHitDist) &&
			FMath::IsWithinInclusive(edgeRayDist, -Tolerance, edgeLength - Tolerance))
		{
			minRayHitDist = testRayDist;
			minHitEdgeStartIdx = edgeStartIdx;
			bMinHitVertex = edgeIntersection.Equals(edgePoint1, Tolerance);

			hitEdgeLength = edgeLength;
			hitEdgePoint1 = edgePoint1;
			hitEdgePoint2 = edgePoint2;
			hitEdgeDir = edgeDir;
			hitEdgeNormal = edgeNormal;
		}
	}

	if (minHitEdgeStartIdx == INDEX_NONE)
	{
		return true;
	}

	// If the shortest ray hit against the polygon is against an edge, then we only need to check against the one edge's normal
	if (!bMinHitVertex)
	{
		float hitNormalDot = testRay | hitEdgeNormal;

		// Make sure we didn't hit an edge that was parallel with the test ray if we thought we intersected the middle of it.
		if (!ensure(!FMath::IsNearlyZero(hitNormalDot, PLANAR_DOT_EPSILON)))
		{
			return false;
		}

		OutResult.bInside = (hitNormalDot < 0.0f);
	}
	// Otherwise, compare against both edge normals of the edges whose shared vertex we hit
	else
	{
		int32 preEdgeIdx = (minHitEdgeStartIdx - 1 + numPolyPoints) % numPolyPoints;
		float preEdgeLength;
		FVector2D preEdgePoint1, preEdgePoint2, preEdgeDir, preEdgeNormal;
		GetPolyEdgeInfo(Polygon, bPolyCW, preEdgeIdx, preEdgePoint1, preEdgePoint2, preEdgeLength, preEdgeDir, preEdgeNormal, Tolerance);

		bool bOverlaps;
		OutResult.bInside = IsRayBoundedByRays(-preEdgeDir, hitEdgeDir, preEdgeNormal, hitEdgeNormal, -testRay, bOverlaps);
	}

	return true;
}

bool UModumateGeometryStatics::GetPolygonContainment(const TArray<FVector2D> &ContainingPolygon, const TArray<FVector2D> &ContainedPolygon, bool& bOutFullyContained, bool& bOutPartiallyContained, float Tolerance)
{
	// We allow 2 contained points because contained polygons are not required to be simple, only containing polygons
	int32 numContainingPoints = ContainingPolygon.Num();
	int32 numContainedPoints = ContainedPolygon.Num();
	if (!ensureAlways((numContainingPoints >= 3) && (numContainedPoints >= 2)))
	{
		return false;
	}

	// See if every potentially contained vertex is indeed contained by the other polygon.
	// - If any vertices are neither contained nor overlapping, then it's not fully contained.
	//   If we needed to detect overlaps/touching, we would need to continue, but in this case we can early exit.
	// - If any vertices are not contained, but are overlapping, then partial containment is still possible.
	FPointInPolyResult pointInPolyResult;
	bOutFullyContained = true;
	bOutPartiallyContained = true;
	bool bAnyVerticesFullyContained = false;

	for (const FVector2D& containedVertex : ContainedPolygon)
	{
		if (!ensure(UModumateGeometryStatics::TestPointInPolygon(containedVertex, ContainingPolygon, pointInPolyResult, Tolerance)))
		{
			return false;
		}
		bAnyVerticesFullyContained = bAnyVerticesFullyContained || (pointInPolyResult.bInside && !pointInPolyResult.bOverlaps);

		if (!pointInPolyResult.bInside)
		{
			bOutFullyContained = false;

			if (!pointInPolyResult.bOverlaps)
			{
				bOutPartiallyContained = false;
				return true;
			}
		}
	}

	// For clarity, if the containing polygon fully contains the contained polygon, we won't consider it partially contained
	if (bOutFullyContained)
	{
		bOutPartiallyContained = false;
	}

	// If the vertices are all overlapping, but none are fully contained,
	// then partial containment is only possible if one of the contained edges is fully contained by the containing polygon.
	// Because edges don't intersect with each other, we can use the contained edge midpoints to test this.
	if (bOutPartiallyContained && !bAnyVerticesFullyContained)
	{
		bOutPartiallyContained = false;
		for (int32 edgeIdxA = 0; edgeIdxA < numContainedPoints; ++edgeIdxA)
		{
			int32 edgeIdxB = (edgeIdxA + 1) % numContainedPoints;
			const FVector2D& edgePointA = ContainedPolygon[edgeIdxA];
			const FVector2D& edgePointB = ContainedPolygon[edgeIdxB];
			FVector2D edgeMidpoint = 0.5f * (edgePointA + edgePointB);

			if (!ensure(UModumateGeometryStatics::TestPointInPolygon(edgeMidpoint, ContainingPolygon, pointInPolyResult, Tolerance)))
			{
				return false;
			}

			if (pointInPolyResult.bInside && !pointInPolyResult.bOverlaps)
			{
				bOutPartiallyContained = true;
				return true;
			}
		}
	}

	return true;
}

bool UModumateGeometryStatics::GetPolygonIntersection(const TArray<FVector2D> &ContainingPolygon, const TArray<FVector2D> &ContainedPolygon, bool& bOverlapping, bool& bOutFullyContained, bool& bOutPartiallyContained)
{
	bOverlapping = false;
	bOutFullyContained = false;
	bOutPartiallyContained = false;

	int32 numContainingPoints = ContainingPolygon.Num();
	int32 numContainedPoints = ContainedPolygon.Num();
	if (!ensureAlways((numContainingPoints >= 3) && (numContainedPoints >= 3)))
	{
		return false;
	}

	for (int32 containingEdgeStartIdx = 0; containingEdgeStartIdx < numContainingPoints; ++containingEdgeStartIdx)
	{
		int32 containingEdgeEndIdx = (containingEdgeStartIdx + 1) % numContainingPoints;
		const FVector2D& containingEdgeStart = ContainingPolygon[containingEdgeStartIdx];
		const FVector2D& containingEdgeEnd = ContainingPolygon[containingEdgeEndIdx];

		for (int32 containedEdgeStartIdx = 0; containedEdgeStartIdx < numContainedPoints; ++containedEdgeStartIdx)
		{
			int32 containedEdgeEndIdx = (containedEdgeStartIdx + 1) % numContainedPoints;
			const FVector2D& containedEdgeStart = ContainedPolygon[containedEdgeStartIdx];
			const FVector2D& containedEdgeEnd = ContainedPolygon[containedEdgeEndIdx];

			FVector2D edgeIntersection;
			bool bEdgesOverlap;
			if (UModumateGeometryStatics::SegmentIntersection2D(containingEdgeStart, containingEdgeEnd, containedEdgeStart, containedEdgeEnd, edgeIntersection, bEdgesOverlap, -RAY_INTERSECT_TOLERANCE))
			{
				bOverlapping = true;
				return true;
			}
		}
	}

	return GetPolygonContainment(ContainingPolygon, ContainedPolygon, bOutFullyContained, bOutPartiallyContained);
}

bool UModumateGeometryStatics::RayIntersection3D(const FVector& RayOriginA, const FVector& RayDirectionA, const FVector& RayOriginB, const FVector& RayDirectionB,
	FVector& OutIntersectionPoint, float &OutRayADist, float &OutRayBDist, bool bRequirePositive, float IntersectionTolerance)
{
	OutRayADist = OutRayBDist = 0.0f;

	if (!RayDirectionA.IsNormalized() || !RayDirectionB.IsNormalized())
	{
		return false;
	}

	// First, check if the rays start at the same origin
	FVector originDelta = RayOriginB - RayOriginA;
	float originDist = originDelta.Size();

	float rayADotB = RayDirectionA | RayDirectionB;
	bool bParallel = (FMath::Abs(rayADotB) > THRESH_NORMALS_ARE_PARALLEL);

	if (FMath::IsNearlyZero(originDist, IntersectionTolerance))
	{
		OutIntersectionPoint = RayOriginA;
		return true;
	}

	// Next, check for parallel rays; they can only intersect if they're overlapping
	if (bParallel)
	{
		// Check if the rays are colinear; if not, then they can't intersect
		float originBOnRayADist = originDelta | RayDirectionA;
		FVector originBOnRayAPoint = RayOriginA + (originBOnRayADist * RayDirectionA);

		float originAOnRayBDist = -originDelta | RayDirectionB;
		FVector originAOnRayBPoint = RayOriginB + (originAOnRayBDist * RayDirectionB);

		float rayDist = FVector::Dist(RayOriginB, originBOnRayAPoint);
		if (rayDist > IntersectionTolerance)
		{
			return false;
		}

		// Coincident colinear rays
		if (rayADotB > 0.0f)
		{
			ensureAlways(FMath::Sign(originBOnRayADist) != FMath::Sign(originAOnRayBDist));

			OutRayADist = FMath::Max(originBOnRayADist, 0.0f);
			OutRayBDist = FMath::Max(originAOnRayBDist, 0.0f);
			if (originBOnRayADist > -IntersectionTolerance)
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
			ensureAlways(FMath::IsNearlyEqual(originBOnRayADist, originAOnRayBDist, IntersectionTolerance));

			OutIntersectionPoint = 0.5f * (RayOriginA + RayOriginB);
			OutRayADist = 0.5f * originBOnRayADist;
			OutRayBDist = 0.5f * originAOnRayBDist;

			return (originBOnRayADist > -IntersectionTolerance) || !bRequirePositive;
		}
	}

	// Find the plane shared by the ray directions, which we expect to succeed if we didn't already detect them as parallel
	FVector planeNormal = RayDirectionA ^ RayDirectionB;
	if (!ensure(planeNormal.Normalize(KINDA_SMALL_NUMBER)))
	{
		return false;
	}

	float planeDistA = RayOriginA | planeNormal;
	float planeDistB = RayOriginB | planeNormal;

	// Ray origins must be in the same plane as their rays' shared plane
	if (!FMath::IsNearlyEqual(planeDistA, planeDistB, IntersectionTolerance))
	{
		return false;
	}

	// Determine ray perpendicular vectors, for intersection
	FVector rayNormalA = (RayDirectionA ^ planeNormal);
	FVector rayNormalB = (RayDirectionB ^ planeNormal);

	if (!rayNormalA.IsUnit(IntersectionTolerance) || !rayNormalB.IsUnit(IntersectionTolerance))
	{
		return false;
	}

	// Compute intersection between rays, using FMath::RayPlaneIntersection as a basis
	// float Distance = (( PlaneOrigin - RayOrigin ) | PlaneNormal) / (RayDirection | PlaneNormal);
	OutRayADist = (originDelta | rayNormalB) / (RayDirectionA | rayNormalB);
	OutRayBDist = (-originDelta | rayNormalA) / (RayDirectionB | rayNormalA);

	// Potentially throw out results that are behind the origins of the rays
	if (bRequirePositive && ((OutRayADist < -IntersectionTolerance) || (OutRayBDist < -IntersectionTolerance)))
	{
		return false;
	}

	// Ensure that intersection points are consistent
	FVector rayAOnBPoint = RayOriginA + (OutRayADist * RayDirectionA);
	FVector rayBOnAPoint = RayOriginB + (OutRayBDist * RayDirectionB);
	if (!FVector::PointsAreNear(rayAOnBPoint, rayBOnAPoint, IntersectionTolerance))
	{
		return false;
	}

	OutIntersectionPoint = rayAOnBPoint;
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

	if (!UModumateGeometryStatics::IsPolygon2DValid(Out2DPositions))
	{
		return false;
	}

	// Find a central point to use for casting rays
	// (essential for concave polygons, where the centroid would not suffice)
	// TODO: Just use centroid for convex polys, as FindPointFurthestFromPolyEdge() can be very expensive.
	FVector2D furthestPoint;
	if (Out2DPositions.Num() > 3 && UModumateGeometryStatics::FindPointFurthestFromPolyEdge(Out2DPositions, furthestPoint))
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

bool UModumateGeometryStatics::ConvertProcMeshToLinesOnPlane(UProceduralMeshComponent* InProcMesh, FVector PlanePosition, FVector PlaneNormal, TArray<TPair<FVector, FVector>> &OutEdges, int32 UseSectionId)
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
		if (UseSectionId != INDEX_NONE && SectionIndex != UseSectionId)
		{
			continue;
		}

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

bool UModumateGeometryStatics::ConvertStaticMeshToLinesOnPlane(UStaticMeshComponent* InStaticMesh, FVector PlanePosition, FVector PlaneNormal, TArray<TPair<FVector, FVector>> &OutEdges)
{
	const FTransform& localToWorld = InStaticMesh->GetComponentToWorld();
	UStaticMesh* staticMesh = InStaticMesh->GetStaticMesh();
	if (staticMesh == nullptr)
	{
		return true;
	}

	static constexpr int32 levelOfDetailIndex = 0;

	const FStaticMeshLODResources& meshResources = staticMesh->GetLODForExport(levelOfDetailIndex);

	TArray<FVector> positions;
	TArray<int32> indices;
	TArray<FVector> normals;
	TArray<FVector2D> UVs;
	TArray<FProcMeshTangent> tangents;

	int32 numSections = meshResources.Sections.Num();
	for (int32 section = 0; section < numSections; ++section)
	{
		UKismetProceduralMeshLibrary::GetSectionFromStaticMesh(staticMesh, levelOfDetailIndex, section, positions, indices, normals, UVs, tangents);
		ensure(indices.Num() % 3 == 0);
		int32 numTriangles = indices.Num() / 3;
		FVector verts[3];
		for (int32 triangle = 0; triangle < numTriangles; ++triangle)
		{

			for (int v = 0; v < 3; ++v)
			{
				FVector vert = positions[indices[triangle * 3 + v]];
				verts[v] = localToWorld.TransformPosition(vert);
			}

			FVector intersections[2];
			int intersectCount = 0;
			for (int edge = 0; edge < 3; ++edge)
			{
				FVector& p1 = verts[edge];
				FVector& p2 = verts[(edge + 1) % 3];
				if (((p1 - PlanePosition) | PlaneNormal) * ((p2 - PlanePosition) | PlaneNormal) < 0.0f)
				{   // Line crosses plane
					intersections[intersectCount++] = FMath::LinePlaneIntersection(p1, p2, PlanePosition, PlaneNormal);
					if (intersectCount == 2)
					{
						break;
					}

				}
			}
			if (intersectCount == 2)
			{
				OutEdges.Add(TPair<FVector, FVector>(intersections[0], intersections[1]));
			}
		}
	}

	return true;
}

bool UModumateGeometryStatics::CreateProcMeshCapFromPlane(UProceduralMeshComponent* InCapProcMesh, TArray<UProceduralMeshComponent*> InProcMeshes, TArray<UStaticMeshComponent*> InStaticMeshes, const FPlane& CutPlane, const int32 SectionIdOnly, UMaterialInterface* CapMaterial)
{
	FVector capMeshLocation = InCapProcMesh->GetComponentLocation();

	TArray<TPair<FVector, FVector>> outEdges;
	for (auto& curProcMesh : InProcMeshes)
	{
		ConvertProcMeshToLinesOnPlane(curProcMesh, CutPlane.GetOrigin(), CutPlane, outEdges, SectionIdOnly);
	}
	for (auto& curStaticMesh : InStaticMeshes)
	{
		ConvertStaticMeshToLinesOnPlane(curStaticMesh, CutPlane.GetOrigin(), CutPlane, outEdges);
	}
	if (outEdges.Num() == 0)
	{
		return false;
	}

	TArray<FUtilEdge3D> clipEdges;
	for (auto& edge : outEdges)
	{
		FUtilEdge3D newEdge;
		newEdge.V0 = edge.Key; newEdge.V1 = edge.Value;
		clipEdges.Add(newEdge);
	}

	// This code is similar to UKismetProceduralMeshLibrary::SliceProceduralMesh()
	if (clipEdges.Num() == 0)
	{
		return false;
	}

	FProcMeshSection capSection;
	int32 capSectionIndex = InCapProcMesh->GetNumSections();

	// Project 3D edges onto slice plane to form 2D edges
	TArray<FUtilEdge2D> edges2D;
	FUtilPoly2DSet polySet;
	FGeomTools::ProjectEdges(edges2D, polySet.PolyToWorld, clipEdges, CutPlane);

	// Find 2D closed polygons from this edge soup
	FGeomTools::Buid2DPolysFromEdges(polySet.Polys, edges2D, FColor::White);

	// Triangulate each poly
	for (int32 polyIdx = 0; polyIdx < polySet.Polys.Num(); polyIdx++)
	{
		if (polySet.Polys[polyIdx].Verts.Num() < 3)
		{
			continue;
		}

		// Generate UVs for the 2D polygon.
		FGeomTools::GeneratePlanarTilingPolyUVs(polySet.Polys[polyIdx], 64.f);

		// Remember start of vert buffer before adding triangles for this poly
		int32 polyVertBase = capSection.ProcVertexBuffer.Num();

		// Transform from 2D poly verts to 3D
		// This code is mostly copied from UKismetProceduralMeshLibrary cpp, but with position modify
		FVector polyNormal = -polySet.PolyToWorld.GetUnitAxis(EAxis::Z);
		FProcMeshTangent polyTangent(polySet.PolyToWorld.GetUnitAxis(EAxis::X), false);
		for (int32 vertexIndex = 0; vertexIndex < polySet.Polys[polyIdx].Verts.Num(); vertexIndex++)
		{
			const FUtilVertex2D& inVertex = polySet.Polys[polyIdx].Verts[vertexIndex];
			FProcMeshVertex newVert;
			newVert.Position = polySet.PolyToWorld.TransformPosition(FVector(inVertex.Pos.X, inVertex.Pos.Y, 0.f)) - capMeshLocation;
			newVert.Normal = polyNormal;
			newVert.Tangent = polyTangent;
			newVert.Color = inVertex.Color;
			newVert.UV0 = inVertex.UV;
			capSection.ProcVertexBuffer.Add(newVert);
			// Update bounding box
			capSection.SectionLocalBox += newVert.Position;
		}

		// Triangulate with GTE
		TArray<FVector2D> tempPolypoints;
		for (auto& curVert : polySet.Polys[polyIdx].Verts)
		{
			tempPolypoints.Add(curVert.Pos);
		}
		TArray<FVector2D> vertices2D;
		TArray<int32> tris2D;
		TArray<FPolyHole2D> validHoles; // empty
		bool bGTEresult = TriangulateVerticesGTE(tempPolypoints, validHoles, tris2D, &vertices2D);
		if (!bGTEresult)
		{
			return false;
		}

		// Append tris into buffer
		TArray<uint32> utris2D;
		for (auto& curTri : tris2D)
		{
			utris2D.Add(curTri + polyVertBase);
		}
		Algo::Reverse(utris2D); // TODO: check winding of input verts
		capSection.ProcIndexBuffer.Append(utris2D);
	}

	// Set geom for cap section
	InCapProcMesh->SetProcMeshSection(capSectionIndex, capSection);

	// Assign cap material
	InCapProcMesh->SetMaterial(capSectionIndex, CapMaterial);

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

bool UModumateGeometryStatics::GetEdgeIntersections(const TArray<FVector> &Positions, const FVector &IntersectionOrigin, const FVector &IntersectionDir, TArray<FEdgeIntersection> &OutEdgeIntersections, float Epsilon)
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

		// Reject parallel rays early, since perfectly aligned edges/points can report intersection by RayIntersection3D
		if (FVector::Parallel(IntersectionDir, edgeDir))
		{
			continue;
		}

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
				OutEdgeIntersections.Add(FEdgeIntersection(edgeIdxA, edgeIdxB, distAlongRay, distAlongEdge, edgeIntersection));
			}
			// if the intersection is on a vertex, create the intersection so that the EdgeIdxA field holds the vertexID
			else if (bNearVertexB)
			{
				intersectedIdxs.Add(edgeIdxB);
				OutEdgeIntersections.Add(FEdgeIntersection(edgeIdxB, (edgeIdxB + 1) % numSourceEdges, distAlongRay, distAlongEdge, edgeIntersection));
			}
			else
			{
				OutEdgeIntersections.Add(FEdgeIntersection(edgeIdxA, edgeIdxB, distAlongRay, distAlongEdge, edgeIntersection));
			}
		}

	}

	OutEdgeIntersections.Sort();
	return (OutEdgeIntersections.Num() > 0);
	
}

void UModumateGeometryStatics::GetUniquePoints2D(const TArray<FVector2D>& InPoints, TArray<FVector2D>& OutPoints, float Tolerance)
{
	OutPoints.Reset();

	for (auto& inPoint : InPoints)
	{
		bool bUniquePoint = true;

		for (auto& outPoint : OutPoints)
		{
			if (outPoint.Equals(inPoint, Tolerance))
			{
				bUniquePoint = false;
				break;
			}
		}

		if (bUniquePoint)
		{
			OutPoints.Add(inPoint);
		}
	}
}

void UModumateGeometryStatics::GetUniquePoints(const TArray<FVector>& InPoints, TArray<FVector>& OutPoints, float Tolerance)
{
	OutPoints.Reset();

	for (auto& inPoint : InPoints)
	{
		bool bUniquePoint = true;

		for (auto& outPoint : OutPoints)
		{
			if (outPoint.Equals(inPoint, Tolerance))
			{
				bUniquePoint = false;
				break;
			}
		}

		if (bUniquePoint)
		{
			OutPoints.Add(inPoint);
		}
	}
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

bool UModumateGeometryStatics::IsPolygon2DValid(const TArray<FVector2D> &Points, FFeedbackContext* InWarn, float DistEpsilon, float DotEpsilon)
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

		if (segAStart.Equals(segAEnd, DistEpsilon))
		{
			if (InWarn)
			{
				InWarn->Logf(ELogVerbosity::Error, TEXT("Line segment (%s, %s) is zero-length!"), *segAStart.ToString(), *segAEnd.ToString());
			}
			return false;
		}

		for (int32 segIdxBStart = 0; segIdxBStart < numPoints; ++segIdxBStart)
		{
			if (segIdxAStart == segIdxBStart)
			{
				continue;
			}

			int32 segIdxBEnd = (segIdxBStart + 1) % numPoints;
			const FVector2D &segBStart = Points[segIdxBStart];
			const FVector2D &segBEnd = Points[segIdxBEnd];

			if (segAStart.Equals(segBStart, DistEpsilon))
			{
				if (InWarn)
				{
					InWarn->Logf(ELogVerbosity::Error, TEXT("Polygon has non-consecutive repeat points (%s): indices %d and %d!"),
						*segAStart.ToString(), segIdxAStart, segIdxBStart);
				}
				return false;
			}

			if (!segAStart.Equals(segBEnd, DistEpsilon) && !segAEnd.Equals(segBStart, DistEpsilon) && !segAEnd.Equals(segBEnd, DistEpsilon))
			{
				FVector2D intersectionPoint;
				bool bSegmentsOverlap;
				if (UModumateGeometryStatics::SegmentIntersection2D(segAStart, segAEnd, segBStart, segBEnd, intersectionPoint, bSegmentsOverlap, DotEpsilon))
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

// Is OuterLine same-as or containing InnerLine?
bool UModumateGeometryStatics::IsLineSegmentWithin2D(const FEdge& OuterLine, const FEdge& InnerLine, float epsilon /*= THRESH_POINTS_ARE_NEAR*/)
{
	return IsLineSegmentWithin2D(FVector2D(OuterLine.Vertex[0]), FVector2D(OuterLine.Vertex[1]),
		FVector2D(InnerLine.Vertex[0]), FVector2D(InnerLine.Vertex[1]), epsilon);
}

bool UModumateGeometryStatics::IsLineSegmentWithin2D(const FVector2D& OuterLineStart, const FVector2D& OuterLineEnd,
	const FVector2D& InnerLineStart, const FVector2D& InnerLineEnd, float epsilon /*= THRESH_POINTS_ARE_NEAR*/)
{
	FVector2D outerDir(OuterLineEnd - OuterLineStart);
	float epsilon2 = epsilon * epsilon;

	float lineLength2 = outerDir.SizeSquared();
	if (lineLength2 < epsilon2)
	{
		return false;
	}

	outerDir.Normalize();
	FVector2D innerDir((InnerLineEnd - InnerLineStart).GetSafeNormal());
	if (FMath::Abs(outerDir | innerDir) < THRESH_NORMALS_ARE_PARALLEL)
	{
		return false;
	}

	FVector2D innerP1 = InnerLineStart - OuterLineStart;
	FVector2D innerP2 = InnerLineEnd - OuterLineStart;

	float projectedInner1 = outerDir | innerP1;
	if (projectedInner1 < -epsilon || projectedInner1 * projectedInner1 > lineLength2 + 2.0f * epsilon2
		|| FVector2D::DistSquared(innerP1, projectedInner1 * outerDir) > epsilon2)
	{
		return false;
	}

	float projectedInner2 = outerDir | innerP2;
	if (projectedInner2 < -epsilon || projectedInner2 * projectedInner2 > lineLength2 + 2.0f * epsilon2
		|| FVector2D::DistSquared(innerP2, projectedInner2 * outerDir) > epsilon2)
	{
		return false;
	}

	return true;
}

bool UModumateGeometryStatics::IsLineSegmentWithin2D(const FVector2d& OuterLineStart, const FVector2d& OuterLineEnd,
	const FVector2d& InnerLineStart, const FVector2d& InnerLineEnd, double epsilon /*= THRESH_POINTS_ARE_NEAR*/)
{
	// TODO: Add this alias to Modumate header?
	using FVector2Double = FVector2<double>;

	FVector2Double outerDir(OuterLineEnd - OuterLineStart);
	double epsilon2 = epsilon * epsilon;

	double lineLength2 = outerDir.SquaredLength();
	if (lineLength2 < epsilon2)
	{
		return false;
	}

	outerDir.Normalize();
	FVector2Double innerDir(InnerLineEnd - InnerLineStart);
	innerDir.Normalize();
	if (FMath::Abs(outerDir.Dot(innerDir)) < THRESH_NORMALS_ARE_PARALLEL)
	{
		return false;
	}

	double lineLength = FMathd::Sqrt(lineLength2) + epsilon;

	FVector2Double innerP1 = InnerLineStart - OuterLineStart;

	double projectedInner1 = outerDir.Dot(innerP1);
	if (projectedInner1 < -epsilon || projectedInner1 > lineLength
		|| innerP1.DistanceSquared(projectedInner1 * outerDir) > epsilon2)
	{
		return false;
	}

	FVector2Double innerP2 = InnerLineEnd - OuterLineStart;
	
	double projectedInner2 = outerDir.Dot(innerP2);
	if (projectedInner2 < -epsilon || projectedInner2 > lineLength
		|| innerP2.DistanceSquared(projectedInner2 * outerDir) > epsilon2)
	{
		return false;
	}

	return true;
}

float Get2DCrossProductSign(const FVector2D& A, const FVector2D& B, float Epsilon = THRESH_NORMALS_ARE_ORTHOGONAL)
{
	float result = A ^ B;
	return FMath::IsNearlyZero(result, Epsilon) ? 0.0f : FMath::Sign(result);
}

bool UModumateGeometryStatics::IsLineSegmentBoundedByPoints2D(const FVector2D &StartPosition, const FVector2D &EndPosition, const TArray<FVector2D> &Positions, const TArray<FVector2D> &EdgeNormals, float Epsilon)
{
	int32 numPoints = Positions.Num();
	FVector2D intersection;
	for (int32 idx = 0; idx < numPoints; idx++)
	{
		int32 nextIdx = (idx + 1) % numPoints;

		bool bSegmentsOverlap;
		bool bSegmentsIntersect = UModumateGeometryStatics::SegmentIntersection2D(
			StartPosition,
			EndPosition,
			Positions[idx],
			Positions[nextIdx],
			intersection,
			bSegmentsOverlap,
			Epsilon);

		if (bSegmentsIntersect && !bSegmentsOverlap)
		{
			// if the intersection is on the vertices of the edges, there are special cases to be handled
			bool bOnEdgeStart = intersection.Equals(StartPosition, Epsilon);
			bool bOnEdgeEnd = intersection.Equals(EndPosition, Epsilon);
			bool bOnBoundsStart = intersection.Equals(Positions[idx], Epsilon);
			bool bOnBoundsEnd = intersection.Equals(Positions[(idx + 1) % numPoints], Epsilon);
			bool bExistingOnBounds = bOnBoundsStart || bOnBoundsEnd;

			// if it isn't on one of the vertices, there is a clean intersection with the polygon,
			// meaning part of the edge is outside it
			if (!(bExistingOnBounds || bOnEdgeStart || bOnEdgeEnd))
			{
				return true;
			}

			FVector2D edgeDir = (EndPosition - StartPosition).GetSafeNormal();

			// if the edges meet at a vertex, check whether the edge is on the correct side of the corner
			if (bExistingOnBounds && (bOnEdgeStart || bOnEdgeEnd))
			{
				int32 cornerIdx = bOnBoundsStart ? idx : nextIdx;
				edgeDir *= bOnEdgeStart ? 1 : -1;

				FVector2D currentPoint = Positions[cornerIdx];
				FVector2D nextPoint = Positions[(cornerIdx + 1) % numPoints];
				FVector2D prevPoint = Positions[(cornerIdx + numPoints - 1) % numPoints];

				FVector2D nextEdgeNormal = EdgeNormals[cornerIdx];
				FVector2D prevEdgeNormal = EdgeNormals[(cornerIdx + numPoints - 1) % numPoints];

				FVector2D dirToNextPoint = (nextPoint - currentPoint).GetSafeNormal();
				FVector2D dirToPrevPoint = (prevPoint - currentPoint).GetSafeNormal();

				bool bIsCornerConvex =
					((dirToPrevPoint | nextEdgeNormal) > THRESH_NORMALS_ARE_ORTHOGONAL) &&
					((dirToNextPoint | prevEdgeNormal) > THRESH_NORMALS_ARE_ORTHOGONAL);

				float prevToNextCrossAmount = Get2DCrossProductSign(dirToPrevPoint, dirToNextPoint);
				float prevToSplitCrossAmount = Get2DCrossProductSign(dirToPrevPoint, edgeDir);
				float nextToPrevCrossAmount = Get2DCrossProductSign(dirToNextPoint, dirToPrevPoint);
				float nextToSplitCrossAmount = Get2DCrossProductSign(dirToNextPoint, edgeDir);

				bool bSplitDirBetweenEdgesAcute = ((prevToNextCrossAmount * prevToSplitCrossAmount) >= 0) &&
					((nextToPrevCrossAmount * nextToSplitCrossAmount) >= 0);

				// this makes the check inclusive by checking whether the edge has the same direction as the polygon edge
				bool bSplitDirBetweenEdgesZero = ((prevToNextCrossAmount * prevToSplitCrossAmount) == 0) ||
					((nextToPrevCrossAmount * nextToSplitCrossAmount) == 0);

				if (!bSplitDirBetweenEdgesZero && (bIsCornerConvex != bSplitDirBetweenEdgesAcute))
				{
					return true;
				}
			}
			else if (bOnEdgeStart || bOnEdgeEnd)
			{
				// If the edge normal of the bounds is pointing away from the direction of the edge to test,
				// then the edge is outside the polygon.

				edgeDir *= bOnEdgeStart ? 1 : -1;
				float edgeBoundsDot = edgeDir | EdgeNormals[idx];
				if (edgeBoundsDot < 0)
				{
					return true;
				}
			}
			else
			{
				// If the edge normal of the bounds is pointing away from the direction of the edge to test, relative to the intersection point,
				// then the edge is outside the polygon.
				// TODO: verify that this behavior is necessary and correct, since it's currently impossible if this function is only used for
				// segments that have already performed splits based on intersections.
				FVector2D dir1 = (StartPosition - intersection).GetSafeNormal();
				FVector2D dir2 = (EndPosition - intersection).GetSafeNormal();
				float dot1 = dir1 | EdgeNormals[idx];
				float dot2 = dir2 | EdgeNormals[idx];
				if (dot1 < 0 || dot2 < 0)
				{
					return true;
				}
			}
		}
	}

	return false;
}

bool UModumateGeometryStatics::ArePlanesCoplanar(const FPlane& PlaneA, const FPlane& PlaneB, float ParallelDotThresh, float PlanarDistEpsilon)
{
	float normalsDot = FVector(PlaneA) | FVector(PlaneB);
	return
		((normalsDot >= ParallelDotThresh)  && FMath::IsNearlyEqual(PlaneA.W, PlaneB.W, PlanarDistEpsilon)) ||
		((normalsDot <= -ParallelDotThresh) && FMath::IsNearlyEqual(PlaneA.W, -PlaneB.W, PlanarDistEpsilon));
}

void UModumateGeometryStatics::GetPlaneIntersections(TArray<FVector>& OutIntersections, const TArray<FVector>& InPoints,
	const FPlane& InPlane, const FVector InLocation /*  = FVector::ZeroVector */)
{
	int32 numPoints = InPoints.Num();
	for (int32 pointIdx = 0; pointIdx < numPoints; pointIdx++)
	{
		FVector layerPoint1 = InPoints[pointIdx] + InLocation;
		FVector layerPoint2 = InPoints[(pointIdx + 1) % numPoints] + InLocation;

		FVector3d point;
		if (layerPoint1 != layerPoint2 && SegmentPlaneIntersectionDouble(layerPoint1, layerPoint2, InPlane, point))
		{
			OutIntersections.Add(FVector(point));
		}
	}
}

bool UModumateGeometryStatics::Points3dSorter(const FVector& rhs, const FVector& lhs)
{
	static constexpr float EPSILON = 1e-3f;
	if (!FMath::IsNearlyEqual(rhs.X, lhs.X, EPSILON))
	{
		return rhs.X > lhs.X;
	}
	else if (!FMath::IsNearlyEqual(rhs.Y, lhs.Y, EPSILON))
	{
		return rhs.Y > lhs.Y;
	}
	else if (!FMath::IsNearlyEqual(rhs.Z, lhs.Z, EPSILON))
	{
		return rhs.Z > lhs.Z;
	}
	return true;
};

void UModumateGeometryStatics::DeCasteljau(const FVector Points[4], int32 iterations, TArray<FVector>& outCurve)
{
	int32 n = 4;
	TArray<FVector> points(Points, n);

	for (int32 i = 0; i < iterations; ++i)
	{
		TArray<FVector> newPoints;
		newPoints.Add(points[0]);
		for (int32 p = 0; p < n - 1; p += 3)
		{
			FVector p1(0.5f * (points[p] + points[p + 1]));
			FVector p2(0.5f * (points[p + 1] + points[p + 2]));
			FVector p3(0.5f * (points[p + 2] + points[p + 3]));
			FVector p4(0.5f * (p1 + p2));
			FVector p5(0.5f * (p2 + p3));
			newPoints.Add(p1);
			newPoints.Add(p4);
			newPoints.Add(0.5f * (p4 + p5));
			newPoints.Add(p5);
			newPoints.Add(p3);
			newPoints.Add(points[p + 3]);
		}
		points = MoveTemp(newPoints);
		n = points.Num();
	}

	outCurve = MoveTemp(points);
};

bool UModumateGeometryStatics::SegmentPlaneIntersectionDouble(FVector3d StartPoint, FVector3d EndPoint, const FPlane& Plane, FVector3d& outIntersectionPoint)
{
	FVector3d normal(Plane);
	static constexpr double EPSILON = SMALL_NUMBER;

	double t = (Plane.W - StartPoint.Dot(normal)) / normal.Dot(EndPoint - StartPoint);
	if (t >= 0.0 - EPSILON && t <= 1.0 + EPSILON)
	{
		outIntersectionPoint = StartPoint + t * (EndPoint - StartPoint);
		return true;
	}
	else
	{
		return false;
	}
}

DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("Modumate Mesh To Silhouette"), STAT_ModumateMeshToLines, STATGROUP_Modumate);

void UModumateGeometryStatics::GetSilhouetteEdges(const TArray<FVector>& Vertices, const TArray<uint32>& Indices, const FVector& ViewDirection,
	TArray<FEdge>& OutEdges, double Epsilon /*= 0.4*/, double AngleThreshold /*= 0.9205 (23 deg) */)
{
	SCOPE_MS_ACCUMULATOR(STAT_ModumateMeshToLines);

	const double EpsilonSquare = Epsilon * Epsilon;
	FVector3d viewDir(ViewDirection);

	struct FLocalEdge
	{
		FVector3d A;
		FVector3d B;
		FVector3d N;
		bool bIsValid{ true };

		operator bool() const { return bIsValid; }
		bool lexicalVerticesCompare() const
		{
			if (A.X == B.X)
			{
				if (A.Y == B.Y)
				{
					return A.Z < B.Z;
				}
				return A.Y < B.Y;
			}
			return A.X < B.X;

		}
		void Normalize() { if (!lexicalVerticesCompare()) { Swap(A, B); } }
		bool operator==(const FLocalEdge& rhs) const
		{
			return ((A == rhs.A) && (B == rhs.B)) || ((A == rhs.B) && (B == rhs.A));
		}
	};

	TArray<FLocalEdge> edges;

	const int32 numTriangles = Indices.Num() / 3;
	for (int32 triangle = 0; triangle < numTriangles; ++triangle)
	{

		FVector3d vert0 = Vertices[Indices[triangle * 3 + 0]];
		FVector3d vert1 = Vertices[Indices[triangle * 3 + 1]];
		FVector3d vert2 = Vertices[Indices[triangle * 3 + 2]];
		if (vert0.DistanceSquared(vert1) > EpsilonSquare && vert0.DistanceSquared(vert2) > EpsilonSquare
			&& vert1.DistanceSquared(vert2) > EpsilonSquare)
		{
			FVector3d triNormal = ((vert2 - vert0).Cross(vert1 - vert0)).Normalized();
			if (viewDir.Dot(triNormal) <= 0.0)
			{
				edges.Add({ vert0, vert1, triNormal });
				edges.Add({ vert1, vert2, triNormal });
				edges.Add({ vert2, vert0, triNormal });
			}
		}
	}

	const int numEdges = edges.Num();
	for (auto& edge : edges)
	{
		edge.Normalize();
	}

	// Brute-force search for internal lines.
	for (int32 e1 = 0; e1 < numEdges; ++e1)
	{
		for (int32 e2 = e1 + 1; e2 < numEdges; ++e2)
		{
			auto& edge1 = edges[e1];
			auto& edge2 = edges[e2];
			if (edge1.A.DistanceSquared(edge2.A) < EpsilonSquare && edge1.B.DistanceSquared(edge2.B) < EpsilonSquare
				&& FMath::Abs(edge1.N.Dot(edge2.N)) > AngleThreshold)
			{
				edge1.bIsValid = false;
				edge2.bIsValid = false;
			}
		}
	}

	for (auto& edge : edges)
	{
		if (edge)
		{
			OutEdges.Emplace(FVector(edge.A), FVector(edge.B));
		}
	}
}

void UModumateGeometryStatics::GetSilhouetteEdges(TArray<FDrawingDesignerLined>& Edges, const FVector& ViewDirection,
	double Epsilon /*= 0.4*/, double AngleThreshold /*= 0.9205 (23 deg) */, bool bFastMode /*= true */)
{
	SCOPE_MS_ACCUMULATOR(STAT_ModumateMeshToLines);

	const FVector3d viewDir(ViewDirection);
	const int32 numEdges = Edges.Num();

	// Cull backfacing lines
	for (auto& e: Edges)
	{
		e.bValid = viewDir.Dot(e.N) <= 0.0;
	}

	if (bFastMode)
	{
		Algo::ForEach(Edges, [](FDrawingDesignerLined& l) { l.Canonicalize(); });
		Edges.Sort();

		for (int32 e1 = 0; e1 < numEdges;)
		{
			auto& edge1 = Edges[e1];
			if (!edge1)
			{
				++e1;
				continue;
			}

			const FVector3d& N = edge1.N;
			bool bDropLine = false;
			bool bSingleLine = false;
			int32 e2;
			for (e2 = e1 + 1; e2 < numEdges; ++e2)
			{
				if (!Edges[e2])
				{
					continue;
				}

				if (Edges[e1] == Edges[e2])
				{
					bSingleLine = false;
					if (FMath::Abs(N.Dot(Edges[e2].N)) > AngleThreshold)
					{
						bDropLine = true;
					}
					else
					{
						bSingleLine = true;
					}
				}
				else
				{
					break;
				}
			}

			if (bDropLine || bSingleLine)
			{
				for (int32 droppedLine = bSingleLine ? e1 + 1 : e1; droppedLine < e2; ++droppedLine)
				{
					Edges[droppedLine].bValid = false;
				}
			}
			e1 = e2;
		}
	}
	else
	{
		const double EpsilonSquare = Epsilon * Epsilon;

		for (int32 e1 = 0; e1 < numEdges; ++e1)
		{
			const auto& edge1 = Edges[e1];
			for (int32 e2 = e1 + 1; e2 < numEdges; ++e2)
			{
				const auto& edge2 = Edges[e2];
				// Increase tolerance for larger lines:
				const double toleranceFactor = FMath::Clamp(FMath::Min(edge1.Len, edge2.Len) / 10.0, 1.0, 2.0);
				const double edgeEpsilon = EpsilonSquare * toleranceFactor * toleranceFactor;
				if (edge2 &&
					((edge1.P1.DistanceSquared(edge2.P1) < edgeEpsilon && edge1.P2.DistanceSquared(edge2.P2) < edgeEpsilon) ||
						(edge1.P1.DistanceSquared(edge2.P2) < edgeEpsilon && edge1.P2.DistanceSquared(edge2.P1) < edgeEpsilon)) &&
					FMath::Abs(edge1.N.Dot(edge2.N)) > AngleThreshold)
				{
					edge1.bValid = false;
					edge2.bValid = false;
				}
			}
		}
	}
}

int32 UModumateGeometryStatics::ClipTriangleAtXYPlane(const FVector3d InTri[3], FVector3d OutTri[6])
{
	bool visible0 = InTri[0].Z >= 0.0;
	bool visible1 = InTri[1].Z >= 0.0;
	bool visible2 = InTri[2].Z >= 0.0;
	int32 numVisible = int32(visible0) + int32(visible1) + int32(visible2);
	if (numVisible == 3 || numVisible == 0)
	{
		return 0;
	}
	// Triangle intersects plane - there are two categories:
	// 1. one vertex in front of plane; clipping produces new triangle retaining that point.
	// 2. two vertices in front of plane; clipping produces quad which is decomposed to two triangles.
	if (numVisible == 1)
	{
		if (visible0)
		{
			FVector3d v(InTri[0]);
			OutTri[0] = v;
			OutTri[1] = v + (v.Z / (v.Z - InTri[1].Z)) * (InTri[1] - v);
			OutTri[1].Z = 0.0;
			OutTri[2] = v + (v.Z / (v.Z - InTri[2].Z)) * (InTri[2] - v);
			OutTri[2].Z = 0.0;
		}
		else if (visible1)
		{
			FVector3d v(InTri[1]);
			OutTri[1] = v;
			OutTri[0] = v + (v.Z / (v.Z - InTri[0].Z)) * (InTri[0] - v);
			OutTri[0].Z = 0.0;
			OutTri[2] = v + (v.Z / (v.Z - InTri[2].Z)) * (InTri[2] - v);
			OutTri[2].Z = 0.0;
		}
		else
		{
			FVector3d v(InTri[2]);
			OutTri[2] = v;
			OutTri[0] = v + (v.Z / (v.Z - InTri[0].Z)) * (InTri[0] - v);
			OutTri[0].Z = 0.0;
			OutTri[1] = v + (v.Z / (v.Z - InTri[1].Z)) * (InTri[1] - v);
			OutTri[2].Z = 0.0;
		}
		return 1;  // One new triangle created
	}
	else
	{
		if (!visible0)
		{
			FVector3d v(InTri[0]);
			OutTri[1] = InTri[1];
			OutTri[2] = InTri[2];
			OutTri[0] = v + (v.Z / (v.Z - InTri[1].Z)) * (InTri[1] - v);
			OutTri[0].Z = 0.0;
			OutTri[3] = OutTri[0];
			OutTri[4] = OutTri[2];
			OutTri[5] = v + (v.Z / (v.Z - InTri[2].Z)) * (InTri[2] - v);
			OutTri[5].Z = 0.0;
		}
		else if (!visible1)
		{
			FVector3d v(InTri[1]);
			OutTri[0] = InTri[0];
			OutTri[2] = InTri[2];
			OutTri[1] = v + (v.Z / (v.Z - InTri[2].Z)) * (InTri[2] - v);
			OutTri[1].Z = 0.0;
			OutTri[5] = OutTri[1];
			OutTri[3] = InTri[0];
			OutTri[4] = v + (v.Z / (v.Z - InTri[0].Z)) * (InTri[0] - v);
			OutTri[4].Z = 0.0;
		}
		else
		{
			FVector3d v(InTri[2]);
			OutTri[0] = InTri[0];
			OutTri[1] = InTri[1];
			OutTri[2] = v + (v.Z / (v.Z - InTri[0].Z)) * (InTri[0] - v);
			OutTri[2].Z = 0.0;
			OutTri[5] = OutTri[2];
			OutTri[3] = OutTri[1];
			OutTri[4] = v + (v.Z / (v.Z - InTri[1].Z)) * (InTri[1] - v);
			OutTri[4].Z = 0.0;
		}
		return 2;  // Two new triangles created
	}
}

void UModumateGeometryStatics::GetBoxCorners(const FBox& Box, TArray<FVector>& OutCorners)
{
	OutCorners.Append({
		Box.Min, FVector(Box.Min.X, Box.Min.Y, Box.Max.Z),
		FVector(Box.Min.X, Box.Max.Y, Box.Min.Z), FVector(Box.Min.X, Box.Max.Y, Box.Max.Z),
		FVector(Box.Max.X, Box.Min.Y, Box.Min.Z), FVector(Box.Max.X, Box.Min.Y, Box.Max.Z),
		FVector(Box.Max.X, Box.Max.Y, Box.Min.Z), Box.Max });
}
bool UModumateGeometryStatics::TrimProceduralMesh(UProceduralMeshComponent* InProcMesh, TArray<FSlicer> Slicers, EProcMeshSliceCapOption CapOption, UMaterialInterface* CapMaterial)
{
	for (int32 i = 0; i < Slicers.Num(); i++)
	{
		UProceduralMeshComponent* outMeshComp; //unused but required param
		UKismetProceduralMeshLibrary::SliceProceduralMesh(InProcMesh, Slicers[i].SliceOrigin, Slicers[i].SliceNormal, false, outMeshComp, CapOption, nullptr);
	}
	return true;
}