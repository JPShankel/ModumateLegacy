// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "ModumateCore/ModumateObjectStatics.h"

#include "BIMKernel/BIMProperties.h"
#include "Database/ModumateSimpleMesh.h"
#include "DocumentManagement/ModumateSnappingView.h"
#include "Graph/Graph3D.h"
#include "ModumateCore/ExpressionEvaluator.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "ModumateCore/ModumateGeometryStatics.h"
#include "Objects/ModumateObjectInstance.h"
#include "Objects/PlaneHostedObj.h"
#include "Objects/SurfaceGraph.h"
#include "UnrealClasses/CompoundMeshActor.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"

using namespace Modumate;

bool UModumateObjectStatics::GetPolygonProfile(const FBIMAssemblySpec *TrimAssembly, const FSimplePolygon*& OutProfile)
{
	if (!ensure(TrimAssembly && (TrimAssembly->Extrusions.Num() == 1)))
	{
		return false;
	}

	const FBIMExtrusionSpec& trimExtrusion = TrimAssembly->Extrusions[0];
	if (!ensure(trimExtrusion.SimpleMeshes.Num() == 1))
	{
		return false;
	}

	const FSimpleMeshRef &simpleMesh = trimExtrusion.SimpleMeshes[0];
	if (!ensure(simpleMesh.Asset.IsValid() && (simpleMesh.Asset->Polygons.Num() == 1)))
	{
		return false;
	}

	OutProfile = &simpleMesh.Asset->Polygons[0];
	return true;
}

// TODO: Separate into two functions: 1. from a potential world-space origin generate world-space corners for the portal that
// would create its parent metaplane; 2. from world-space corners and portal properties generate a world-space transform
// and extents for the portal actor to update itself with slicing and stretching.
bool UModumateObjectStatics::GetRelativeTransformOnPlanarObj(
	const FModumateObjectInstance *PlanarObj,
	const FVector &WorldPos, float DistanceFromBottom,
	bool bUseDistanceFromBottom, FVector2D &OutRelativePos,
	FQuat &OutRelativeRot)
{
	const FModumateObjectInstance *metaPlaneObject = nullptr;
	if (PlanarObj->GetObjectType() == EObjectType::OTMetaPlane)
	{
		metaPlaneObject = PlanarObj;
	}
	else
	{
		metaPlaneObject = PlanarObj->GetParentObject();
		if (metaPlaneObject == nullptr || metaPlaneObject->GetObjectType() != EObjectType::OTMetaPlane)
		{
			return false;
		}
	}
		
	FVector hostOrigin = metaPlaneObject->GetObjectLocation();
	FVector hostNormal = metaPlaneObject->GetNormal();
	FQuat hostRot = metaPlaneObject->GetObjectRotation();

	FVector pointToProject = WorldPos;

	if (bUseDistanceFromBottom)
	{
		const FGraph3DFace *parentFace = PlanarObj->GetDocument()->GetVolumeGraph().FindFace(metaPlaneObject->ID);
		FVector2D faceRelativePos = parentFace->ProjectPosition2D(WorldPos);

		FVector2D faceRelativeRayEnd = parentFace->ProjectPosition2D(WorldPos - FVector::UpVector);
		if (!faceRelativeRayEnd.Equals(faceRelativePos))
		{
			FVector2D faceBottomRayDir = (faceRelativeRayEnd - faceRelativePos).GetSafeNormal();
			float minRayDist = 0.0f;
			bool bHaveRayHit = false;

			const TArray<FVector2D> &facePoints = parentFace->Cached2DPositions;
			int32 numPoints = facePoints.Num();
			for (int32 pointIdx1 = 0; pointIdx1 < numPoints; ++pointIdx1)
			{
				int32 pointIdx2 = (pointIdx1 + 1) % numPoints;
				const FVector2D &point1 = facePoints[pointIdx1];
				const FVector2D &point2 = facePoints[pointIdx2];
				FVector2D edgeDelta = point2 - point1;
				float edgeLen = edgeDelta.Size();
				if (!ensureAlways(!FMath::IsNearlyZero(edgeLen)))
				{
					continue;
				}
				FVector2D edgeDir = edgeDelta / edgeLen;

				FVector2D edgeIntersection, rayDists;
				bool bIntersection = UModumateGeometryStatics::RayIntersection2D(faceRelativePos, faceBottomRayDir, point1, edgeDir,
					edgeIntersection, rayDists.X, rayDists.Y);
				bool bRaysParallel = FMath::Abs(faceBottomRayDir | edgeDir) >= THRESH_NORMALS_ARE_PARALLEL;

				if (bIntersection && !bRaysParallel && (rayDists.Y < edgeLen) &&
					((rayDists.X < minRayDist) || !bHaveRayHit))
				{
					minRayDist = rayDists.X;
					bHaveRayHit = true;
				}
			}

			if (bHaveRayHit)
			{
				FVector worldRayStart = parentFace->DeprojectPosition(faceRelativePos);
				FVector worldRayEnd = parentFace->DeprojectPosition(faceRelativeRayEnd);
				FVector worldRay = (worldRayEnd - worldRayStart).GetSafeNormal();

				pointToProject += (minRayDist - DistanceFromBottom) * worldRay;
			}
		}
	}

	OutRelativePos = UModumateGeometryStatics::ProjectPoint2D(pointToProject, hostRot.GetAxisX(), hostRot.GetAxisY(), hostOrigin);

	OutRelativeRot = FQuat::Identity;
	return true;
}

