// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "Objects/ModumateObjectStatics.h"

#include "Algo/ForEach.h"
#include "BIMKernel/Core/BIMProperties.h"
#include "Database/ModumateSimpleMesh.h"
#include "DocumentManagement/ModumateSnappingView.h"
#include "Graph/Graph2D.h"
#include "Graph/Graph3D.h"
#include "ModumateCore/ExpressionEvaluator.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "ModumateCore/ModumateGeometryStatics.h"
#include "Objects/DesignOption.h"
#include "Objects/LayeredObjectInterface.h"
#include "Objects/MetaEdgeSpan.h"
#include "Objects/MetaPlaneSpan.h"
#include "Objects/ModumateObjectInstance.h"
#include "Objects/ModumateObjectDeltaStatics.h"
#include "Objects/PlaneHostedObj.h"
#include "Objects/Portal.h"
#include "Objects/SurfaceGraph.h"
#include "Objects/Terrain.h"
#include "Objects/StructureLine.h"
#include "Objects/EdgeHosted.h"
#include "Objects/MetaGraph.h"
#include "Drafting/ModumateDraftingElements.h"
#include "UnrealClasses/CompoundMeshActor.h"
#include "UnrealClasses/DynamicMeshActor.h"
#include "UnrealClasses/EditModelGameState.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"

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
	const AModumateObjectInstance *PlanarObj,
	const FVector &WorldPos, float DistanceFromBottom,
	bool bUseDistanceFromBottom, FVector2D &OutRelativePos,
	FQuat &OutRelativeRot)
{
	const AModumateObjectInstance *metaPlaneObject = nullptr;
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
		const FGraph3DFace *parentFace = PlanarObj->GetDocument()->FindVolumeGraph(metaPlaneObject->ID)->FindFace(metaPlaneObject->ID);
		if (!ensure(parentFace))
		{
			return false;
		}
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
	const AModumateObjectInstance *PlanarObj,
	const FVector2D &RelativePos, const FQuat &RelativeRot,
	FVector &OutWorldPos, FQuat &OutWorldRot)
{
	if (!(PlanarObj->GetObjectType() == EObjectType::OTMetaPlane ||
		PlanarObj->GetObjectType() == EObjectType::OTMetaPlaneSpan))
	{
		return false;
	}

	FVector hostOrigin = PlanarObj->GetLocation();
	FVector hostNormal = PlanarObj->GetNormal();
	FQuat hostRot = PlanarObj->GetRotation();

	// TODO: support more than just portal-style "flipped or not" relative rotation about Z
	bool bSameNormals = RelativeRot.IsIdentity(KINDA_SMALL_NUMBER);

	OutWorldPos = hostOrigin +
		(hostRot.GetAxisX() * RelativePos.X) +
		(hostRot.GetAxisY() * RelativePos.Y);
	
	// Portal rotation should be +Z up, +Y away from the wall, and +X along the wall.
	OutWorldRot = FRotationMatrix::MakeFromYZ(hostNormal, -hostRot.GetAxisY()).ToQuat() * RelativeRot;

	return true;
}

