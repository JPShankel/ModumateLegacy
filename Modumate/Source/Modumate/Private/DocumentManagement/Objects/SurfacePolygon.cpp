// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "DocumentManagement/Objects/SurfacePolygon.h"

#include "DocumentManagement/ModumateDocument.h"
#include "Graph/Graph2D.h"
#include "ModumateCore/ModumateObjectStatics.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"

namespace Modumate
{
	FMOISurfacePolygonImpl::FMOISurfacePolygonImpl(FModumateObjectInstance *moi)
		: FMOIPlaneImplBase(moi)
		, MeshPointOffset(0.25f)
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

	bool FMOISurfacePolygonImpl::CleanObject(EObjectDirtyFlags DirtyFlag, TArray<TSharedPtr<FDelta>>* OutSideEffectDeltas)
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
				bInteriorPolygon, bInnerBoundsPolygon, CachedOrigin, CachedPoints, CachedHoles, MeshPointOffset))
			{
				return false;
			}

			// Skip exterior polygons and inner bounds polygons; they can't be visible anyway, so they shouldn't set up any dynamic meshes.
			if (!bInteriorPolygon || bInnerBoundsPolygon || (CachedPoints.Num() < 3))
			{
				return true;
			}

			AEditModelGameMode_CPP *gameMode = World.IsValid() ? World->GetAuthGameMode<AEditModelGameMode_CPP>() : nullptr;
			MaterialData.EngineMaterial = gameMode ? gameMode->MetaPlaneMaterial : nullptr;

			bool bEnableCollision = !MOI->GetIsInPreviewMode();
			DynamicMeshActor->SetupMetaPlaneGeometry(CachedPoints, MaterialData, GetAlpha(), true, &CachedHoles, bEnableCollision);
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
}
