// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "ModumateCore/ModumateTargetingStatics.h"

#include "DocumentManagement/ModumateDocument.h"
#include "Graph/Graph3D.h"
#include "Objects/ModumateObjectInstance.h"



int32 UModumateTargetingStatics::GetFaceIndexFromTargetHit(const AModumateObjectInstance* HitObject, const FVector& HitLocation, const FVector& HitNormal)
{
	if (HitObject == nullptr)
	{
		return INDEX_NONE;
	}

	const static float edgeTolerance = 0.01f;

	switch (HitObject->GetObjectType())
	{
	case EObjectType::OTRoofFace:
	case EObjectType::OTWallSegment:
	case EObjectType::OTFloorSegment:
	case EObjectType::OTCeiling:
	case EObjectType::OTRailSegment:
	case EObjectType::OTSystemPanel:
	{
		const AModumateObjectInstance* hostParent = HitObject->GetParentObject();
		if (!ensure(hostParent && (hostParent->GetObjectType() == EObjectType::OTMetaPlaneSpan)))
		{
			return INDEX_NONE;
		}

		FVector planeUp = hostParent->GetNormal();
		float planeHitNormalDot = planeUp | HitNormal;
		if (planeHitNormalDot >= THRESH_NORMALS_ARE_PARALLEL)
		{
			return 0;
		}
		else if (planeHitNormalDot <= -THRESH_NORMALS_ARE_PARALLEL)
		{
			return 1;
		}
		else
		{
			TArray<FVector> sidePoints;
			TArray<FVector2D> sidePoints2D;
			int32 numCorners = hostParent->GetNumCorners();
			for (int32 curBottomIdx = 0; curBottomIdx < numCorners; ++curBottomIdx)
			{
				int32 nextBottomIdx = (curBottomIdx + 1) % numCorners;
				int32 curTopIdx = curBottomIdx + numCorners;
				int32 nextTopIdx = nextBottomIdx + numCorners;

				FVector curBottomCorner = HitObject->GetCorner(curBottomIdx);
				FVector nextBottomCorner = HitObject->GetCorner(nextBottomIdx);
				FVector curTopCorner = HitObject->GetCorner(curTopIdx);
				FVector nextTopCorner = HitObject->GetCorner(nextTopIdx);

				sidePoints = { curBottomCorner, nextBottomCorner, nextTopCorner, curTopCorner };
				FPlane sidePlane(ForceInitToZero);
				if (!ensure(UModumateGeometryStatics::GetPlaneFromPoints(sidePoints, sidePlane)) ||
					!FVector::Parallel(HitNormal, sidePlane) ||
					!FMath::IsNearlyZero(sidePlane.PlaneDot(HitLocation), PLANAR_DOT_EPSILON))
				{
					continue;
				}

				FVector sidePlaneBasisX, sidePlaneBasisY;
				UModumateGeometryStatics::FindBasisVectors(sidePlaneBasisX, sidePlaneBasisY, sidePlane);
				FVector2D projectedHitPoint = UModumateGeometryStatics::ProjectPoint2D(HitLocation, sidePlaneBasisX, sidePlaneBasisY, curBottomCorner);

				sidePoints2D.Reset(sidePoints.Num());
				for (const FVector& sidePoint : sidePoints)
				{
					sidePoints2D.Add(UModumateGeometryStatics::ProjectPoint2D(sidePoint, sidePlaneBasisX, sidePlaneBasisY, curBottomCorner));
				}

				FPointInPolyResult pointInPolyResult;
				if (UModumateGeometryStatics::TestPointInPolygon(projectedHitPoint, sidePoints2D, pointInPolyResult) &&
					(pointInPolyResult.bInside || pointInPolyResult.bOverlaps))
				{
					return 2 + curBottomIdx;
				}
			}

			return INDEX_NONE;
		}
	}
	break;
	default:
		return INDEX_NONE;
	}
}

void UModumateTargetingStatics::GetConnectedSurfaceGraphs(const AModumateObjectInstance* HitObject, const FVector& HitLocation, TArray<const AModumateObjectInstance*>& OutSurfaceGraphs)
{
	OutSurfaceGraphs.Reset();

	const UModumateDocument* doc = HitObject ? HitObject->GetDocument() : nullptr;
	if (doc == nullptr)
	{
		return;
	}

	const FGraph3D& volumeGraph = *doc->GetVolumeGraph();
	TArray<const AModumateObjectInstance*, TInlineAllocator<8>> connectedPlanes;

	auto getEdgeFaces = [&connectedPlanes, doc, &volumeGraph](int32 EdgeID)
	{
		if (auto edge = volumeGraph.FindEdge(EdgeID))
		{
			for (auto& faceConnection : edge->ConnectedFaces)
			{
				if (auto connectedPlaneMOI = doc->GetObjectById(FMath::Abs(faceConnection.FaceID)))
				{
					connectedPlanes.AddUnique(connectedPlaneMOI);
				}
			}
		}
	};

	// Based on the supported object types, gather the 3D faces that could be adjacent to the hit object
	switch (HitObject->GetObjectType())
	{
	case EObjectType::OTMetaVertex:
		if (auto vertex = volumeGraph.FindVertex(HitObject->ID))
		{
			TSet<int32> connectedFaceIDs;
			vertex->GetConnectedFaces(connectedFaceIDs);
			for (int32 connectedFaceID : connectedFaceIDs)
			{
				if (auto connectedPlaneMOI = doc->GetObjectById(connectedFaceID))
				{
					connectedPlanes.AddUnique(connectedPlaneMOI);
				}
			}
		}
		break;
	case EObjectType::OTMetaEdge:
		getEdgeFaces(HitObject->ID);
		break;
	default:
	{
		const AModumateObjectInstance* planeMOI = HitObject;
		while (planeMOI && (planeMOI->GetObjectType() != EObjectType::OTMetaPlane))
		{
			planeMOI = planeMOI->GetParentObject();
		}

		if (auto face = volumeGraph.FindFace(planeMOI ? planeMOI->ID : MOD_ID_NONE))
		{
			for (FGraphSignedID faceEdge : face->EdgeIDs)
			{
				getEdgeFaces(faceEdge);
			}
		}
	}
	break;
	}

	// Based on current surface graph rules (must be a grandchild of a metaplane),
	// find which ones for which the hit point lies on the perimeter.
	for (auto connectedPlane : connectedPlanes)
	{
		for (auto planeChild : connectedPlane->GetChildObjects())
		{
			for (auto planeGrandChild : planeChild->GetChildObjects())
			{
				if (planeGrandChild->GetObjectType() == EObjectType::OTSurfaceGraph)
				{
					int32 numCorners = planeGrandChild->GetNumCorners();
					for (int32 surfacePointIdxA = 0; surfacePointIdxA < numCorners; ++surfacePointIdxA)
					{
						int32 surfacePointIdxB = (surfacePointIdxA + 1) % numCorners;
						FVector surfacePointA = planeGrandChild->GetCorner(surfacePointIdxA);
						FVector surfacePointB = planeGrandChild->GetCorner(surfacePointIdxB);

						FVector closestPoint = FMath::ClosestPointOnSegment(HitLocation, surfacePointA, surfacePointB);
						if (closestPoint.Equals(HitLocation, RAY_INTERSECT_TOLERANCE))
						{
							OutSurfaceGraphs.AddUnique(planeGrandChild);
							break;
						}
					}
				}
			}
		}
	}
}
