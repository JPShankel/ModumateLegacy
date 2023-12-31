// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "Objects/TerrainVertex.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "DocumentManagement/ModumateDocument.h"
#include "Objects/Terrain.h"
#include "UI/ToolTray/ToolTrayBlockProperties.h"
#include "UI/Properties/InstPropWidgetLinearDimension.h"


FMOITerrainVertexData::FMOITerrainVertexData()
{}

AMOITerrainVertex::AMOITerrainVertex()
{
	StateData.CustomData.SaveStructData(InstanceData);
	BaseColor = FColor(0x00, 0x35, 0xFF);

	FWebMOIProperty prop;

	prop.name = TEXT("Height");
	prop.type = EWebMOIPropertyType::height;
	prop.displayName = TEXT("Height");
	prop.isEditable = true;
	prop.isVisible = true;
	WebProperties.Add(prop.name, prop);
}

FVector AMOITerrainVertex::GetLocation() const
{
	return CachedLocation;
}

bool AMOITerrainVertex::CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas)
{
	switch (DirtyFlag)
	{
	case EObjectDirtyFlags::Structure:
	{
		AMOITerrain* terrainMoi = Cast<AMOITerrain>(GetParentObject());
		if (!ensure(terrainMoi))
		{
			return false;
		}

		FVector origin = terrainMoi->GetLocation();
		const auto graph2d = GetDocument()->FindSurfaceGraph(terrainMoi->ID);
		const FGraph2DVertex* graphVert = graph2d->FindVertex(ID);
		if (ensure(graphVert))
		{
			CachedLocation = origin + graphVert->Position.X * FVector::ForwardVector + graphVert->Position.Y * FVector::RightVector;
			SetActorLocation(CachedLocation);
		}
		else
		{
			return false;
		}

		double * instanceHeight = terrainMoi->InstanceData.Heights.Find(ID);
		CachedHeight = instanceHeight == nullptr ? 0.0f : *instanceHeight;
		break;
	}

	case EObjectDirtyFlags::Visuals:
		return TryUpdateVisuals();

	default:
		break;
	}

	return true;
}

void AMOITerrainVertex::RegisterInstanceDataUI(class UToolTrayBlockProperties* PropertiesUI)
{
	static const FString heightPropertyName(TEXT("Height"));
	if (auto scaleField = PropertiesUI->RequestPropertyField<UInstPropWidgetLinearDimension>(this, heightPropertyName))
	{
		scaleField->RegisterValue(this, CachedHeight);
		scaleField->ValueChangedEvent.AddDynamic(this, &AMOITerrainVertex::OnInstPropUIChangedHeight);
	}
}

void AMOITerrainVertex::OnInstPropUIChangedHeight(float NewHeight)
{
	AMOITerrain* terrainMoi = Cast<AMOITerrain>(GetParentObject());
	if (!ensure(terrainMoi))
	{
		return;
	}

	FMOITerrainData newInstanceData = terrainMoi->InstanceData;
	double* vertHeight = newInstanceData.Heights.Find(ID);
	if (ensure(Document && vertHeight))
	{
		*vertHeight = NewHeight;
		auto deltaPtr = MakeShared<FMOIDelta>();
		FMOIStateData& newState = deltaPtr->AddMutationState(terrainMoi);
		newState.CustomData.SaveStructData(newInstanceData);

		Document->ApplyDeltas({ deltaPtr }, GetWorld());
	}
}

bool AMOITerrainVertex::FromWebMOI(const FString& InJson)
{
	if (AModumateObjectInstance::FromWebMOI(InJson))
	{
		OnInstPropUIChangedHeight(InstanceData.Height);
		return true;
	}
	return false;
}

bool AMOITerrainVertex::ToWebMOI(FWebMOI& OutMOI) const
{
	const AMOITerrain* terrainMoi = Cast<AMOITerrain>(GetParentObject());
	if (!ensure(terrainMoi))
	{
		return false;
	}
	FMOITerrainData newInstanceData = terrainMoi->InstanceData;
	if (AModumateObjectInstance::ToWebMOI(OutMOI))
	{
		FWebMOIProperty* moiProp = OutMOI.properties.Find(TEXT("Height"));
		if (!ensure(moiProp))
		{
			return false;
		}

		moiProp->valueArray.Empty();
		double* height = newInstanceData.Heights.Find(ID);
		if (ensure(height != nullptr))
		{
			moiProp->valueArray.Add(FString::SanitizeFloat(*height));
		}

		return true;
	}
	return false;
}

