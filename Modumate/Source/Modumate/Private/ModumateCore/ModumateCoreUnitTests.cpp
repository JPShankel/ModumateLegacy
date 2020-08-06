#include "CoreMinimal.h"

#include "Algo/Transform.h"
#include "ModumateCore/ExpressionEvaluator.h"
#include "ModumateCore/ModumateConsoleCommand.h"
#include "ModumateCore/ModumateDimensionStatics.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "ModumateCore/ModumateGeometryStatics.h"

namespace Modumate
{
	// Modumate Geometry Statics

	// Ray Intersection Tests
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGeometryRayIntersection, "Modumate.Core.Geometry.RayIntersection", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
	bool FModumateGeometryRayIntersection::RunTest(const FString& Parameters)
	{
		FVector2D intersectionPoint;
		float rayDistA, rayDistB;
		bool bIntersection, bColinear;
		bool bSuccess = true;;

		bIntersection = UModumateGeometryStatics::RayIntersection2D(
			FVector2D(1.0f, 1.0f), FVector2D(2.0f, -1.0f).GetSafeNormal(),
			FVector2D(-1.0f, -10.0f), FVector2D(1.0f, 0.0f),
			intersectionPoint, rayDistA, rayDistB, bColinear, true);

		bSuccess = bSuccess && bIntersection && (rayDistA > 0.0f) && (rayDistB > 0.0f) &&
			intersectionPoint.Equals(FVector2D(23.0f, -10.f));

		// coincident, non-colinear rays
		bIntersection = UModumateGeometryStatics::RayIntersection2D(
			FVector2D(0.0f, 0.0f), FVector2D(1.0f, 0.0f),
			FVector2D(4.0f, 1.0f), FVector2D(1.0f, 0.0f),
			intersectionPoint, rayDistA, rayDistB, bColinear, false);
		bSuccess = bSuccess && !bIntersection;

		// coincident, colinear rays
		bIntersection = UModumateGeometryStatics::RayIntersection2D(
			FVector2D(0.0f, 0.0f), FVector2D(1.0f, 0.0f),
			FVector2D(4.0f, 0.0f), FVector2D(1.0f, 0.0f),
			intersectionPoint, rayDistA, rayDistB, bColinear, false);
		bSuccess = bSuccess && bIntersection &&
			FMath::IsNearlyEqual(rayDistA, 4.0f) &&
			FMath::IsNearlyEqual(rayDistB, 0.0f) &&
			intersectionPoint.Equals(FVector2D(4.0f, 0.0f));

		// anti-parallel, colinear rays pointing at each other
		bIntersection = UModumateGeometryStatics::RayIntersection2D(
			FVector2D(0.0f, 0.0f), FVector2D(1.0f, 0.0f),
			FVector2D(4.0f, 0.0f), FVector2D(-1.0f, 0.0f),
			intersectionPoint, rayDistA, rayDistB, bColinear, false);
		bSuccess = bSuccess && bIntersection &&
			FMath::IsNearlyEqual(rayDistA, 2.0f) &&
			FMath::IsNearlyEqual(rayDistB, 2.0f) &&
			intersectionPoint.Equals(FVector2D(2.0f, 0.0f));

		// anti-parallel, colinear rays pointing away from each other
		bIntersection = UModumateGeometryStatics::RayIntersection2D(
			FVector2D(0.0f, 0.0f), FVector2D(-1.0f, 0.0f),
			FVector2D(4.0f, 0.0f), FVector2D(1.0f, 0.0f),
			intersectionPoint, rayDistA, rayDistB, bColinear, false);
		bSuccess = bSuccess && bIntersection &&
			FMath::IsNearlyEqual(rayDistA, -2.0f) &&
			FMath::IsNearlyEqual(rayDistB, -2.0f) &&
			intersectionPoint.Equals(FVector2D(2.0f, 0.0f));

		return bSuccess;
	}

