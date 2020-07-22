// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "DocumentManagement/Objects/SurfacePolygon.h"

#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "DocumentManagement/ModumateDocument.h"
#include "ModumateCore/ModumateObjectStatics.h"

namespace Modumate
{
	FMOISurfacePolygonImpl::FMOISurfacePolygonImpl(FModumateObjectInstance *moi)
		: FMOIPlaneImplBase(moi)
	{

	}

	void FMOISurfacePolygonImpl::UpdateVisibilityAndCollision(bool &bOutVisible, bool &bOutCollisionEnabled)
	{
		if (MOI && DynamicMeshActor.IsValid())
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
				bOutVisible = false;
				bOutCollisionEnabled = false;
				break;
			}

			DynamicMeshActor->SetActorHiddenInGame(!bOutVisible);
			DynamicMeshActor->SetActorEnableCollision(bOutCollisionEnabled);
		}
	}

	void FMOISurfacePolygonImpl::SetupDynamicGeometry()
	{
		GotGeometry = true;

		AEditModelGameMode_CPP *gameMode = World.IsValid() ? World->GetAuthGameMode<AEditModelGameMode_CPP>() : nullptr;
		MaterialData.EngineMaterial = gameMode ? gameMode->MetaPlaneMaterial : nullptr;

		bool bEnableCollision = !MOI->GetIsInPreviewMode();
		DynamicMeshActor->SetupMetaPlaneGeometry(MOI->GetControlPoints(), MaterialData, GetAlpha(), true, &CachedHoles, bEnableCollision);

		MOI->MarkDirty(EObjectDirtyFlags::Visuals);
	}

	void FMOISurfacePolygonImpl::UpdateCachedGraphData()
	{
		// TODO: update CachedHoles, and potentially other members that need to be derived from the surface graph
		// For now, control points are already set and updated by object deltas, so they don't need to be updated here
	}

	float FMOISurfacePolygonImpl::GetAlpha() const
	{
		return 1.0f;
	}
}
