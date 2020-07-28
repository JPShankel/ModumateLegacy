// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "DocumentManagement/Objects/SurfaceVertex.h"

#include "ModumateCore/ModumateObjectStatics.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
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
			auto controller = MOI->GetWorld()->GetFirstPlayerController<AEditModelPlayerController_CPP>();
			bool bEnabledByViewMode = controller->EMPlayerState->IsObjectTypeEnabledByViewMode(EObjectType::OTSurfaceVertex);
			bOutVisible = bOutCollisionEnabled = bEnabledByViewMode;

			VertexActor->SetActorHiddenInGame(!bOutVisible);
			VertexActor->SetActorTickEnabled(bOutVisible);
			VertexActor->SetActorEnableCollision(bOutCollisionEnabled);
		}
	}

	bool FMOISurfaceVertexImpl::CleanObject(EObjectDirtyFlags DirtyFlag, TArray<TSharedPtr<FDelta>>* OutSideEffectDeltas)
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
				if (connectedMOI->GetObjectType() == EObjectType::OTSurfaceEdge)
				{
					connectedMOI->MarkDirty(DirtyFlag);
				}
			}
		}

		return true;
	}
}