bool UModumateObjectStatics::GetWorldTransformOnPlanarObj(
	const FModumateObjectInstance *PlanarObj,
	const FVector2D &RelativePos, const FQuat &RelativeRot,
	FVector &OutWorldPos, FQuat &OutWorldRot)
{
	const FModumateObjectInstance *metaPlaneObject = nullptr;
	if (PlanarObj->GetObjectType() == EObjectType::OTMetaPlane)
	{
		metaPlaneObject = PlanarObj;
	}
	else
	{
		metaPlaneObject = PlanarObj->GetParentObject();
		if (metaPlaneObject == nullptr || metaPlaneObject->GetObjectType() != EObjectType::OTMetaPlane)
		{
			return false;
		}
	}

	FVector hostOrigin = metaPlaneObject->GetObjectLocation();
	FVector hostNormal = metaPlaneObject->GetNormal();
	FQuat hostRot = metaPlaneObject->GetObjectRotation();

	// TODO: support more than just portal-style "flipped or not" relative rotation about Z
	bool bSameNormals = RelativeRot.IsIdentity(KINDA_SMALL_NUMBER);

	FVector startExtrusionDelta, endExtrusionDelta;
	UModumateObjectStatics::GetExtrusionDeltas(PlanarObj, startExtrusionDelta, endExtrusionDelta);
	const FVector &extrusionDelta = bSameNormals ? endExtrusionDelta : startExtrusionDelta;

	OutWorldPos = hostOrigin +
		(hostRot.GetAxisX() * RelativePos.X) +
		(hostRot.GetAxisY() * RelativePos.Y) +
		extrusionDelta;
	
	// Portal rotation should be +Z up, +Y away from the wall, and +X along the wall.
	OutWorldRot = FRotationMatrix::MakeFromYZ(hostNormal, -hostRot.GetAxisY()).ToQuat() * RelativeRot;

	return true;
}

int32 UModumateObjectStatics::GetParentFaceIndex(const FModumateObjectInstance *FaceMountedObj)
{
	// TODO: generalize face-mounted data through an interface/virtual function, for FF&E mounting support
	if ((FaceMountedObj == nullptr) || !ensure(FaceMountedObj->GetObjectType() == EObjectType::OTSurfaceGraph))
	{
		return INDEX_NONE;
	}

	FMOISurfaceGraphData surfaceGraphData;
	if (FaceMountedObj->GetStateData().CustomData.LoadStructData(surfaceGraphData))
	{
		return surfaceGraphData.ParentFaceIndex;
	}

	return INDEX_NONE;
}

