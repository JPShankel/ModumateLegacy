// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "ModumateCore/ModumateObjectStatics.h"

#include "Algo/ForEach.h"
#include "BIMKernel/Core/BIMProperties.h"
#include "Database/ModumateSimpleMesh.h"
#include "DocumentManagement/ModumateSnappingView.h"
#include "Graph/Graph2D.h"
#include "Graph/Graph3D.h"
#include "ModumateCore/ExpressionEvaluator.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "ModumateCore/ModumateGeometryStatics.h"
#include "Objects/ModumateObjectInstance.h"
#include "Objects/PlaneHostedObj.h"
#include "Objects/SurfaceGraph.h"
#include "Drafting/ModumateDraftingElements.h"
#include "UnrealClasses/CompoundMeshActor.h"
#include "UnrealClasses/DynamicMeshActor.h"
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
		
	FVector hostOrigin = metaPlaneObject->GetLocation();
	FVector hostNormal = metaPlaneObject->GetNormal();
	FQuat hostRot = metaPlaneObject->GetRotation();

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

	FVector hostOrigin = metaPlaneObject->GetLocation();
	FVector hostNormal = metaPlaneObject->GetNormal();
	FQuat hostRot = metaPlaneObject->GetRotation();

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
		if (!ensure(hostParent && (hostParent->GetObjectType() == EObjectType::OTMetaPlane)) ||
			hostParent->IsDirty(EObjectDirtyFlags::Structure) || hostParent->IsDirty(EObjectDirtyFlags::Mitering))
		{
			return false;
		}

		FVector hostLocation = hostParent->GetLocation();
		FVector hostNormal = hostParent->GetNormal();
		int32 numCorners = hostParent->GetNumCorners();
		if (numCorners < 3)
		{
			return false;
		}

		if (FaceIndex < 2)
		{
			auto meshActor = Cast<ADynamicMeshActor>(Host->GetActor());
			if (!ensure(meshActor && (meshActor->LayerGeometries.Num() > 0)))
			{
				return false;
			}

			bool bOnStartingSide = (FaceIndex != 0);
			auto& layerPoints = bOnStartingSide ? meshActor->LayerGeometries[0].UniquePointsA : meshActor->LayerGeometries.Last().UniquePointsB;
			for (auto& layerPoint : layerPoints)
			{
				OutFacePoints.Add(hostLocation + layerPoint);
			}

			OutNormal = hostNormal * (bOnStartingSide ? -1.0f : 1.0f);
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

		OutPerimeter.Add(perimeterVertexObj->GetLocation());
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

			hole.Points.Add(perimeterVertexObj->GetLocation());
		}
	}

	return true;
}

bool UModumateObjectStatics::GetEdgeFaceConnections(const Modumate::FGraph3DEdge* GraphEdge, const FModumateDocument* Doc,
	bool& bOutConnectedToAnyFace, bool& bOutConnectedToValidFace)
{
	bOutConnectedToAnyFace = false;
	bOutConnectedToValidFace = false;

	if ((GraphEdge == nullptr) || (Doc == nullptr))
	{
		return false;
	}

	for (const auto &edgeFaceConn : GraphEdge->ConnectedFaces)
	{
		const auto *connectedFaceMOI = Doc->GetObjectById(FMath::Abs(edgeFaceConn.FaceID));
		if (connectedFaceMOI && (connectedFaceMOI->GetObjectType() == EObjectType::OTMetaPlane))
		{
			bOutConnectedToAnyFace = true;

			// TODO: if the evaluation is expensive, or should be dependent on data that requires cleaning,
			// then this should require `connectedFaceMOI->IsDirty(EObjectDirtyFlags::Visuals)` to be true, and return false otherwise.
			bool bConnectedFaceVisible, bConnectedFaceCollidable;
			if (!GetMetaObjEnabledFlags(connectedFaceMOI, bConnectedFaceVisible, bConnectedFaceCollidable))
			{
				return false;
			}

			if (bConnectedFaceVisible)
			{
				bOutConnectedToValidFace = true;
				break;
			}
		}
	}

	return true;
}

