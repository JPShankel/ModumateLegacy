// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "ModumateCore/LayerGeomDef.h"


#define DEBUG_CHECK_LAYERS (!UE_BUILD_SHIPPING)

FLayerGeomDef::FLayerGeomDef()
{

}

FLayerGeomDef::FLayerGeomDef(const TArray<FVector>& Points, float InThickness, const FVector& InNormal)
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

	for (const FVector& pointA : PointsA)
	{
		PointsB.Add(pointA + pointsBDelta);
	}

	Origin = PointsA[0];
	AxisX = (Points[1] - Points[0]).GetSafeNormal();
	AxisY = (Normal ^ AxisX).GetSafeNormal();
	bValid = CachePoints2D();
}

FLayerGeomDef::FLayerGeomDef(const TArray<FVector>& InPointsA, const TArray<FVector>& InPointsB,
	const FVector& InNormal, const FVector& InAxisX, const TArray<FPolyHole3D>* InHoles)
{
	Init(InPointsA, InPointsB, InNormal, InAxisX, InHoles);
}

void FLayerGeomDef::Init(const TArray<FVector>& InPointsA, const TArray<FVector>& InPointsB,
	const FVector& InNormal, const FVector& InAxisX, const TArray<FPolyHole3D>* InHoles)
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

FVector2D FLayerGeomDef::ProjectPoint2D(const FVector& Point3D) const
{
	return UModumateGeometryStatics::ProjectPoint2D(Point3D, AxisX, AxisY, Origin);
}

FVector2D FLayerGeomDef::ProjectPoint2DSnapped(const FVector& Point3D, float Tolerance) const
{
	FVector2D projectedPoint = ProjectPoint2D(Point3D);

	for (const FVector2D& existingPoint : CachedPointsA2D)
	{
		if (projectedPoint.Equals(existingPoint, Tolerance))
		{
			return existingPoint;
		}
	}

	return projectedPoint;
}

FVector FLayerGeomDef::Deproject2DPoint(const FVector2D& Point2D, bool bSideA) const
{
	return UModumateGeometryStatics::Deproject2DPoint(Point2D, AxisX, AxisY, Origin) +
		(bSideA ? FVector::ZeroVector : (Thickness * Normal));
}