bool UModumateObjectStatics::GetGeometryFromFaceIndex(const FModumateObjectInstance *Host, int32 FaceIndex,
	TArray<FVector> &OutFacePoints, FVector &OutNormal, FVector &OutFaceAxisX, FVector &OutFaceAxisY)
{
	OutFacePoints.Reset();

	if ((Host == nullptr) || (FaceIndex == INDEX_NONE))
	{
		return false;
	}

	switch (Host->GetObjectType())
	{
	case EObjectType::OTRoofFace:
	case EObjectType::OTWallSegment:
	case EObjectType::OTFloorSegment:
	case EObjectType::OTCeiling:
	case EObjectType::OTRailSegment:
	case EObjectType::OTSystemPanel:
	{
		const FModumateObjectInstance *hostParent = Host->GetParentObject();
		if (!ensure(hostParent && (hostParent->GetObjectType() == EObjectType::OTMetaPlane)))
		{
			return false;
		}

		FVector hostNormal = hostParent->GetNormal();
		int32 numCorners = hostParent->GetNumCorners();
		if (numCorners < 3)
		{
			return false;
		}

		if (FaceIndex < 2)
		{
			bool bTopFace = (FaceIndex == 0);
			int32 cornerIdxOffset = bTopFace ? numCorners : 0;
			for (int32 i = 0; i < numCorners; ++i)
			{
				int32 cornerIdx = i + cornerIdxOffset;
				OutFacePoints.Add(Host->GetCorner(cornerIdx));
			}

			OutNormal = hostNormal * (bTopFace ? 1.0f : -1.0f);
			UModumateGeometryStatics::FindBasisVectors(OutFaceAxisX, OutFaceAxisY, OutNormal);

			return true;
		}
		else if (FaceIndex < (numCorners + 2))
		{
			int32 bottomStartCornerIdx = FaceIndex - 2;
			int32 bottomEndCornerIdx = (bottomStartCornerIdx + 1) % numCorners;
			int32 topStartCornerIdx = bottomStartCornerIdx + numCorners;
			int32 topEndCornerIdx = bottomEndCornerIdx + numCorners;

			FVector bottomStartCorner = Host->GetCorner(bottomStartCornerIdx);
			FVector bottomEndCorner = Host->GetCorner(bottomEndCornerIdx);
			FVector topStartCorner = Host->GetCorner(topStartCornerIdx);
			FVector topEndCorner = Host->GetCorner(topEndCornerIdx);

			FVector bottomEdgeDir = (bottomEndCorner - bottomStartCorner).GetSafeNormal();
			FVector topEdgeDir = (topEndCorner - topStartCorner).GetSafeNormal();
			if (!FVector::Parallel(bottomEdgeDir, topEdgeDir) || !bottomEdgeDir.IsNormalized() || !topEdgeDir.IsNormalized())
			{
				return false;
			}

			FVector startEdgeDir = (topStartCorner - bottomStartCorner).GetSafeNormal();
			FVector endEdgeDir = (topEndCorner - bottomEndCorner).GetSafeNormal();
			FVector startNormal = (bottomEdgeDir ^ startEdgeDir).GetSafeNormal();
			FVector endNormal = (bottomEdgeDir ^ endEdgeDir).GetSafeNormal();
			if (!FVector::Coincident(startNormal, endNormal) || !startNormal.IsNormalized() || !endNormal.IsNormalized())
			{
				return false;
			}

			OutFacePoints.Add(bottomStartCorner);
			OutFacePoints.Add(bottomEndCorner);
			OutFacePoints.Add(topEndCorner);
			OutFacePoints.Add(topStartCorner);

			OutNormal = startNormal;
			OutFaceAxisX = bottomEdgeDir;
			OutFaceAxisY = OutNormal ^ OutFaceAxisX;

			return true;
		}
		else
		{
			return false;
		}
	}
	break;
	default:
		return false;
	}
}

