// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "DocumentManagement/ModumateObjectInstanceMetaEdge.h"

#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "UnrealClasses/LineActor.h"
#include "DocumentManagement/ModumateDocument.h"
#include "DocumentManagement/ModumateMiterNodeInterface.h"
#include "ModumateCore/ModumateObjectStatics.h"


namespace Modumate
{
	FMOIMetaEdgeImpl::FMOIMetaEdgeImpl(FModumateObjectInstance *moi)
		: FModumateObjectInstanceImplBase(moi)
		, World(nullptr)
		, LineActor(nullptr)
		, HoverColor(FColor::White)
		, HoverThickness(3.0f)
	{
		MOI->SetControlPoints(TArray<FVector>());
		MOI->AddControlPoint(FVector::ZeroVector);
		MOI->AddControlPoint(FVector::ZeroVector);
	}

	void FMOIMetaEdgeImpl::SetLocation(const FVector &p)
	{
		FVector midPoint = GetLocation();
		FVector delta = p - midPoint;
		MOI->SetControlPoint(0,MOI->GetControlPoint(0) + delta);
		MOI->SetControlPoint(1,MOI->GetControlPoint(1) + delta);
	}

	FVector FMOIMetaEdgeImpl::GetLocation() const
	{
		return 0.5f * (MOI->GetControlPoint(0) + MOI->GetControlPoint(1));
	}

	FVector FMOIMetaEdgeImpl::GetCorner(int32 index) const
	{
		return MOI->GetControlPoint(index);
	}

	void FMOIMetaEdgeImpl::OnCursorHoverActor(AEditModelPlayerController_CPP *controller, bool bEnableHover)
	{
		FModumateObjectInstanceImplBase::OnCursorHoverActor(controller, bEnableHover);

		MOI->UpdateVisibilityAndCollision();
	}

	AActor *FMOIMetaEdgeImpl::CreateActor(UWorld *world, const FVector &loc, const FQuat &rot)
	{
		World = world;
		LineActor = world->SpawnActor<ALineActor>();
		LineActor->SetIsHUD(false);
		LineActor->UpdateMetaEdgeVisuals(false);
		return LineActor.Get();
	}

	bool FMOIMetaEdgeImpl::CleanObject(EObjectDirtyFlags DirtyFlag)
	{
		if (!FModumateObjectInstanceImplBase::CleanObject(DirtyFlag))
		{
			return false;
		}

		switch (DirtyFlag)
		{
		case EObjectDirtyFlags::Structure:
		{
			// If our own geometry has been updated, then we need to re-evaluate our mitering.
			MOI->MarkDirty(EObjectDirtyFlags::Mitering);
		}
		break;
		case EObjectDirtyFlags::Mitering:
		{
			// TODO: clean the miter details by performing the mitering for this edge's connected plane-hosted objects
			bool bUpdateSuccess = CachedMiterData.GatherDetails(MOI);
			if (bUpdateSuccess)
			{
				bool bMiterSuccess = CachedMiterData.CalculateMitering();
			}
		}
		break;
		default:
			break;
		}

		if (MOI)
		{
			MOI->GetConnectedMOIs(CachedConnectedMOIs);
			for (FModumateObjectInstance *connectedMOI : CachedConnectedMOIs)
			{
				if (connectedMOI->GetObjectType() == EObjectType::OTMetaPlane)
				{
					connectedMOI->MarkDirty(DirtyFlag);
				}
			}
		}

		return true;
	}

	void FMOIMetaEdgeImpl::SetupDynamicGeometry()
	{
		if (ensureAlways((MOI->GetControlPoints().Num() == 2) && LineActor.IsValid()))
		{
			LineActor->Point1 = MOI->GetControlPoint(0);
			LineActor->Point2 = MOI->GetControlPoint(1);
			MOI->UpdateVisibilityAndCollision();
		}
	}

	void FMOIMetaEdgeImpl::UpdateDynamicGeometry()
	{
		SetupDynamicGeometry();
	}

	void FMOIMetaEdgeImpl::UpdateVisibilityAndCollision(bool &bOutVisible, bool &bOutCollisionEnabled)
	{
		if (MOI && LineActor.IsValid())
		{
			AEditModelPlayerState_CPP* emPlayerState = Cast<AEditModelPlayerState_CPP>(LineActor->GetWorld()->GetFirstPlayerController()->PlayerState);

			bool bShouldBeVisible, bShouldCollisionBeEnabled, bConnectedToAnyPlane;
			UModumateObjectStatics::ShouldMetaObjBeEnabled(MOI, bShouldBeVisible, bShouldCollisionBeEnabled, bConnectedToAnyPlane);
			bOutVisible = !MOI->IsRequestedHidden() && bShouldBeVisible;
			bOutCollisionEnabled = !MOI->IsCollisionRequestedDisabled() && bShouldCollisionBeEnabled;

			LineActor->SetVisibilityInApp(bOutVisible);
			if (bOutVisible)
			{
				float thicknessMultiplier = GetThicknessMultiplier();
				if (MOI->IsHovered() && emPlayerState && emPlayerState->ShowHoverEffects)
				{
					LineActor->Color = HoverColor;
					LineActor->Thickness = HoverThickness * thicknessMultiplier;
				}
				else
				{
					LineActor->UpdateMetaEdgeVisuals(bConnectedToAnyPlane, thicknessMultiplier);
				}
			}

			LineActor->SetActorEnableCollision(bOutCollisionEnabled);
		}
	}

	void FMOIMetaEdgeImpl::OnSelected(bool bNewSelected)
	{
		FModumateObjectInstanceImplBase::OnSelected(bNewSelected);

		MOI->UpdateVisibilityAndCollision();
	}

	void FMOIMetaEdgeImpl::GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping, bool bForSelection) const
	{
		// Don't report edge points for snapping, otherwise they will conflict with vertices
		if (!bForSnapping)
		{
			FVector edgeDir = (MOI->GetControlPoint(1) - MOI->GetControlPoint(0)).GetSafeNormal();

			outPoints.Add(FStructurePoint(MOI->GetControlPoint(0), edgeDir, 0));
			outPoints.Add(FStructurePoint(MOI->GetControlPoint(1), edgeDir, 1));
		}

		outLines.Add(FStructureLine(MOI->GetControlPoint(0), MOI->GetControlPoint(1), 0, 1));
	}

	const FMiterData& FMOIMetaEdgeImpl::GetMiterData() const
	{
		return CachedMiterData;
	}

	float FMOIMetaEdgeImpl::GetThicknessMultiplier() const
	{
		return (MOI && MOI->IsSelected()) ? 3.0f : 1.0f;
	}
}
