// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "DocumentManagement/Objects/SurfaceVertex.h"

#include "ModumateCore/ModumateObjectStatics.h"
#include "UnrealClasses/ModumateVertexActor_CPP.h"

namespace Modumate
{
	FMOISurfaceVertexImpl::FMOISurfaceVertexImpl(FModumateObjectInstance *moi)
		: FMOIVertexImplBase(moi)
	{
	}

	void FMOISurfaceVertexImpl::UpdateVisibilityAndCollision(bool &bOutVisible, bool &bOutCollisionEnabled)
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

	bool FMOISurfaceVertexImpl::CleanObject(EObjectDirtyFlags DirtyFlag)
	{
		if (!FMOIVertexImplBase::CleanObject(DirtyFlag))
		{
			return false;
		}

		if (MOI)
		{
			MOI->GetConnectedMOIs(CachedConnectedMOIs);
			for (FModumateObjectInstance *connectedMOI : CachedConnectedMOIs)
			{
				if (connectedMOI->GetObjectType() == EObjectType::OTSurfaceEdge)
				{
					connectedMOI->MarkDirty(DirtyFlag);
				}
			}
		}

		return true;
	}
}
