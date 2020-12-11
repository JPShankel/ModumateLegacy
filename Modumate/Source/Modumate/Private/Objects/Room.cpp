// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "Objects/Room.h"

#include "ToolsAndAdjustments/Common/AdjustmentHandleActor.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "Graph/Graph3D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "DocumentManagement/ModumateDocument.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "ModumateCore/ModumateGeometryStatics.h"
#include "ModumateCore/ModumateRoomStatics.h"

AMOIRoom::AMOIRoom()
	: AModumateObjectInstance()
	, DynamicMaterial(nullptr)
{
}

FVector AMOIRoom::GetLocation() const
{
	if (DynamicMeshActor != nullptr)
	{
		return DynamicMeshActor->GetActorLocation();
	}
	return FVector::ZeroVector;
}

FQuat AMOIRoom::GetRotation() const
{
	return FQuat::Identity;
}

bool AMOIRoom::OnHovered(AEditModelPlayerController_CPP *controller, bool bIsHovered)
{
	if (!AModumateObjectInstance::OnHovered(controller, bIsHovered))
	{
		return false;
	}

	UpdateMaterial();
	return true;
}

bool AMOIRoom::OnSelected(bool bIsSelected)
{
	if (!AModumateObjectInstance::OnSelected(bIsSelected))
	{
		return false;
	}

	UpdateMaterial();
	return true;
}

void AMOIRoom::SetupDynamicGeometry()
{
	const UModumateDocument *doc = GetDocument();
	if (doc == nullptr)
	{
		return;
	}

	TArray<TArray<FVector>> polygons;
	const Modumate::FGraph3D &volumeGraph = doc->GetVolumeGraph();
	// TODO: refactor room faces using strongly-typed InstanceProperties
	for (FGraphSignedID faceID : { MOD_ID_NONE })//GetControlPointIndices())
	{
		const Modumate::FGraph3DFace *graphFace = volumeGraph.FindFace(faceID);
		const AModumateObjectInstance *planeObj = doc->GetObjectById(FMath::Abs(faceID));
		if (!ensureAlways(graphFace && planeObj))
		{
			continue;
		}

		polygons.Add(graphFace->CachedPositions);
	}

	DynamicMeshActor->SetupRoomGeometry(polygons, Material);
	UModumateRoomStatics::UpdateDerivedRoomProperties(this);

	// Update the cached config, now that its derived room properties have been set.
	if (!UModumateRoomStatics::GetRoomConfig(this, CachedRoomConfig))
	{
		// If we failed to get a config, then we should at least set a default one here, and re-update the derived properties.
		UModumateRoomStatics::SetRoomConfigFromKey(this, UModumateRoomStatics::DefaultRoomConfigKey);
		UModumateRoomStatics::UpdateDerivedRoomProperties(this);
	}

	UpdateMaterial();
}

void AMOIRoom::UpdateDynamicGeometry()
{
	SetupDynamicGeometry();
}

void AMOIRoom::GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping, bool bForSelection) const
{
	const UModumateDocument *doc = GetDocument();
	if (doc)
	{
		// TODO: refactor room faces using strongly-typed InstanceProperties
		for (FGraphSignedID faceID : { MOD_ID_NONE })//GetControlPointIndices())
		{
			const AModumateObjectInstance *planeObj = doc->GetObjectById(FMath::Abs(faceID));
			if (planeObj == nullptr)
			{
				continue;
			}

			planeObj->GetStructuralPointsAndLines(TempPoints, TempLines, true);
			outPoints.Append(TempPoints);
			outLines.Append(TempLines);
		}
	}
}

float AMOIRoom::GetAlpha()
{
	return (IsHovered() ? 1.0f : 0.75f) * (IsSelected() ? 1.0f : 0.75f);
}

void AMOIRoom::UpdateMaterial()
{
	if (GameMode.IsValid() && DynamicMeshActor.IsValid())
	{
		Material.EngineMaterial = GameMode->MetaPlaneMaterial;
		Material.DefaultBaseColor.Color = CachedRoomConfig.RoomColor;
		Material.DefaultBaseColor.Color.A = static_cast<uint8>(255.0f * GetAlpha());
		Material.DefaultBaseColor.bValid = true;

		UModumateFunctionLibrary::SetMeshMaterial(DynamicMeshActor->Mesh, Material, 0, &DynamicMeshActor->CachedMIDs[0]);
	}
}

void AMOIRoom::SetIsDynamic(bool bIsDynamic)
{
	if (DynamicMeshActor.IsValid())
	{
		DynamicMeshActor->SetIsDynamic(bIsDynamic);
	}
}

bool AMOIRoom::GetIsDynamic() const
{
	return DynamicMeshActor.IsValid() && DynamicMeshActor->GetIsDynamic();
}
