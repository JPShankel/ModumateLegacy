// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Objects/SurfacePolygon.h"

#include "DocumentManagement/ModumateDocument.h"
#include "Objects/SurfaceGraph.h"
#include "Graph/Graph2D.h"
#include "ModumateCore/ModumateObjectStatics.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"

FMOISurfacePolygonImpl::FMOISurfacePolygonImpl(FModumateObjectInstance *moi)
	: FMOIPlaneImplBase(moi)
	, bInteriorPolygon(false)
	, bInnerBoundsPolygon(false)
{

}

void FMOISurfacePolygonImpl::UpdateVisibilityAndCollision(bool &bOutVisible, bool &bOutCollisionEnabled)
{
	if (MOI && DynamicMeshActor.IsValid())
	{
		bOutVisible = false;
		bOutCollisionEnabled = false;

		if (bInteriorPolygon)
		{
			bool bHaveChildren = (MOI->GetChildIDs().Num() > 0);
			auto controller = MOI->GetWorld()->GetFirstPlayerController<AEditModelPlayerController_CPP>();
			switch (controller->EMPlayerState->GetSelectedViewMode())
			{
			case EEditViewModes::SurfaceGraphs:
				bOutVisible = true;
				bOutCollisionEnabled = true;
				break;
			case EEditViewModes::ObjectEditing:
				bOutVisible = !bHaveChildren;
				bOutCollisionEnabled = !bHaveChildren;
				break;
			default:
				break;
			}
		}

		DynamicMeshActor->SetActorHiddenInGame(!bOutVisible);
		DynamicMeshActor->SetActorEnableCollision(bOutCollisionEnabled);
	}
}

bool FMOISurfacePolygonImpl::CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas)
{
	if (!ensure(MOI))
	{
		return false;
	}

	switch (DirtyFlag)
	{
	case EObjectDirtyFlags::Structure:
	{
		if (!UModumateObjectStatics::GetGeometryFromSurfacePoly(MOI->GetDocument(), MOI->ID,
			bInteriorPolygon, bInnerBoundsPolygon, CachedTransform, CachedPoints, CachedHoles))
		{
			return false;
		}

		// Skip exterior polygons and inner bounds polygons; they can't be visible anyway, so they shouldn't set up any dynamic meshes.
		if (!bInteriorPolygon || bInnerBoundsPolygon || (CachedPoints.Num() < 3))
		{
			return true;
		}

		// Update cached geometry data required by FMOIPlaneImplBase parent class
		// TODO: refactor plane objects to use more of the same data (FTransform vs. axes & location)
		FQuat rotation = CachedTransform.GetRotation();
		CachedPlane = FPlane(CachedTransform.GetLocation(), rotation.GetAxisZ());
		CachedAxisX = rotation.GetAxisX();
		CachedAxisY = rotation.GetAxisY();
		CachedOrigin = CachedPoints[0];
		CachedCenter = CachedOrigin;

		AEditModelGameMode_CPP *gameMode = World.IsValid() ? World->GetAuthGameMode<AEditModelGameMode_CPP>() : nullptr;
		MaterialData.EngineMaterial = gameMode ? gameMode->MetaPlaneMaterial : nullptr;

		// Offset the vertices used for the surface polygon away from the host, to prevent z-fighting
		FVector offsetDelta = CachedTransform.GetRotation().GetAxisZ() * FMOISurfaceGraphImpl::VisualNormalOffset;
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
		bool bEnableCollision = !MOI->GetIsInPreviewMode();
		DynamicMeshActor->SetupMetaPlaneGeometry(CachedOffsetPoints, MaterialData, GetAlpha(), true, &CachedOffsetHoles, bEnableCollision);
	}
	case EObjectDirtyFlags::Visuals:
		MOI->UpdateVisibilityAndCollision();
		break;
	default:
		break;
	}

	return true;
}

float FMOISurfacePolygonImpl::GetAlpha() const
{
	return 1.0f;
}