bool UModumateObjectStatics::GetGeometryFromFaceIndex(const FModumateObjectInstance *Host, int32 FaceIndex,
	TArray<FVector>& OutFacePoints, FTransform& OutFaceOrigin)
{
	FVector faceNormal, faceAxisX, faceAxisY;
	if (GetGeometryFromFaceIndex(Host, FaceIndex, OutFacePoints, faceNormal, faceAxisX, faceAxisY) && ensure(OutFacePoints.Num() > 0))
	{
		OutFaceOrigin.SetLocation(OutFacePoints[0]);
		OutFaceOrigin.SetRotation(FRotationMatrix::MakeFromXY(faceAxisX, faceAxisY).ToQuat());
		OutFaceOrigin.SetScale3D(FVector::OneVector);
		return true;
	}

	return false;
}

bool UModumateObjectStatics::GetGeometryFromSurfacePoly(const FModumateDocument* Doc, int32 SurfacePolyID, bool& bOutInterior, bool& bOutInnerBounds,
	FTransform& OutOrigin, TArray<FVector>& OutPerimeter, TArray<FPolyHole3D>& OutHoles)
{
	OutOrigin = FTransform();
	OutPerimeter.Reset();
	OutHoles.Reset();

	if (!ensure(Doc))
	{
		return false;
	}

	const FModumateObjectInstance* surfacePolyObj = Doc->GetObjectById(SurfacePolyID);
	int32 surfaceGraphID = surfacePolyObj ? surfacePolyObj->GetParentID() : MOD_ID_NONE;
	auto surfaceGraph = Doc->FindSurfaceGraph(surfaceGraphID);
	const FModumateObjectInstance* surfaceGraphObj = Doc->GetObjectById(surfaceGraphID);
	const FGraph2DPolygon* surfacePolygon = surfaceGraph.IsValid() ? surfaceGraph->FindPolygon(SurfacePolyID) : nullptr;
	if (!ensure(surfacePolygon))
	{
		return false;
	}

	bOutInterior = surfacePolygon->bInterior;
	bOutInnerBounds = surfaceGraph->GetInnerBounds().Contains(SurfacePolyID);
	OutOrigin = surfaceGraphObj->GetWorldTransform();

	for (int32 perimeterVertexID : surfacePolygon->CachedPerimeterVertexIDs)
	{
		auto perimeterVertexObj = Doc->GetObjectById(perimeterVertexID);
		if (!ensure(perimeterVertexObj) || perimeterVertexObj->IsDirty(EObjectDirtyFlags::Structure))
		{
			return false;
		}

		OutPerimeter.Add(perimeterVertexObj->GetObjectLocation());
	}

	for (int32 interiorPolyID : surfacePolygon->ContainedPolyIDs)
	{
		const FGraph2DPolygon* interiorSurfacePoly = surfaceGraph->FindPolygon(interiorPolyID);
		if (!ensure(interiorSurfacePoly) || !interiorSurfacePoly->bInterior)
		{
			continue;
		}

		auto& hole = OutHoles.AddDefaulted_GetRef();
		for (int32 interiorPerimeterVertexID : interiorSurfacePoly->CachedPerimeterVertexIDs)
		{
			auto perimeterVertexObj = Doc->GetObjectById(interiorPerimeterVertexID);
			if (!ensure(perimeterVertexObj) || perimeterVertexObj->IsDirty(EObjectDirtyFlags::Structure))
			{
				return false;
			}

			hole.Points.Add(perimeterVertexObj->GetObjectLocation());
		}
	}

	return true;
}

