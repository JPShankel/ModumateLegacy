// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Objects/PlaneBase.h"

#include "ModumateCore/ModumateFunctionLibrary.h"
#include "ToolsAndAdjustments/Handles/AdjustPolyEdgeHandle.h"
#include "UnrealClasses/EditModelGameMode.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/ModumateGameInstance.h"

AMOIPlaneBase::AMOIPlaneBase()
	: AModumateObjectInstance()
	, SelectedColor(0x03, 0xC4, 0xA8)
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
void AMOIPlaneBase::SetupAdjustmentHandles(AEditModelPlayerController *controller)
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

bool AMOIPlaneBase::GetUpdatedVisuals(bool& bOutVisible, bool& bOutCollisionEnabled)
{
	if (!AModumateObjectInstance::GetUpdatedVisuals(bOutVisible, bOutCollisionEnabled))
	{
		return false;
	}

	if (bOutVisible)
	{
		UpdateMaterial();
	}

	return true;
}

bool AMOIPlaneBase::OnSelected(bool bIsSelected)
{
	if (!AModumateObjectInstance::OnSelected(bIsSelected))
	{
		return false;
	}

	MarkDirty(EObjectDirtyFlags::Visuals);

	return true;
}

bool AMOIPlaneBase::OnHovered(AEditModelPlayerController *controller, bool bIsHovered)
{
	if (!AModumateObjectInstance::OnHovered(controller, bIsHovered))
	{
		return false;
	}

	MarkDirty(EObjectDirtyFlags::Visuals);

	return true;
}

void AMOIPlaneBase::PostCreateObject(bool bNewObject)
{
	auto* gameMode = GetWorld()->GetGameInstance<UModumateGameInstance>()->GetEditModelGameMode();
	if (gameMode)
	{
		MaterialData.EngineMaterial = gameMode->MetaPlaneMaterial;
	}
	AModumateObjectInstance::PostCreateObject(bNewObject);

	MarkConnectedVisualsDirty();
}

float AMOIPlaneBase::GetAlpha() const
{
	return IsHovered() ? 1.5f : 1.0f;
}

void AMOIPlaneBase::UpdateMaterial()
{
	if (DynamicMeshActor.IsValid())
	{
		// Color
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

void AMOIPlaneBase::MarkConnectedVisualsDirty()
{
	MarkDirty(EObjectDirtyFlags::Visuals);

	// Update the visuals of all of our connected edges
	GetConnectedMOIs(TempConnectedMOIs);
	for (AModumateObjectInstance* connectedMOI : TempConnectedMOIs)
	{
		connectedMOI->MarkDirty(EObjectDirtyFlags::Visuals);
	}
}
