// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "Objects/TerrainPolygon.h"

#include "ModumateCore/ModumateFunctionLibrary.h"
#include "ModumateCore/ModumateObjectStatics.h"
#include "UnrealClasses/EditModelGameMode.h"
#include "DocumentManagement/ModumateDocument.h"


AMOITerrainPolygon::AMOITerrainPolygon()
{
	BaseColor = FColor(160, 240, 170);
	HoveredColor = FColor(100, 230, 120);
}

bool AMOITerrainPolygon::CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas)
{
	switch (DirtyFlag)
	{
	case EObjectDirtyFlags::Structure:
	{
		auto terrainMoi = GetParentObject();
		if (!ensure(terrainMoi))
		{
			return false;
		}

		FVector origin = terrainMoi->GetLocation();
		const auto graph2d = GetDocument()->FindSurfaceGraph(terrainMoi->ID);
		const FGraph2DPolygon* graphPoly = graph2d->FindPolygon(ID);
		if (ensure(graphPoly))
		{
			if (!graphPoly->bInterior)
			{   // Outer polys shouldn't be visible.
				return true;
			}

			CachedPoints.Empty();
			for (int32 v : graphPoly->CachedPerimeterVertexIDs)
			{
				auto position = graph2d->FindVertex(v)->Position;
				CachedPoints.Add(origin + position.X * FVector::ForwardVector + position.Y * FVector::RightVector);
			}

			CachedOrigin = origin;
			CachedCenter = origin;

			AEditModelGameMode* gameMode = GetWorld()->GetAuthGameMode<AEditModelGameMode>();
			MaterialData.EngineMaterial = gameMode ? gameMode->MetaPlaneMaterial : nullptr;

			DynamicMeshActor->SetupMetaPlaneGeometry(CachedPoints, MaterialData, 1.0f, true, nullptr, !IsInPreviewMode());
		}

		break;
	}

	case EObjectDirtyFlags::Visuals:
		return TryUpdateVisuals();

	default:
		break;
	}

	return true;
}

bool AMOITerrainPolygon::GetUpdatedVisuals(bool& bOutVisible, bool& bOutCollisionEnabled)
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
