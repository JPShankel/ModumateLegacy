// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Objects/MetaPlane.h"

#include "DocumentManagement/ModumateDocument.h"
#include "Graph/Graph3D.h"
#include "ModumateCore/ModumateObjectStatics.h"
#include "UnrealClasses/EditModelGameMode.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "UnrealClasses/ModumateGameInstance.h"

AMOIMetaPlane::AMOIMetaPlane()
	: AMOIPlaneBase()
{

}

bool AMOIMetaPlane::GetUpdatedVisuals(bool &bOutVisible, bool &bOutCollisionEnabled)
{
	if (!DynamicMeshActor.IsValid())
	{
		return false;
	}

	bool bPreviouslyVisible = IsVisible();

	if (!UModumateObjectStatics::GetMetaObjEnabledFlags(this, bOutVisible, bOutCollisionEnabled))
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

void AMOIMetaPlane::SetupDynamicGeometry()
{
	const TArray<int32> &newChildIDs = GetChildIDs();
	bool bChildrenChanged = (newChildIDs != LastChildIDs);
	LastChildIDs = newChildIDs;

	UpdateCachedGraphData();

	auto* gameMode = GetWorld()->GetGameInstance<UModumateGameInstance>()->GetEditModelGameMode();
	MaterialData.EngineMaterial = gameMode ? gameMode->MetaPlaneMaterial : nullptr;

	bool bEnableCollision = !IsInPreviewMode();

	DynamicMeshActor->SetupMetaPlaneGeometry(CachedPoints, MaterialData, GetAlpha(), true, &CachedHoles, bEnableCollision);

	MarkDirty(EObjectDirtyFlags::Visuals);
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

