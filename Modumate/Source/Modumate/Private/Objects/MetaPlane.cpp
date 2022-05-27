// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Objects/MetaPlane.h"

#include "DocumentManagement/ModumateDocument.h"
#include "Graph/Graph3D.h"
#include "Objects/ModumateObjectStatics.h"
#include "UnrealClasses/EditModelGameMode.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "UI/ToolTray/ToolTrayBlockProperties.h"
#include "UI/Properties/InstPropWidgetCycle.h"

FMOIMetaPlaneData::FMOIMetaPlaneData()
{}

AMOIMetaPlane::AMOIMetaPlane()
	: AMOIPlaneBase()
{
	FWebMOIProperty prop;

	prop.Name = TEXT("FlipDirection");
	prop.Type = EWebMOIPropertyType::flipDirection;
	prop.DisplayName = TEXT("Flip Direction");
	prop.isEditable = true;
	prop.isVisible = true;
	WebProperties.Add(prop.Name, prop);

	prop.Name = TEXT("CalculatedArea");
	prop.Type = EWebMOIPropertyType::label;
	prop.DisplayName = TEXT("Area");
	prop.isEditable = true;
	prop.isVisible = true;
	WebProperties.Add(prop.Name, prop);
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

	AMOIPlaneBase::UpdateLineArrowVisual();

	return true;
}


void AMOIMetaPlane::ShowAdjustmentHandles(AEditModelPlayerController* Controller, bool bShow)
{
	Super::ShowAdjustmentHandles(Controller, bShow);
	UpdateLineArrowVisual();
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

void AMOIMetaPlane::RegisterInstanceDataUI(class UToolTrayBlockProperties* PropertiesUI)
{
	static const FString cyclePropertyName(TEXT("Flip Direction"));
	if (auto cycleField = PropertiesUI->RequestPropertyField<UInstPropWidgetCycle>(this, cyclePropertyName))
	{
		cycleField->RegisterValue(this);
		cycleField->ValueChangedEvent.AddDynamic(this, &AMOIMetaPlane::OnInstPropUIChangedCycle);
	}
}

void AMOIMetaPlane::UpdateCachedGraphData()
{
	auto* graph = GetDocument()->FindVolumeGraph(ID);
	const FGraph3DFace* graphFace = graph ? graph->FindFace(ID) : nullptr;

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

void AMOIMetaPlane::OnInstPropUIChangedCycle(int32 BasisValue)
{
	Document->ReverseMetaObjects(GetWorld(), {}, {ID});
}

bool AMOIMetaPlane::FromWebMOI(const FString& InJson)
{
	if (AModumateObjectInstance::FromWebMOI(InJson))
	{
		if (InstanceData.FlipDirection)
		{
			OnInstPropUIChangedCycle(1);
		}

		return true;
	}
	return false;
}

bool AMOIMetaPlane::ToWebMOI(FWebMOI& OutMOI) const
{
	if (AModumateObjectInstance::ToWebMOI(OutMOI))
	{
		auto controller = GetWorld()->GetFirstPlayerController<AEditModelPlayerController>();

		FWebMOIProperty* moiProp = OutMOI.Properties.Find(TEXT("FlipDirection"));
		moiProp->ValueArray.Empty();
		moiProp->ValueArray.Add("false");

		const double area = CalculateArea(controller);

		if (area != 0.0)
		{
			EUnit defaultUnit = controller->GetDocument()->GetCurrentSettings().DimensionUnit;
			EDimensionUnits unitType = controller->GetDocument()->GetCurrentSettings().DimensionType;
			FText areaDisplayText = UModumateDimensionStatics::CentimetersToDisplayText(area, 2, unitType, defaultUnit);

			moiProp = OutMOI.Properties.Find(TEXT("CalculatedArea"));
			if (ensure(moiProp))
			{
				moiProp->ValueArray.Empty();
				moiProp->ValueArray.Add(areaDisplayText.ToString());
			}
		}

		return true;
	}
	return false;
}

double AMOIMetaPlane::CalculateArea(AEditModelPlayerController* AEMPlayerController) const
{
	double totalArea = 0.0f;
	for (auto& obj : AEMPlayerController->EMPlayerState->SelectedObjects)
	{
		if (obj->GetAssembly().ObjectType == EObjectType::OTMetaPlane)
		{
			auto* graph = GetDocument()->FindVolumeGraph(obj->ID);
			const FGraph3DFace* graphFace = graph ? graph->FindFace(obj->ID) : nullptr;
			if (graphFace)
			{
				totalArea += graphFace->CalculateArea();
			}
			else
			{
				return 0.0f;
			}
			
		}
	}

	return totalArea;
}