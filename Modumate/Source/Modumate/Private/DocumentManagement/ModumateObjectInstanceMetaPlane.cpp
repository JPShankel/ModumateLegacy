// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "ModumateObjectInstanceMetaPlane.h"

#include "EditModelGameMode_CPP.h"
#include "EditModelPlayerController_CPP.h"
#include "EditModelPlayerState_CPP.h"
#include "ModumateDocument.h"
#include "ModumateObjectStatics.h"

namespace Modumate
{
	FMOIMetaPlaneImpl::FMOIMetaPlaneImpl(FModumateObjectInstance *moi)
		: FMOIPlaneImplBase(moi)
	{

	}

	void FMOIMetaPlaneImpl::UpdateVisibilityAndCollision(bool &bOutVisible, bool &bOutCollisionEnabled)
	{
		if (MOI && DynamicMeshActor.IsValid())
		{
			bool bShouldBeVisible, bShouldCollisionBeEnabled, bConnectedToAnyPlane;
			UModumateObjectStatics::ShouldMetaObjBeEnabled(MOI, bShouldBeVisible, bShouldCollisionBeEnabled, bConnectedToAnyPlane);
			bOutVisible = !MOI->IsRequestedHidden() && bShouldBeVisible;
			bOutCollisionEnabled = !MOI->IsCollisionRequestedDisabled() && bShouldCollisionBeEnabled;
			DynamicMeshActor->SetActorHiddenInGame(!bOutVisible);
			DynamicMeshActor->SetActorEnableCollision(bOutCollisionEnabled);
		}
	}

	void FMOIMetaPlaneImpl::SetupDynamicGeometry()
	{
		bool bNewPlane = !GotGeometry;
		GotGeometry = true;

		const TArray<int32> &newChildIDs = MOI->GetChildIDs();
		bool bChildrenChanged = (newChildIDs != LastChildIDs);
		LastChildIDs = newChildIDs;

		UpdateCachedGraphData();

		AEditModelGameMode_CPP *gameMode = World.IsValid() ? World->GetAuthGameMode<AEditModelGameMode_CPP>() : nullptr;
		MaterialData.EngineMaterial = gameMode ? gameMode->MetaPlaneMaterial : nullptr;

		bool bEnableCollision = !MOI->GetIsInPreviewMode();
		DynamicMeshActor->SetupMetaPlaneGeometry(MOI->GetControlPoints(), MaterialData, GetAlpha(), true, bEnableCollision);

		MOI->UpdateVisibilityAndCollision();

		// If this plane was just created, or if its children changed, then make sure its neighbors
		// have the correct visuals and collision as well, since they might be dependent on this one.
		if (bNewPlane || bChildrenChanged)
		{
			UpdateConnectedVisuals();
		}
	}

	void FMOIMetaPlaneImpl::SetFromDataRecordAndRotation(const FMOIDataRecordV1 &dataRec, const FVector &origin, const FQuat &rotation)
	{
		if (ensure(dataRec.ControlPoints.Num() == MOI->GetControlPoints().Num()))
		{
			for (int32 i = 0; i < dataRec.ControlPoints.Num(); ++i)
			{
				MOI->SetControlPoint(i,origin + rotation.RotateVector(dataRec.ControlPoints[i] - origin));
			}

			const FGraph3DFace *graphFace = MOI ? MOI->GetDocument()->GetVolumeGraph().FindFace(MOI->ID) : nullptr;
			if (ensure(graphFace && (graphFace->CachedPositions.Num() > 0)))
			{
				FVector planeNormal = graphFace->CachedPlane;
				FVector planeBase = planeNormal * graphFace->CachedPlane.W;
				FVector originIntercept = FVector::PointPlaneProject(origin, planeBase, planeNormal);
				planeBase = origin + rotation.RotateVector(originIntercept - origin);

				CachedPlane = FPlane(planeBase, rotation.RotateVector(planeNormal).GetSafeNormal());
				CachedAxisX = rotation.RotateVector(graphFace->Cached2DX);
				CachedAxisY = rotation.RotateVector(graphFace->Cached2DY);
				CachedOrigin = origin + rotation.RotateVector(graphFace->CachedPositions[0] - origin);
				CachedCenter = origin + rotation.RotateVector(graphFace->CachedCenter - origin);
			}

			MOI->MarkDirty(EObjectDirtyFlags::Structure);
		}
	}

	void FMOIMetaPlaneImpl::SetFromDataRecordAndDisplacement(const FMOIDataRecordV1 &dataRec, const FVector &displacement)
	{
		if (ensure(dataRec.ControlPoints.Num() == MOI->GetControlPoints().Num()))
		{
			for (int32 i = 0; i < dataRec.ControlPoints.Num(); ++i)
			{
				MOI->SetControlPoint(i,dataRec.ControlPoints[i] + displacement);
			}

			const FGraph3DFace *graphFace = MOI ? MOI->GetDocument()->GetVolumeGraph().FindFace(MOI->ID) : nullptr;
			if (ensure(graphFace && (graphFace->CachedPositions.Num() > 0)))
			{
				FVector planeNormal = graphFace->CachedPlane;
				FVector planeBase = planeNormal * graphFace->CachedPlane.W;

				CachedPlane = FPlane(planeBase + displacement, planeNormal);
				CachedOrigin = graphFace->CachedPositions[0] + displacement;
				CachedCenter = graphFace->CachedCenter + displacement;
			}

			MOI->MarkDirty(EObjectDirtyFlags::Structure);
		}
	}

	void FMOIMetaPlaneImpl::UpdateDynamicGeometry()
	{
		if (!GotGeometry)
		{
			return;
		}

		UpdateCachedGraphData();

		// TODO: needs to take an argument for holes, once holes can be provided by faces
		DynamicMeshActor->SetupMetaPlaneGeometry(MOI->GetControlPoints(), MaterialData, GetAlpha(), false, true);
		auto children = MOI->GetChildObjects();

		for (auto *child : children)
		{
			child->UpdateGeometry();
		}
	}

	void FMOIMetaPlaneImpl::OnSelected(bool bNewSelected)
	{
		FDynamicModumateObjectInstanceImpl::OnSelected(bNewSelected);

		if (MOI && DynamicMeshActor.IsValid())
		{
			UpdateConnectedVisuals();
		}
	}

	void FMOIMetaPlaneImpl::UpdateConnectedVisuals()
	{
		if (MOI)
		{
			// Update the visuals of all of our connected edges
			MOI->GetConnectedMOIs(TempConnectedMOIs);
			for (FModumateObjectInstance *connectedMOI : TempConnectedMOIs)
			{
				if (connectedMOI)
				{
					connectedMOI->UpdateVisibilityAndCollision();
				}
			}
		}
	}

	void FMOIMetaPlaneImpl::UpdateCachedGraphData()
	{
		// Don't update from the graph directly if we're in preview mode
		// TODO: we shouldn't be calling this anyway in that situation
		if (MOI->GetIsInPreviewMode())
		{
			return;
		}

		const FGraph3DFace *graphFace = MOI ? MOI->GetDocument()->GetVolumeGraph().FindFace(MOI->ID) : nullptr;

		if (ensure(graphFace && (graphFace->CachedPositions.Num() > 0)))
		{
			CachedPlane = graphFace->CachedPlane;
			CachedAxisX = graphFace->Cached2DX;
			CachedAxisY = graphFace->Cached2DY;
			CachedOrigin = graphFace->CachedPositions[0];
			CachedCenter = graphFace->CachedCenter;
			// TODO: update holes
		}
	}

	float FMOIMetaPlaneImpl::GetAlpha() const
	{
		return 1.0f;
	}
}
