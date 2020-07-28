// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "DocumentManagement/Objects/MetaVertex.h"

#include "ModumateCore/ModumateObjectStatics.h"
#include "UnrealClasses/ModumateVertexActor_CPP.h"

namespace Modumate
{
	FMOIMetaVertexImpl::FMOIMetaVertexImpl(FModumateObjectInstance *moi)
		: FMOIVertexImplBase(moi)
	{
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

	bool FMOIMetaVertexImpl::CleanObject(EObjectDirtyFlags DirtyFlag, TArray<TSharedPtr<FDelta>>* OutSideEffectDeltas)
	{
		if (!FMOIVertexImplBase::CleanObject(DirtyFlag, OutSideEffectDeltas))
		{
			return false;
		}

		if (MOI)
		{
			MOI->GetConnectedMOIs(CachedConnectedMOIs);
			for (FModumateObjectInstance *connectedMOI : CachedConnectedMOIs)
			{
				if (connectedMOI->GetObjectType() == EObjectType::OTMetaEdge)
				{
					connectedMOI->MarkDirty(DirtyFlag);
				}
			}
		}

		return true;
	}
}