void UModumateObjectStatics::EdgeConnectedToValidPlane(const Modumate::FGraph3DEdge *GraphEdge, const FModumateDocument *Doc,
	bool &bOutConnectedToEmptyPlane, bool &bOutConnectedToSelectedPlane)
{
	bOutConnectedToEmptyPlane = false;
	bOutConnectedToSelectedPlane = false;

	if ((GraphEdge == nullptr) || (Doc == nullptr))
	{
		return;
	}

	for (const auto &edgeFaceConn : GraphEdge->ConnectedFaces)
	{
		const auto *connectedFaceMOI = Doc->GetObjectById(FMath::Abs(edgeFaceConn.FaceID));
		if (connectedFaceMOI && (connectedFaceMOI->GetObjectType() == EObjectType::OTMetaPlane))
		{
			const TArray<int32> &faceMOIChildIDs = connectedFaceMOI->GetChildIDs();
			int32 numChildren = faceMOIChildIDs.Num();

			// Connected to empty plane
			if (numChildren == 0)
			{
				bOutConnectedToEmptyPlane = true;
			}

			if (connectedFaceMOI->IsSelected())
			{
				bOutConnectedToSelectedPlane = true;
			}
			else if (numChildren == 1)
			{
				// Connected to plane that is selectd or whose child is selected
				const auto *faceChild = Doc->GetObjectById(faceMOIChildIDs[0]);
				if (faceChild && faceChild->IsSelected())
				{
					bOutConnectedToSelectedPlane = true;
				}
			}
		}
	}
}

void UModumateObjectStatics::ShouldMetaObjBeEnabled(const FModumateObjectInstance *MetaMOI,
	bool &bOutShouldBeVisible, bool &bOutShouldCollisionBeEnabled, bool &bOutIsConnected)
{
	bOutShouldBeVisible = bOutShouldCollisionBeEnabled = bOutIsConnected = false;

	if (MetaMOI == nullptr)
	{
		return;
	}

	const AActor *metaActor = MetaMOI->GetActor();
	const FModumateDocument *doc = MetaMOI->GetDocument();
	UWorld *world = metaActor ? metaActor->GetWorld() : nullptr;
	AEditModelPlayerController_CPP *playerController = world ? world->GetFirstPlayerController<AEditModelPlayerController_CPP>() : nullptr;

	if (playerController && doc)
	{
		// If we're in room-viewing mode, we don't show any objects besides Rooms
		if (playerController->EMPlayerState->SelectedViewMode == EEditViewModes::Rooms)
		{
			return;
		}

		EObjectType objectType = MetaMOI->GetObjectType();
		int32 objID = MetaMOI->ID;
		const FGraph3D &volumeGraph = doc->GetVolumeGraph();

		bool bInMetaToolMode = false;
		EToolMode curToolMode = playerController->GetToolMode();
		switch (curToolMode)
		{
		case EToolMode::VE_WALL:
		case EToolMode::VE_FLOOR:
		case EToolMode::VE_ROOF_FACE:
		case EToolMode::VE_STAIR:
		case EToolMode::VE_METAPLANE:
		case EToolMode::VE_STRUCTURELINE:
		case EToolMode::VE_CEILING:
			bInMetaToolMode = true;
			break;
		default:
			break;
		}

		bool bEnabledByViewMode = playerController->EMPlayerState->IsObjectTypeEnabledByViewMode(objectType);
		bool bConnectedToEmptyPlane = false;
		bool bConnectedToSelectedPlane = false;

		switch (objectType)
		{
		case EObjectType::OTMetaVertex:
		{
			auto *graphVertex = volumeGraph.FindVertex(objID);
			if (graphVertex == nullptr)
			{
				bOutShouldBeVisible = false;
				bOutShouldCollisionBeEnabled = false;
				break;
			}

			bOutIsConnected = (graphVertex->ConnectedEdgeIDs.Num() > 0);
			for (FGraphSignedID connectedEdgeID : graphVertex->ConnectedEdgeIDs)
			{
				const auto *connectedEdge = volumeGraph.FindEdge(connectedEdgeID);
				EdgeConnectedToValidPlane(connectedEdge, doc, bConnectedToEmptyPlane, bConnectedToSelectedPlane);
				if (bConnectedToSelectedPlane)
				{
					break;
				}
			}

			bOutShouldBeVisible = (bEnabledByViewMode || bConnectedToSelectedPlane || MetaMOI->IsSelected());
			bOutShouldCollisionBeEnabled = bOutShouldBeVisible || bConnectedToEmptyPlane || bInMetaToolMode;
			break;
		}
		case EObjectType::OTMetaEdge:
		{
			auto *graphEdge = volumeGraph.FindEdge(objID);
			if (graphEdge == nullptr)
			{
				bOutShouldBeVisible = false;
				bOutShouldCollisionBeEnabled = false;
				break;
			}

			bool bEdgeHasChild = false;
			bool bEdgeHasSelectedChild = false;
			const TArray<int32> &edgeChildIDs = MetaMOI->GetChildIDs();
			for (int32 edgeChildID : edgeChildIDs)
			{
				const FModumateObjectInstance *edgeChildObj = MetaMOI->GetDocument()->GetObjectById(edgeChildID);
				if (edgeChildObj)
				{
					bEdgeHasChild = true;

					if (edgeChildObj->IsSelected())
					{
						bEdgeHasSelectedChild = true;
					}
				}
			}

			bOutIsConnected = (graphEdge->ConnectedFaces.Num() > 0);
			EdgeConnectedToValidPlane(graphEdge, doc, bConnectedToEmptyPlane, bConnectedToSelectedPlane);

			// Edges with GroupIDs should be hidden outside of meta-plane view mode
			// TODO: add more sophistication to this, based on how GroupIDs are used besides Roof Perimeters
			if (!bEnabledByViewMode && (graphEdge->GroupIDs.Num() > 0))
			{
				bOutShouldBeVisible = false;
				bOutShouldCollisionBeEnabled = false;
			}
			else
			{
				bOutShouldBeVisible = (bConnectedToSelectedPlane || bEdgeHasSelectedChild ||
					bConnectedToEmptyPlane || bEnabledByViewMode || MetaMOI->IsSelected() ||
					(!bOutIsConnected && !bEdgeHasChild));
				bOutShouldCollisionBeEnabled = bOutShouldBeVisible || bInMetaToolMode;
			}
			break;
		}
		case EObjectType::OTMetaPlane:
		{
			auto *graphFace = volumeGraph.FindFace(objID);

			// TODO: this should only be the case when the object is about to be deleted, there should be a flag for that.
			if (graphFace == nullptr)
			{
				bOutShouldBeVisible = false;
				bOutShouldCollisionBeEnabled = false;
				break;
			}

			bOutIsConnected = true;
			bool bPlaneHasChildren = (MetaMOI->GetChildIDs().Num() > 0);

			bOutShouldBeVisible = (bEnabledByViewMode || !bPlaneHasChildren);
			bOutShouldCollisionBeEnabled = bOutShouldBeVisible || bInMetaToolMode;
			break;
		}
		default:
			break;
		}
	}
}