int32 UModumateObjectStatics::GetParentFaceIndex(const AModumateObjectInstance *FaceMountedObj)
{
	// TODO: generalize face-mounted data through an interface/virtual function, for FF&E mounting support
	if ((FaceMountedObj == nullptr) || !ensure(FaceMountedObj->GetObjectType() == EObjectType::OTSurfaceGraph
		|| FaceMountedObj->GetObjectType() == EObjectType::OTTerrain))
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

bool UModumateObjectStatics::GetGeometryFromFaceIndex(const AModumateObjectInstance *Host, int32 FaceIndex,
	TArray<FVector> &OutFacePoints, FVector &OutNormal, FVector &OutFaceAxisX, FVector &OutFaceAxisY)
{
	OutFacePoints.Reset();

	auto planeHostedObj = Cast<AMOIPlaneHostedObj>(Host);
	if ((planeHostedObj == nullptr) || (FaceIndex == INDEX_NONE))
	{
		return false;
	}

	const AModumateObjectInstance *hostParent = Host->GetParentObject();
	if (!ensure(hostParent && (hostParent->GetObjectType() == EObjectType::OTMetaPlaneSpan)) ||
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
		bool bOnStartingSide = (FaceIndex != 0);

		auto& extendedSurfaceFaces = planeHostedObj->GetExtendedSurfaceFaces();
		auto& extendedSurfaceFace = bOnStartingSide ? extendedSurfaceFaces.Key : extendedSurfaceFaces.Value;
		for (auto& facePoint : extendedSurfaceFace)
		{
			OutFacePoints.Add(hostLocation + facePoint);
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

bool UModumateObjectStatics::GetGeometryFromFaceIndex(const AModumateObjectInstance *Host, int32 FaceIndex,
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

bool UModumateObjectStatics::GetGeometryFromSurfacePoly(const UModumateDocument* Doc, int32 SurfacePolyID, bool& bOutInterior, bool& bOutInnerBounds,
	FTransform& OutOrigin, TArray<FVector>& OutPerimeter, TArray<FPolyHole3D>& OutHoles)
{
	OutOrigin = FTransform();
	OutPerimeter.Reset();
	OutHoles.Reset();

	if (!ensure(Doc))
	{
		return false;
	}

	const AModumateObjectInstance* surfacePolyObj = Doc->GetObjectById(SurfacePolyID);
	int32 surfaceGraphID = surfacePolyObj ? surfacePolyObj->GetParentID() : MOD_ID_NONE;
	auto surfaceGraph = Doc->FindSurfaceGraph(surfaceGraphID);
	const AModumateObjectInstance* surfaceGraphObj = Doc->GetObjectById(surfaceGraphID);
	const FGraph2DPolygon* surfacePolygon = surfaceGraph.IsValid() ? surfaceGraph->FindPolygon(SurfacePolyID) : nullptr;
	if (!ensureMsgf(surfacePolygon && surfaceGraphObj,
		TEXT("Missing SurfacePolygon and/or SurfaceGraph for PolyID %d, GraphID %d!"), SurfacePolyID, surfaceGraphID))
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

bool UModumateObjectStatics::GetEdgeFaceConnections(const FGraph3DEdge* GraphEdge, const UModumateDocument* Doc,
	bool& bOutConnectedToAnyFace, bool& bOutConnectedToValidFace, bool& bOutConnectedToVisibleChild)
{
	bOutConnectedToAnyFace = false;
	bOutConnectedToValidFace = false;
	bOutConnectedToVisibleChild = false;

	if ((GraphEdge == nullptr) || (Doc == nullptr))
	{
		return false;
	}

	for (const auto &edgeFaceConn : GraphEdge->ConnectedFaces)
	{
		const auto *connectedFaceMOI = Doc->GetObjectById(FMath::Abs(edgeFaceConn.FaceID));
		if (connectedFaceMOI && (connectedFaceMOI->GetObjectType() == EObjectType::OTMetaPlane))
		{
			if (connectedFaceMOI->IsDirty(EObjectDirtyFlags::Visuals))
			{
				return false;
			}

			bOutConnectedToAnyFace = true;
			bOutConnectedToValidFace |= connectedFaceMOI->IsVisible();

			auto& faceChildIDs = connectedFaceMOI->GetChildIDs();
			auto faceChild = (faceChildIDs.Num() == 1) ? Doc->GetObjectById(faceChildIDs[0]) : nullptr;
			if (!faceChild)
			{
				// Since span members are not children of MetaPlane, check for them if faceChild is nullptr
				TArray<int32> spanMemberIDs; GetSpansForFaceObject(Doc, connectedFaceMOI, spanMemberIDs);
				if (spanMemberIDs.Num() > 0)
				{
					const auto spanMOI = Cast<AMOIMetaPlaneSpan>(Doc->GetObjectById(spanMemberIDs[0]));
					faceChild = (spanMOI && spanMOI->InstanceData.GraphMembers.Num() > 0) ? Doc->GetObjectById(spanMOI->InstanceData.GraphMembers[0]) : nullptr;
				}
			}
			bOutConnectedToVisibleChild |= (faceChild && faceChild->IsVisible());
		}
	}

	return true;
}

bool UModumateObjectStatics::GetEdgePolyConnections(const FGraph2DEdge* SurfaceEdge, const UModumateDocument* Doc,
	bool& bOutConnectedToAnyPolygon, bool& bOutConnectedToVisiblePolygon, bool& bOutConnectedToVisibleChild)
{
	bOutConnectedToAnyPolygon = false;
	bOutConnectedToVisiblePolygon = false;
	bOutConnectedToVisibleChild = false;

	if ((SurfaceEdge == nullptr) || (Doc == nullptr))
	{
		return false;
	}

	const FGraph2DPolygon *leftInteriorPoly, *rightInteriorPoly;
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
			if (connectedPolyMOI->IsDirty(EObjectDirtyFlags::Visuals))
			{
				return false;
			}

			bOutConnectedToAnyPolygon = true;
			bOutConnectedToVisiblePolygon |= connectedPolyMOI->IsVisible();

			auto& polyChildIDs = connectedPolyMOI->GetChildIDs();
			auto polyChild = (polyChildIDs.Num() == 1) ? Doc->GetObjectById(polyChildIDs[0]) : nullptr;
			bOutConnectedToVisibleChild |= (polyChild && polyChild->IsVisible());
		}
	}

	return true;
}

bool UModumateObjectStatics::GetNonPhysicalEnabledFlags(const AModumateObjectInstance* NonPhysicalMOI, bool& bOutVisible, bool& bOutCollisionEnabled)
{
	EObjectType objectType = NonPhysicalMOI ? NonPhysicalMOI->GetObjectType() : EObjectType::OTNone;
	EToolCategories objectCategory = UModumateTypeStatics::GetObjectCategory(objectType);
	switch (objectCategory)
	{
	case EToolCategories::MetaGraph:
		return GetMetaObjEnabledFlags(NonPhysicalMOI, bOutVisible, bOutCollisionEnabled);
	case EToolCategories::SurfaceGraphs:
	case EToolCategories::SiteTools:
		return GetSurfaceObjEnabledFlags(NonPhysicalMOI, bOutVisible, bOutCollisionEnabled);
	default:
		switch (objectType)
		{
		case EObjectType::OTMetaEdgeSpan:
		case EObjectType::OTMetaPlaneSpan:
			bOutVisible = bOutCollisionEnabled = false;
			return true;
		}
		ensureMsgf(false, TEXT("Invalid object; must be member of Volume Graph or Surface Graph!"));
		return false;
	}
}

bool UModumateObjectStatics::GetMetaObjEnabledFlags(const AModumateObjectInstance *MetaMOI, bool& bOutVisible, bool& bOutCollisionEnabled)
{
	bOutVisible = bOutCollisionEnabled = false;

	// For objects that are already destroyed, report that we correctly determined that they should be disabled.
	// This only needs its own logic because we want to keep around actors of destroyed MOIs, and correctly disable them,
	// in case they need to be reused quickly for subsequent preview deltas or doing undo/redo.
	if (MetaMOI && MetaMOI->IsDestroyed())
	{
		return true;
	}

	const AActor* metaActor = MetaMOI ? MetaMOI->GetActor() : nullptr;
	const UModumateDocument* doc = MetaMOI ? MetaMOI->GetDocument() : nullptr;
	UWorld *world = metaActor ? metaActor->GetWorld() : nullptr;

	// Without a Player Controller (most likely on a server), the object might as well be hidden, and if so that's a successful result that shouldn't cause deletion.
	AEditModelPlayerController *playerController = world ? world->GetFirstPlayerController<AEditModelPlayerController>() : nullptr;
	if (playerController == nullptr)
	{
		return true;
	}

	EObjectType objectType = MetaMOI->GetObjectType();
	int32 objID = MetaMOI->ID;
	const FGraph3D& volumeGraph = *doc->FindVolumeGraph(objID);
	if (!volumeGraph.ContainsObject(objID))
	{
		return false;
	}

	// We may need to use children / sibling children connectivity to disable visibility and/or collision.
	bool bConnectedToAnyFace = false;
	bool bConnectedToVisibleFace = false;
	bool bConnectedToVisibleChild = false;
	bool bHasChildren = (MetaMOI->GetChildIDs().Num() > 0);
	// Check if this meta object is part of a span
	TArray<int32> spanIDs;
	if (MetaMOI->GetObjectType() == EObjectType::OTMetaPlane)
	{
		GetSpansForFaceObject(doc, MetaMOI, spanIDs);
	}
	else if (MetaMOI->GetObjectType() == EObjectType::OTMetaEdge)
	{
		GetSpansForEdgeObject(doc, MetaMOI, spanIDs);
	}
	bool bHasSpan = spanIDs.Num() > 0;

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
		if (!ensure(graphVertex))
		{
			return false;
		}

		for (FGraphSignedID connectedEdgeID : graphVertex->ConnectedEdgeIDs)
		{
			bool bEdgeConnectedToAnyFace, bEdgeConnectedToVisibleFace, bEdgeConnectedToVisibleChild;
			if (!GetEdgeFaceConnections(volumeGraph.FindEdge(connectedEdgeID), doc, bEdgeConnectedToAnyFace, bEdgeConnectedToVisibleFace, bEdgeConnectedToVisibleChild))
			{
				return false;
			}

			bConnectedToAnyFace |= bEdgeConnectedToAnyFace;
			bConnectedToVisibleFace |= bEdgeConnectedToVisibleFace;
			bConnectedToVisibleChild |= bEdgeConnectedToVisibleChild;
		}

		bOutVisible = !MetaMOI->IsRequestedHidden() &&
			(bMetaViewMode || (bHybridViewMode && !bHasChildren)) &&
			(!bConnectedToAnyFace || bConnectedToVisibleFace);
		bOutCollisionEnabled = bOutVisible || bInCompatibleToolCategory || bConnectedToVisibleChild;
		break;
	}
	case EObjectType::OTMetaEdge:
	{
		auto *graphEdge = volumeGraph.FindEdge(objID);

		if (!GetEdgeFaceConnections(graphEdge, doc, bConnectedToAnyFace, bConnectedToVisibleFace, bConnectedToVisibleChild))
		{
			return false;
		}

		bOutVisible = !MetaMOI->IsRequestedHidden() &&
			(bMetaViewMode || (bHybridViewMode && !(bHasChildren || bHasSpan))) &&
			(!bConnectedToAnyFace || bConnectedToVisibleFace);
		bOutCollisionEnabled = bOutVisible || IsMetaEdgeObjCollidable(MetaMOI) || bConnectedToVisibleChild;
		break;
	}
	case EObjectType::OTMetaPlane:
	{
		bOutVisible = !MetaMOI->IsRequestedHidden() && (bMetaViewMode || (bHybridViewMode && !(bHasChildren || bHasSpan)));
		bOutCollisionEnabled = !MetaMOI->IsCollisionRequestedDisabled() && (bOutVisible || IsMetaPlaneObjCollidable(MetaMOI));
		break;
	}
	default:
		break;
	}

	return true;
}

bool UModumateObjectStatics::GetSurfaceObjEnabledFlags(const AModumateObjectInstance* SurfaceMOI, bool& bOutVisible, bool& bOutCollisionEnabled)
{
	bOutVisible = bOutCollisionEnabled = false;

	// For objects that are already destroyed, report that we correctly determined that they should be disabled.
	// This only needs its own logic because we want to keep around actors of destroyed MOIs, and correctly disable them,
	// in case they need to be reused quickly for subsequent preview deltas or doing undo/redo.
	if (SurfaceMOI && SurfaceMOI->IsDestroyed())
	{
		return true;
	}

	const AActor* surfaceActor = SurfaceMOI ? SurfaceMOI->GetActor() : nullptr;
	const UModumateDocument* doc = SurfaceMOI ? SurfaceMOI->GetDocument() : nullptr;
	UWorld* world = surfaceActor ? surfaceActor->GetWorld() : nullptr;

	// Without a Player Controller (most likely on a server), the object might as well be hidden, and if so that's a successful result that shouldn't cause deletion.
	AEditModelPlayerController* playerController = world ? world->GetFirstPlayerController<AEditModelPlayerController>() : nullptr;
	if (playerController == nullptr)
	{
		return true;
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
	bool bConnectedToVisiblePolygon = false;
	bool bConnectedToVisibleChild = false;
	bool bHasChildren = (SurfaceMOI->GetChildIDs().Num() > 0);

	EToolCategories curToolCategory = UModumateTypeStatics::GetToolCategory(playerController->GetToolMode());
	bool bInCompatibleToolCategory =
		(curToolCategory == EToolCategories::SurfaceGraphs) ||
		(curToolCategory == EToolCategories::Attachments) ||
		(curToolCategory == EToolCategories::SiteTools);

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
			bool bEdgeConnectedToAnyPolygon, bEdgeConnectedToVisiblePolygon, bEdgeConnectedToVisibleChild;
			if (!GetEdgePolyConnections(surfaceGraph->FindEdge(connectedEdgeID), doc,
				bEdgeConnectedToAnyPolygon, bEdgeConnectedToVisiblePolygon, bEdgeConnectedToVisibleChild))
			{
				return false;
			}

			bConnectedToAnyPolygon |= bEdgeConnectedToAnyPolygon;
			bConnectedToVisiblePolygon |= bEdgeConnectedToVisiblePolygon;
			bConnectedToVisibleChild |= bEdgeConnectedToVisibleChild;
		}

		bOutVisible =
			(bSurfaceViewMode || (bHybridViewMode && !bHasChildren)) &&
			(!bConnectedToAnyPolygon || bConnectedToVisiblePolygon);
		bOutCollisionEnabled = bOutVisible || (bInCompatibleToolCategory && bConnectedToVisibleChild);
		break;
	}
	case EObjectType::OTSurfaceEdge:
	{
		auto* surfaceEdge = surfaceGraph->FindEdge(objID);

		if (!GetEdgePolyConnections(surfaceEdge, doc, bConnectedToAnyPolygon, bConnectedToVisiblePolygon, bConnectedToVisibleChild))
		{
			return false;
		}

		bOutVisible =
			(bSurfaceViewMode || (bHybridViewMode && !bHasChildren)) &&
			(!bConnectedToAnyPolygon || bConnectedToVisiblePolygon);
		bOutCollisionEnabled = bOutVisible || (bInCompatibleToolCategory && bConnectedToVisibleChild);
		break;
	}
	case EObjectType::OTSurfacePolygon:
	{
		bOutVisible = !SurfaceMOI->IsRequestedHidden() && (bSurfaceViewMode || (bHybridViewMode && !bHasChildren));
		bOutCollisionEnabled = !SurfaceMOI->IsCollisionRequestedDisabled() && (bOutVisible || bInCompatibleToolCategory);
		break;
	}

	case EObjectType::OTTerrainVertex:
	case EObjectType::OTTerrainEdge:
	case EObjectType::OTTerrainPolygon:
	{
		GetTerrainSurfaceObjectEnabledFlags(SurfaceMOI, bOutVisible, bOutCollisionEnabled);
		break;
	}

	default:
		break;
	}

	return true;
}

