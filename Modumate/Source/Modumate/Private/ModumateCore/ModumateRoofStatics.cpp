// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "ModumateCore/ModumateRoofStatics.h"

#include "DocumentManagement/ModumateDocument.h"
#include "Graph/Graph3D.h"
#include "Objects/ModumateObjectInstance.h"
#include "Objects/RoofPerimeter.h"
#include "ModumateCore/ModumateGeometryStatics.h"

#define DEBUG_ROOFS UE_BUILD_DEBUG




FTessellationPolygon::FTessellationPolygon()
	: BaseUp(ForceInitToZero)
	, PolyNormal(ForceInitToZero)
	, StartPoint(ForceInitToZero)
	, StartDir(ForceInitToZero)
	, EndPoint(ForceInitToZero)
	, EndDir(ForceInitToZero)
	, CachedEdgeDir(ForceInitToZero)
	, CachedEdgeIntersection(ForceInitToZero)
	, CachedPlane(ForceInitToZero)
	, CachedStartRayDist(0.0f)
	, CachedEndRayDist(0.0f)
	, bCachedEndsConverge(false)
	, StartExtensionDist(0.0f)
	, EndExtensionDist(0.0f)
{
}

FTessellationPolygon::FTessellationPolygon(const FVector &InBaseUp, const FVector &InPolyNormal,
	const FVector &InStartPoint, const FVector &InStartDir,
	const FVector &InEndPoint, const FVector &InEndDir, float InStartExtensionDist, float InEndExtensionDist)
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
	, StartExtensionDist(InStartExtensionDist)
	, EndExtensionDist(InEndExtensionDist)
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

		// Make sure that the lines aren't clipped below (or at) the height of the edge
		float edgeHeightFudged = (StartPoint | BaseUp) + RAY_INTERSECT_TOLERANCE;
		if (((clipStartPoint | BaseUp) < edgeHeightFudged) || ((clipEndPoint | BaseUp) < edgeHeightFudged))
		{
			return false;
		}

		if (bCachedEndsConverge)
		{
			// If this polygon starts as a triangle, make sure the clip points aren't equal to the existing edge converging intersection
			if (clipStartPoint.Equals(CachedEdgeIntersection, RAY_INTERSECT_TOLERANCE) &&
				clipEndPoint.Equals(CachedEdgeIntersection, RAY_INTERSECT_TOLERANCE))
			{
				return false;
			}

			// If this polygon starts as a triangle, make sure the clip points are inside the triangle.
			static float constexpr BARY_EPSILON = 0.05f;
			FVector clipStartBary = FMath::ComputeBaryCentric2D(clipStartPoint, StartPoint, EndPoint, CachedEdgeIntersection);
			FVector clipEndBary = FMath::ComputeBaryCentric2D(clipEndPoint, StartPoint, EndPoint, CachedEdgeIntersection);
			if ((clipStartBary.GetMin() < -BARY_EPSILON) || clipEndBary.GetMin() < -BARY_EPSILON)
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
		if (bCachedEndsConverge && (StartExtensionDist == 0.0f) && (EndExtensionDist == 0.0f))
		{
			PolygonVerts.Add(StartPoint);
			PolygonVerts.Add(EndPoint);
			PolygonVerts.Add(CachedEdgeIntersection);
		}
		else
		{
			PolygonVerts.Add(StartPoint);
			PolygonVerts.Add(EndPoint);

			PolygonVerts.Add(EndPoint + EndExtensionDist * EndDir);
			PolygonVerts.Add(StartPoint + StartExtensionDist * StartDir);
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

		auto addNextPolyVert = [this](const FVector& NewPolyVert) -> bool
		{
			if ((PolygonVerts.Num() > 0) && PolygonVerts.Last().Equals(NewPolyVert, RAY_INTERSECT_TOLERANCE))
			{
				return false;
			}

			PolygonVerts.Add(NewPolyVert);
			return true;
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
						addNextPolyVert(clipEndPoint);
						bConsumed = true;
						bReachedStartRay = testClipPointOnRay(StartPoint, StartDir, clipEndPoint, curLengthOnEndRay);
					}
					else if (clipEndPoint.Equals(lastPolyVert, RAY_INTERSECT_TOLERANCE))
					{
						addNextPolyVert(clipStartPoint);
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

				addNextPolyVert(clipPointA);
				addNextPolyVert(clipPointB);

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

	return IsValid();
}

bool FTessellationPolygon::IsValid() const
{
	return (PolygonVerts.Num() >= 3);
}

void FTessellationPolygon::DrawDebug(const FColor &Color, UWorld* World, const FTransform &Transform)
{
	if (World == nullptr)
	{
		World = GWorld;
	}

	static bool bPersistent = false;
	static float lifetime = 4.0f;
	static uint8 depth = 0xFF;
	static float edgeThickness = 2.0f;
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

FRoofEdgeProperties::FRoofEdgeProperties()
	: bOverridden(false)
	, Slope(0.5f)
	, bHasFace(true)
	, Overhang(0.0f)
{
}

FRoofEdgeProperties::FRoofEdgeProperties(bool bInOverridden, float InSlope, bool bInHasFace, float InOverhang)
	: bOverridden(bInOverridden)
	, Slope(InSlope)
	, bHasFace(bInHasFace)
	, Overhang(InOverhang)
{
}

bool UModumateRoofStatics::GetAllProperties(
	const AModumateObjectInstance *RoofObject,
	TArray<FVector> &OutEdgePoints, TArray<FGraphSignedID> &OutEdgeIDs,
	FRoofEdgeProperties &OutDefaultProperties, TArray<FRoofEdgeProperties> &OutEdgeProperties)
{
	OutEdgePoints.Reset();
	OutEdgeIDs.Reset();
	OutDefaultProperties = FRoofEdgeProperties();
	OutEdgeProperties.Reset();

	FMOIRoofPerimeterData roofPerimeterData;
	if (!ensure(RoofObject && (RoofObject->GetObjectType() == EObjectType::OTRoofPerimeter) &&
		RoofObject->GetStateData().CustomData.LoadStructData(roofPerimeterData)))
	{
		return false;
	}

	// TODO: remove this ASAP - it's only here because the ordered edge ID list is derived, non-definitional implementation-specific data
	// that belongs in neither the top-level StateData (i.e. via interpreting ControlPoints/Indices or BIMProperties)
	// nor the serialized CustomData that should only be modified via deltas. Solutions:
	// - When MOIs are UObjects that can be safely and performantly down-casted, this implementation-specific derived data can be safely exposed publicly
	// - Alternatively, the ordered edge ID list can be stored in CustomData, but needs to be modified during delta application via side effect deltas
	const UModumateDocument* doc = RoofObject->GetDocument();
	const AMOIRoofPerimeter* roofPerimeterImpl = (const AMOIRoofPerimeter*)RoofObject;
	OutEdgeIDs = roofPerimeterImpl->GetCachedEdgeIDs();
	int32 numEdges = OutEdgeIDs.Num();

	for (FGraphSignedID signedEdgeID : OutEdgeIDs)
	{
		int32 edgeID = FMath::Abs(signedEdgeID);
		const AModumateObjectInstance* edgeMOI = doc->GetObjectById(edgeID);
		FVector edgeStart = edgeMOI->GetCorner(signedEdgeID > 0 ? 0 : 1);
		OutEdgePoints.Add(edgeStart);

		OutEdgeProperties.Add(roofPerimeterData.EdgeProperties.FindRef(edgeID));
	}

	return true;
}

bool UModumateRoofStatics::TessellateSlopedEdges(const TArray<FVector> &EdgePoints, const TArray<FRoofEdgeProperties> &EdgeProperties,
	TArray<FVector> &OutCombinedPolyVerts, TArray<int32> &OutPolyVertIndices, const FVector &NormalHint, UWorld *DebugDrawWorld)
{
	OutCombinedPolyVerts.Reset();
	OutPolyVertIndices.Reset();

	int32 numEdges = EdgePoints.Num();
	bool bFlip = false;

	if (!ensureAlways((numEdges >= 3) && (numEdges == EdgeProperties.Num())))
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
		float edgeSlope = EdgeProperties[i].Slope;
		static const float maxSlope = 10000.0f;
		if (EdgeProperties[i].bHasFace && !ensureAlwaysMsgf((edgeSlope > 0.0f) && (edgeSlope <= maxSlope),
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

		float edgeSlope = EdgeProperties[edgeIdx].Slope;
		FVector edgeTangent, edgeNormal;

		if (EdgeProperties[edgeIdx].bHasFace)
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

		FVector ridgeDir(ForceInitToZero);

		if (FVector::Parallel(edgeNormal, prevEdgeNormal))
		{
			ridgeDir = edgeTangents[edgeIdx];
		}
		else
		{
			ridgeDir = (prevEdgeNormal ^ edgeNormal).GetSafeNormal();
			if ((ridgeDir | baseNormal) < 0.0f)
			{
				ridgeDir *= -1.0f;
			}
		}

		if (ridgeDir.IsNearlyZero())
		{
			return false;
		}

		ridgeDirs.Add(ridgeDir);
	}

	// Create tessellation polygons for each edge of the base polygon
	TArray<FTessellationPolygon> clippingPolygons, combinedPolygons;
	for (int32 edgeIdx = 0; edgeIdx < numEdges; ++edgeIdx)
	{
		int32 edgeStartIdx = edgeIdx;
		int32 edgeEndIdx = ((edgeIdx + 1) % numEdges);

		const FVector &edgeStartPoint = EdgePoints[edgeStartIdx];
		const FVector &edgeEndPoint = EdgePoints[edgeEndIdx];

		const FVector &edgeStartRidgeDir = ridgeDirs[edgeStartIdx];
		const FVector &edgeEndRidgeDir = ridgeDirs[edgeEndIdx];

		const FVector &edgeNormal = edgeNormals[edgeIdx];

		clippingPolygons.Add(FTessellationPolygon(baseNormal, edgeNormal, edgeStartPoint, edgeStartRidgeDir, edgeEndPoint, edgeEndRidgeDir, 0.0f, 0.0f));

		float edgeOverhang = EdgeProperties[edgeIdx].Overhang;
		if (edgeOverhang > 0.0f)
		{
			const FVector &edgeInsideDir = edgeInsideDirs[edgeIdx];
			if (!FVector::Parallel(edgeStartRidgeDir, edgeInsideDir) && !FVector::Parallel(edgeEndRidgeDir, edgeInsideDir))
			{
				float startExtensionDist = edgeOverhang / (edgeStartRidgeDir | edgeInsideDir);
				float endExtensionDist = edgeOverhang / (edgeEndRidgeDir | edgeInsideDir);

				combinedPolygons.Add(FTessellationPolygon(baseNormal, edgeNormal, edgeStartPoint, -edgeStartRidgeDir, edgeEndPoint, -edgeEndRidgeDir, startExtensionDist, endExtensionDist));
			}
		}
	}

	// Clip edge polygons by non-adjacent polygons
	int32 debugNumClips = 0;
	for (int32 edgeIdx = 0; edgeIdx < numEdges; ++edgeIdx)
	{
		int32 prevEdgeIdx = (edgeIdx + numEdges - 1) % numEdges;
		int32 nextEdgeIdx = (edgeIdx + 1) % numEdges;
		FTessellationPolygon &edgePoly = clippingPolygons[edgeIdx];

		for (int32 otherEdgeIdx = 0; otherEdgeIdx < numEdges; ++otherEdgeIdx)
		{
			if ((otherEdgeIdx != edgeIdx) && (otherEdgeIdx != prevEdgeIdx) && (otherEdgeIdx != nextEdgeIdx))
			{
				const FTessellationPolygon &otherEdgePoly = clippingPolygons[otherEdgeIdx];
				bool bClip = edgePoly.ClipWithPolygon(otherEdgePoly);

				if (bClip)
				{
					++debugNumClips;
				}
			}
		}

		//Draw debug lines to diagnose tessellation issues
#if DEBUG_ROOFS
		float debugHue = FMath::Frac(edgeIdx * UE_GOLDEN_RATIO);
		FLinearColor debugColor = FLinearColor::MakeFromHSV8(debugHue * 0xFF, 0xFF, 0xFF);
		edgePoly.DrawDebug(debugColor.ToFColor(false), DebugDrawWorld);
#endif

		if (!ensureAlways(edgePoly.bCachedEndsConverge || (edgePoly.ClippingPoints.Num() > 0)))
		{
			UE_LOG(LogTemp, Warning, TEXT("Edge #%d's clipping face doesn't converge and wasn't clipped!"));
#if !DEBUG_ROOFS
			continue;
#endif
		}

		bool bPolyValid = edgePoly.UpdatePolygonVerts();

		if (EdgeProperties[edgeIdx].bHasFace && bPolyValid)
		{
			combinedPolygons.Add(edgePoly);
		}
	}

	// Collect the vertices for each edge tessellation polygon
	for (auto &edgePoly : combinedPolygons)
	{
		if (edgePoly.IsValid())
		{
			OutCombinedPolyVerts.Append(edgePoly.PolygonVerts);
			OutPolyVertIndices.Add(OutCombinedPolyVerts.Num() - 1);
		}
	}

	return true;
}
