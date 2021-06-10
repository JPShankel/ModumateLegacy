// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "Objects/TerrainPolygon.h"

#include "ModumateCore/ModumateFunctionLibrary.h"
#include "ModumateCore/ModumateObjectStatics.h"
#include "UnrealClasses/EditModelGameMode.h"
#include "UnrealClasses/ModumateGameInstance.h"
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

			CreateMaterialMoi(OutSideEffectDeltas);

			bool bInteriorPolygon, bInnerBoundsPolygon;
			if (!UModumateObjectStatics::GetGeometryFromSurfacePoly(GetDocument(), ID,
				bInteriorPolygon, bInnerBoundsPolygon, CachedTransform, CachedPoints, CachedHoles))
			{
				return false;
			}

			CachedOrigin = origin;
			CachedCenter = origin;

			UModumateGameInstance* gameInstance = GetGameInstance<UModumateGameInstance>();
			auto* gameMode = gameInstance ? gameInstance->GetEditModelGameMode() : nullptr;
			MaterialData.EngineMaterial = gameMode ? gameMode->MetaPlaneMaterial : nullptr;

			DynamicMeshActor->SetupMetaPlaneGeometry(CachedPoints, MaterialData, 1.0f, true, &CachedHoles, !IsInPreviewMode());
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

	bOutVisible = false; bOutCollisionEnabled = false;
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

bool AMOITerrainPolygon::CreateMaterialMoi(TArray<FDeltaPtr>* SideEffectDeltas)
{
	if (GetChildIDs().Num() == 0 && SideEffectDeltas)
	{
		UModumateDocument* doc = GetDocument();
		int32 nextID = doc->GetNextAvailableID();
		FMOIStateData stateData(nextID, EObjectType::OTTerrainMaterial, ID);
		auto createMoiDelta = MakeShared<FMOIDelta>();
		createMoiDelta->AddCreateDestroyState(stateData, EMOIDeltaType::Create);
		SideEffectDeltas->Add(createMoiDelta);
	}

	return true;
}
