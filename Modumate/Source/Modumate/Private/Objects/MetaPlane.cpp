// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Objects/MetaPlane.h"

#include "DocumentManagement/ModumateDocument.h"
#include "Graph/Graph3D.h"
#include "ModumateCore/ModumateObjectStatics.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"

AMOIMetaPlane::AMOIMetaPlane()
	: AMOIPlaneBase()
{

}

void AMOIMetaPlane::GetUpdatedVisuals(bool &bOutVisible, bool &bOutCollisionEnabled)
{
	if (DynamicMeshActor.IsValid())
	{
		bool bPreviouslyVisible = IsVisible();

		UModumateObjectStatics::GetMetaObjEnabledFlags(this, bOutVisible, bOutCollisionEnabled);

		DynamicMeshActor->SetActorHiddenInGame(!bOutVisible);
		DynamicMeshActor->SetActorEnableCollision(bOutCollisionEnabled);

		if (bOutVisible)
		{
			AMOIPlaneBase::UpdateMaterial();
		}

		if (bPreviouslyVisible != bOutVisible)
		{
			UpdateConnectedVisuals();
		}
	}
}

void AMOIMetaPlane::SetupDynamicGeometry()
{
	const TArray<int32> &newChildIDs = GetChildIDs();
	bool bChildrenChanged = (newChildIDs != LastChildIDs);
	LastChildIDs = newChildIDs;

	UpdateCachedGraphData();

	AEditModelGameMode_CPP *gameMode = GetWorld()->GetAuthGameMode<AEditModelGameMode_CPP>();
	MaterialData.EngineMaterial = gameMode ? gameMode->MetaPlaneMaterial : nullptr;

	bool bEnableCollision = !IsInPreviewMode();

	DynamicMeshActor->SetupMetaPlaneGeometry(CachedPoints, MaterialData, GetAlpha(), true, &CachedHoles, bEnableCollision);

	UpdateVisuals();
}

void AMOIMetaPlane::UpdateCachedGraphData()
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

float AMOIMetaPlane::GetAlpha() const
{
	return 1.0f;
}

