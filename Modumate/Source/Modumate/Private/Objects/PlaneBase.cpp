// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Objects/PlaneBase.h"

#include "ModumateCore/ModumateFunctionLibrary.h"
#include "UnrealClasses/EditModelGameMode.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "Materials/MaterialInstanceDynamic.h"

AMOIPlaneBase::AMOIPlaneBase()
	: AModumateObjectInstance()
	, SelectedColor(0x03, 0xC4, 0xA8)
	, HoveredColor(0x9E, 0x9E, 0x9E)
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
	CacheIsSelected = bIsSelected;
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

	// Create arrow component for certain object type only
	if (UModumateTypeStatics::GetObjectTypeWithDirectionIndicator().Contains(GetObjectType()))
	{
		if (!LineArrowCylinderMesh)
		{
			LineArrowCylinderMesh = NewObject<UStaticMeshComponent>(this);
			LineArrowCylinderMesh->RegisterComponent();
			LineArrowCylinderMesh->SetHiddenInGame(false);
			LineArrowCylinderMesh->SetVisibility(false);
			LineArrowCylinderMesh->SetStaticMesh(gameMode->DirectionArrowMeshForFace);
			LineArrowCylinderMesh->SetBoundsScale(1000.f); // Prevent occlusion
			LineArrowCylinderMesh->SetCastShadow(false);
			ArrowDynMat = UMaterialInstanceDynamic::Create(LineArrowCylinderMesh->GetMaterial(0), LineArrowCylinderMesh);
			LineArrowCylinderMesh->SetMaterial(0, ArrowDynMat);
		}
	}
}

void AMOIPlaneBase::PreDestroy()
{
	if (LineArrowCylinderMesh)
	{
		LineArrowCylinderMesh->SetVisibility(false);
	}
	Super::PreDestroy();
}

void AMOIPlaneBase::UpdateLineArrowVisual()
{
	if (!LineArrowCylinderMesh)
	{
		return;
	}

	AEditModelPlayerController* controller = Cast<AEditModelPlayerController>(GetWorld()->GetFirstPlayerController());
	// Only show arrow if it's the only one selected
	bool bShowDir = CacheIsSelected && controller->EMPlayerState->SelectedObjects.Num() == 1;
	if (controller && controller->GetAlwaysShowGraphDirection())
	{
		bShowDir = bVisible;
	}

	LineArrowCylinderMesh->SetVisibility(bShowDir);

	if (bShowDir)
	{
		LineArrowCylinderMesh->SetWorldLocation(GetLocation());
		LineArrowCylinderMesh->SetWorldRotation(GetNormal().Rotation());
		// Mesh scaling is updated in material via WPO, but scale here provides a base size for material to transform 
		LineArrowCylinderMesh->SetWorldScale3D(FVector(0.1f));

		if (ArrowDynMat)
		{
			static const FName colorParamName(TEXT("Color"));
			ArrowDynMat->SetVectorParameterValue(colorParamName, CacheIsSelected ? FColor::Black : SelectedColor);
		}
	}
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
