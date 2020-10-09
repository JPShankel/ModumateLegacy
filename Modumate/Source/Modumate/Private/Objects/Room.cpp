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

FMOIRoomImpl::FMOIRoomImpl(FModumateObjectInstance *moi)
	: FModumateObjectInstanceImplBase(moi)
	, DynamicMaterial(nullptr)
{
}

FMOIRoomImpl::~FMOIRoomImpl()
{
}

FVector FMOIRoomImpl::GetLocation() const
{
	if (DynamicMeshActor != nullptr)
	{
		return DynamicMeshActor->GetActorLocation();
	}
	return FVector::ZeroVector;
}

FQuat FMOIRoomImpl::GetRotation() const
{
	return FQuat::Identity;
}

void FMOIRoomImpl::OnHovered(AEditModelPlayerController_CPP *controller, bool bIsHovered)
{
	FModumateObjectInstanceImplBase::OnHovered(controller, bIsHovered);

	UpdateMaterial();
}

void FMOIRoomImpl::OnSelected(bool bIsSelected)
{
	FModumateObjectInstanceImplBase::OnSelected(bIsSelected);

	UpdateMaterial();
}

void FMOIRoomImpl::SetupDynamicGeometry()
{
	const FModumateDocument *doc = MOI ? MOI->GetDocument() : nullptr;
	if (doc == nullptr)
	{
		return;
	}

	TArray<TArray<FVector>> polygons;
	const Modumate::FGraph3D &volumeGraph = doc->GetVolumeGraph();
	for (FGraphSignedID faceID : MOI->GetControlPointIndices())
	{
		const Modumate::FGraph3DFace *graphFace = volumeGraph.FindFace(faceID);
		const FModumateObjectInstance *planeObj = doc->GetObjectById(FMath::Abs(faceID));
		if (!ensureAlways(graphFace && planeObj))
		{
			continue;
		}

		polygons.Add(graphFace->CachedPositions);
	}

	DynamicMeshActor->SetupRoomGeometry(polygons, Material);
	UModumateRoomStatics::UpdateDerivedRoomProperties(MOI);

	// Update the cached config, now that its derived room properties have been set.
	if (!UModumateRoomStatics::GetRoomConfig(MOI, CachedRoomConfig))
	{
		// If we failed to get a config, then we should at least set a default one here, and re-update the derived properties.
		UModumateRoomStatics::SetRoomConfigFromKey(MOI, UModumateRoomStatics::DefaultRoomConfigKey);
		UModumateRoomStatics::UpdateDerivedRoomProperties(MOI);
	}

	UpdateMaterial();
}

void FMOIRoomImpl::UpdateDynamicGeometry()
{
	SetupDynamicGeometry();
}

void FMOIRoomImpl::GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping, bool bForSelection) const
{
	const FModumateDocument *doc = MOI ? MOI->GetDocument() : nullptr;
	if (doc)
	{
		for (FGraphSignedID faceID : MOI->GetControlPointIndices())
		{
			const FModumateObjectInstance *planeObj = doc->GetObjectById(FMath::Abs(faceID));
			if (!ensureAlways(planeObj))
			{
				continue;
			}

			planeObj->GetStructuralPointsAndLines(TempPoints, TempLines, true);
			outPoints.Append(TempPoints);
			outLines.Append(TempLines);
		}
	}
}

float FMOIRoomImpl::GetAlpha()
{
	return (MOI->IsHovered() ? 1.0f : 0.75f) * (MOI->IsSelected() ? 1.0f : 0.75f);
}

void FMOIRoomImpl::UpdateMaterial()
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

void FMOIRoomImpl::SetIsDynamic(bool bIsDynamic)
{
	if (DynamicMeshActor.IsValid())
	{
		DynamicMeshActor->SetIsDynamic(bIsDynamic);
	}
}

bool FMOIRoomImpl::GetIsDynamic() const
{
	return DynamicMeshActor.IsValid() && DynamicMeshActor->GetIsDynamic();
}
