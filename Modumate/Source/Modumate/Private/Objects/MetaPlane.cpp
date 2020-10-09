// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Objects/MetaPlane.h"

#include "DocumentManagement/ModumateDocument.h"
#include "Graph/Graph3D.h"
#include "ModumateCore/ModumateObjectStatics.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"

FMOIMetaPlaneImpl::FMOIMetaPlaneImpl(FModumateObjectInstance *moi)
	: FMOIPlaneImplBase(moi)
{

}

void FMOIMetaPlaneImpl::PostCreateObject(bool bNewObject)
{
	UpdateConnectedVisuals();
}

void FMOIMetaPlaneImpl::UpdateVisibilityAndCollision(bool &bOutVisible, bool &bOutCollisionEnabled)
{
	FMOIPlaneImplBase::UpdateVisibilityAndCollision(bOutVisible, bOutCollisionEnabled);

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
	const TArray<int32> &newChildIDs = MOI->GetChildIDs();
	bool bChildrenChanged = (newChildIDs != LastChildIDs);
	LastChildIDs = newChildIDs;

	UpdateCachedGraphData();

	AEditModelGameMode_CPP *gameMode = World.IsValid() ? World->GetAuthGameMode<AEditModelGameMode_CPP>() : nullptr;
	MaterialData.EngineMaterial = gameMode ? gameMode->MetaPlaneMaterial : nullptr;

	bool bEnableCollision = !MOI->GetIsInPreviewMode();

	DynamicMeshActor->SetupMetaPlaneGeometry(CachedPoints, MaterialData, GetAlpha(), true, &CachedHoles, bEnableCollision);

	MOI->UpdateVisibilityAndCollision();

	// If this plane's children changed, then make sure its neighbors
	// have the correct visuals and collision as well, since they might be dependent on this one.
	if (bChildrenChanged)
	{
		UpdateConnectedVisuals();
	}
}

void FMOIMetaPlaneImpl::OnCursorHoverActor(AEditModelPlayerController_CPP* controller, bool EnableHover)
{
	FMOIPlaneImplBase::OnCursorHoverActor(controller, EnableHover);

	if (MOI && DynamicMeshActor.IsValid())
	{
		UpdateConnectedVisuals();
	}
}

void FMOIMetaPlaneImpl::OnSelected(bool bNewSelected)
{
	FModumateObjectInstanceImplBase::OnSelected(bNewSelected);

	if (MOI && DynamicMeshActor.IsValid())
	{
		UpdateConnectedVisuals();
	}
}

void FMOIMetaPlaneImpl::UpdateConnectedVisuals()
{
	if (MOI)
	{
		MOI->UpdateVisibilityAndCollision();
		// Update the visuals of all of our connected edges
		MOI->GetConnectedMOIs(TempConnectedMOIs);
		for (FModumateObjectInstance *connectedMOI : TempConnectedMOIs)
		{
			if ((connectedMOI->GetObjectType() == EObjectType::OTMetaEdge))
			{
				connectedMOI->MarkDirty(EObjectDirtyFlags::Visuals);
			}
		}
	}
}

void FMOIMetaPlaneImpl::UpdateCachedGraphData()
{
	const Modumate::FGraph3DFace *graphFace = MOI ? MOI->GetDocument()->GetVolumeGraph().FindFace(MOI->ID) : nullptr;

	if (ensure(graphFace && (graphFace->CachedPositions.Num() > 0)))
	{
		CachedPoints = graphFace->CachedPositions;
		CachedPlane = graphFace->CachedPlane;
		CachedAxisX = graphFace->Cached2DX;
		CachedAxisY = graphFace->Cached2DY;
		CachedOrigin = graphFace->CachedPositions[0];
		CachedCenter = graphFace->CachedCenter;
		CachedHoles = graphFace->CachedHoles;
	}
}

float FMOIMetaPlaneImpl::GetAlpha() const
{
	return 1.0f;
}

