// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Objects/MetaPlane.h"

#include "DocumentManagement/ModumateDocument.h"
#include "Graph/Graph3D.h"
#include "ModumateCore/ModumateObjectStatics.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"

FMOIMetaPlaneImpl::FMOIMetaPlaneImpl()
	: FMOIPlaneImplBase()
{

}

void FMOIMetaPlaneImpl::GetUpdatedVisuals(bool &bOutVisible, bool &bOutCollisionEnabled)
{
	if (DynamicMeshActor.IsValid())
	{
		bool bPreviouslyVisible = IsVisible();

		UModumateObjectStatics::GetMetaObjEnabledFlags(this, bOutVisible, bOutCollisionEnabled);

		DynamicMeshActor->SetActorHiddenInGame(!bOutVisible);
		DynamicMeshActor->SetActorEnableCollision(bOutCollisionEnabled);

		if (bOutVisible)
		{
			FMOIPlaneImplBase::UpdateMaterial();
		}

		if (bPreviouslyVisible != bOutVisible)
		{
			UpdateConnectedVisuals();
		}
	}
}

void FMOIMetaPlaneImpl::SetupDynamicGeometry()
{
	const TArray<int32> &newChildIDs = GetChildIDs();
	bool bChildrenChanged = (newChildIDs != LastChildIDs);
	LastChildIDs = newChildIDs;

	UpdateCachedGraphData();

	AEditModelGameMode_CPP *gameMode = World.IsValid() ? World->GetAuthGameMode<AEditModelGameMode_CPP>() : nullptr;
	MaterialData.EngineMaterial = gameMode ? gameMode->MetaPlaneMaterial : nullptr;

	bool bEnableCollision = !IsInPreviewMode();

	DynamicMeshActor->SetupMetaPlaneGeometry(CachedPoints, MaterialData, GetAlpha(), true, &CachedHoles, bEnableCollision);

	UpdateVisuals();
}

void FMOIMetaPlaneImpl::UpdateCachedGraphData()
{
	const Modumate::FGraph3DFace *graphFace = GetDocument()->GetVolumeGraph().FindFace(ID);

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