bool UModumateObjectStatics::IsMetaEdgeObjCollidable(const AModumateObjectInstance* MetaMOI)
{
	// For whatever reason if world and playerController not found, default to true
	AEditModelPlayerController* playerController = MetaMOI && MetaMOI->GetWorld() ? MetaMOI->GetWorld()->GetFirstPlayerController<AEditModelPlayerController>() : nullptr;
	if (playerController == nullptr)
	{
		return true;
	}
	const UModumateDocument* doc = MetaMOI->GetDocument();
	// MetaView is always compatible. MetaObj collision is only compatible in SeparatorView when its span hosting obj is not hidden
	EToolCategories curToolCategory = UModumateTypeStatics::GetToolCategory(playerController->GetToolMode());
	if (curToolCategory == EToolCategories::MetaGraph)
	{
		return true;
	}
	else if (curToolCategory == EToolCategories::Separators)
	{
		TArray<int32> spanIDs;
		GetSpansForEdgeObject(doc, MetaMOI, spanIDs);
		if (spanIDs.Num() > 0)
		{
			const auto* spanMOI = doc->GetObjectById(spanIDs[0]);
			const auto* hostedChild = (spanMOI && spanMOI->GetChildIDs().Num() > 0) ? doc->GetObjectById(spanMOI->GetChildIDs()[0]) : nullptr;
			if (hostedChild)
			{
				return !hostedChild->IsCollisionRequestedDisabled();
			}
		}
		else // If not EdgeSpan, check if it is connected to any collidable faces
		{
			const FGraph3D& volumeGraph = *doc->FindVolumeGraph(MetaMOI->ID);
			if (!volumeGraph.ContainsObject(MetaMOI->ID))
			{
				return true;
			}
			auto* graphEdge = volumeGraph.FindEdge(MetaMOI->ID);
			for (const auto& edgeFaceConn : graphEdge->ConnectedFaces)
			{
				const auto* connectedFaceMOI = doc->GetObjectById(FMath::Abs(edgeFaceConn.FaceID));
				if (connectedFaceMOI && (connectedFaceMOI->GetObjectType() == EObjectType::OTMetaPlane) && !connectedFaceMOI->IsCollisionEnabled())
				{
					return false;
				}
			}
			return true;
		}
	}
	return false;
}

bool UModumateObjectStatics::IsMetaPlaneObjCollidable(const AModumateObjectInstance* MetaMOI)
{
	// For whatever reason if world and playerController not found, default to true
	AEditModelPlayerController* playerController = MetaMOI && MetaMOI->GetWorld() ? MetaMOI->GetWorld()->GetFirstPlayerController<AEditModelPlayerController>() : nullptr;
	if (playerController == nullptr)
	{
		return true;
	}
	const UModumateDocument* doc = MetaMOI->GetDocument();
	// MetaView is always compatible. MetaObj collision is only compatible in SeparatorView when its span hosting obj is not hidden
	EToolCategories curToolCategory = UModumateTypeStatics::GetToolCategory(playerController->GetToolMode());
	if (curToolCategory == EToolCategories::MetaGraph)
	{
		return true;
	}
	else if (curToolCategory == EToolCategories::Separators)
	{
		TArray<int32> spanIDs;
		GetSpansForFaceObject(doc, MetaMOI, spanIDs);
		if (spanIDs.Num() > 0)
		{
			const auto* spanMOI = doc->GetObjectById(spanIDs[0]);
			const auto* hostedChild = (spanMOI && spanMOI->GetChildIDs().Num() > 0) ? doc->GetObjectById(spanMOI->GetChildIDs()[0]) : nullptr;
			if (hostedChild)
			{
				return !hostedChild->IsCollisionRequestedDisabled();
			}
		}
		else
		{
			return true;
		}
	}
	return false;
}

