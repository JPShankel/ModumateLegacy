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
		, MeshPointOffset(0.5f)
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

		UpdateCachedGraphData();

		AEditModelGameMode_CPP *gameMode = World.IsValid() ? World->GetAuthGameMode<AEditModelGameMode_CPP>() : nullptr;
		MaterialData.EngineMaterial = gameMode ? gameMode->MetaPlaneMaterial : nullptr;

		FVector polyNormal(CachedPlane);
		CachedOffsetPoints.Reset();
		for (const FVector &cp : MOI->GetControlPoints())
		{
			CachedOffsetPoints.Add(cp + MeshPointOffset * polyNormal);
		}

		bool bEnableCollision = !MOI->GetIsInPreviewMode();
		DynamicMeshActor->SetupMetaPlaneGeometry(CachedOffsetPoints, MaterialData, GetAlpha(), true, &CachedHoles, bEnableCollision);

		MOI->MarkDirty(EObjectDirtyFlags::Visuals);
	}

	void FMOISurfacePolygonImpl::UpdateCachedGraphData()
	{
		// TODO: update CachedHoles, and potentially other members that need to be derived from the surface graph
		// For now, control points are already set and updated by object deltas, so they don't need to be updated here

		const FModumateObjectInstance *surfaceGraphObj = MOI ? MOI->GetParentObject() : nullptr;
		if (surfaceGraphObj == nullptr)
		{
			return;
		}

		CachedOrigin = surfaceGraphObj->GetObjectLocation();
		FQuat graphRot = surfaceGraphObj->GetObjectRotation();
		CachedAxisX = graphRot.GetAxisX();
		CachedAxisY = graphRot.GetAxisY();
		CachedPlane = FPlane(CachedOrigin, surfaceGraphObj->GetNormal());
	}

	float FMOISurfacePolygonImpl::GetAlpha() const
	{
		return 1.0f;
	}
}
