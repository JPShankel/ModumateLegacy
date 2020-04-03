// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "ModumateObjectInstanceMetaVertex.h"

#include "AdjustmentHandleActor_CPP.h"
#include "EditModelGameMode_CPP.h"
#include "EditModelPlayerController_CPP.h"
#include "ModumateObjectStatics.h"
#include "ModumateVertexActor_CPP.h"

namespace Modumate
{
	FMOIMetaVertexImpl::FMOIMetaVertexImpl(FModumateObjectInstance *moi)
		: FModumateObjectInstanceImplBase(moi)
		, DefaultHandleSize(0.0006f)
		, SelectedHandleSize(0.001f)
	{
		MOI->ControlPoints.SetNum(1);
		MOI->ControlPoints[0] = FVector::ZeroVector;
	}

	void FMOIMetaVertexImpl::SetLocation(const FVector &p)
	{
		MOI->ControlPoints[0] = p;
		if (VertexActor.IsValid())
		{
			VertexActor->SetMOILocation(p);
		}
	}

	FVector FMOIMetaVertexImpl::GetLocation() const
	{
		return MOI->ControlPoints[0];
	}

	FVector FMOIMetaVertexImpl::GetCorner(int32 index) const
	{
		return GetLocation();
	}

	void FMOIMetaVertexImpl::UpdateVisibilityAndCollision(bool &bOutVisible, bool &bOutCollisionEnabled)
	{
		if (MOI && VertexActor.IsValid())
		{
			bool bShouldBeVisible, bShouldCollisionBeEnabled, bConnectedToAnyPlane;
			UModumateObjectStatics::ShouldMetaObjBeEnabled(MOI, bShouldBeVisible, bShouldCollisionBeEnabled, bConnectedToAnyPlane);
			bOutVisible = !MOI->IsRequestedHidden() && bShouldBeVisible;
			bOutCollisionEnabled = !MOI->IsCollisionRequestedDisabled() && bShouldCollisionBeEnabled;

			VertexActor->SetActorHiddenInGame(!bOutVisible);
			VertexActor->SetActorTickEnabled(bOutVisible);
			VertexActor->SetActorEnableCollision(bOutCollisionEnabled);
		}
	}

	void FMOIMetaVertexImpl::GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping, bool bForSelection) const
	{
		outPoints.Add(FStructurePoint(MOI->ControlPoints[0], FVector::ZeroVector, 0));
	}

	AActor *FMOIMetaVertexImpl::CreateActor(UWorld *world, const FVector &loc, const FQuat &rot)
	{
		World = world;
		VertexActor = World->SpawnActor<AModumateVertexActor_CPP>(AModumateVertexActor_CPP::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);

		// Set appearance
		AEditModelGameMode_CPP *gameMode = world->GetAuthGameMode<AEditModelGameMode_CPP>();
		VertexActor->SetActorMesh(gameMode->PointAdjusterMesh);
		VertexActor->SetHandleScaleScreenSize(DefaultHandleSize);

		return VertexActor.Get();
	}

	bool FMOIMetaVertexImpl::CleanObject(EObjectDirtyFlags DirtyFlag)
	{
		if (!FModumateObjectInstanceImplBase::CleanObject(DirtyFlag))
		{
			return false;
		}

		if (MOI)
		{
			MOI->GetConnectedMOIs(CachedConnectedMOIs);
			for (FModumateObjectInstance *connectedMOI : CachedConnectedMOIs)
			{
				if (connectedMOI->ObjectType == EObjectType::OTMetaEdge)
				{
					connectedMOI->MarkDirty(DirtyFlag);
				}
			}
		}

		return true;
	}

	void FMOIMetaVertexImpl::SetupDynamicGeometry()
	{
		if (ensureAlways((MOI->ControlPoints.Num() == 1) && VertexActor.IsValid()))
		{
			VertexActor->SetMOILocation(MOI->ControlPoints[0]);
			MOI->UpdateVisibilityAndCollision();
		}
	}

	void FMOIMetaVertexImpl::UpdateDynamicGeometry()
	{
		SetupDynamicGeometry();
	}

	void FMOIMetaVertexImpl::OnSelected(bool bNewSelected)
	{
		FModumateObjectInstanceImplBase::OnSelected(bNewSelected);

		MOI->UpdateVisibilityAndCollision();

		if (VertexActor.IsValid())
		{
			VertexActor->SetHandleScaleScreenSize(bNewSelected ? SelectedHandleSize : DefaultHandleSize);
		}
	}
}
