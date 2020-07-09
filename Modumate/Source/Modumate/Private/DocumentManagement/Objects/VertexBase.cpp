// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "DocumentManagement/Objects/VertexBase.h"

#include "ToolsAndAdjustments/Common/AdjustmentHandleActor.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "ModumateCore/ModumateObjectStatics.h"
#include "UnrealClasses/ModumateVertexActor_CPP.h"

namespace Modumate
{
	FMOIVertexImplBase::FMOIVertexImplBase(FModumateObjectInstance *moi)
		: FModumateObjectInstanceImplBase(moi)
		, DefaultHandleSize(0.0004f)
		, SelectedHandleSize(0.0006f)
	{
		MOI->SetControlPoints(TArray<FVector>());
		MOI->AddControlPoint(FVector::ZeroVector);
	}

	void FMOIVertexImplBase::SetLocation(const FVector &p)
	{
		MOI->SetControlPoint(0,p);
		if (VertexActor.IsValid())
		{
			VertexActor->SetMOILocation(p);
		}
	}

	FVector FMOIVertexImplBase::GetLocation() const
	{
		return MOI->GetControlPoint(0);
	}

	FVector FMOIVertexImplBase::GetCorner(int32 index) const
	{
		return GetLocation();
	}

	void FMOIVertexImplBase::GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping, bool bForSelection) const
	{
		outPoints.Add(FStructurePoint(MOI->GetControlPoint(0), FVector::ZeroVector, 0));
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

	void FMOIVertexImplBase::SetupDynamicGeometry()
	{
		if (ensureAlways((MOI->GetControlPoints().Num() == 1) && VertexActor.IsValid()))
		{
			VertexActor->SetMOILocation(MOI->GetControlPoint(0));
			MOI->UpdateVisibilityAndCollision();
		}
	}

	void FMOIVertexImplBase::UpdateDynamicGeometry()
	{
		SetupDynamicGeometry();
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
}
