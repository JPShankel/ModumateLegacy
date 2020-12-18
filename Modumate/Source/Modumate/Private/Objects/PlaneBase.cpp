// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Objects/PlaneBase.h"

#include "ModumateCore/ModumateFunctionLibrary.h"
#include "ToolsAndAdjustments/Handles/AdjustPolyEdgeHandle.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"

AMOIPlaneBase::AMOIPlaneBase()
	: AModumateObjectInstance()
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

FVector AMOIPlaneBase::GetCorner(int32 index) const
{
	if (ensure(CachedPoints.IsValidIndex(index)))
	{
		return CachedPoints[index];
	}

	return GetLocation();
}

int32 AMOIPlaneBase::GetNumCorners() const
{
	return CachedPoints.Num();
}

FQuat AMOIPlaneBase::GetRotation() const
{
	return FRotationMatrix::MakeFromXY(CachedAxisX, CachedAxisY).ToQuat();
}

FVector AMOIPlaneBase::GetLocation() const
{
	return CachedCenter;
}

FVector AMOIPlaneBase::GetNormal() const
{
	return CachedPlane;
}

void AMOIPlaneBase::GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping, bool bForSelection) const
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
void AMOIPlaneBase::SetupAdjustmentHandles(AEditModelPlayerController_CPP *controller)
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

bool AMOIPlaneBase::ShowStructureOnSelection() const
{
	return false;
}

void AMOIPlaneBase::GetUpdatedVisuals(bool& bOutVisible, bool& bOutCollisionEnabled)
{
	AModumateObjectInstance::GetUpdatedVisuals(bOutVisible, bOutCollisionEnabled);

	if (bOutVisible)
	{
		UpdateMaterial();
	}
}

bool AMOIPlaneBase::OnSelected(bool bIsSelected)
{
	if (!AModumateObjectInstance::OnSelected(bIsSelected))
	{
		return false;
	}

	UpdateVisuals();
	return true;
}

bool AMOIPlaneBase::OnHovered(AEditModelPlayerController_CPP *controller, bool bIsHovered)
{
	if (!AModumateObjectInstance::OnHovered(controller, bIsHovered))
	{
		return false;
	}

	UpdateVisuals();
	return true;
}

void AMOIPlaneBase::PostCreateObject(bool bNewObject)
{
	AModumateObjectInstance::PostCreateObject(bNewObject);

	UpdateConnectedVisuals();
}

float AMOIPlaneBase::GetAlpha() const
{
	return IsHovered() ? 1.5f : 1.0f;
}

void AMOIPlaneBase::UpdateMaterial()
{
	if (DynamicMeshActor.IsValid())
	{
		AEditModelGameMode_CPP* gameMode = GetWorld()->GetAuthGameMode<AEditModelGameMode_CPP>();
		// Color
		if (gameMode)
		{
			MaterialData.EngineMaterial = gameMode->MetaPlaneMaterial;
			if (IsSelected())
			{
				MaterialData.Color = SelectedColor;
			}
			else if (IsHovered())
			{
				MaterialData.Color = HoveredColor;
			}
			else
			{
				MaterialData.Color = BaseColor;
			}

			DynamicMeshActor->CachedMIDs.SetNumZeroed(1);
			UModumateFunctionLibrary::SetMeshMaterial(DynamicMeshActor->Mesh, MaterialData, 0, &DynamicMeshActor->CachedMIDs[0]);
		}
	}
}

void AMOIPlaneBase::UpdateConnectedVisuals()
{
	UpdateVisuals();
	// Update the visuals of all of our connected edges
	GetConnectedMOIs(TempConnectedMOIs);
	for (AModumateObjectInstance* connectedMOI : TempConnectedMOIs)
	{
		connectedMOI->MarkDirty(EObjectDirtyFlags::Visuals);
	}
}