	// Point in poly tests
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGeometryPointInPoly, "Modumate.Core.Geometry.PointInPoly", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
	bool FModumateGeometryPointInPoly::RunTest(const FString& Parameters)
	{
		TArray<FVector2D> triangle({
			FVector2D(0.0f, 0.0f),
			FVector2D(10.0f, 0.0f),
			FVector2D(5.0f, 8.0f),
		});

		FPointInPolyResult pointInPolyResult;
		bool bSuccess;

		bSuccess = UModumateGeometryStatics::TestPointInPolygon(FVector2D(5.0f, 4.0f), triangle, pointInPolyResult);
		TestTrue(TEXT("bInsideTriangle1"), bSuccess && pointInPolyResult.bInside && !pointInPolyResult.bOverlaps);

		bSuccess = UModumateGeometryStatics::TestPointInPolygon(FVector2D(-5.0f, 4.0f), triangle, pointInPolyResult);
		TestTrue(TEXT("!bInsideTriangle2"), bSuccess && !pointInPolyResult.bInside && !pointInPolyResult.bOverlaps);

		bSuccess = UModumateGeometryStatics::TestPointInPolygon(FVector2D(15.0f, 4.0f), triangle, pointInPolyResult);
		TestTrue(TEXT("!bInsideTriangle3"), bSuccess && !pointInPolyResult.bInside && !pointInPolyResult.bOverlaps);

		bSuccess = UModumateGeometryStatics::TestPointInPolygon(FVector2D(5.0f, 14.0f), triangle, pointInPolyResult);
		TestTrue(TEXT("!bInsideTriangle4"), bSuccess && !pointInPolyResult.bInside && !pointInPolyResult.bOverlaps);

		bSuccess = UModumateGeometryStatics::TestPointInPolygon(FVector2D(15.0f, 0.0f), triangle, pointInPolyResult);
		TestTrue(TEXT("!bInsideTriangle5"), bSuccess && !pointInPolyResult.bInside && !pointInPolyResult.bOverlaps);

		TArray<FVector2D> concaveUShape({
			FVector2D(0.0f, 0.0f),
			FVector2D(10.0f, 0.0f),
			FVector2D(10.0f, 1.0f),
			FVector2D(10.0f, 10.0f),
			FVector2D(8.0f, 10.0f),
			FVector2D(8.0f, 2.0f),
			FVector2D(2.0f, 2.0f),
			FVector2D(2.0f, 10.0f),
			FVector2D(0.0f, 10.0f),
			FVector2D(0.0f, 1.0f),
		});

		// Sanity concave intersection tests
		bSuccess = UModumateGeometryStatics::TestPointInPolygon(FVector2D(1.0f, 5.0f), concaveUShape, pointInPolyResult);
		TestTrue(TEXT("bInsideU1"), bSuccess && pointInPolyResult.bInside && !pointInPolyResult.bOverlaps);

		bSuccess = UModumateGeometryStatics::TestPointInPolygon(FVector2D(5.0f, 1.0f), concaveUShape, pointInPolyResult);
		TestTrue(TEXT("bInsideU2"), bSuccess && pointInPolyResult.bInside && !pointInPolyResult.bOverlaps);

		bSuccess = UModumateGeometryStatics::TestPointInPolygon(FVector2D(9.0f, 5.0f), concaveUShape, pointInPolyResult);
		TestTrue(TEXT("bInsideU3"), bSuccess && pointInPolyResult.bInside && !pointInPolyResult.bOverlaps);

		bSuccess = UModumateGeometryStatics::TestPointInPolygon(FVector2D(5.0f, 5.0f), concaveUShape, pointInPolyResult);
		TestTrue(TEXT("!bInsideU4"), bSuccess && !pointInPolyResult.bInside && !pointInPolyResult.bOverlaps);

		bSuccess = UModumateGeometryStatics::TestPointInPolygon(FVector2D(-5.0f, 5.0f), concaveUShape, pointInPolyResult);
		TestTrue(TEXT("!bInsideU5"), bSuccess && !pointInPolyResult.bInside && !pointInPolyResult.bOverlaps);

		bSuccess = UModumateGeometryStatics::TestPointInPolygon(FVector2D(15.0f, 5.0f), concaveUShape, pointInPolyResult);
		TestTrue(TEXT("!bInsideU6"), bSuccess && !pointInPolyResult.bInside && !pointInPolyResult.bOverlaps);

		// Inclusive overlap epsilon tests
		float halfEpsilon = 0.5f * RAY_INTERSECT_TOLERANCE;
		bSuccess = UModumateGeometryStatics::TestPointInPolygon(FVector2D(-halfEpsilon, 5.0f), concaveUShape, pointInPolyResult);
		TestTrue(TEXT("!bInsideU7"), bSuccess && !pointInPolyResult.bInside && pointInPolyResult.bOverlaps);

		bSuccess = UModumateGeometryStatics::TestPointInPolygon(FVector2D(halfEpsilon, 5.0f), concaveUShape, pointInPolyResult);
		TestTrue(TEXT("!bInsideU8"), bSuccess && !pointInPolyResult.bInside && pointInPolyResult.bOverlaps);

		bSuccess = UModumateGeometryStatics::TestPointInPolygon(FVector2D(10 - halfEpsilon, 5.0f), concaveUShape, pointInPolyResult);
		TestTrue(TEXT("!bInsideU9"), bSuccess && !pointInPolyResult.bInside && pointInPolyResult.bOverlaps);

		bSuccess = UModumateGeometryStatics::TestPointInPolygon(FVector2D(10 + halfEpsilon, 5.0f), concaveUShape, pointInPolyResult);
		TestTrue(TEXT("!bInsideU10"), bSuccess && !pointInPolyResult.bInside && pointInPolyResult.bOverlaps);

		bSuccess = UModumateGeometryStatics::TestPointInPolygon(FVector2D(halfEpsilon, 10.0f - halfEpsilon), concaveUShape, pointInPolyResult);
		TestTrue(TEXT("!bInsideU11"), bSuccess && !pointInPolyResult.bInside && pointInPolyResult.bOverlaps);

		bSuccess = UModumateGeometryStatics::TestPointInPolygon(FVector2D(-halfEpsilon, 10.0f + halfEpsilon), concaveUShape, pointInPolyResult);
		TestTrue(TEXT("!bInsideU12"), bSuccess && !pointInPolyResult.bInside && pointInPolyResult.bOverlaps);

		// Edge cases from point intersections
		bSuccess = UModumateGeometryStatics::TestPointInPolygon(FVector2D(1.0f, 1.0f), concaveUShape, pointInPolyResult);
		TestTrue(TEXT("bInsideU13"), bSuccess && pointInPolyResult.bInside && !pointInPolyResult.bOverlaps);

		bSuccess = UModumateGeometryStatics::TestPointInPolygon(FVector2D(1.0f, 2.0f), concaveUShape, pointInPolyResult);
		TestTrue(TEXT("bInsideU14"), bSuccess && pointInPolyResult.bInside && !pointInPolyResult.bOverlaps);

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGeometryPointInComplexPoly, "Modumate.Core.Geometry.PointInComplexPoly", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGeometryPointInComplexPoly::RunTest(const FString& Parameters)
	{
		// Check a complex concave polygon with a peninsula
		TArray<FVector2D> complexPolygon({
			FVector2D(0.0f, 0.0f),
			FVector2D(1.0f, 1.0f),
			FVector2D(0.0f, 0.0f),
			FVector2D(10.0f, 0.0f),
			FVector2D(9.0f, 1.0f),
			FVector2D(10.0f, 0.0f),
			FVector2D(10.0f, 10.0f),
			FVector2D(5.0f, 9.0f),
			FVector2D(4.0f, 8.0f),
			FVector2D(3.0f, 8.0f),
			FVector2D(4.0f, 8.0f),
			FVector2D(5.0f, 9.0f),
			FVector2D(0.0f, 10.0f),
		});

		TArray<FVector2D> complexPerimeter({
			FVector2D(0.0f, 0.0f),
			FVector2D(10.0f, 0.0f),
			FVector2D(10.0f, 10.0f),
			FVector2D(5.0f, 9.0f),
			FVector2D(0.0f, 10.0f),
		});

		FPointInPolyResult pointInPolyResult;
		bool bSuccess;

		bSuccess = UModumateGeometryStatics::TestPointInPolygon(FVector2D(0.5f, 0.5f), complexPolygon, complexPerimeter, pointInPolyResult);
		TestTrue(TEXT("OverlapsComplex1"), bSuccess && !pointInPolyResult.bInside && pointInPolyResult.bOverlaps);

		bSuccess = UModumateGeometryStatics::TestPointInPolygon(FVector2D(1.0f, 1.0f), complexPolygon, complexPerimeter, pointInPolyResult);
		TestTrue(TEXT("OverlapsComplex2"), bSuccess && !pointInPolyResult.bInside && pointInPolyResult.bOverlaps);

		bSuccess = UModumateGeometryStatics::TestPointInPolygon(FVector2D(9.5f, 0.5f), complexPolygon, complexPerimeter, pointInPolyResult);
		TestTrue(TEXT("OverlapsComplex3"), bSuccess && !pointInPolyResult.bInside && pointInPolyResult.bOverlaps);

		bSuccess = UModumateGeometryStatics::TestPointInPolygon(FVector2D(9.0f, 1.0f), complexPolygon, complexPerimeter, pointInPolyResult);
		TestTrue(TEXT("OverlapsComplex4"), bSuccess && !pointInPolyResult.bInside && pointInPolyResult.bOverlaps);

		bSuccess = UModumateGeometryStatics::TestPointInPolygon(FVector2D(0.25f, 0.5f), complexPolygon, complexPerimeter, pointInPolyResult);
		TestTrue(TEXT("InsideComplex1"), bSuccess && pointInPolyResult.bInside && !pointInPolyResult.bOverlaps);

		bSuccess = UModumateGeometryStatics::TestPointInPolygon(FVector2D(0.5f, 1.0f), complexPolygon, complexPerimeter, pointInPolyResult);
		TestTrue(TEXT("InsideComplex2"), bSuccess && pointInPolyResult.bInside && !pointInPolyResult.bOverlaps);

		bSuccess = UModumateGeometryStatics::TestPointInPolygon(FVector2D(8.0f, 0.5f), complexPolygon, complexPerimeter, pointInPolyResult);
		TestTrue(TEXT("InsideComplex3"), bSuccess && pointInPolyResult.bInside && !pointInPolyResult.bOverlaps);

		bSuccess = UModumateGeometryStatics::TestPointInPolygon(FVector2D(8.0f, 1.0f), complexPolygon, complexPerimeter, pointInPolyResult);
		TestTrue(TEXT("InsideComplex4"), bSuccess && pointInPolyResult.bInside && !pointInPolyResult.bOverlaps);

		bSuccess = UModumateGeometryStatics::TestPointInPolygon(FVector2D(9.5f, 0.75f), complexPolygon, complexPerimeter, pointInPolyResult);
		TestTrue(TEXT("InsideComplex5"), bSuccess && pointInPolyResult.bInside && !pointInPolyResult.bOverlaps);

		bSuccess = UModumateGeometryStatics::TestPointInPolygon(FVector2D(2.0f, 7.0f), complexPolygon, complexPerimeter, pointInPolyResult);
		TestTrue(TEXT("InsideComplex6"), bSuccess && pointInPolyResult.bInside && !pointInPolyResult.bOverlaps);

		bSuccess = UModumateGeometryStatics::TestPointInPolygon(FVector2D(2.0f, 8.0f), complexPolygon, complexPerimeter, pointInPolyResult);
		TestTrue(TEXT("InsideComplex7"), bSuccess && pointInPolyResult.bInside && !pointInPolyResult.bOverlaps);

		bSuccess = UModumateGeometryStatics::TestPointInPolygon(FVector2D(2.0f, 9.0f), complexPolygon, complexPerimeter, pointInPolyResult);
		TestTrue(TEXT("InsideComplex8"), bSuccess && pointInPolyResult.bInside && !pointInPolyResult.bOverlaps);

		bSuccess = UModumateGeometryStatics::TestPointInPolygon(FVector2D(2.0f, 10.0f), complexPolygon, complexPerimeter, pointInPolyResult);
		TestTrue(TEXT("OutsideComplex1"), bSuccess && !pointInPolyResult.bInside && !pointInPolyResult.bOverlaps);

		bSuccess = UModumateGeometryStatics::TestPointInPolygon(FVector2D(-1.0f, 1.0f), complexPolygon, complexPerimeter, pointInPolyResult);
		TestTrue(TEXT("OutsideComplex2"), bSuccess && !pointInPolyResult.bInside && !pointInPolyResult.bOverlaps);

		bSuccess = UModumateGeometryStatics::TestPointInPolygon(FVector2D(5.0f, 10.0f), complexPolygon, complexPerimeter, pointInPolyResult);
		TestTrue(TEXT("OutsideComplex3"), bSuccess && !pointInPolyResult.bInside && !pointInPolyResult.bOverlaps);

		bSuccess = UModumateGeometryStatics::TestPointInPolygon(FVector2D(-1.0f, 0.0f), complexPolygon, complexPerimeter, pointInPolyResult);
		TestTrue(TEXT("OutsideComplex4"), bSuccess && !pointInPolyResult.bInside && !pointInPolyResult.bOverlaps);

		bSuccess = UModumateGeometryStatics::TestPointInPolygon(FVector2D(-1.0f, 9.0f), complexPolygon, complexPerimeter, pointInPolyResult);
		TestTrue(TEXT("OutsideComplex5"), bSuccess && !pointInPolyResult.bInside && !pointInPolyResult.bOverlaps);

		bSuccess = UModumateGeometryStatics::TestPointInPolygon(FVector2D(-1.0f, 10.0f), complexPolygon, complexPerimeter, pointInPolyResult);
		TestTrue(TEXT("OutsideComplex6"), bSuccess && !pointInPolyResult.bInside && !pointInPolyResult.bOverlaps);

		return true;
	}

	// Poly intersection tests
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGeometryConvexPolyIntersection, "Modumate.Core.Geometry.ConvexPolyIntersection", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
	bool FModumateGeometryConvexPolyIntersection::RunTest(const FString& Parameters)
	{
		// TODO: these tests are currently unhelpful now that polygon intersections are primarily implemented specifically for faces,
		// where they can assume that all edge intersections have been resolved as vertices,
		// and the only other place that arbitrary polygon intersection is used is duplicated inside of polygon triangulation for holes.
		// Rewrite these tests once we have consolidated polygon intersection functions for all of the needed use cases.

#if 0
		// Poly intersection test group 1 - convex, non-overlapping

		TArray<FVector2D> smallPoly({
			FVector2D(1.0f, 1.0f),
			FVector2D(8.0f, 1.0f),
			FVector2D(8.0f, 7.0f),
			FVector2D(2.0f, 8.0f),
		});

		TArray<FVector2D> bigPoly({
			FVector2D(-1.0f, -1.0f),
			FVector2D(0.0f, 10.0f),
			FVector2D(12.0f, 11.0f),
			FVector2D(11.0f, 0.0f),
		});

		bool bPolyAInPolyB, bOverlapping;

		UModumateGeometryStatics::PolyIntersection(smallPoly, bigPoly, bPolyAInPolyB, bOverlapping);
		TestTrue(TEXT("bPolyAInPolyB"), bPolyAInPolyB && !bOverlapping);

		UModumateGeometryStatics::PolyIntersection(bigPoly, smallPoly, bPolyAInPolyB, bOverlapping);
		TestTrue(TEXT("!bPolyAInPolyB"), !bPolyAInPolyB && !bOverlapping);

		// Poly intersection test group 2 - convex, touching

		TArray<FVector2D> planePoly({
			FVector2D(0.0f, 0.0f),
			FVector2D(20.0f, 0.0f),
			FVector2D(20.0f, 10.0f),
			FVector2D(0.0f, 10.0f),
		});

		TArray<FVector2D> portalPoly({
			FVector2D(5.0f, 0.0f),
			FVector2D(5.0f, 7.0f),
			FVector2D(8.0f, 7.0f),
			FVector2D(8.0f, 0.0f),
		});

		// Simulate placing a door on a wall
		UModumateGeometryStatics::PolyIntersection(portalPoly, planePoly, bPolyAInPolyB, bOverlapping);
		TestTrue(TEXT("bPolyAInPolyB"), bPolyAInPolyB && !bOverlapping);

		// Ensure it still detects touching and not overlapping within the tolerance

		TArray<FVector2D> portalPolyLower;
		Algo::Transform(portalPoly, portalPolyLower, [](const FVector2D &p) { return p - FVector2D(0.0f, 0.5f * RAY_INTERSECT_TOLERANCE); });
		UModumateGeometryStatics::PolyIntersection(portalPolyLower, planePoly, bPolyAInPolyB, bOverlapping);
		TestTrue(TEXT("bPolyAInPolyB"), bPolyAInPolyB && !bOverlapping);

		TArray<FVector2D> portalPolyHigher;
		Algo::Transform(portalPoly, portalPolyHigher, [](const FVector2D &p) { return p + FVector2D(0.0f, 0.5f * RAY_INTERSECT_TOLERANCE); });
		UModumateGeometryStatics::PolyIntersection(portalPolyHigher, planePoly, bPolyAInPolyB, bOverlapping);
		TestTrue(TEXT("bPolyAInPolyB"), bPolyAInPolyB && !bOverlapping);

		// Ensure it doesn't detect touching outside of the tolerance

		portalPolyLower.Reset();
		Algo::Transform(portalPoly, portalPolyLower, [](const FVector2D &p) { return p - FVector2D(0.0f, 1.5f * RAY_INTERSECT_TOLERANCE); });
		UModumateGeometryStatics::PolyIntersection(portalPolyLower, planePoly, bPolyAInPolyB, bOverlapping);
		TestTrue(TEXT("!bPolyAInPolyB"), !bPolyAInPolyB && bOverlapping);

		portalPolyHigher.Reset();
		Algo::Transform(portalPoly, portalPolyHigher, [](const FVector2D &p) { return p + FVector2D(0.0f, 1.5f * RAY_INTERSECT_TOLERANCE); });
		UModumateGeometryStatics::PolyIntersection(portalPolyHigher, planePoly, bPolyAInPolyB, bOverlapping);
		TestTrue(TEXT("bPolyAInPolyB"), bPolyAInPolyB && !bOverlapping);


		// Poly intersection test group 3 - convex, overlapping

		TArray<FVector2D> rightPoly({
			FVector2D(1.0f, 1.0f),
			FVector2D(8.0f, 1.0f),
			FVector2D(8.0f, 7.0f),
			FVector2D(2.0f, 8.0f),
			});

		TArray<FVector2D> leftPoly({
			FVector2D(-1.0f, 4.0f),
			FVector2D(0.0f, 6.0f),
			FVector2D(12.0f, 6.0f),
			FVector2D(11.0f, 4.0f),
			});

		UModumateGeometryStatics::PolyIntersection(rightPoly, leftPoly, bPolyAInPolyB, bOverlapping);
		TestTrue(TEXT("!bPolyAInPolyB"), !bPolyAInPolyB && bOverlapping);

		UModumateGeometryStatics::PolyIntersection(leftPoly, rightPoly, bPolyAInPolyB, bOverlapping);
		TestTrue(TEXT("!bPolyAInPolyB"), !bPolyAInPolyB && bOverlapping);
#endif

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGeometryRayBounding, "Modumate.Core.Geometry.RayBounding", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGeometryRayBounding::RunTest(const FString& Parameters)
	{
		bool bBounded, bOverlaps;

		// Convex test
		FVector2D boundingRay1A(1.0f, 0.0f);
		FVector2D boundingNormal1A(0.0f, 1.0f);
		FVector2D boundingRay1B(0.0f, 1.0f);
		FVector2D boundingNormal1B(1.0f, 0.0f);

		bBounded = UModumateGeometryStatics::IsRayBoundedByRays(boundingRay1A, boundingRay1B, boundingNormal1A, boundingNormal1B, FVector2D(-1.0f, 0.0f), bOverlaps);
		TestTrue(TEXT("Test1 (-1, 0) not bounded"), !bBounded && !bOverlaps);

		bBounded = UModumateGeometryStatics::IsRayBoundedByRays(boundingRay1A, boundingRay1B, boundingNormal1A, boundingNormal1B, FVector2D(1.0f, 0.0f), bOverlaps);
		TestTrue(TEXT("Test1 (1, 0) overlaps"), !bBounded && bOverlaps);

		bBounded = UModumateGeometryStatics::IsRayBoundedByRays(boundingRay1A, boundingRay1B, boundingNormal1A, boundingNormal1B, FVector2D(0.0f, 1.0f), bOverlaps);
		TestTrue(TEXT("Test1 (0, 1) overlaps"), !bBounded && bOverlaps);

		bBounded = UModumateGeometryStatics::IsRayBoundedByRays(boundingRay1A, boundingRay1B, boundingNormal1A, boundingNormal1B, FVector2D(1.0f, 1.0f).GetSafeNormal(), bOverlaps);
		TestTrue(TEXT("Test1 (0.7, 0.7) bounded"), bBounded && !bOverlaps);

		// Concave test
		FVector2D boundingRay2A(1.0f, 0.0f);
		FVector2D boundingNormal2A(0.0f, -1.0f);
		FVector2D boundingRay2B(0.0f, 1.0f);
		FVector2D boundingNormal2B(-1.0f, 0.0f);

		bBounded = UModumateGeometryStatics::IsRayBoundedByRays(boundingRay2A, boundingRay2B, boundingNormal2A, boundingNormal2B, FVector2D(-1.0f, 0.0f), bOverlaps);
		TestTrue(TEXT("Test2 (-1, 0) bounded"), bBounded && !bOverlaps);

		bBounded = UModumateGeometryStatics::IsRayBoundedByRays(boundingRay2A, boundingRay2B, boundingNormal2A, boundingNormal2B, FVector2D(1.0f, 0.0f), bOverlaps);
		TestTrue(TEXT("Test2 (1, 0) overlaps"), !bBounded && bOverlaps);

		bBounded = UModumateGeometryStatics::IsRayBoundedByRays(boundingRay2A, boundingRay2B, boundingNormal2A, boundingNormal2B, FVector2D(0.0f, 1.0f), bOverlaps);
		TestTrue(TEXT("Test2 (0, 1) overlaps"), !bBounded && bOverlaps);

		bBounded = UModumateGeometryStatics::IsRayBoundedByRays(boundingRay2A, boundingRay2B, boundingNormal2A, boundingNormal2B, FVector2D(1.0f, 1.0f).GetSafeNormal(), bOverlaps);
		TestTrue(TEXT("Test2 (0.7, 0.7) not bounded"), !bBounded && !bOverlaps);

		return true;
	}

	// Layer definition and triangulation via structured data
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGeometryLayerTriangulation, "Modumate.Core.Geometry.LayerTriangulation", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGeometryLayerTriangulation::RunTest(const FString& Parameters)
	{
		bool bSuccess = true;

		// Simple case: same vertices on both sides, no holes
		FLayerGeomDef simpleLayer(TArray<FVector>({
			FVector::ZeroVector, FVector(10.0f, 0.0f, 0.0f),
			FVector(10.0f, 0.0f, 5.0f), FVector(0.0f, 0.0f, 5.0f) }),
			1.0f, FVector(0.0f, 1.0f, 0.0f));
		TArray<FVector> simpleVerts, simpleNormals;
		TArray<int32> simpleTris;
		TArray<FVector2D> simpleUVs;
		TArray<FProcMeshTangent> simpleTangents;
		bool bSimpleTriSuccess = simpleLayer.TriangulateMesh(simpleVerts, simpleTris, simpleNormals, simpleUVs, simpleTangents);
		bSuccess = bSuccess && bSimpleTriSuccess;

		// Harder case: different vertices on both sides, hole(s)
		TArray<FVector> complexLayerVertsA({
			FVector::ZeroVector, FVector(10.0f, 0.0f, 0.0f),
			FVector(10.0f, 0.0f, 5.0f), FVector(0.0f, 0.0f, 5.0f)
		});
		TArray<FVector> complexLayerVertsB({
			FVector(0.0f, 1.0f, 0.0f), FVector(12.0f, 1.0f, 0.0f),
			FVector(12.0f, 1.0f, 6.0f), FVector(0.0f, 1.0f, 6.0f)
		});
		TArray<FPolyHole3D> holes({
			FPolyHole3D(TArray<FVector>({
				FVector(2.0f, 0.0f, 1.0f), FVector(5.0f, 0.0f, 1.0f),
				FVector(5.0f, 0.0f, 3.0f), FVector(2.0f, 0.0f, 3.0f)
			}))
		});

		FLayerGeomDef complexLayer(complexLayerVertsA, complexLayerVertsB, FVector(0.0f, 1.0f, 0.0f), FVector(1.0f, 0.0f, 0.0f), &holes);
		TArray<FVector> complexVerts, complexNormals;
		TArray<int32> complexTris;
		TArray<FVector2D> complexUVs;
		TArray<FProcMeshTangent> complexTangents;
		bool bComplexTriSuccess = complexLayer.TriangulateMesh(complexVerts, complexTris, complexNormals, complexUVs, complexTangents);
		bSuccess = bSuccess && bComplexTriSuccess;

		return bSuccess;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGeometryTriangulateVertices, "Modumate.Core.Geometry.TriangulateVertices", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGeometryTriangulateVertices::RunTest(const FString& Parameters)
	{
		TArray<FVector2D> OutVertices;
		TArray<int32> OutTriangles;

		TArray<FVector2D> InVertices;
		TArray<FPolyHole2D> InHoles;

		InVertices = {
			FVector2D(0.0f, 0.0f),
			FVector2D(0.0f, 100.0f),
			FVector2D(100.0f, 100.0f),
			FVector2D(100.0f, 0.0f)
		};

		TestTrue(TEXT("simple square"), UModumateGeometryStatics::TriangulateVerticesPoly2Tri(InVertices, InHoles, OutTriangles, &OutVertices));

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGeometryTriangulateHoles, "Modumate.Core.Geometry.TriangulateHoles", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGeometryTriangulateHoles::RunTest(const FString& Parameters)
	{
		TArray<FVector2D> perimeter, outVertices;
		TArray<FPolyHole2D> holes;
		TArray<int32> outTriangleIndices;
		TSet<FVector2D> uniqueVertices;
		bool bTriangulationSuccess;
		int32 expectedUniqueVertices;

		perimeter = {
			FVector2D(0.0f, 0.0f),
			FVector2D(0.0f, 100.0f),
			FVector2D(100.0f, 100.0f),
			FVector2D(100.0f, 0.0f)
		};

		holes = { FPolyHole2D({
			FVector2D(25.0f, 25.0f),
			FVector2D(75.0f, 25.0f),
			FVector2D(75.0f, 75.0f),
			FVector2D(25.0f, 75.0f)
		}) };

		bTriangulationSuccess = UModumateGeometryStatics::TriangulateVerticesPoly2Tri(perimeter, holes, outTriangleIndices, &outVertices);
		TestTrue(TEXT("Square with a hole in the middle"), bTriangulationSuccess && (outTriangleIndices.Num() > 6) && (outVertices.Num() == 8));

		// Based on our current limitations and workarounds for triangulation, polygon "islands"
		// (a polygon that is initially a subset of another polygon, and shares one vertex with the outer polygon)
		// must be passed as holes in order to not pass duplicate vertices,
		// and the island holes must be separated from the outer polygon in order to triangulate.
#define PASS_ISLANDS_AS_HOLES 1

		perimeter = {
			FVector2D(0.0f, 0.0f),
#if !PASS_ISLANDS_AS_HOLES
			FVector2D(148.749756f, 639.932617f),
			FVector2D(320.486328f, 276.400391f),
			FVector2D(0.0f, 0.0f),
#endif
			FVector2D(1158.06592f, 0.0f),
			FVector2D(1158.06592, 1337.09644f),
			FVector2D(0.0f, 1337.09644f)
		};

		holes = {
#if PASS_ISLANDS_AS_HOLES
			FPolyHole2D({
				FVector2D(320.486328f, 276.400391f),
				FVector2D(148.749756f, 639.932617f),
				FVector2D(0.0f, 0.0f)
			})
#endif
		};

		bTriangulationSuccess = UModumateGeometryStatics::TriangulateVerticesPoly2Tri(perimeter, holes, outTriangleIndices, &outVertices);
		uniqueVertices.Reset();
		uniqueVertices.Append(outVertices);
#if PASS_ISLANDS_AS_HOLES
		expectedUniqueVertices = 7;
		int32 expectedNumTriangleIndices = 21;
#else
		expectedUniqueVertices = 6;
		int32 expectedNumTriangleIndices = 15;
#endif
		TestTrue(TEXT("Square with an implicit triangular hole touching at one corner"), bTriangulationSuccess &&
			(outTriangleIndices.Num() >= expectedNumTriangleIndices) && (outVertices.Num() == 7) && (uniqueVertices.Num() == expectedUniqueVertices));

		perimeter = {
			FVector2D(0.0f, 0.0f),
			FVector2D(0.0f, 100.0f),
			FVector2D(100.0f, 100.0f),
			FVector2D(100.0f, 0.0f),
			FVector2D(50.0f, 0.0f)
		};

		holes = { FPolyHole2D({
			FVector2D(50.0f, 0.0f),
			FVector2D(75.0f, 50.0f),
			FVector2D(50.0f, 75.0f),
			FVector2D(25.0f, 50.0f)
		}) };

		bTriangulationSuccess = UModumateGeometryStatics::TriangulateVerticesPoly2Tri(perimeter, holes, outTriangleIndices, &outVertices);
		uniqueVertices.Reset();
		uniqueVertices.Append(outVertices);
#if PASS_ISLANDS_AS_HOLES
		expectedUniqueVertices = 9;
#else
		expectedUniqueVertices = 8;
#endif
		TestTrue(TEXT("Pentagon with a diamond hole touching at one point"), bTriangulationSuccess &&
			(outTriangleIndices.Num() > 6) && (outVertices.Num() == 9) && (uniqueVertices.Num() == expectedUniqueVertices));

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGeometryTriangulationErrors, "Modumate.Core.Geometry.TriangulationErrors", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::NegativeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGeometryTriangulationErrors::RunTest(const FString& Parameters)
	{
		TArray<FVector2D> OutVertices;
		TArray<int32> OutTriangles;

		TArray<FVector2D> InVertices;
		TArray<FPolyHole2D> InHoles;

		InVertices = {
			FVector2D(0.0f, 0.0f),
			FVector2D(0.0f, 100.0f),
			FVector2D(100.0f, 100.0f),
			FVector2D(100.0f, 0.0f),
			FVector2D(50.0f, 0.0f),
			FVector2D(50.0f, 50.0f),
			FVector2D(50.0f, 0.0f)
		};

		TestTrue(TEXT("square with peninsula"), !UModumateGeometryStatics::TriangulateVerticesPoly2Tri(InVertices, InHoles, OutTriangles, &OutVertices));

		InVertices = {
			FVector2D(0.0f, 0.0f),
			FVector2D(0.0f, 100.0f),
			FVector2D(100.0f, 100.0f),
			FVector2D(100.0f, 0.0f),
			FVector2D(50.0f, 0.0f),
			FVector2D(50.0f, 50.0f),
			FVector2D(50.0f, 0.0f),
			FVector2D(25.0f, 0.0f),
			FVector2D(25.0f, 50.0f),
			FVector2D(30.0f, 50.0f),
			FVector2D(25.0f, 50.0f),
			FVector2D(25.0f, 0.0f)
		};

		TestTrue(TEXT("square with multiple peninsulas"), !UModumateGeometryStatics::TriangulateVerticesPoly2Tri(InVertices, InHoles, OutTriangles, &OutVertices));

		return false;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGeometryBasisVectors, "Modumate.Core.Geometry.BasisVectors", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGeometryBasisVectors::RunTest(const FString& Parameters)
	{
		bool bSuccess = true;
		FRandomStream rand;

		for (int32 i = 0; i < 1000; ++i)
		{
			FVector basisZ = rand.VRand();
			FVector basisX, basisY;
			UModumateGeometryStatics::FindBasisVectors(basisX, basisY, basisZ);

			FVector crossZ = basisX ^ basisY;

			if (!basisZ.Equals(crossZ) || !basisY.IsNormalized() ||
				(!FMath::IsNearlyEqual(FMath::Abs(basisZ.Z), 1.0f) && (basisY.Z > 0.0f)))
			{
				bSuccess = false;
			}
		}

		bool bShortestDistanceSuccess = true;
		for (int32 i = 0; i < 1000; ++i)
		{
			FVector o1 = rand.GetUnitVector() * static_cast<float>(rand.RandRange(-500, 500));
			FVector d1 = rand.GetUnitVector();

			FVector o2 = rand.GetUnitVector() * static_cast<float>(rand.RandRange(-500, 500));
			FVector d2 = rand.GetUnitVector();

			FVector intercept1, intercept2;
			float distance;

			if (!UModumateGeometryStatics::FindShortestDistanceBetweenRays(o1, d1, o2, d2, intercept1, intercept2, distance))
			{
				bShortestDistanceSuccess = FMath::IsNearlyEqual(FMath::PointDistToLine(o1, d2, o2), distance) && bShortestDistanceSuccess;
				bShortestDistanceSuccess = FMath::IsNearlyEqual(FMath::PointDistToLine(o2, d1, o1), distance) && bShortestDistanceSuccess;
			}
			else
			{
				bShortestDistanceSuccess = FVector::Orthogonal(intercept1 - intercept2, d1) && bShortestDistanceSuccess;
				bShortestDistanceSuccess = FVector::Orthogonal(intercept1 - intercept2, d2) && bShortestDistanceSuccess;
				bShortestDistanceSuccess = FMath::IsNearlyEqual((intercept1 - intercept2).Size(), distance);
			}
		}

		return bSuccess && bShortestDistanceSuccess;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGeometryPlanarity, "Modumate.Core.Geometry.Planarity", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGeometryPlanarity::RunTest(const FString& Parameters)
	{
		static const float planarityTestEpsilon = PLANAR_DOT_EPSILON;
		FRandomStream rand;

		uint32 firstRandInt = rand.GetUnsignedInt();
		TestTrue(TEXT("Consistent first random uint"), (firstRandInt == 907633515));

		for (int32 testIndex = 0; testIndex < 1000; ++testIndex)
		{
			FVector planeN = rand.VRand();
			float planeW = rand.FRandRange(-128.0f, 128.0f);
			FPlane randPlane(planeN, planeW);
			int32 numPlanePoints = rand.RandRange(4, 8);
			TArray<FVector> planePoints;

			FVector planeBasisX, planeBasisY;
			UModumateGeometryStatics::FindBasisVectors(planeBasisX, planeBasisY, planeN);

			// Generate random points on the plane, that form a circle
			for (int32 i = 0; i < numPlanePoints; ++i)
			{
				float radius = 100.0f;
				float angle = (i * 2 * PI) / numPlanePoints;
				FVector randPoint =
					(radius * FMath::Cos(angle) * planeBasisX) +
					(radius * FMath::Sin(angle) * planeBasisY);

				randPoint = FVector::PointPlaneProject(randPoint, randPlane);
				float randPlaneDot = randPlane.PlaneDot(randPoint);
				bool bPointOnPlane = FMath::IsNearlyZero(randPlaneDot, planarityTestEpsilon);

				TestTrue(TEXT("Random point is on plane"), bPointOnPlane);

				if (!bPointOnPlane)
				{
					return false;
				}

				planePoints.Add(randPoint);
			}

			// Randomly shuffle the plane points
			for (int32 i = 0; i < numPlanePoints; ++i)
			{
				planePoints.Swap(i, rand.RandRange(0, numPlanePoints - 1));
			}

			// Make sure we can derive a plane using our own method
			FPlane calculatedPlane;
			bool bPointsPlanar = UModumateGeometryStatics::GetPlaneFromPoints(planePoints, calculatedPlane, planarityTestEpsilon);
			TestTrue(TEXT("GetPlaneFromPoints found plane from generated planar points"), bPointsPlanar);

			// Make sure that the calculated plane's normal agrees with the actual normal
			bool bCalculatedPlaneParallel = FVector::Parallel(planeN, FVector(calculatedPlane));
			TestTrue(TEXT("GetPlaneFromPoints's normal agrees with generated plane normal"), bCalculatedPlaneParallel);

			// Now, serialize and deserialize the plane points, to see if any significant error is introduced
			TArray<FVector> deserializedPoints;
			for (const FVector &planePoint : planePoints)
			{
				FModumateCommandParameter serializedPoint(planePoint);
				deserializedPoints.Add(serializedPoint);
			}

			// Make sure we can derive a plane using the deserialized data
			bPointsPlanar = UModumateGeometryStatics::GetPlaneFromPoints(deserializedPoints, calculatedPlane, planarityTestEpsilon);
			TestTrue(TEXT("GetPlaneFromPoints found plane from deserialized planar points"), bPointsPlanar);

			// Make sure that the deserialized plane's normal agrees with the actual normal
			bCalculatedPlaneParallel = FVector::Parallel(planeN, FVector(calculatedPlane));
			TestTrue(TEXT("GetPlaneFromPoints's normal agrees with deserialized plane normal"), bCalculatedPlaneParallel);
		}

		uint32 finalRandInt = rand.GetUnsignedInt();
		TestTrue(TEXT("Consistent final random uint"), (finalRandInt == 597090039));

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateGeometryPlaneIntersects, "Modumate.Core.Geometry.PlaneIntersects", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateGeometryPlaneIntersects::RunTest(const FString& Parameters)
	{
		FPlane plane(FVector::ZeroVector, FVector(0.0f, 0.0f, 1.0f));
		FVector tangent = FVector(1.0f, 0.0f, 0.0f);

		FVector intersection;
		TestTrue(TEXT("Segment on plane should not have intersection"), !FMath::SegmentPlaneIntersection(FVector::ZeroVector, tangent, plane, intersection));

		// would look like a block-lettered S
		TArray<FVector> S = {
			FVector(1.0f, 0.0f, 1.0f),
			FVector(3.0f, 0.0f, 1.0f),
			FVector(3.0f, 0.0f, 4.0f),
			FVector(2.0f, 0.0f, 4.0f),
			FVector(2.0f, 0.0f, 5.0f),
			FVector(3.0f, 0.0f, 5.0f),
			FVector(3.0f, 0.0f, 6.0f),
			FVector(1.0f, 0.0f, 6.0f),
			FVector(1.0f, 0.0f, 3.0f),
			FVector(2.0f, 0.0f, 3.0f),
			FVector(2.0f, 0.0f, 2.0f),
			FVector(1.0f, 0.0f, 2.0f)
		};

		FVector UnitXZ = FVector(1.0f, 0.0f, 1.0f).GetSafeNormal();

		TArray<FPlane> testPlanes = {
			FPlane(FVector::UpVector, FVector::UpVector),
			FPlane(2 * FVector::UpVector, FVector::UpVector),
			FPlane(2.5*FVector::UpVector, FVector::UpVector),
			FPlane(3 * FVector::UpVector, FVector::UpVector),
			FPlane(4 * FVector::UpVector, FVector::UpVector),
			FPlane(6 * FVector::UpVector, FVector::UpVector),
			FPlane(7 * FVector::UpVector, FVector::UpVector),
			FPlane(FVector::ForwardVector, FVector::ForwardVector),
			FPlane(2 * FVector::ForwardVector, FVector::ForwardVector),
			FPlane(2.5 * FVector::ForwardVector, FVector::ForwardVector),
			FPlane(3 * FVector::ForwardVector, FVector::ForwardVector),
			FPlane(4 * FVector::ForwardVector, FVector::ForwardVector),
			FPlane(UnitXZ, UnitXZ),
			FPlane(2 * UnitXZ, UnitXZ),
			FPlane(3 * UnitXZ, UnitXZ),
			FPlane(4 * UnitXZ, UnitXZ),
			FPlane(5 * UnitXZ, UnitXZ)
		};

		TArray<int32> expectedResults = { 2, 3, 2, 3, 3, 2, 0, 4, 6, 4, 4, 0, 0, 2, 4, 2, 2 };

		for (int32 idx = 0; idx < testPlanes.Num(); idx++)
		{
			auto& testPlane = testPlanes[idx];

			int32 intersections = 0;

			// loop through edges to calculate the amount of times there is an intersection
			for (int32 polyIdx = 0; polyIdx < S.Num(); polyIdx++)
			{
				FVector segmentStart = S[polyIdx];
				FVector segmentEnd = S[(polyIdx+1) % S.Num()];

				FVector outPoint;
				if (FMath::SegmentPlaneIntersection(segmentStart, segmentEnd, testPlane, outPoint))
				{
					intersections++;
				}
			}

			TestTrue(TEXT("Even intersections"), intersections == expectedResults[idx]);
		}

		return true;
	}

	// Modumate command parameter types test
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateCommandParameterTest, "Modumate.Core.Command.ParameterTest", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateCommandParameterTest::RunTest(const FString& Parameters)
	{
		bool bSuccess = true;

		FModumateCommandParameter floatVal("floatVal", 1.0f);
		FModumateCommandParameter intVal("intVal", 2);

		float fv = floatVal.AsFloat();
		bSuccess = bSuccess && ensureAlways(fv == 1.0f);

		int32 iv = intVal.AsInt();
		bSuccess = bSuccess && ensureAlways(iv == 2);

		fv = intVal.AsFloat();
		bSuccess = bSuccess && ensureAlways(fv == 2.0f);

		iv = floatVal.AsInt();
		bSuccess = bSuccess && ensureAlways(iv == 1.0f);

		FModumateCommand testCom = FModumateCommand(TEXT("testCommand"))
			.Param(TEXT("intVal"), 3)
			.Param(TEXT("floatVal"), 4.0f)
			.Param(TEXT("boolFalse"), false)
			.Param(TEXT("boolTrue"), true);

		FModumateFunctionParameterSet testComSet = testCom.GetParameterSet();

		iv = FModumateCommandParameter(testComSet.GetValue(TEXT("intVal")));
		bSuccess = bSuccess && ensureAlways(iv == 3);

		fv = FModumateCommandParameter(testComSet.GetValue(TEXT("floatVal")));
		bSuccess = bSuccess && ensureAlways(fv == 4.0f);

		fv = FModumateCommandParameter(testComSet.GetValue(TEXT("intVal")));
		bSuccess = bSuccess && ensureAlways(fv == 3.0f);

		iv = FModumateCommandParameter(testComSet.GetValue(TEXT("floatVal")));
		bSuccess = bSuccess && ensureAlways(iv == 4);

		iv = testComSet.GetValue(TEXT("intVal"), 6);
		bSuccess = bSuccess && ensureAlways(iv == 3);

		iv = testComSet.GetValue(TEXT("missingVal"), 6);
		bSuccess = bSuccess && ensureAlways(iv == 6);

		fv = testComSet.GetValue(TEXT("intVal"), 6);
		bSuccess = bSuccess && 	ensureAlways(fv == 3.0f);

		fv = testComSet.GetValue(TEXT("missingVal"), 6);
		bSuccess = bSuccess && ensureAlways(fv == 6.0f);

		bool bv = testComSet.GetValue(TEXT("boolTrue"));
		bSuccess = bSuccess && ensureAlways(bv);

		bv = testComSet.GetValue(TEXT("boolFalse"));
		bSuccess = bSuccess && ensureAlways(!bv);

		return bSuccess;
	}

	// Modumate expressions test
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateExpressionUnitTest, "Modumate.Core.Expression.UnitTest", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
	bool FModumateExpressionUnitTest::RunTest(const FString& Parameters)
	{
		UE_LOG(LogUnitTest, Display, TEXT("Modumate Evaluator - Unit Test Started"));

		TMap<FString, float> vars1;
		vars1.Add("A", 1.0f);
		vars1.Add("ZED", 2.0f);
		vars1.Add("AAB", 4.5f);
		vars1.Add("W2", 6.0f);

		auto tryCase = [vars1](const TCHAR *expression, float expectedValue, bool bExpectSuccess = true) -> bool
		{
			float value = 0.0f;
			bool bEvaluationSuccess = Expression::Evaluate(vars1, expression, value);

			if (bExpectSuccess)
			{
				if (FMath::IsNearlyEqual(value, expectedValue, KINDA_SMALL_NUMBER))
				{
					UE_LOG(LogUnitTest, Display, TEXT("Formula: %s, expected: %f, got: %f - SUCCESS"), expression, expectedValue, value);
				}
				else
				{
					UE_LOG(LogUnitTest, Error, TEXT("Formula: %s, expected: %f, got: %f - FAILURE"), expression, expectedValue, value);
				}
			}
			else
			{
				if (bEvaluationSuccess)
				{
					UE_LOG(LogUnitTest, Error, TEXT("Formula: %s, expected error, got: %f - FAILURE"), expression, value);
				}
				else
				{
					UE_LOG(LogUnitTest, Display, TEXT("Formula: %s, expected error, got error - SUCCESS"), expression);
				}
			}

			return bEvaluationSuccess == bExpectSuccess;
		};

		tryCase(TEXT("(1 + (2 + 3))"), 6);
		tryCase(TEXT("(4.5 * 2)/(1 + 2)"), 3.0f);
		tryCase(TEXT("((2AAB))/(A + ZED)"), 3.0f);
		tryCase(TEXT("((ZED + 2/3))"), 2.6666f);
		tryCase(TEXT("((ZED + 2/3) * (AAB - (A+ZED)))"), 4.0f);
		tryCase(TEXT("3A"), 3.0f);
		tryCase(TEXT("2W2"), 12.0f);
		tryCase(TEXT("(1+W2)ZED"), 14.0f);

		UE_LOG(LogUnitTest, Display, TEXT("Modumate Evaluator - Unit Test Ended"));

		return true;
	}

	// expression expected failures test
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateExpressionUnitTestFails, "Modumate.Core.Expression.UnitTestFails", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::NegativeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateExpressionUnitTestFails::RunTest(const FString& Parameters)
	{
		TMap<FString, float> vars1;
		vars1.Add("A", 1.0f);
		vars1.Add("ZED", 2.0f);
		vars1.Add("AAB", 4.5f);
		vars1.Add("W2", 6.0f);

		float value = 0.0f;
		TestTrue(TEXT("Expected expression failure 1"), Expression::Evaluate(vars1, TEXT("W2W2"), value));

		TestTrue(TEXT("Expected expression failure 2"), Expression::Evaluate(vars1, TEXT("W2ZED"), value));

		return true;
	}

	// Modumate dimension format test
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateDimensionFormatUnitTest, "Modumate.Core.DimensionFormat.UnitTest", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateDimensionFormatUnitTest::RunTest(const FString& Parameters)
	{
		struct TestCase
		{
			FString dimStr;
			EDimensionFormat expectedFormat;
			float expectedCentimeters;

			TestCase(const TCHAR *ds, EDimensionFormat ef, float ec) : dimStr(ds), expectedFormat(ef), expectedCentimeters(ec) {};
		};

		static TArray<TestCase> testCases = { 
			{TEXT("1"),EDimensionFormat::JustInches,InchesToCentimeters * 1},
			{TEXT("33"),EDimensionFormat::JustInches,InchesToCentimeters * 33},
			{TEXT("1.2"),EDimensionFormat::JustCentimeters,1.2f},
			{TEXT("0.1"),EDimensionFormat::JustCentimeters,0.1f},
			{TEXT("4'"),EDimensionFormat::JustFeet,InchesToCentimeters * 4 * 12},
			{TEXT("15ft"),EDimensionFormat::JustFeet,InchesToCentimeters * 15 * 12},
			{TEXT("3\""),EDimensionFormat::JustInches,InchesToCentimeters * 3},
			{TEXT(".5\""),EDimensionFormat::JustInches,InchesToCentimeters * 0.5f},
			{TEXT("3.2\""),EDimensionFormat::JustInches,InchesToCentimeters * 3.2f},
			{TEXT("7in"),EDimensionFormat::JustInches,InchesToCentimeters * 7},
			{TEXT("3 7/8\""),EDimensionFormat::JustInches,InchesToCentimeters * (3.0f + (7.0f/8.0f))},
			{TEXT("7 3/4in"),EDimensionFormat::JustInches,InchesToCentimeters * (7.0f + (3.0f/4.0f))},
			{TEXT("1' 5\""),EDimensionFormat::FeetAndInches,InchesToCentimeters * (12+5)},
			{TEXT("5ft 8in"),EDimensionFormat::FeetAndInches,InchesToCentimeters * (12 * 5 + 8)},
			{TEXT("4' 3/8\""),EDimensionFormat::FeetAndInches,InchesToCentimeters * (12.0f * 4.0f + (3.0f/8.0f))},
			{TEXT("11ft 5 1/2in"),EDimensionFormat::FeetAndInches,InchesToCentimeters * (12.0f * 11.0f + 5.0f + (1.0f / 2.0f))},
			{TEXT("8' 2 5/8\""),EDimensionFormat::FeetAndInches,InchesToCentimeters * (12.0f * 8.0f + 2.0f + (5.0f / 8.0f))},
			{TEXT("6m"),EDimensionFormat::JustMeters,600.0f},
			{TEXT("1.6m"),EDimensionFormat::JustMeters,160.0f},
			{TEXT("8cm"),EDimensionFormat::JustCentimeters,8.0f},
			{TEXT("9.6cm"),EDimensionFormat::JustCentimeters,9.6f},
			{TEXT("6m 3cm"),EDimensionFormat::MetersAndCentimeters,603.0f},
			{TEXT("1m 4.5cm"),EDimensionFormat::MetersAndCentimeters,104.5f},
			{TEXT("zed"),EDimensionFormat::Error,0.0f},
			{TEXT("2.3maynard"),EDimensionFormat::Error,0.0f},
			{TEXT("11ft 2cm"),EDimensionFormat::Error,0.0f}
		};

		bool ret = true;
		for (auto &tc : testCases)
		{
			FModumateFormattedDimension dim = UModumateDimensionStatics::StringToFormattedDimension(tc.dimStr);
			ret = (dim.Format == tc.expectedFormat && FMath::IsNearlyEqual(dim.Centimeters, tc.expectedCentimeters)) && ret;
		}
		return ret;
	}

	// Modumate expressions test
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateNumericalSequenceUnitTest, "Modumate.Core.NumericalSequence.UnitTest", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateNumericalSequenceUnitTest::RunTest(const FString& Parameters)
	{
		FString testSeqStr;

		testSeqStr = UModumateFunctionLibrary::GetNextStringInNumericalSequence(testSeqStr, TCHAR('a'), TCHAR('z'));
		bool ret = ensureAlways(testSeqStr == TEXT("a"));

		int32 j;
		for (j = 0; j < 25; ++j)
		{
			testSeqStr = UModumateFunctionLibrary::GetNextStringInNumericalSequence(testSeqStr, TCHAR('a'), TCHAR('z'));
		}
		ret = ensureAlways(testSeqStr == TEXT("z")) && ret;

		return ret;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateIDListNormalization, "Modumate.Core.IDListNormalization", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateIDListNormalization::RunTest(const FString& Parameters)
	{
		TArray<int32> inListA = { 3, 5, 8, 2, 7 };
		TArray<int32> outListA = { 2, 7, 3, 5, 8 };
		TestTrue(TEXT("ListA can be normalized"), UModumateFunctionLibrary::NormalizeIDs(inListA));
		TestEqual(TEXT("ListA normalized correctly"), inListA, outListA);

		TArray<int32> inListB = { 3 };
		TArray<int32> outListB = { 3 };
		TestTrue(TEXT("ListB can be normalized"), UModumateFunctionLibrary::NormalizeIDs(inListB));
		TestEqual(TEXT("ListB normalized correctly"), inListB, outListB);

		TArray<int32> inListC = { 5, 2, 3, 2 };
		TArray<int32> outListC = { 5, 2, 3, 2 };
		TestTrue(TEXT("ListC can not be normalized"), !UModumateFunctionLibrary::NormalizeIDs(inListC));
		TestEqual(TEXT("ListC didn't get normalized"), inListC, outListC);

		return true;
	}
}
