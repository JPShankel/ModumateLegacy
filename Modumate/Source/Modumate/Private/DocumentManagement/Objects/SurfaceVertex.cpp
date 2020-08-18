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
		, CachedLocation(ForceInitToZero)
	{
	}

	FVector FMOISurfaceVertexImpl::GetLocation() const
	{
		return CachedLocation;
	}

	FVector FMOISurfaceVertexImpl::GetCorner(int32 index) const
	{
		ensure(index == 0);
		return GetLocation();
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
		// TODO: Use FMOIVertexImplBase once MetaVertex conforms to this non-ControlPoints-derived data.
		/*if (!FMOIVertexImplBase::CleanObject(DirtyFlag, OutSideEffectDeltas))
		{
			return false;
		}*/

		auto surfaceGraphObj = MOI ? MOI->GetParentObject() : nullptr;
		if (!ensure(surfaceGraphObj && VertexActor.IsValid()))
		{
			return false;
		}

		switch (DirtyFlag)
		{
		case EObjectDirtyFlags::Structure:
		{
			if (MOI->GetIsInPreviewMode())
			{
				if (ensureAlways(MOI->GetControlPoints().Num() == 1))
				{
					CachedLocation = MOI->GetControlPoint(0);
				}
			}
			else
			{
				auto surfaceGraph = MOI->GetDocument()->FindSurfaceGraph(surfaceGraphObj->ID);
				auto surfaceVertex = surfaceGraph ? surfaceGraph->FindVertex(MOI->ID) : nullptr;
				if (ensureAlways(surfaceVertex))
				{
					FTransform surfaceGraphTransform = surfaceGraphObj->GetWorldTransform();
					CachedLocation = UModumateGeometryStatics::Deproject2DPointTransform(surfaceVertex->Position, surfaceGraphTransform);
				}
			}

			VertexActor->SetMOILocation(CachedLocation);
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