void UModumateObjectStatics::GetGraphIDsFromMOIs(const TSet<AModumateObjectInstance *> &MOIs, TSet<int32> &OutGraphObjIDs)
{
	OutGraphObjIDs.Reset();

	for (AModumateObjectInstance *obj : MOIs)
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

bool UModumateObjectStatics::GetPlaneHostedValues(const AModumateObjectInstance* PlaneHostedObj, float &OutThickness, float &OutStartOffset, FVector &OutNormal)
{
	OutThickness = 0.0f;
	OutStartOffset = 0.0f;
	OutNormal = PlaneHostedObj->GetNormal();

	auto layeredObj = PlaneHostedObj->GetLayeredInterface();
	if (layeredObj == nullptr)
	{
		return false;
	}

	OutThickness = layeredObj->GetCachedLayerDims().TotalUnfinishedWidth;
	float offsetPCT = 0.0f;

	FMOIPlaneHostedObjData planeHostedObjData;
	FMOIPortalData portalData;
	if (PlaneHostedObj->GetStateData().CustomData.LoadStructData(planeHostedObjData))
	{
		OutStartOffset = (-0.5f * OutThickness) + planeHostedObjData.Offset.GetOffsetDistance(planeHostedObjData.FlipSigns.Y, OutThickness);
	}
	else if (PlaneHostedObj->GetStateData().CustomData.LoadStructData(portalData))
	{
		OutStartOffset = (-0.5f * OutThickness) + portalData.Offset.GetOffsetDistance(portalData.bNormalInverted ? -1.0f : 1.0f, OutThickness);
	}
	else
	{
		OutStartOffset = 0.0f;
	}

	return true;
}

bool UModumateObjectStatics::GetExtrusionDeltas(const AModumateObjectInstance *PlaneHostedObj, FVector &OutStartDelta, FVector &OutEndDelta)
{
	float thickness, startOffset;
	FVector normal;
	if (!GetPlaneHostedValues(PlaneHostedObj, thickness, startOffset, normal))
	{
		OutStartDelta = FVector::ZeroVector;
		OutEndDelta = FVector::ZeroVector;
		return false;
	}

	OutStartDelta = startOffset * normal;
	OutEndDelta = (startOffset + thickness) * normal;
	return true;
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
		FVector hitNormal = SnappedCursor.HitNormal;
		if (Assembly->bZalign)
		{
			hitNormal = Assembly->Normal;
		}

		return GetMountedTransform(SnappedCursor.WorldPosition, hitNormal,
			Assembly->Normal, Assembly->Tangent, OutTransform);
	}

	return false;
}

