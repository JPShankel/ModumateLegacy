// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Objects/VertexBase.h"

#include "ToolsAndAdjustments/Common/AdjustmentHandleActor.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "ModumateCore/ModumateObjectStatics.h"
#include "UnrealClasses/ModumateVertexActor_CPP.h"

FMOIVertexImplBase::FMOIVertexImplBase(FModumateObjectInstance *moi)
	: FModumateObjectInstanceImplBase(moi)
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
	VertexActor = World->SpawnActor<AModumateVertexActor_CPP>(AModumateVertexActor_CPP::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);

	// Set appearance
	AEditModelGameMode_CPP *gameMode = world->GetAuthGameMode<AEditModelGameMode_CPP>();
	VertexActor->SetActorMesh(gameMode->MetaPlaneVertexIconMesh);
	VertexActor->SetHandleScaleScreenSize(DefaultHandleSize);

	return VertexActor.Get();
}

void FMOIVertexImplBase::OnSelected(bool bNewSelected)
{
	FModumateObjectInstanceImplBase::OnSelected(bNewSelected);

	MOI->UpdateVisibilityAndCollision();

	if (VertexActor.IsValid())
	{
		VertexActor->SetHandleScaleScreenSize(bNewSelected ? SelectedHandleSize : DefaultHandleSize);
	}
}