bool UModumateObjectStatics::GetEdgePolyConnections(const Modumate::FGraph2DEdge* SurfaceEdge, const FModumateDocument* Doc,
	bool& bOutConnectedToAnyPolygon, bool& bOutConnectedToValidPolygon)
{
	bOutConnectedToAnyPolygon = false;
	bOutConnectedToValidPolygon = false;

	if ((SurfaceEdge == nullptr) || (Doc == nullptr))
	{
		return false;
	}

	const Modumate::FGraph2DPolygon *leftInteriorPoly, *rightInteriorPoly;
	if (!SurfaceEdge->GetAdjacentInteriorPolygons(leftInteriorPoly, rightInteriorPoly))
	{
		return false;
	}

	for (auto* adjacentPoly : { leftInteriorPoly, rightInteriorPoly })
	{
		if (adjacentPoly == nullptr)
		{
			continue;
		}

		const auto* connectedPolyMOI = Doc->GetObjectById(adjacentPoly->ID);
		if (connectedPolyMOI && (connectedPolyMOI->GetObjectType() == EObjectType::OTSurfacePolygon))
		{
			bOutConnectedToAnyPolygon = true;

			// TODO: if the evaluation is expensive, or should be dependent on data that requires cleaning,
			// then this should require `connectedFaceMOI->IsDirty(EObjectDirtyFlags::Visuals)` to be true, and return false otherwise.
			bool bConnectedPolyVisible, bConnectedPolyCollidable;
			if (!GetSurfaceObjEnabledFlags(connectedPolyMOI, bConnectedPolyVisible, bConnectedPolyCollidable))
			{
				return false;
			}

			if (bConnectedPolyVisible)
			{
				bOutConnectedToValidPolygon = true;
				break;
			}
		}
	}

	return true;
}

bool UModumateObjectStatics::GetNonPhysicalEnabledFlags(const FModumateObjectInstance* NonPhysicalMOI, bool& bOutVisible, bool& bOutCollisionEnabled)
{
	EObjectType objectType = NonPhysicalMOI ? NonPhysicalMOI->GetObjectType() : EObjectType::OTNone;
	EToolCategories objectCategory = UModumateTypeStatics::GetObjectCategory(objectType);
	switch (objectCategory)
	{
	case EToolCategories::MetaGraph:
		return GetMetaObjEnabledFlags(NonPhysicalMOI, bOutVisible, bOutCollisionEnabled);
	case EToolCategories::SurfaceGraphs:
		return GetSurfaceObjEnabledFlags(NonPhysicalMOI, bOutVisible, bOutCollisionEnabled);
	default:
		ensureMsgf(false, TEXT("Invalid object; must be member of Volume Graph or Surface Graph!"));
		return false;
	}
}

