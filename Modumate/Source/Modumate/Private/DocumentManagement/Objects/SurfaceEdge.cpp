// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "DocumentManagement/Objects/SurfaceEdge.h"

#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "UnrealClasses/LineActor.h"
#include "ModumateCore/ModumateObjectStatics.h"


namespace Modumate
{
	FMOISurfaceEdgeImpl::FMOISurfaceEdgeImpl(FModumateObjectInstance *moi)
		: FMOIEdgeImplBase(moi)
	{
	}

	bool FMOISurfaceEdgeImpl::CleanObject(EObjectDirtyFlags DirtyFlag)
	{
		if (!FMOIEdgeImplBase::CleanObject(DirtyFlag))
		{
			return false;
		}

		if (MOI)
		{
			MOI->GetConnectedMOIs(CachedConnectedMOIs);
			for (FModumateObjectInstance *connectedMOI : CachedConnectedMOIs)
			{
				if (connectedMOI->GetObjectType() == EObjectType::OTSurfacePolygon)
				{
					connectedMOI->MarkDirty(DirtyFlag);
				}
			}
		}

		return true;
	}

	void FMOISurfaceEdgeImpl::UpdateVisibilityAndCollision(bool &bOutVisible, bool &bOutCollisionEnabled)
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
}
