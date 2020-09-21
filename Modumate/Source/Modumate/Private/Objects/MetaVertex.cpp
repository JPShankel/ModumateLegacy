// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Objects/MetaVertex.h"

#include "DocumentManagement/ModumateDocument.h"
#include "Graph/Graph3D.h"
#include "ModumateCore/ModumateObjectStatics.h"
#include "UnrealClasses/ModumateVertexActor_CPP.h"

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

bool FMOIMetaVertexImpl::CleanObject(EObjectDirtyFlags DirtyFlag, TArray<TSharedPtr<Modumate::FDelta>>* OutSideEffectDeltas)
{
	switch (DirtyFlag)
	{
	case EObjectDirtyFlags::Structure:
	{
		auto vertex = MOI->GetDocument()->GetVolumeGraph().FindVertex(MOI->ID);
		if (!ensure(vertex))
		{
			return false;
		}

		SetLocation(vertex->Position);
	}
	break;
	case EObjectDirtyFlags::Visuals:
	{
		MOI->UpdateVisibilityAndCollision();
	}
	break;
	}

	MOI->GetConnectedMOIs(CachedConnectedMOIs);
	for (FModumateObjectInstance *connectedMOI : CachedConnectedMOIs)
	{
		if (connectedMOI->GetObjectType() == EObjectType::OTMetaEdge)
		{
			connectedMOI->MarkDirty(DirtyFlag);
		}
	}

	return true;
}

void FMOIMetaVertexImpl::GetTangents(TArray<FVector>& OutTangents) const
{
	FVector vertexLocation = GetLocation();

	for (FModumateObjectInstance* connectedMOI : CachedConnectedMOIs)
	{
		if (connectedMOI && (connectedMOI->GetObjectType() == EObjectType::OTMetaEdge))
		{
			FVector edgeStart = connectedMOI->GetCorner(0);
			FVector edgeEnd = connectedMOI->GetCorner(1);
			FVector edgeDir = (edgeEnd - edgeStart).GetSafeNormal();

			if (vertexLocation.Equals(edgeStart))
			{
				OutTangents.Add(edgeDir);
			}
			else if (vertexLocation.Equals(edgeEnd))
			{
				OutTangents.Add(-edgeDir);
			}
		}
	}
}