bool FLayerGeomDef::CachePoints2D()
{
	// Simply project the 3D layer pairs into 2D for triangulation
	CachedPointsA2D.Reset(PointsA.Num());
	for (const FVector& pointA : PointsA)
	{
		CachedPointsA2D.Add(ProjectPoint2D(pointA));
	}

	CachedPointsB2D.Reset(PointsB.Num());
	for (const FVector& pointB : PointsB)
	{
		CachedPointsB2D.Add(ProjectPoint2DSnapped(pointB));
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
		const FPolyHole3D& hole3D = Holes3D[holeIdx];
		FPolyHole2D& hole2D = CachedHoles2D.AddDefaulted_GetRef();
		bool& bHoleValid = HolesValid.Add_GetRef(true);
		for (const FVector& holePoint3D : hole3D.Points)
		{
			hole2D.Points.Add(ProjectPoint2DSnapped(holePoint3D));
		}

		// TODO: if triangulation is tolerant of holes that overlap with their containing polygon, or each other, then potentially remove these checks?
		// Otherwise, they only exist to detect inconsistencies between triangulations of the front and back of the layer, based on different hole handling.

		bool bHoleOverlapsA, bHolePartiallyInA, bHoleFullyInA;
		bool bSuccessA = UModumateGeometryStatics::GetPolygonIntersection(CachedPointsA2D, hole2D.Points, bHoleOverlapsA, bHolePartiallyInA, bHoleFullyInA);

		bool bHoleOverlapsB, bHolePartiallyInB, bHoleFullyInB;
		bool bSuccessB = UModumateGeometryStatics::GetPolygonIntersection(CachedPointsB2D, hole2D.Points, bHoleOverlapsB, bHolePartiallyInB, bHoleFullyInB);

		if (!bSuccessA || !bSuccessB ||
			bHoleOverlapsA || !(bHolePartiallyInA || bHoleFullyInA) ||
			bHoleOverlapsB || !(bHolePartiallyInB || bHoleFullyInB))
		{
			bHoleValid = false;
			continue;
		}

		for (const FPolyHole2D& otherHole2D : ValidHoles2D)
		{
			bool bHoleOverlapsOther, bHolePartiallyInOther, bHoleFullyInOther;
			UModumateGeometryStatics::GetPolygonIntersection(otherHole2D.Points, hole2D.Points, bHoleOverlapsOther, bHolePartiallyInOther, bHoleFullyInOther);

			if (bHolePartiallyInOther || bHoleFullyInOther)
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

void FLayerGeomDef::AppendTriangles(const TArray<FVector>& Verts, const TArray<int32>& SourceTriIndices,
	TArray<int32>& OutTris, bool bReverseTris)
{
	int32 numVertices = Verts.Num();
	int32 numTriIndices = SourceTriIndices.Num();
	for (int32 i = 0; i < numTriIndices; ++i)
	{
		int32 triIndex = bReverseTris ? SourceTriIndices[numTriIndices - 1 - i] : SourceTriIndices[i];
		OutTris.Add(triIndex + numVertices);
	}
}

bool FLayerGeomDef::TriangulateSideFace(const FVector2D& Point2DA1, const FVector2D& Point2DA2,
	const FVector2D& Point2DB1, const FVector2D& Point2DB2, bool bReverseTris,
	TArray<FVector>& OutVerts, TArray<int32>& OutTris,
	TArray<FVector>& OutNormals, TArray<FVector2D>& OutUVs, TArray<FProcMeshTangent>& OutTangents,
	const FVector2D& UVScale, const FVector& UVAnchor, float UVRotOffset) const
{
	TempSidePoints.SetNum(4);
	TempSidePoints[0] = Deproject2DPoint(Point2DA1, true);
	TempSidePoints[1] = Deproject2DPoint(Point2DA2, true);
	TempSidePoints[2] = Deproject2DPoint(Point2DB1, false);
	TempSidePoints[3] = Deproject2DPoint(Point2DB2, false);

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

bool FLayerGeomDef::TriangulateMesh(TArray<FVector>& OutVerts, TArray<int32>& OutTris, TArray<FVector>& OutNormals,
	TArray<FVector2D>& OutUVs, TArray<FProcMeshTangent>& OutTangents, const FVector& UVAnchor, float UVRotOffset) const
{
	const FVector2D uvScale = FVector2D(0.01f, 0.01f);

	if (!bValid)
	{
		return false;
	}

	TArray<FVector2D> verticesA2D, verticesB2D;
	TArray<int32> trisA2D, trisB2D;
	bool bTriangulatedA = UModumateGeometryStatics::TriangulateVerticesGTE(CachedPointsA2D, ValidHoles2D, trisA2D, &verticesA2D);

	if (!bTriangulatedA)
	{
		return false;
	}

	int32 numHoles = ValidHoles2D.Num();

	// If the layer has *any* thickness, then triangulate its opposite side so that there can be distinct triangles
	if (Thickness > SMALL_NUMBER)
	{
		bool bTriangulatedB = UModumateGeometryStatics::TriangulateVerticesGTE(CachedPointsB2D, ValidHoles2D, trisB2D, &verticesB2D);

		if (!bTriangulatedB)
		{
			return false;
		}

		// The perimeter and hole merging calculations should match; otherwise, we can't fix any differences between them.
		bool bSamePerimeters = (CachedPointsA2D.Num() == CachedPointsA2D.Num());
		if (!ensure(bSamePerimeters))
		{
			return false;
		}
	}

	int32 numPerimeterPoints = CachedPointsA2D.Num();

	// Side A
	AppendTriangles(OutVerts, trisA2D, OutTris, false);
	for (const FVector2D& vertexA2D : verticesA2D)
	{
		FVector vertexA = Deproject2DPoint(vertexA2D, true);
		OutVerts.Add(vertexA);
		FVector2D uvA = UModumateGeometryStatics::ProjectPoint2D(vertexA, -AxisX, AxisY, UVAnchor);
		OutUVs.Add(uvA * uvScale);
		OutNormals.Add(-Normal);
		OutTangents.Add(FProcMeshTangent(-AxisX, false));
	}

	// If the layer is infinitely thin, then exit before we bother creating triangles for the other sides.
	if (Thickness < SMALL_NUMBER)
	{
		return true;
	}

	// Side B
	AppendTriangles(OutVerts, trisB2D, OutTris, true);
	for (const FVector2D& vertexB2D : verticesB2D)
	{
		FVector vertexB = Deproject2DPoint(vertexB2D, false);
		OutVerts.Add(vertexB);
		FVector2D uvB = UModumateGeometryStatics::ProjectPoint2D(vertexB, AxisX, AxisY, UVAnchor);
		OutUVs.Add(uvB * uvScale);
		OutNormals.Add(Normal);
		OutTangents.Add(FProcMeshTangent(AxisX, false));
	}

	// If the layer is just *very* thin, then only make triangles for the opposite extrusion, not the sides
	// TODO: double-precision would give us a better opportunity to accurately make *tiny* slivers for the sides of extremely thin layers, if desired
	if (Thickness <= PLANAR_DOT_EPSILON)
	{
		return true;
	}

	// Perimeter sides
	for (int32 perimIdx1 = 0; perimIdx1 < numPerimeterPoints; ++perimIdx1)
	{
		int32 perimIdx2 = (perimIdx1 + 1) % numPerimeterPoints;

		TriangulateSideFace(CachedPointsA2D[perimIdx1], CachedPointsA2D[perimIdx2],
			CachedPointsB2D[perimIdx1], CachedPointsB2D[perimIdx2], !bCoincident,
			OutVerts, OutTris, OutNormals, OutUVs, OutTangents, uvScale, UVAnchor, UVRotOffset);
	}

	// Add side faces for insides of holes
	for (int32 holeIdx = 0; holeIdx < numHoles; ++holeIdx)
	{
		const FPolyHole2D& hole = ValidHoles2D[holeIdx];
		int32 numHolePoints = hole.Points.Num();
		bool bPointsConcave, bHolePointsCW;
		UModumateGeometryStatics::GetPolygonWindingAndConcavity(hole.Points, bHolePointsCW, bPointsConcave);
		for (int32 holeSideIdx1 = 0; holeSideIdx1 < numHolePoints; ++holeSideIdx1)
		{
			int32 holeSideIdx2 = (holeSideIdx1 + 1) % numHolePoints;
			const FVector2D& holeSidePoint1 = hole.Points[holeSideIdx1];
			const FVector2D& holeSidePoint2 = hole.Points[holeSideIdx2];
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

void FLayerGeomDef::GetRangesForHolesOnPlane(TArray<TPair<float, float>>& OutRanges, TPair<FVector, FVector>& Intersection,
	const FVector& HoleOffset, const FPlane& Plane, const FVector& PlaneAxisX, const FVector& PlaneAxisY, const FVector& PlaneOrigin) const
{
	FVector intersectionStart = Intersection.Key;
	FVector intersectionEnd = Intersection.Value;

	// TODO: only works when the plane goes through the object once (2 intersections)
	FVector2D start = UModumateGeometryStatics::ProjectPoint2D(PlaneOrigin, PlaneAxisX, PlaneAxisY, intersectionStart);
	FVector2D end = UModumateGeometryStatics::ProjectPoint2D(PlaneOrigin, PlaneAxisX, PlaneAxisY, intersectionEnd);
	float length = (end - start).Size();
	FVector2D intersectionDir = (end - start).GetSafeNormal();

	TArray<TPair<float, float>> holeRanges;
	TArray<TPair<float, float>> mergedRanges;
	int32 idx = 0;

	// intersect holes with the plane, and figure out the ranges along the original line where there is a valid hole
	for (auto& hole : Holes3D)
	{
		if (!HolesValid[idx])
		{
			continue;
		}
		TArray<FVector> projectedHolePoints;
		for (auto& point : hole.Points)
		{
			// TODO: cache this value
			projectedHolePoints.Add(point + HoleOffset);
		}

		// TODO: once there is potential for concave holes, generalize the sorter in GetDraftingLines and sort here as well
		TArray<FVector> holeIntersections;
		UModumateGeometryStatics::GetPlaneIntersections(holeIntersections, projectedHolePoints, Plane);

		if (holeIntersections.Num() != 2)
		{
			continue;
		}
		FVector2D holeStart = UModumateGeometryStatics::ProjectPoint2D(PlaneOrigin, PlaneAxisX, PlaneAxisY, holeIntersections[0]);
		FVector2D holeEnd = UModumateGeometryStatics::ProjectPoint2D(PlaneOrigin, PlaneAxisX, PlaneAxisY, holeIntersections[1]);

		float holeDotIntersection = (holeStart - start) | intersectionDir;
		FVector2D holeStartOnIntersection = holeDotIntersection * intersectionDir;
		if (holeDotIntersection < 0.0f || holeStartOnIntersection.Size() > length)
		{	// If one end of hole is off the segment both must be.
			continue;
		}

		float hs = (holeStart - start).Size() / length;
		float he = (holeEnd - start).Size() / length;
		if (hs > he)
		{
			Swap(hs, he);
		}
		holeRanges.Add(TPair<float, float>(hs, he));
		idx++;
	}

	holeRanges.Sort();

	//TODO: merging the ranges may be unnecessary because it seems like if holes overlap, one of them will be invalid
	for (auto& range : holeRanges)
	{
		if (mergedRanges.Num() == 0)
		{
			mergedRanges.Push(range);
			continue;
		}
		auto& currentRange = mergedRanges.Top();
		// if the sorted ranges overlap, combine them
		if (range.Value > currentRange.Value && range.Key < currentRange.Value)
		{
			mergedRanges.Top() = TPair<float, float>(currentRange.Key, range.Value);
		}
		else if (range.Key > currentRange.Value)
		{
			mergedRanges.Push(range);
		}
	}

	// lines for drafting are drawn in the areas where there aren't holes, 
	// so OutRanges inverts the merged ranges (which is where there are holes)
	if (mergedRanges.Num() == 0)
	{
		OutRanges.Add(TPair<float, float>(0, 1));
	}
	else
	{
		OutRanges.Add(TPair<float, float>(0, mergedRanges[0].Key));
		for (int32 rangeIdx = 0; rangeIdx < mergedRanges.Num() - 1; rangeIdx++)
		{
			OutRanges.Add(TPair<float, float>(mergedRanges[rangeIdx].Value, mergedRanges[rangeIdx + 1].Key));
		}
		OutRanges.Add(TPair<float, float>(mergedRanges[mergedRanges.Num() - 1].Value, 1));
	}
}