bool UModumateObjectStatics::GetMetaObjEnabledFlags(const FModumateObjectInstance *MetaMOI, bool& bOutVisible, bool& bOutCollisionEnabled)
{
	bOutVisible = bOutCollisionEnabled = false;

	const AActor* metaActor = MetaMOI ? MetaMOI->GetActor() : nullptr;
	const FModumateDocument* doc = MetaMOI ? MetaMOI->GetDocument() : nullptr;
	UWorld *world = metaActor ? metaActor->GetWorld() : nullptr;
	AEditModelPlayerController_CPP *playerController = world ? world->GetFirstPlayerController<AEditModelPlayerController_CPP>() : nullptr;
	if (playerController == nullptr)
	{
		return false;
	}

	EObjectType objectType = MetaMOI->GetObjectType();
	int32 objID = MetaMOI->ID;
	const FGraph3D& volumeGraph = doc->GetVolumeGraph();
	if (!volumeGraph.ContainsObject(objID))
	{
		return false;
	}

	// We may need to use children / sibling children connectivity to disable visibility and/or collision.
	bool bConnectedToAnyFace = false;
	bool bConnectedToValidFace = false;
	bool bHasChildren = (MetaMOI->GetChildIDs().Num() > 0);

	EToolCategories curToolCategory = UModumateTypeStatics::GetToolCategory(playerController->GetToolMode());
	bool bInCompatibleToolCategory = (curToolCategory == EToolCategories::MetaGraph) || (curToolCategory == EToolCategories::Separators);

	EEditViewModes curViewMode = playerController->EMPlayerState->SelectedViewMode;
	bool bMetaViewMode = (curViewMode == EEditViewModes::MetaGraph);
	bool bHybridViewMode = (curViewMode != EEditViewModes::Physical);

	switch (objectType)
	{
	case EObjectType::OTMetaVertex:
	{
		auto *graphVertex = volumeGraph.FindVertex(objID);

		for (FGraphSignedID connectedEdgeID : graphVertex->ConnectedEdgeIDs)
		{
			bool bEdgeConnectedToAnyFace, bEdgeConnectedToValidFace;
			if (!GetEdgeFaceConnections(volumeGraph.FindEdge(connectedEdgeID), doc, bEdgeConnectedToAnyFace, bEdgeConnectedToValidFace))
			{
				return false;
			}

			bConnectedToAnyFace |= bEdgeConnectedToAnyFace;
			bConnectedToValidFace |= bEdgeConnectedToValidFace;
		}

		bOutVisible =
			(bMetaViewMode || (bHybridViewMode && !bHasChildren)) &&
			(!bConnectedToAnyFace || bConnectedToValidFace);
		bOutCollisionEnabled = bOutVisible || bInCompatibleToolCategory;
		break;
	}
	case EObjectType::OTMetaEdge:
	{
		auto *graphEdge = volumeGraph.FindEdge(objID);

		if (!GetEdgeFaceConnections(graphEdge, doc, bConnectedToAnyFace, bConnectedToValidFace))
		{
			return false;
		}

		// Edges with GroupIDs should be hidden outside of meta-plane view mode
		// TODO: add more sophistication to this, based on how GroupIDs are used besides Roof Perimeters
		if (bMetaViewMode && graphEdge->GroupIDs.Num() > 0)
		{
			bOutVisible = true;
			bOutCollisionEnabled = true;
		}
		else
		{
			bOutVisible =
				(bMetaViewMode || (bHybridViewMode && !bHasChildren)) &&
				(!bConnectedToAnyFace || bConnectedToValidFace);
			bOutCollisionEnabled = bOutVisible || bInCompatibleToolCategory;
		}
		break;
	}
	case EObjectType::OTMetaPlane:
	{
		bOutVisible = !MetaMOI->IsRequestedHidden() && (bMetaViewMode || (bHybridViewMode && !bHasChildren));
		bOutCollisionEnabled = !MetaMOI->IsCollisionRequestedDisabled() && (bOutVisible || bInCompatibleToolCategory);
		break;
	}
	default:
		break;
	}

	return true;
}