bool UModumateObjectStatics::GetFFEBoxSidePoints(const AActor *Actor, const FVector &AssemblyNormal, TArray<FVector> &OutPoints)
{
	OutPoints.Reset();

	if ((Actor == nullptr) || !AssemblyNormal.IsNormalized())
	{
		return false;
	}

	// Copied from AMOIObject::GetStructuralPointsAndLines, so that we can supply
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

bool UModumateObjectStatics::GetExtrusionProfilePoints(const FBIMAssemblySpec& Assembly, const FDimensionOffset& OffsetX, const FDimensionOffset& OffsetY,
	const FVector2D& FlipSigns, TArray<FVector2D>& OutProfilePoints, FBox2D& OutProfileExtents)
{
	OutProfilePoints.Reset();
	OutProfileExtents.Init();
	const FSimplePolygon* polyProfile = nullptr;

	if (!ensure((FMath::Abs(FlipSigns.X) == 1.0f) && (FMath::Abs(FlipSigns.Y) == 1.0f)))
	{
		return false;
	}

	FVector2D profileScale = FlipSigns;
	if (ensureAlways(Assembly.Extrusions.Num() > 0))
	{
		profileScale *= FVector2D(Assembly.Extrusions[0].Scale);
	}
	else
	{
		return false;
	}

	if (!UModumateObjectStatics::GetPolygonProfile(&Assembly, polyProfile) || (polyProfile->Points.Num() == 0))
	{
		return false;
	}

	FVector2D profileCenter = polyProfile->Extents.GetCenter();
	FVector2D profileSize = polyProfile->Extents.GetSize() * profileScale.GetAbs();
	FVector2D offsetDists(
		OffsetX.GetOffsetDistance(FlipSigns.X, profileSize.X),
		OffsetY.GetOffsetDistance(FlipSigns.Y, profileSize.Y)
	);

	for (const FVector2D& point : polyProfile->Points)
	{
		FVector2D fixedPoint2D = ((point - profileCenter) * profileScale) + offsetDists;
		OutProfilePoints.Add(fixedPoint2D);
		OutProfileExtents += fixedPoint2D;
	}

	return true;
}

bool UModumateObjectStatics::GetExtrusionObjectPoints(const FBIMAssemblySpec& Assembly, const FVector& LineNormal, const FVector& LineUp,
	const FDimensionOffset& OffsetX, const FDimensionOffset& OffsetY, const FVector2D& FlipSigns, TArray<FVector>& OutObjectPoints)
{
	OutObjectPoints.Reset();

	TArray<FVector2D> profilePoints;
	FBox2D profileExtents;
	if (!GetExtrusionProfilePoints(Assembly, OffsetX, OffsetY, FlipSigns, profilePoints, profileExtents))
	{
		return false;
	}

	for (const FVector2D& profilePoint : profilePoints)
	{
		OutObjectPoints.Add((profilePoint.X * LineNormal) + (profilePoint.Y * LineUp));
	}

	return true;
}

void UModumateObjectStatics::GetExtrusionCutPlaneDraftingLines(const TSharedPtr<FDraftingComposite>& ParentPage, const FPlane& Plane,
	const FVector& AxisX, const FVector& AxisY, const FVector& Origin, const FBox2D& BoundingBox, const TArray<FVector>& Perimeter,
	const FVector& StartPosition, const FVector& EndPosition, FModumateLayerType LayerType, float Epsilon /*= 0.0f*/)
{
	if (Perimeter.Num() == 0)
	{
		return;
	}

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
			points.Emplace(UModumateGeometryStatics::ProjectPoint2D(intersect, AxisX, AxisY, Origin));
		}
		if (FMath::SegmentPlaneIntersection(startCap, p1, Plane, intersect))
		{
			startCapPoints.Emplace(UModumateGeometryStatics::ProjectPoint2D(intersect, AxisX, AxisY, Origin));
		}
		if (FMath::SegmentPlaneIntersection(endCap, p2, Plane, intersect))
		{
			endCapPoints.Emplace(UModumateGeometryStatics::ProjectPoint2D(intersect, AxisX, AxisY, Origin));
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
				TSharedPtr<FDraftingLine> line = MakeShared<FDraftingLine>(
					FModumateUnitCoord2D::WorldCentimeters(boxClipped0),
					FModumateUnitCoord2D::WorldCentimeters(boxClipped1),
					ModumateUnitParams::FThickness::Points(0.75f), FMColor::Black);
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
			if (bSilhouette || (bFacing && ((normal | previousNormal) <= facetThreshold)))
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

void UModumateObjectStatics::GetTerrainSurfaceObjectEnabledFlags(const AModumateObjectInstance* TerrainSurfaceObj, bool& bOutVisible, bool& bOutCollisionEnabled)
{
	bOutVisible = !TerrainSurfaceObj->IsRequestedHidden();
	bOutCollisionEnabled = !TerrainSurfaceObj->IsCollisionRequestedDisabled();

	if (TerrainSurfaceObj->GetObjectType() != EObjectType::OTTerrainVertex &&
		TerrainSurfaceObj->GetObjectType() != EObjectType::OTTerrainEdge)
	{
		return;
	}

	const AMOITerrain* terrainMoi = Cast<AMOITerrain>(TerrainSurfaceObj->GetParentObject());
	if (!ensure(terrainMoi))
	{
		return;
	}

	// Check if surface object is part of any terrain interior polygon
	const auto graph2d = terrainMoi->GetDocument()->FindSurfaceGraph(terrainMoi->ID);
	const auto& polygons = graph2d->GetPolygons();
	for (const auto& polygonKvp : polygons)
	{
		// Terrain vertices and edges visibility follow the parent's vsibility, using one of the following conditions:
		// 1. Always follow visibility of parent polygon, regardless its status to parent perimeter 
		// 2. Only follow visibility of parent polygon if it's part of the completed perimeter
#if 1
		bOutVisible = terrainMoi->GetIsTranslucent() && !terrainMoi->IsRequestedHidden();
		bOutCollisionEnabled = bOutVisible; // Assume collision is always equal to its visibility?
		return;
#else
		if (polygonKvp.Value.bInterior)
		{
			const FGraph2DPolygon* polygonInterior = graph2d->FindPolygon(polygonKvp.Key);
			if (polygonInterior)
			{
				bool bPartOfInteriorPoly = TerrainSurfaceObj->GetObjectType() == EObjectType::OTTerrainVertex 
					&& polygonInterior->CachedPerimeterVertexIDs.Contains(TerrainSurfaceObj->ID);

				if (!bPartOfInteriorPoly && TerrainSurfaceObj->GetObjectType() == EObjectType::OTTerrainEdge)
				{
					bool bSameDir = false;
					bPartOfInteriorPoly = polygonInterior->FindPerimeterEdgeIndex(TerrainSurfaceObj->ID, bSameDir) != INDEX_NONE;
				}

				// If it belongs to a terrain interior polygon, check if it is affected by its translucency
				if (bPartOfInteriorPoly)
				{
					bOutVisible = terrainMoi->GetIsTranslucent() && !terrainMoi->IsRequestedHidden();
					bOutCollisionEnabled = bOutVisible; // Assume collision is always equal to its visibility?
					return;
				}
			}
		}
#endif
	}
}

bool UModumateObjectStatics::IsObjectInGroup(const UModumateDocument* Doc, const AModumateObjectInstance* Object, int32 GroupID /*= MOD_ID_NONE*/)
{
	if (GroupID == MOD_ID_NONE)
	{
		GroupID = Doc->GetActiveVolumeGraphID();
	}

	// Find eventual hosting graph3d element, or first element for spans, if any.
	while (Object && UModumateTypeStatics::Graph3DObjectTypeFromObjectType(Object->GetObjectType()) == EGraph3DObjectType::None
		&& !UModumateTypeStatics::IsSpanObject(Object))
	{
		Object = Object->GetParentObject();
	}
	if (!Object)
	{
		return true;
	}

	int32 graphId = Object->ID;
	if (UModumateTypeStatics::IsSpanObject(Object))
	{
		graphId = Object->GetObjectType() == EObjectType::OTMetaEdgeSpan ?
			Cast<AMOIMetaEdgeSpan>(Object)->InstanceData.GraphMembers[0] :
			Cast<AMOIMetaPlaneSpan>(Object)->InstanceData.GraphMembers[0];
	}

	const FGraph3D* volumeGraph = Doc->FindVolumeGraph(graphId);
	return volumeGraph && volumeGraph->GraphID == GroupID;
}

template<class T>
int32 GetGraphIDForSpanObjectT(const AModumateObjectInstance* Object)
{
	const T* castedOb = Cast<T>(Object);
	if (castedOb && castedOb->InstanceData.GraphMembers.Num() > 0)
	{
		auto* graph = Object->GetDocument()->FindVolumeGraph(castedOb->InstanceData.GraphMembers[0]);
		if (graph != nullptr)
		{
			return graph->GraphID;
		}
	}
	return INDEX_NONE;
}

int32 UModumateObjectStatics::GetGraphIDForSpanObject(const AModumateObjectInstance* Object)
{
	if (Object->GetObjectType() == EObjectType::OTMetaEdgeSpan)
	{
		return GetGraphIDForSpanObjectT<AMOIMetaEdgeSpan>(Object);
	}
	else if (Object->GetObjectType() == EObjectType::OTMetaPlaneSpan)
	{
		return GetGraphIDForSpanObjectT<AMOIMetaPlaneSpan>(Object);
	}

	return INDEX_NONE;
}

int32 UModumateObjectStatics::GetGroupIdForObject(const UModumateDocument* Doc, int32 MoiId)
{
	const AModumateObjectInstance* object = Doc->GetObjectById(MoiId);
	if (object && object->GetObjectType() == EObjectType::OTMetaGraph)
	{
		return object->ID;
	}

	while (object && UModumateTypeStatics::Graph3DObjectTypeFromObjectType(object->GetObjectType()) == EGraph3DObjectType::None
		&& !UModumateTypeStatics::IsSpanObject(object))
	{
		object = object->GetParentObject();
	}

	if (!object)
	{
		return MOD_ID_NONE;
	}

	int32 graphId = UModumateObjectStatics::GetGraphIDForSpanObject(object);
	if (graphId != INDEX_NONE)
	{
		return graphId;
	}

	const FGraph3D* volumeGraph = Doc->FindVolumeGraph(object->ID);
	return volumeGraph ? volumeGraph->GraphID : MOD_ID_NONE;
}

// Walk up group heirarchy looking for first non-zero Symbol GUID, if any.
bool UModumateObjectStatics::IsObjectInSymbol(const UModumateDocument* Doc, int32 MoiId, FGuid* OutSymbolId /*= nullptr*/,
	int32* OutGroupId /*= nullptr*/)
{
	const AModumateObjectInstance* object = Doc->GetObjectById(MoiId);
	if (object == nullptr)
	{
		return false;
	}
	if (object->GetObjectType() != EObjectType::OTMetaGraph)
	{
		MoiId = GetGroupIdForObject(Doc, MoiId);
		object = Doc->GetObjectById(MoiId);
		if (object == nullptr)
		{
			check(false);
			return false;
		}
	}

	do
	{
		if (object->GetStateData().AssemblyGUID.IsValid())
		{
			if (OutSymbolId)
			{
				*OutSymbolId = object->GetStateData().AssemblyGUID;
			}
			if (OutGroupId)
			{
				*OutGroupId = MoiId;
			}
			return true;
		}

		MoiId = object->GetParentID();
		object = Doc->GetObjectById(MoiId);
	} while (object != nullptr);

	return false;
}

bool UModumateObjectStatics::IsObjectInSubgroup(const UModumateDocument* Doc, const AModumateObjectInstance* Object, int32 ActiveGroup,
	int32& OutSubgroup, bool& bOutIsInGroup)
{
	if (ActiveGroup == MOD_ID_NONE)
	{
		ActiveGroup = Doc->GetActiveVolumeGraphID();
	}

	bOutIsInGroup = false;
	while (Object && UModumateTypeStatics::Graph3DObjectTypeFromObjectType(Object->GetObjectType()) == EGraph3DObjectType::None
		&& !UModumateTypeStatics::IsSpanObject(Object))
	{
		Object = Object->GetParentObject();
	}
	if (!Object)
	{
		bOutIsInGroup = true;
		return false;
	}

	int32 graphId = UModumateObjectStatics::GetGraphIDForSpanObject(Object); // INDEX_NONE for non-spans
	const FGraph3D* volumeGraph = graphId != INDEX_NONE ? Doc->GetVolumeGraph(graphId) : Doc->FindVolumeGraph(Object->ID);

	if (!volumeGraph)
	{
		return false;
	}

	if (volumeGraph->GraphID == ActiveGroup)
	{
		bOutIsInGroup = true;
		return false;
	}

	const AModumateObjectInstance* graphObject = Doc->GetObjectById(volumeGraph->GraphID);
	while (graphObject)
	{
		const auto* parentGraphObject = graphObject->GetParentObject();
		if (parentGraphObject && parentGraphObject->ID == ActiveGroup)
		{
			OutSubgroup = graphObject->ID;
			return true;
		}
		graphObject = parentGraphObject;
	}

	return false;
}

bool UModumateObjectStatics::GetGroupIdsForGroupChange(const UModumateDocument* Doc, int32 NewgroupID, TArray<int32>& OutAffectedGroups)
{
	int32 oldGroupID = Doc->GetActiveVolumeGraphID();
	OutAffectedGroups.Empty();
	NewgroupID = NewgroupID == MOD_ID_NONE ? Doc->GetRootVolumeGraphID() : NewgroupID;
	if (oldGroupID == NewgroupID)
	{
		return false;
	}

	bool bOldGroupIsDescendent = false;
	const AModumateObjectInstance* groupObject = Doc->GetObjectById(oldGroupID);

	if (!ensure(groupObject))
	{
		return false;
	}
	// Assignment within condition:
	while (bool(groupObject = groupObject->GetParentObject()) )
	{
		if (groupObject->ID == NewgroupID)
		{   // Old group is descendent of new group:
			return GetGroupIdsForGroupChangeHelper(Doc, NewgroupID, oldGroupID, OutAffectedGroups, bOldGroupIsDescendent);
		}
	}

	// New group is descendent of old group or are unrelated:
	if (!GetGroupIdsForGroupChangeHelper(Doc, oldGroupID, NewgroupID, OutAffectedGroups, bOldGroupIsDescendent))
	{
		return false;
	}
	if (!bOldGroupIsDescendent)
	{
		return GetGroupIdsForGroupChangeHelper(Doc, NewgroupID, MOD_ID_NONE, OutAffectedGroups, bOldGroupIsDescendent);
	}

	return true;
}

bool UModumateObjectStatics::GetGroupIdsForGroupChangeHelper(const UModumateDocument* Doc, int32 NewGroupID, int32 OldGroupID, TArray<int32>& OutAffectedGroups, bool& bOutFoundOldGroup)
{
	const AModumateObjectInstance* newGroup = Doc->GetObjectById(NewGroupID);
	if (!ensure(newGroup))
	{
		return false;
	}

	OutAffectedGroups.Push(NewGroupID);
	for (int32 id : newGroup->GetChildIDs())
	{
		if (id != OldGroupID)
		{
			if (!GetGroupIdsForGroupChangeHelper(Doc, id, OldGroupID, OutAffectedGroups, bOutFoundOldGroup))
			{
				return false;
			}
		}
		else
		{
			bOutFoundOldGroup = true;
		}
	}

	return true;
}

void UModumateObjectStatics::GetObjectsInGroups(UModumateDocument* Doc, const TArray<int32>& GroupIDs, TSet<AModumateObjectInstance*>& OutObjects,
	bool bSpansToo /*= false*/)
{
	for (int32 groupID: GroupIDs)
	{
		FGraph3D* graph3d =  Doc->GetVolumeGraph(groupID);
		if (ensure(graph3d))
		{
			auto& graphObjectIDs = graph3d->GetAllObjects();
			for (auto kvp: graphObjectIDs)
			{
				auto* metaMoi = Doc->GetObjectById(kvp.Key);
				if (ensure(metaMoi))
				{
					OutObjects.Add(metaMoi);
					OutObjects.Append(metaMoi->GetAllDescendents());

					TArray<int32> spans;
					UModumateObjectStatics::GetSpansForFaceObject(Doc, metaMoi, spans);
					UModumateObjectStatics::GetSpansForEdgeObject(Doc, metaMoi, spans);
					for (int32 spanId : spans)
					{
						auto* spanMoi = Doc->GetObjectById(spanId);
						if (spanMoi)
						{
							OutObjects.Append(spanMoi->GetAllDescendents());
							if (bSpansToo)
							{
								OutObjects.Add(spanMoi);
							}
						}
					}
				}

			}
		}
	}
}

void UModumateObjectStatics::GetObjectsInGroupRecursive(UModumateDocument* Doc, int32 GroupID, TSet<AModumateObjectInstance*>& OutObjects)
{
	const AModumateObjectInstance* groupObject = Doc->GetObjectById(GroupID);
	if (ensure(groupObject) && ensure(groupObject->GetObjectType() == EObjectType::OTMetaGraph))
	{
		auto groups = groupObject->GetAllDescendents();
		TArray<int32> groupIDs;
		Algo::Transform(groups, groupIDs, [](const AModumateObjectInstance* Object) { return Object->ID; });
		groupIDs.Add(GroupID);
		GetObjectsInGroups(Doc, groupIDs, OutObjects);
	}
}

void UModumateObjectStatics::GetDesignOptionsForGroup(UModumateDocument* Doc, int32 GroupID, TArray<int32>& OutDesignOptionIDs)
{
	TArray<AMOIDesignOption*> designOptions;
	Doc->GetObjectsOfTypeCasted<AMOIDesignOption>(EObjectType::OTDesignOption, designOptions);

	for (auto* moi : designOptions)
	{
		AMOIDesignOption* doMOI = Cast<AMOIDesignOption>(moi);
		if (ensure(doMOI) && doMOI->InstanceData.groups.Contains(GroupID))
		{
			OutDesignOptionIDs.AddUnique(doMOI->ID);
		}
	}
}

void UModumateObjectStatics::UpdateDesignOptionVisibility(UModumateDocument* Doc)
{
#if UE_SERVER
	return;
#endif
	TArray<AMOIDesignOption*> designOptions;
	Algo::Transform(
		Doc->GetObjectsOfType(EObjectType::OTDesignOption), 
		designOptions,
		[](AModumateObjectInstance* MOI) {return Cast<AMOIDesignOption>(MOI); 
	});

	auto groupVisible = [designOptions](int32 GroupID) {
		for (auto* designOption : designOptions)
		{
			if (ensure(designOption) && designOption->InstanceData.isShowing && designOption->InstanceData.groups.Contains(GroupID))
			{
				return true;
			}
		}
		return false;
	};

	AEditModelPlayerController* controller = Cast<AEditModelPlayerController>(Doc->GetWorld()->GetFirstPlayerController());
	AEditModelPlayerState* playerState = controller ? controller->EMPlayerState : nullptr;

	TSet<int32> visibleGroups;
	TSet<int32> hiddenGroups;
	for (auto& designOption : designOptions)
	{
		for (auto groupID : designOption->InstanceData.groups)
		{
			if (groupVisible(groupID))
			{
				visibleGroups.Add(groupID);
			}
			else
			{
				hiddenGroups.Add(groupID);
			}
		}
	}

	TSet<AModumateObjectInstance*> visibleObs;
	UModumateObjectStatics::GetObjectsInGroups(Doc, visibleGroups.Array(), visibleObs);
	if (visibleObs.Num() > 0)
	{
		TArray<int32> visibleObIDs;
		Algo::Transform(visibleObs, visibleObIDs, [](const AModumateObjectInstance* MOI) {return MOI->ID; });
		playerState->UnhideObjectsById(visibleObIDs, false);
	}

	TSet<AModumateObjectInstance*> hiddenObs;
	UModumateObjectStatics::GetObjectsInGroups(Doc, hiddenGroups.Array(), hiddenObs);
	if (hiddenObs.Num() > 0)
	{
		TArray<int32> hiddenObIDs;
		Algo::Transform(hiddenObs, hiddenObIDs, [](const AModumateObjectInstance* MOI) {return MOI->ID; });
		playerState->AddHideObjectsById(hiddenObIDs, false);
	}
}

TSet<int32> UModumateObjectStatics::GetAllVisibleGroupsViaDesignOptions(const UModumateDocument* Doc)
{
	TArray<const AMOIMetaGraph*> allGroupMois;
	Doc->GetObjectsOfTypeCasted(EObjectType::OTMetaGraph, allGroupMois);
	TSet<int32> allGroups;
	Algo::Transform(allGroupMois, allGroups, [](const AMOIMetaGraph* MOI) { return MOI->ID; });

	TArray<const AMOIDesignOption*> allOptionMois;
	Doc->GetObjectsOfTypeCasted(EObjectType::OTDesignOption, allOptionMois);
	if (allOptionMois.Num() == 0)
	{
		return allGroups;
	}

	TSet<int32> groupsInOptions, groupsEnabledInOptions;
	for (const auto* option : allOptionMois)
	{
		groupsInOptions.Append(option->InstanceData.groups);
		if (option->InstanceData.isShowing)
		{
			groupsEnabledInOptions.Append(option->InstanceData.groups);
		}
	}

	return allGroups.Difference(groupsInOptions.Difference(groupsEnabledInOptions));
}

TSet<int32> UModumateObjectStatics::GetAllVisibleGroupsViaDesignOptions(const UModumateDocument* Doc, const TArray<int32>& SelectedOptions)
{
	TArray<const AMOIMetaGraph*> allGroupMois;
	Doc->GetObjectsOfTypeCasted(EObjectType::OTMetaGraph, allGroupMois);
	TSet<int32> allGroups;
	Algo::Transform(allGroupMois, allGroups, [](const AMOIMetaGraph* MOI) { return MOI->ID; });

	if (SelectedOptions.Num() == 0)
	{
		return allGroups;
	}

	TArray<const AMOIDesignOption*> allOptionMois;
	Doc->GetObjectsOfTypeCasted(EObjectType::OTDesignOption, allOptionMois);
	TSet<int32> groupsInOptions, groupsEnabledInOptions;
	for (const auto* option : allOptionMois)
	{
		groupsInOptions.Append(option->InstanceData.groups);
		if (SelectedOptions.Contains(option->ID))
		{
			groupsEnabledInOptions.Append(option->InstanceData.groups);
		}
	}

	return allGroups.Difference(groupsInOptions.Difference(groupsEnabledInOptions));
}

void UModumateObjectStatics::HideObjectsInGroups(UModumateDocument* Doc, const TArray<int32>& GroupIDs, bool bHide)
{
	TSet<AModumateObjectInstance*> groupObjects;
	for (int32 groupID : GroupIDs)
	{   // Hide entire graph
		FGraph3D* graph = Doc->GetVolumeGraph(groupID);
		if (ensure(graph))
		{
			for (const auto& kvp : graph->GetAllObjects())
			{
				groupObjects.Add(Doc->GetObjectById(kvp.Key));
			}
		}
	}

	UModumateObjectStatics::GetObjectsInGroups(Doc, GroupIDs, groupObjects);
	UModumateFunctionLibrary::SetMOIAndDescendentsHidden(groupObjects.Array(), bHide);
}

void UModumateObjectStatics::GetWebMOIArrayForObjects(const TArray<const AModumateObjectInstance*>& Objects, FString& OutJson)
{
	TArray<FWebMOI> webMois;
	GetWebMOIArrayForObjects(Objects, webMois);

	if (webMois.Num() == 0)
	{
		OutJson = TEXT("[]");
		return;
	}

	OutJson = TEXT("[");

	FString jsonMoi;
	WriteJsonGeneric(jsonMoi, &webMois[0]);
	OutJson += jsonMoi;

	for (int32 i = 1; i < webMois.Num(); ++i)
	{
		jsonMoi.Empty();
		if (WriteJsonGeneric(jsonMoi, &webMois[i]))
		{
			OutJson += TEXT(",");
			OutJson += jsonMoi;
		}
	}
	OutJson += TEXT("]");
}

void UModumateObjectStatics::GetWebMOIArrayForObjects(const TArray<const AModumateObjectInstance*>& Objects, TArray<FWebMOI>& OutMOIs)
{
	for (auto& ob : Objects)
	{
		FWebMOI& webMoi = OutMOIs.AddDefaulted_GetRef();
		ob->ToWebMOI(webMoi);
	}
}



void UModumateObjectStatics::GetSpansForFaceObject(const UModumateDocument* Doc, const AModumateObjectInstance* FaceObject, TArray<int32>& OutSpans)
{
	if (FaceObject != nullptr && FaceObject->GetObjectType() == EObjectType::OTMetaPlane)
	{
		Doc->GetSpansForGraphElement(FaceObject->ID, OutSpans);
	}
}

void UModumateObjectStatics::GetSpansForEdgeObject(const UModumateDocument* Doc, const AModumateObjectInstance* EdgeObject, TArray<int32>& OutSpans)
{
	if (EdgeObject != nullptr && EdgeObject->GetObjectType() == EObjectType::OTMetaEdge)
	{
		Doc->GetSpansForGraphElement(EdgeObject->ID, OutSpans);
	}
}

const FGraph3DFace* UModumateObjectStatics::GetFaceFromSpanObject(const UModumateDocument* Doc, int32 SpanID)
{
	const AMOIMetaPlaneSpan* span = Cast<AMOIMetaPlaneSpan>(Doc->GetObjectById(SpanID));

	if (span == nullptr)
	{
		return nullptr;
	}

	return span->GetPerimeterFace();
}

bool UModumateObjectStatics::TryJoinSelectedMetaSpan(UWorld* World)
{
#if !UE_SERVER
	// TODO: Check for valid span
	AEditModelPlayerController* controller = Cast<AEditModelPlayerController>(World->GetFirstPlayerController());
	AEditModelPlayerState* playerState = controller ? controller->EMPlayerState : nullptr;
	if (!playerState)
	{
		return false;
	}

	TArray<int32> spanIDs;
	Algo::TransformIf(playerState->SelectedObjects, spanIDs,
		[](const AModumateObjectInstance* MOI) {return MOI->GetLayeredInterface() != nullptr || MOI->GetObjectType() == EObjectType::OTFaceHosted; },
		[](const AModumateObjectInstance* MOI) {return MOI->GetParentID(); }
	);

	auto markAllSpansDirty = [controller](const TArray<int32>& checkSpanIDs)
	{
		if (controller && controller->GetDocument())
		{
			for (int32 spanID : checkSpanIDs)
			{
				AModumateObjectInstance* moi = controller->GetDocument()->GetObjectById(spanID);
				if (moi != nullptr)
				{
					moi->MarkDirty(EObjectDirtyFlags::Structure);
				}
			}
		}
	};

	TArray<FDeltaPtr> deltas;
	if (spanIDs.Num() > 0)
	{
		TArray<const FGraph3DFace*> memberFaces;
		for (const auto curSpanID : spanIDs)
		{
			const auto curObj = controller->GetDocument()->GetObjectById(curSpanID);
			const auto curSpanMOI = Cast<AMOIMetaPlaneSpan>(curObj);
			const FGraph3D* graph = controller->GetDocument()->FindVolumeGraph(curSpanMOI->InstanceData.GraphMembers[0]);
			Algo::Transform(curSpanMOI->InstanceData.GraphMembers, memberFaces,
				[graph](int32 FaceID) {return graph->FindFace(FaceID); });
		}
		if (memberFaces.Num() >= 2)
		{
			FPlane basePlane = memberFaces[0]->CachedPlane;
			TArray<int32> pendingFaceReverseIDs;
			for (int32 i = 1; i < memberFaces.Num(); ++i)
			{
				// If current face is reverse dir of base face, reverse it
				if (basePlane.Equals(memberFaces[i]->CachedPlane * -1, THRESH_POINTS_ARE_NEAR))
				{
					pendingFaceReverseIDs.Add(memberFaces[i]->ID);
				}
			}
			if (pendingFaceReverseIDs.Num() > 0)
			{
				TArray<int32> pendingEdgeReverseIDs; //Currently no edges check
				controller->GetDocument()->GetDeltaForReverseMetaObjects(World, pendingEdgeReverseIDs, pendingFaceReverseIDs, deltas);
			}
		}
		markAllSpansDirty(spanIDs);
		FModumateObjectDeltaStatics::GetDeltasForFaceSpanMerge(controller->GetDocument(), spanIDs, deltas);
	}
	else
	{
		Algo::TransformIf(playerState->SelectedObjects, spanIDs,
			[](const AModumateObjectInstance* MOI)
			{
				return MOI->GetObjectType() == EObjectType::OTStructureLine || MOI->GetObjectType() == EObjectType::OTEdgeHosted;
			},
			[](const AModumateObjectInstance* MOI) {return MOI->GetParentID(); }
			);

		if (spanIDs.Num() > 0)
		{
			markAllSpansDirty(spanIDs);
			FModumateObjectDeltaStatics::GetDeltasForEdgeSpanMerge(controller->GetDocument(), spanIDs, deltas);
		}
	}
	return controller->GetDocument()->ApplyDeltas(deltas, World, false);
#endif
	return false;
}

void UModumateObjectStatics::SeparateSelectedMetaSpan(UWorld* World)
{
	AEditModelPlayerController* controller = Cast<AEditModelPlayerController>(World->GetFirstPlayerController());
	AEditModelPlayerState* playerState = controller ? controller->EMPlayerState : nullptr;
	if (!playerState)
	{
		return;
	}

	UModumateDocument* document = controller->GetDocument();
	TSet<int32> newSelectionItems;
	TArray<int32> allSelectedSpans;
	int32 newObjID = document->GetNextAvailableID();

	AMOIMetaPlaneSpan* targetSpan = nullptr;
	for (auto curObj : playerState->SelectedObjects)
	{
		if (curObj->GetParentObject())
		{
			if (curObj->GetParentObject()->GetObjectType() == EObjectType::OTMetaEdgeSpan ||
				curObj->GetParentObject()->GetObjectType() == EObjectType::OTMetaPlaneSpan)
			{
				allSelectedSpans.Add(curObj->GetParentObject()->ID);
			}
		}
	}

	TArray<FDeltaPtr> deltas;
	FModumateObjectDeltaStatics::GetDeltasForSpanSplit(document, allSelectedSpans, deltas, &newSelectionItems);
	document->ApplyDeltas(deltas , World);

	TSet<AModumateObjectInstance*> selectedObjects;
	for (int32 id : newSelectionItems)
	{
		AModumateObjectInstance* moi = document->GetObjectById(id);
		if (moi && !UModumateTypeStatics::IsSpanObject(moi))
		{
			selectedObjects.Add(moi);
		}
	}

	if (selectedObjects.Num() > 0)
	{	// Add to selection, so original selected hosted object remains selected.
		controller->SetObjectsSelected(selectedObjects, true, false);
	}
}

bool UModumateObjectStatics::GetHostingMOIsForMOI(UModumateDocument* Doc, AModumateObjectInstance* Moi, TArray<AModumateObjectInstance*>& OutMOIs)
{
	while (Moi && UModumateTypeStatics::Graph3DObjectTypeFromObjectType(Moi->GetObjectType()) == EGraph3DObjectType::None
		&& !UModumateTypeStatics::IsSpanObject(Moi))
	{
		Moi = Moi->GetParentObject();
	}
	if (!Moi)
	{
		return false;
	}

	if (UModumateTypeStatics::IsSpanObject(Moi))
	{
		const TArray<int32>& graphIds = Moi->GetObjectType() == EObjectType::OTMetaEdgeSpan ?
			Cast<AMOIMetaEdgeSpan>(Moi)->InstanceData.GraphMembers :
			Cast<AMOIMetaPlaneSpan>(Moi)->InstanceData.GraphMembers;
		for (int32 graphId : graphIds)
		{
			auto graphMoi = Doc->GetObjectById(graphId);
			if (graphMoi)
			{
				OutMOIs.Add(graphMoi);
			}
		}
	}
	else
	{
		OutMOIs.Add(Moi);
	}

	return OutMOIs.Num() > 0;
}

void UModumateObjectStatics::GetAllDescendents(const AModumateObjectInstance* Moi, TArray<const AModumateObjectInstance*>& OutMOIs)
{
	if (Moi == nullptr)
	{
		return;
	}
	const UModumateDocument* doc = Moi->GetDocument();
	TArray<const AModumateObjectInstance*> allMOIs(Moi->GetAllDescendents());
	TArray<int32> spandIDs;
	GetSpansForFaceObject(doc, Moi, spandIDs);
	GetSpansForEdgeObject(doc, Moi, spandIDs);
	TArray<const AModumateObjectInstance*> spanObjects;
	for (int32 spanID : spandIDs)
	{
		const AModumateObjectInstance* spanObject = doc->GetObjectById(spanID);
		allMOIs.Add(spanObject);
		allMOIs.Append(spanObject->GetAllDescendents());
	}
	OutMOIs.Append(MoveTemp(allMOIs));
}

bool UModumateTypeStatics::IsSpanObject(const AModumateObjectInstance* Object)
{
	return Object && (Object->GetObjectType() == EObjectType::OTMetaEdgeSpan
		|| Object->GetObjectType() == EObjectType::OTMetaPlaneSpan);
}

TArray<EObjectType> UModumateObjectStatics::CompatibleObjectTypes;
bool UModumateObjectStatics::bCompatibleObjectsInitialized = false;

bool UModumateObjectStatics::IsValidParentObjectType(EObjectType ParentObjectType)
{
	if (!bCompatibleObjectsInitialized)
	{
		CompatibleObjectTypes.Add(EObjectType::OTWallSegment);
		CompatibleObjectTypes.Add(EObjectType::OTFloorSegment);
		CompatibleObjectTypes.Add(EObjectType::OTRoofFace);
		CompatibleObjectTypes.Add(EObjectType::OTCeiling);
		CompatibleObjectTypes.Add(EObjectType::OTCountertop);
		CompatibleObjectTypes.Add(EObjectType::OTSystemPanel);
		CompatibleObjectTypes.Add(EObjectType::OTMetaPlane);
		CompatibleObjectTypes.Add(EObjectType::OTMetaPlaneSpan);
		bCompatibleObjectsInitialized = true;
	}
	
	return CompatibleObjectTypes.Contains(ParentObjectType);
}

bool UModumateObjectStatics::GetPlaneHostedZonePlane(const AMOIPlaneHostedObj* PlaneHostedObj, const FString& ZoneID, EZoneOrigin Origin, float Offset, FPlane& OutPlane)
{
	if (PlaneHostedObj == nullptr)
	{
		return false;
	}

	const AMOIMetaPlaneSpan* parentSpan = Cast<AMOIMetaPlaneSpan>(PlaneHostedObj->GetParentObject());
	if (parentSpan == nullptr)
	{
		return false;
	}

	float thickness = Offset;
	if (Origin != EZoneOrigin::MassingPlane)
	{
		const FBIMLayerSpec* layerSpec = nullptr;
		for (const auto& lp : PlaneHostedObj->GetAssembly().Layers)
		{
			if (lp.PresetZoneID == ZoneID)
			{
				layerSpec = &lp;
				break;
			}
			thickness += lp.ThicknessCentimeters;
		}

		if (ensure(layerSpec != nullptr))
		{
			switch (Origin)
			{
			case EZoneOrigin::Back: thickness += layerSpec->ThicknessCentimeters; break;
			case EZoneOrigin::Center: thickness += layerSpec->ThicknessCentimeters * 0.5f; break;
			};
		}
	}

	OutPlane = parentSpan->GetPerimeterFace()->CachedPlane;
	FVector outplaneOrigin = OutPlane.GetOrigin();

	outplaneOrigin -= OutPlane.GetNormal() * PlaneHostedObj->CalculateThickness() * 0.5f;
	outplaneOrigin += OutPlane.GetNormal() * thickness;

	OutPlane = FPlane(outplaneOrigin, OutPlane.GetNormal());

	return true;
}

void UModumateObjectStatics::GetMOIAlignmentTargets(const UModumateDocument* Doc, const AMOIMetaPlaneSpan* SpanMOI, TSet<int32>& OutTargets)
{
	OutTargets.Reset();
	if (SpanMOI == nullptr)
	{
		return;
	}

	const FGraph3DFace* spanFace = SpanMOI->GetPerimeterFace();
	const FGraph3D* graph3D = spanFace && spanFace->EdgeIDs.Num() > 0 ? Doc->FindVolumeGraph(spanFace->EdgeIDs[0]) : nullptr;

	if (!ensure(graph3D != nullptr))
	{
		return;
	}

	TArray<int32> spanMembers = SpanMOI->GetFaceSpanMembers();

	TSet<FGraphSignedID> allFaces;
	for (auto edge : spanFace->EdgeIDs)
	{
		const FGraph3DEdge* edgePtr = graph3D->GetEdges().Find(FMath::Abs(edge));
		if (edgePtr == nullptr)
		{
			continue;
		}
		for (auto faceCon : edgePtr->ConnectedFaces)
		{
			// negative indices indicate reverse winding
			int32 faceAbs = FMath::Abs(faceCon.FaceID);
			if (!spanMembers.Contains(faceAbs))
			{
				const FGraph3DFace* facePtr = graph3D->GetFaces().Find(faceAbs);
				if (ensure(facePtr != nullptr) && UModumateGeometryStatics::ArePlanesCoplanar(facePtr->CachedPlane, spanFace->CachedPlane))
				{
					allFaces.Add(faceAbs);
				}
			}
		}
	}

	for (auto face : allFaces)
	{
		TArray<const AModumateObjectInstance*> descendents;
		UModumateObjectStatics::GetAllDescendents(Doc->GetObjectById(face), descendents);
		for (auto* descendent : descendents)
		{
			if (Cast<AMOIPlaneHostedObj>(descendent) != nullptr)
			{
				OutTargets.Add(descendent->ID);
			}
		}
	}
}
