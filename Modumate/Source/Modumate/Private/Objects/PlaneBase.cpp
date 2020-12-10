// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Objects/PlaneBase.h"

#include "ModumateCore/ModumateFunctionLibrary.h"
#include "ToolsAndAdjustments/Handles/AdjustPolyEdgeHandle.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"

FMOIPlaneImplBase::FMOIPlaneImplBase()
	: FModumateObjectInstance()
	, SelectedColor(0x1C, 0x9F, 0xFF)
	, HoveredColor(0xCF, 0xCF, 0xCF)
	, BaseColor(0xFF, 0xFF, 0xFF)
	, CachedPlane(ForceInitToZero)
	, CachedAxisX(ForceInitToZero)
	, CachedAxisY(ForceInitToZero)
	, CachedOrigin(ForceInitToZero)
	, CachedCenter(ForceInitToZero)
{
}

FVector FMOIPlaneImplBase::GetCorner(int32 index) const
{
	if (ensure(CachedPoints.IsValidIndex(index)))
	{
		return CachedPoints[index];
	}

	return GetLocation();
}

int32 FMOIPlaneImplBase::GetNumCorners() const
{
	return CachedPoints.Num();
}

FQuat FMOIPlaneImplBase::GetRotation() const
{
	return FRotationMatrix::MakeFromXY(CachedAxisX, CachedAxisY).ToQuat();
}

FVector FMOIPlaneImplBase::GetLocation() const
{
	return CachedCenter;
}

FVector FMOIPlaneImplBase::GetNormal() const
{
	return CachedPlane;
}

void FMOIPlaneImplBase::GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping, bool bForSelection) const
{
	// Don't return points or lines if we're snapping,
	// since otherwise the plane will interfere with edges and vertices.
	if (bForSnapping)
	{
		return;
	}

	int32 numPoints = CachedPoints.Num();
	for (int32 pointIdxA = 0; pointIdxA < numPoints; ++pointIdxA)
	{
		int32 pointIdxB = (pointIdxA + 1) % numPoints;
		const FVector &pointA = CachedPoints[pointIdxA];
		const FVector &pointB = CachedPoints[pointIdxB];
		FVector dir = (pointB - pointA).GetSafeNormal();

		outPoints.Add(FStructurePoint(pointA, dir, pointIdxA));
		outLines.Add(FStructureLine(pointA, pointB, pointIdxA, pointIdxB));
	}
}

// Adjustment Handles
void FMOIPlaneImplBase::SetupAdjustmentHandles(AEditModelPlayerController_CPP *controller)
{
	if (HasAdjustmentHandles())
	{
		return;
	}

	int32 numPoints = CachedPoints.Num();
	for (int32 i = 0; i < numPoints; ++i)
	{
		auto edgeHandle = MakeHandle<AAdjustPolyEdgeHandle>();
		edgeHandle->SetTargetIndex(i);
	}
}

bool FMOIPlaneImplBase::ShowStructureOnSelection() const
{
	return false;
}

void FMOIPlaneImplBase::GetUpdatedVisuals(bool& bOutVisible, bool& bOutCollisionEnabled)
{
	FModumateObjectInstance::GetUpdatedVisuals(bOutVisible, bOutCollisionEnabled);

	if (bOutVisible)
	{
		UpdateMaterial();
	}
}

bool FMOIPlaneImplBase::OnSelected(bool bIsSelected)
{
	if (!FModumateObjectInstance::OnSelected(bIsSelected))
	{
		return false;
	}

	UpdateVisuals();
	return true;
}

bool FMOIPlaneImplBase::OnHovered(AEditModelPlayerController_CPP *controller, bool bIsHovered)
{
	if (!FModumateObjectInstance::OnHovered(controller, bIsHovered))
	{
		return false;
	}

	UpdateVisuals();
	return true;
}

void FMOIPlaneImplBase::PostCreateObject(bool bNewObject)
{
	FModumateObjectInstance::PostCreateObject(bNewObject);

	UpdateConnectedVisuals();
}

float FMOIPlaneImplBase::GetAlpha() const
{
	return IsHovered() ? 1.5f : 1.0f;
}

void FMOIPlaneImplBase::UpdateMaterial()
{
	if (DynamicMeshActor.IsValid())
	{
		AEditModelGameMode_CPP* gameMode = World.IsValid() ? World->GetAuthGameMode<AEditModelGameMode_CPP>() : nullptr;
		// Color
		if (gameMode)
		{
			MaterialData.EngineMaterial = gameMode->MetaPlaneMaterial;
			if (IsSelected())
			{
				MaterialData.DefaultBaseColor.Color = SelectedColor;
			}
			else if (IsHovered())
			{
				MaterialData.DefaultBaseColor.Color = HoveredColor;
			}
			else
			{
				MaterialData.DefaultBaseColor.Color = BaseColor;
			}

			MaterialData.DefaultBaseColor.bValid = true;

			DynamicMeshActor->CachedMIDs.SetNumZeroed(1);
			UModumateFunctionLibrary::SetMeshMaterial(DynamicMeshActor->Mesh, MaterialData, 0, &DynamicMeshActor->CachedMIDs[0]);
		}
	}
}

void FMOIPlaneImplBase::UpdateConnectedVisuals()
{
	UpdateVisuals();
	// Update the visuals of all of our connected edges
	GetConnectedMOIs(TempConnectedMOIs);
	for (FModumateObjectInstance* connectedMOI : TempConnectedMOIs)
	{
		connectedMOI->MarkDirty(EObjectDirtyFlags::Visuals);
	}
}