void UModumateObjectStatics::GetGraphIDsFromMOIs(const TSet<FModumateObjectInstance *> &MOIs, TSet<int32> &OutGraphObjIDs)
{
	OutGraphObjIDs.Reset();

	for (FModumateObjectInstance *obj : MOIs)
	{
		EObjectType objectType = obj ? obj->GetObjectType() : EObjectType::OTNone;
		EGraph3DObjectType graphObjType = UModumateTypeStatics::Graph3DObjectTypeFromObjectType(objectType);
		int32 objectID = obj ? obj->ID : MOD_ID_NONE;

		if (graphObjType != EGraph3DObjectType::None)
		{
			OutGraphObjIDs.Add(objectID);
		}
	}
}

void UModumateObjectStatics::GetPlaneHostedValues(const FModumateObjectInstance *PlaneHostedObj, float &OutThickness, float &OutStartOffset, FVector &OutNormal)
{
	OutThickness = PlaneHostedObj->CalculateThickness();
	float offsetPCT = 0.0f;

	FMOIPlaneHostedObjData planeHostedObjData;
	if (PlaneHostedObj->GetStateData().CustomData.LoadStructData(planeHostedObjData))
	{
		offsetPCT = planeHostedObjData.Justification;
	}

	OutStartOffset = -offsetPCT * OutThickness;
	OutNormal = PlaneHostedObj->GetNormal();
}

