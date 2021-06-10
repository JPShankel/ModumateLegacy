// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "Objects/TerrainMaterial.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "ModumateCore/ModumateObjectStatics.h"
#include "UnrealClasses/EditModelGameMode.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "UI/Properties/InstPropWidgetCombo.h"
#include "UI/ToolTray/ToolTrayBlockProperties.h"
#include "UI/Custom/ModumateDropDownUserWidget.h"
#include "DocumentManagement/ModumateDocument.h"
#include "UMG/Public/Components/ComboBoxString.h"

const FBIMTagPath AMOITerrainMaterial::SiteNcp(TEXT("Part_3FlexDims0Fixed_Terrain"));

AMOITerrainMaterial::AMOITerrainMaterial()
{
	BaseColor = FColor(160, 240, 170);
	HoveredColor = FColor(100, 230, 120);
}

bool AMOITerrainMaterial::CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas)
{
	switch (DirtyFlag)
	{
	case EObjectDirtyFlags::Structure:
		return UpdateStructure();
		break;

	case EObjectDirtyFlags::Visuals:
		return TryUpdateVisuals();

	default:
		break;
	}

	return true;
}

void AMOITerrainMaterial::RegisterInstanceDataUI(class UToolTrayBlockProperties* PropertiesUI) 
{
	static const FString siteMaterialPropertyName(TEXT("Site material"));
	if (auto materialSelection = PropertiesUI->RequestPropertyField<UInstPropWidgetCombo>(this, siteMaterialPropertyName))
	{
		materialSelection->SetVisibility(ESlateVisibility::Visible);
		const FBIMPresetCollection& presets = GetDocument()->GetPresetCollection();
		materialSelection->ComboBox->ClearOptions();
		for (const FGuid& guid : MaterialGuids)
		{
			const FBIMPresetInstance * preset = presets.PresetFromGUID(guid);
			if (preset)
			{
				materialSelection->ComboBox->AddOption(preset->DisplayName.ToString());
			}
		}
		int32 index = 0;
		MaterialGuids.Find(InstanceData.Material, index);
		CachedSelection = FMath::Max(index, 0);
		materialSelection->RegisterValue(this, CachedSelection);
		materialSelection->DisplayValue();
		materialSelection->ValueChangedEvent.AddDynamic(this, &AMOITerrainMaterial::OnInstPropUIChangedSelection);
	}

}

void AMOITerrainMaterial::OnInstPropUIChangedSelection(int32 NewValue)
{
	auto* doc = GetDocument();
	if (doc && NewValue >= 0 && NewValue < MaterialGuids.Num())
	{
		FMOITerrainMaterialData newInstanceData = InstanceData;
		newInstanceData.Material = MaterialGuids[NewValue];
		auto deltaPtr = MakeShared<FMOIDelta>();
		FMOIStateData& newState = deltaPtr->AddMutationState(this);
		newState.CustomData.SaveStructData(newInstanceData);

		Document->ApplyDeltas({ deltaPtr }, GetWorld());
	}
}

void AMOITerrainMaterial::PostCreateObject(bool bNewObject)
{
	Super::PostCreateObject(bNewObject);

	SetupSiteMaterials();
	if (!MaterialGuids.Contains(InstanceData.Material) && MaterialGuids.Num() > 0)
	{
		InstanceData.Material = MaterialGuids[0];
	}
}

bool AMOITerrainMaterial::UpdateStructure()
{
	auto* doc = GetDocument();
	int32 polyID = GetParentID();
	const auto graph2d = doc->FindSurfaceGraphByObjID(polyID);

	if (ensure(graph2d))
	{
		const FGraph2DPolygon* graphPoly = graph2d->FindPolygon(polyID);
		auto* polyObject = doc->GetObjectById(polyID);
		if (polyObject->GetNumCorners() == 0)
		{
			return false;
		}

		bool bInteriorPolygon, bInnerBoundsPolygon;
		if (!UModumateObjectStatics::GetGeometryFromSurfacePoly(GetDocument(), polyID,
			bInteriorPolygon, bInnerBoundsPolygon, CachedTransform, CachedPoints, CachedHoles))
		{
			return false;
		}
		// Skip exterior polygons and inner bounds polygons; they can't be visible anyway, so they shouldn't set up any dynamic meshes.
		if (!bInteriorPolygon || bInnerBoundsPolygon || (CachedPoints.Num() < 3))
		{
			return true;
		}

		const FVector origin = polyObject->GetLocation();

		CachedOrigin = origin;
		CachedCenter = origin;

		UModumateGameInstance* gameInstance = GetGameInstance<UModumateGameInstance>();
		const AEditModelGameMode* gameMode = gameInstance ? gameInstance->GetEditModelGameMode() : nullptr;
		MaterialData.EngineMaterial = gameMode ? gameMode->MetaPlaneMaterial : nullptr;

		DynamicMeshActor->SetupMetaPlaneGeometry(CachedPoints, MaterialData, 1.0f, true, &CachedHoles, !IsInPreviewMode());

		AModumateObjectInstance* parentTerrain = doc->GetObjectById(graph2d->GetID());
		parentTerrain->MarkDirty(EObjectDirtyFlags::Visuals);
	}

	return true;
}

void AMOITerrainMaterial::SetupSiteMaterials()
{
	const FBIMPresetCollection& presets = GetDocument()->GetPresetCollection();
	UModumateGameInstance* gameInstance = GetWorld()->GetGameInstance<UModumateGameInstance>();
	if (!gameInstance || !gameInstance->ObjectDatabase)
	{
		return;
	}

	MaterialGuids.Empty();
	TArray<FGuid> presetGuids;
	presets.GetPresetsForNCP(SiteNcp, MaterialGuids, false);
}
