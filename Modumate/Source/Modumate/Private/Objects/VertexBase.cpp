// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Objects/VertexBase.h"

#include "ModumateCore/ModumateObjectStatics.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "ToolsAndAdjustments/Common/AdjustmentHandleActor.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/VertexActor.h"

FMOIVertexImplBase::FMOIVertexImplBase()
	: AModumateObjectInstance()
	, SelectedColor(0x00, 0x35, 0xFF)
	, BaseColor(0x00, 0x00, 0x00)
	, DefaultHandleSize(0.0004f)
	, SelectedHandleSize(0.0006f)
{
}

FVector FMOIVertexImplBase::GetLocation() const
{
	return VertexActor.IsValid() ? VertexActor->MoiLocation : FVector::ZeroVector;
}

FVector FMOIVertexImplBase::GetCorner(int32 index) const
{
	ensure(index == 0);
	return GetLocation();
}

int32 FMOIVertexImplBase::GetNumCorners() const
{
	return VertexActor.IsValid() ? 1 : 0;
}

void FMOIVertexImplBase::GetUpdatedVisuals(bool& bOutVisible, bool& bOutCollisionEnabled)
{
	if (VertexActor.IsValid())
	{
		UModumateObjectStatics::GetNonPhysicalEnabledFlags(this, bOutVisible, bOutCollisionEnabled);

		VertexActor->SetActorHiddenInGame(!bOutVisible);
		VertexActor->SetActorTickEnabled(bOutVisible);
		VertexActor->SetActorEnableCollision(bOutCollisionEnabled);
	}
}

void FMOIVertexImplBase::GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping, bool bForSelection) const
{
	TArray<FVector> vertexTangents;
	GetTangents(vertexTangents);

	// TODO: support multiple axis affordances for a single point
	FVector defaultTangent = (vertexTangents.Num() > 0) ? vertexTangents[0] : FVector::ZeroVector;

	outPoints.Add(FStructurePoint(GetLocation(), defaultTangent, 0));
}

AActor *FMOIVertexImplBase::CreateActor(UWorld *world, const FVector &loc, const FQuat &rot)
{
	World = world;
	VertexActor = World->SpawnActor<AVertexActor>(AVertexActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);

	// Set appearance
	AEditModelGameMode_CPP *gameMode = world->GetAuthGameMode<AEditModelGameMode_CPP>();
	VertexActor->SetActorMesh(gameMode->MetaPlaneVertexIconMesh);
	VertexActor->SetHandleScaleScreenSize(DefaultHandleSize);

	FArchitecturalMaterial material;
	material.EngineMaterial = gameMode ? gameMode->VertexMaterial : nullptr;
	material.DefaultBaseColor.Color = BaseColor;
	material.DefaultBaseColor.bValid = true;
	VertexActor->SetActorMaterial(material);

	return VertexActor.Get();
}

bool FMOIVertexImplBase::OnSelected(bool bIsSelected)
{
	if (!AModumateObjectInstance::OnSelected(bIsSelected))
	{
		return false;
	}

	UpdateVisuals();

	if (VertexActor.IsValid())
	{
		VertexActor->SetHandleScaleScreenSize(bIsSelected ? SelectedHandleSize : DefaultHandleSize);

		VertexActor->Material.DefaultBaseColor.Color = bIsSelected ? SelectedColor : BaseColor;
		VertexActor->Material.DefaultBaseColor.bValid = true;

		UModumateFunctionLibrary::SetMeshMaterial(VertexActor->MeshComp, VertexActor->Material, 0);
	}

	return true;
}