bool UModumateObjectStatics::GetSurfaceObjEnabledFlags(const FModumateObjectInstance* SurfaceMOI, bool& bOutVisible, bool& bOutCollisionEnabled)
{
	bOutVisible = bOutCollisionEnabled = false;

	const AActor* surfaceActor = SurfaceMOI ? SurfaceMOI->GetActor() : nullptr;
	const FModumateDocument* doc = SurfaceMOI ? SurfaceMOI->GetDocument() : nullptr;
	UWorld* world = surfaceActor ? surfaceActor->GetWorld() : nullptr;
	AEditModelPlayerController_CPP* playerController = world ? world->GetFirstPlayerController<AEditModelPlayerController_CPP>() : nullptr;
	if (playerController == nullptr)
	{
		return false;
	}

	EObjectType objectType = SurfaceMOI->GetObjectType();
	int32 objID = SurfaceMOI->ID;
	auto surfaceGraph = doc->FindSurfaceGraphByObjID(objID);
	if (!surfaceGraph.IsValid() || !surfaceGraph->ContainsObject(objID))
	{
		return false;
	}

	// We may need to use children / sibling children connectivity to disable visibility and/or collision.
	bool bConnectedToAnyPolygon = false;
	bool bConnectedToValidPolygon = false;
	bool bHasChildren = (SurfaceMOI->GetChildIDs().Num() > 0);

	EToolCategories curToolCategory = UModumateTypeStatics::GetToolCategory(playerController->GetToolMode());
	bool bInCompatibleToolCategory = (curToolCategory == EToolCategories::SurfaceGraphs) || (curToolCategory == EToolCategories::Attachments);

	EEditViewModes curViewMode = playerController->EMPlayerState->SelectedViewMode;
	bool bSurfaceViewMode = (curViewMode == EEditViewModes::SurfaceGraphs);
	bool bHybridViewMode = (curViewMode == EEditViewModes::AllObjects);

	switch (objectType)
	{
	case EObjectType::OTSurfaceVertex:
	{
		auto* surfaceVertex = surfaceGraph->FindVertex(objID);

		for (FGraphSignedID connectedEdgeID : surfaceVertex->Edges)
		{
			bool bEdgeConnectedToAnyPolygon, bEdgeConnectedToValidPolygon;
			if (!GetEdgePolyConnections(surfaceGraph->FindEdge(connectedEdgeID), doc, bEdgeConnectedToAnyPolygon, bEdgeConnectedToValidPolygon))
			{
				return false;
			}

			bConnectedToAnyPolygon |= bEdgeConnectedToAnyPolygon;
			bConnectedToValidPolygon |= bEdgeConnectedToValidPolygon;
		}

		bOutVisible =
			(bSurfaceViewMode || (bHybridViewMode && !bHasChildren)) &&
			(!bConnectedToAnyPolygon || bConnectedToValidPolygon);
		bOutCollisionEnabled = bOutVisible || bInCompatibleToolCategory;
		break;
	}
	case EObjectType::OTSurfaceEdge:
	{
		auto* surfaceEdge = surfaceGraph->FindEdge(objID);

		if (!GetEdgePolyConnections(surfaceEdge, doc, bConnectedToAnyPolygon, bConnectedToValidPolygon))
		{
			return false;
		}

		bOutVisible =
			(bSurfaceViewMode || (bHybridViewMode && !bHasChildren)) &&
			(!bConnectedToAnyPolygon || bConnectedToValidPolygon);
		bOutCollisionEnabled = bOutVisible || bInCompatibleToolCategory;
		break;
	}
	case EObjectType::OTSurfacePolygon:
	{
		bOutVisible = !SurfaceMOI->IsRequestedHidden() && (bSurfaceViewMode || (bHybridViewMode && !bHasChildren));
		bOutCollisionEnabled = !SurfaceMOI->IsCollisionRequestedDisabled() && (bOutVisible || bInCompatibleToolCategory);
		break;
	}
	default:
		break;
	}

	return true;
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

bool UModumateObjectStatics::GetExtrusionPerimeterPoints(const FModumateObjectInstance* MOI,
       const FVector& LineUp, const FVector& LineNormal, TArray<FVector>& outPerimeterPoints)
{
       const FSimplePolygon* polyProfile = nullptr;
       const FModumateObjectInstance* parent = MOI ? MOI->GetParentObject() : nullptr;
       if (parent == nullptr)
       {
               return false;
       }

       FVector scale(FVector::OneVector);
       const auto& assembly = MOI->GetAssembly();
       if (ensureAlways(assembly.Extrusions.Num() > 0))
       {
               scale = MOI->GetAssembly().Extrusions[0].Scale;
       }
       else
       {
               return false;
       }

       if (!UModumateObjectStatics::GetPolygonProfile(&MOI->GetAssembly(), polyProfile)
               || polyProfile->Points.Num() == 0)
       {
               return false;
       }

       for (const auto& point : polyProfile->Points)
       {
               outPerimeterPoints.Add(point.X * LineUp * scale.X + point.Y * LineNormal * scale.Y);
       }

       return true;
}

void UModumateObjectStatics::GetExtrusionCutPlaneDraftingLines(const TSharedPtr<Modumate::FDraftingComposite>& ParentPage, const FPlane& Plane,
	const FVector& AxisX, const FVector& AxisY, const FVector& Origin, const FBox2D& BoundingBox, const TArray<FVector>& Perimeter,
	const FVector& StartPosition, const FVector& EndPosition, Modumate::FModumateLayerType LayerType, float Epsilon /*= 0.0f*/)
{
	FVector startCap(Perimeter.Last(0) + StartPosition);
	FVector endCap(Perimeter.Last(0) + EndPosition);

	TArray<FVector2D> points;
	TArray<FVector2D> startCapPoints;
	TArray<FVector2D> endCapPoints;
	for (const auto& edgePoint : Perimeter)
	{
		FVector p1(edgePoint + StartPosition);
		FVector p2(edgePoint + EndPosition);
		FVector intersect;
		if (FMath::SegmentPlaneIntersection(p1, p2, Plane, intersect))
		{
			points.Emplace(UModumateGeometryStatics::ProjectPoint2D(Origin, -AxisX, -AxisY, intersect));
		}
		if (FMath::SegmentPlaneIntersection(startCap, p1, Plane, intersect))
		{
			startCapPoints.Emplace(UModumateGeometryStatics::ProjectPoint2D(Origin, -AxisX, -AxisY, intersect));
		}
		if (FMath::SegmentPlaneIntersection(endCap, p2, Plane, intersect))
		{
			endCapPoints.Emplace(UModumateGeometryStatics::ProjectPoint2D(Origin, -AxisX, -AxisY, intersect));
		}
		startCap = p1;
		endCap = p2;
	}

	if (Epsilon != 0.0f && points.Num() > 2)
	{
		const float Epsilon2 = Epsilon * Epsilon;
		TArray<FVector2D> thinnedPoints;
		thinnedPoints.Add(points[0]);
		for (int32 i = 1; i < points.Num(); ++i)
		{
			if (FVector2D::DistSquared(points[i], thinnedPoints.Last()) >= Epsilon2)
			{
				thinnedPoints.Add(points[i]);
			}
		}
		points = MoveTemp(thinnedPoints);
	}

	if (points.Num() + startCapPoints.Num() + endCapPoints.Num() >= 2)
	{
		TArray<FEdge> draftEdges;
		int32 numPoints = points.Num();
		if (numPoints >= 2)
		{
			for (int32 p = 0; p < numPoints; ++p)
			{
				draftEdges.Emplace(FVector(points[p], 0), FVector(points[(p + 1) % numPoints], 0));
			}
		}

		if (startCapPoints.Num() == endCapPoints.Num() && startCapPoints.Num() >= 2)
		{
			numPoints = startCapPoints.Num();
			for (int32 p = 0; p < numPoints; ++p)
			{
				draftEdges.Emplace(FVector(startCapPoints[p], 0), FVector(endCapPoints[p], 0));
				draftEdges.Emplace(FVector(startCapPoints[p], 0), FVector(startCapPoints[(p + 1) % numPoints], 0));
				draftEdges.Emplace(FVector(endCapPoints[p], 0), FVector(endCapPoints[(p + 1) % numPoints], 0));
			}
		}

		for (const auto& edge : draftEdges)
		{
			FVector2D vert0(edge.Vertex[0]);
			FVector2D vert1(edge.Vertex[1]);

			FVector2D boxClipped0;
			FVector2D boxClipped1;

			if (UModumateFunctionLibrary::ClipLine2DToRectangle(vert0, vert1, BoundingBox, boxClipped0, boxClipped1))
			{
				TSharedPtr<Modumate::FDraftingLine> line = MakeShared<Modumate::FDraftingLine>(
					Modumate::Units::FCoordinates2D::WorldCentimeters(boxClipped0),
					Modumate::Units::FCoordinates2D::WorldCentimeters(boxClipped1),
					Modumate::Units::FThickness::Points(0.25f), Modumate::FMColor::Black);
				ParentPage->Children.Add(line);
				line->SetLayerTypeRecursive(LayerType);
			}

		}
	}
}

TArray<FEdge> UModumateObjectStatics::GetExtrusionBeyondLinesFromMesh(const FPlane& Plane, const TArray<FVector>& Perimeter,
	const FVector& StartPosition, const FVector& EndPosition)
{
	const FVector viewNormal = Plane;
	TArray<FEdge> beamEdges;
	FVector LineUp, LineNormal;

	int32 numEdges = Perimeter.Num();
	int32 closestPoint = 0;
	float closestDist = INFINITY;
	for (int32 i = 0; i < numEdges; ++i)
	{
		float dist = Plane.PlaneDot(Perimeter[i] + StartPosition);
		if (dist < closestDist)
		{
			closestPoint = i;
			closestDist = dist;
		}
	}

	if (closestDist < 0.0f)
	{
		return beamEdges;
	}

	const FVector LineDir = (EndPosition - StartPosition).GetSafeNormal();

	FVector previousNormal(0.0f);
	bool bPreviousFacing = false;

	static constexpr float facetThreshold = 0.985;  // 10 degrees

	// Test closest facet for winding direction:
	bool bPerimeterFacingDir;
	FVector tp0 = Perimeter[(closestPoint + numEdges - 1) % numEdges];
	FVector tp1 = Perimeter[closestPoint];
	FVector tp2 = Perimeter[(closestPoint + 1) % numEdges];
	if (((tp2 - tp1).GetSafeNormal() | viewNormal) < ((tp0 - tp1).GetSafeNormal() | viewNormal))
	{
		FVector normal(((tp2 - tp1) ^ LineDir).GetSafeNormal());
		bPerimeterFacingDir = (normal | viewNormal) > 0;
	}
	else
	{
		FVector normal(((tp1 - tp0) ^ LineDir).GetSafeNormal());
		bPerimeterFacingDir = (normal | viewNormal) > 0;
	}

	for (int32 edge = 0; edge <= numEdges; ++edge)
	{
		FVector p1 = Perimeter[(closestPoint + edge) % numEdges];
		FVector p2 = Perimeter[(closestPoint + edge + 1) % numEdges];
		FVector normal((p2 - p1) ^ LineDir);
		normal.Normalize();
		bool bFacing = ((normal | viewNormal) <= 0) ^ bPerimeterFacingDir;
		if (!previousNormal.IsZero())
		{
			bool bSilhouette = bFacing ^ bPreviousFacing;
			if (bSilhouette || bFacing && (normal | previousNormal) <= facetThreshold)
			{
				FVector draftLineStart(p1 + StartPosition);
				FVector draftLineEnd(p1 + EndPosition);
				FEdge edgeAlongBeam(draftLineStart, draftLineEnd);
				edgeAlongBeam.Count = int(bSilhouette);  // Use count field to encode internal/silhouette.
				beamEdges.Add(edgeAlongBeam);
			}
		}

		previousNormal = normal;
		bPreviousFacing = bFacing;

		if (edge < numEdges)
		{
			beamEdges.Emplace(p1 + StartPosition, p2 + StartPosition);
			beamEdges.Emplace(p1 + EndPosition, p2 + EndPosition);
		}
	}

	return beamEdges;
}