void UModumateObjectStatics::GetExtrusionDeltas(const FModumateObjectInstance *PlaneHostedObj, FVector &OutStartDelta, FVector &OutEndDelta)
{
	float thickness, startOffset;
	FVector normal;
	GetPlaneHostedValues(PlaneHostedObj, thickness, startOffset, normal);

	OutStartDelta = startOffset * normal;
	OutEndDelta = (startOffset + thickness) * normal;
}

bool UModumateObjectStatics::GetMountedTransform(const FVector &MountOrigin, const FVector &MountNormal,
	const FVector &LocalDesiredNormal, const FVector &LocalDesiredTangent, FTransform &OutTransform)
{
	OutTransform.SetIdentity();

	// If the mounting normal isn't vertical, then align the tangent with world Z+
	FVector mountAxisY(0.0f, 1.0f, 0.0f);
	if (!FVector::Parallel(MountNormal, FVector::UpVector))
	{
		FVector mountAxisX = (MountNormal ^ FVector::UpVector).GetSafeNormal();
		mountAxisY = mountAxisX ^ MountNormal;
	}

	if (!ensure(!FVector::Parallel(mountAxisY, MountNormal) &&
		!FVector::Parallel(LocalDesiredTangent, LocalDesiredNormal)))
	{
		return false;
	}

	// Make the rotations that represents the mount, the local desired rotation,
	// and then the actual desired mount rotation.
	FQuat mountRot = FRotationMatrix::MakeFromYZ(mountAxisY, MountNormal).ToQuat();
	FQuat localRot = FRotationMatrix::MakeFromYZ(LocalDesiredTangent, LocalDesiredNormal).ToQuat();
	FQuat worldRot = mountRot * localRot;

	OutTransform.SetLocation(MountOrigin);
	OutTransform.SetRotation(worldRot);

	return true;
}

bool UModumateObjectStatics::GetFFEMountedTransform(
	const FBIMAssemblySpec *Assembly, const FSnappedCursor &SnappedCursor, FTransform &OutTransform)
{
	if (!SnappedCursor.HitNormal.IsNearlyZero())
	{
		return GetMountedTransform(SnappedCursor.WorldPosition, SnappedCursor.HitNormal,
			Assembly->Normal, Assembly->Tangent, OutTransform);
	}

	return false;
}

bool UModumateObjectStatics::GetFFEBoxSidePoints(AActor *Actor, const FVector &AssemblyNormal, TArray<FVector> &OutPoints)
{
	OutPoints.Reset();

	if ((Actor == nullptr) || !AssemblyNormal.IsNormalized())
	{
		return false;
	}

	// Copied from FMOIObjectImpl::GetStructuralPointsAndLines, so that we can supply
	// an override vector for the bounding box points, to get only the side that corresponds
	// to the FF&E's mounting normal vector.
	FVector actorOrigin, actorBoxExtent;
	Actor->GetActorBounds(false, actorOrigin, actorBoxExtent);
	FQuat actorRot = Actor->GetActorQuat();
	FVector actorLoc = Actor->GetActorLocation();

	// This calculates the extent more accurately since it's unaffected by actor rotation
	actorBoxExtent = Actor->CalculateComponentsBoundingBoxInLocalSpace(true).GetSize() * 0.5f;

	// Supply the opposite of the FF&E's mounting normal vector, to get the "bottom" points of the bounding box.
	TArray<FStructurePoint> structurePoints;
	TArray<FStructureLine> structureLines;
	FModumateSnappingView::GetBoundingBoxPointsAndLines(actorOrigin, actorRot, actorBoxExtent,
		structurePoints, structureLines, -AssemblyNormal);

	if (structurePoints.Num() != 4)
	{
		return false;
	}

	for (const FStructurePoint &point : structurePoints)
	{
		OutPoints.Add(point.Point);
	}

	return true;
}
