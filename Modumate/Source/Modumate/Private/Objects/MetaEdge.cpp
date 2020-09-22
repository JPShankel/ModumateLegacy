// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Objects/MetaEdge.h"

#include "DocumentManagement/ModumateDocument.h"
#include "Objects/MiterNode.h"
#include "Graph/Graph3D.h"
#include "ModumateCore/ModumateObjectStatics.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "UnrealClasses/LineActor.h"


FMOIMetaEdgeImpl::FMOIMetaEdgeImpl(FModumateObjectInstance *moi)
	: FMOIEdgeImplBase(moi)
{
}

bool FMOIMetaEdgeImpl::CleanObject(EObjectDirtyFlags DirtyFlag, TArray<TSharedPtr<Modumate::FDelta>>* OutSideEffectDeltas)
{
	switch (DirtyFlag)
	{
	case EObjectDirtyFlags::Structure:
	{
		auto& graph = MOI->GetDocument()->GetVolumeGraph();
		auto edge = graph.FindEdge(MOI->ID);
		auto vertexStart = edge ? graph.FindVertex(edge->StartVertexID) : nullptr;
		auto vertexEnd = edge ? graph.FindVertex(edge->EndVertexID) : nullptr;
		if (!ensure(LineActor.IsValid() && vertexStart && vertexEnd))
		{
			return false;
		}

		LineActor->Point1 = vertexStart->Position;
		LineActor->Point2 = vertexEnd->Position;

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
	case EObjectDirtyFlags::Visuals:
	{
		MOI->UpdateVisibilityAndCollision();
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

const FMiterData& FMOIMetaEdgeImpl::GetMiterData() const
{
	return CachedMiterData;
}
