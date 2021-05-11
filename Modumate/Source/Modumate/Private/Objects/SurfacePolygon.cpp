// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Objects/SurfacePolygon.h"

#include "DocumentManagement/ModumateDocument.h"
#include "Objects/SurfaceGraph.h"
#include "Graph/Graph2D.h"
#include "ModumateCore/ModumateObjectStatics.h"
#include "UnrealClasses/EditModelGameMode.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "UnrealClasses/ModumateGameInstance.h"

AMOISurfacePolygon::AMOISurfacePolygon()
	: AMOIPlaneBase()
	, bInteriorPolygon(false)
	, bInnerBoundsPolygon(false)
{

}

bool AMOISurfacePolygon::GetUpdatedVisuals(bool &bOutVisible, bool &bOutCollisionEnabled)
{
	if (!DynamicMeshActor.IsValid())
	{
		return false;
	}

	bool bPreviouslyVisible = IsVisible();

	if (!UModumateObjectStatics::GetSurfaceObjEnabledFlags(this, bOutVisible, bOutCollisionEnabled))
	{
		return false;
	}

	DynamicMeshActor->SetActorHiddenInGame(!bOutVisible);
	DynamicMeshActor->SetActorEnableCollision(bOutCollisionEnabled);

	if (bOutVisible)
	{
		AMOIPlaneBase::UpdateMaterial();
	}

	if (bPreviouslyVisible != bOutVisible)
	{
		MarkConnectedVisualsDirty();
	}

	return true;
}

bool AMOISurfacePolygon::CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas)
{
	switch (DirtyFlag)
	{
	case EObjectDirtyFlags::Structure:
	{
		if (!UModumateObjectStatics::GetGeometryFromSurfacePoly(GetDocument(), ID,
			bInteriorPolygon, bInnerBoundsPolygon, CachedTransform, CachedPoints, CachedHoles))
		{
			return false;
		}

		// Skip exterior polygons and inner bounds polygons; they can't be visible anyway, so they shouldn't set up any dynamic meshes.
		if (!bInteriorPolygon || bInnerBoundsPolygon || (CachedPoints.Num() < 3))
		{
			return true;
		}

		// Update cached geometry data required by AMOIPlaneBase parent class
		// TODO: refactor plane objects to use more of the same data (FTransform vs. axes & location)
		FQuat rotation = CachedTransform.GetRotation();
		CachedPlane = FPlane(CachedTransform.GetLocation(), rotation.GetAxisZ());
		CachedAxisX = rotation.GetAxisX();
		CachedAxisY = rotation.GetAxisY();
		CachedOrigin = CachedPoints[0];
		CachedCenter = CachedOrigin;

		auto* gameMode = GetWorld()->GetGameInstance<UModumateGameInstance>()->GetEditModelGameMode();
		MaterialData.EngineMaterial = gameMode ? gameMode->MetaPlaneMaterial : nullptr;

		// Offset the vertices used for the surface polygon away from the host, to prevent z-fighting
		FVector offsetDelta = CachedTransform.GetRotation().GetAxisZ() * AMOISurfaceGraph::VisualNormalOffset;
		CachedOffsetPoints = CachedPoints;
		CachedOffsetHoles = CachedHoles;
		for (FVector& offsetPoint : CachedOffsetPoints)
		{
			offsetPoint += offsetDelta;
		}
		for (auto& hole : CachedOffsetHoles)
		{
			for (FVector& offsetPoint : hole.Points)
			{
				offsetPoint += offsetDelta;
			}
		}

		// Now build the triangulated mesh with holes for the surface polygon
		bool bEnableCollision = !IsInPreviewMode();
		DynamicMeshActor->SetupMetaPlaneGeometry(CachedOffsetPoints, MaterialData, GetAlpha(), true, &CachedOffsetHoles, bEnableCollision);
	}
	case EObjectDirtyFlags::Visuals:
		return TryUpdateVisuals();
	default:
		break;
	}

	return true;
}

float AMOISurfacePolygon::GetAlpha() const
{
	return 1.0f;
}
