// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "DocumentManagement/Objects/SurfaceVertex.h"

#include "DocumentManagement/ModumateDocument.h"
#include "Graph/Graph2D.h"
#include "ModumateCore/ModumateGeometryStatics.h"
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
		UWorld* world = MOI ? MOI->GetWorld() : nullptr;
		auto controller = world ? world->GetFirstPlayerController<AEditModelPlayerController_CPP>() : nullptr;
		if (controller && VertexActor.IsValid())
		{
			bool bEnabledByViewMode = controller->EMPlayerState->IsObjectTypeEnabledByViewMode(EObjectType::OTSurfaceVertex);
			bOutVisible = bOutCollisionEnabled = bEnabledByViewMode;

			VertexActor->SetActorHiddenInGame(!bOutVisible);
			VertexActor->SetActorTickEnabled(bOutVisible);
			VertexActor->SetActorEnableCollision(bOutCollisionEnabled);
		}
	}

	bool FMOISurfaceVertexImpl::CleanObject(EObjectDirtyFlags DirtyFlag, TArray<TSharedPtr<FDelta>>* OutSideEffectDeltas)
	{
		auto surfaceGraphObj = MOI ? MOI->GetParentObject() : nullptr;
		if (!ensure(surfaceGraphObj && VertexActor.IsValid()))
		{
			return false;
		}

		switch (DirtyFlag)
		{
		case EObjectDirtyFlags::Structure:
		{
			auto surfaceGraph = MOI->GetDocument()->FindSurfaceGraph(surfaceGraphObj->ID);
			auto surfaceVertex = surfaceGraph ? surfaceGraph->FindVertex(MOI->ID) : nullptr;
			if (ensureAlways(surfaceVertex))
			{
				FTransform surfaceGraphTransform = surfaceGraphObj->GetWorldTransform();
				VertexActor->SetMOILocation(UModumateGeometryStatics::Deproject2DPointTransform(surfaceVertex->Position, surfaceGraphTransform));
			}

			break;
		}
		case EObjectDirtyFlags::Visuals:
			MOI->UpdateVisibilityAndCollision();
			break;
		default:
			break;
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
