// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "DocumentManagement/Objects/MetaVertex.h"

#include "DocumentManagement/ModumateDocument.h"
#include "Graph/Graph3D.h"
#include "ModumateCore/ModumateObjectStatics.h"
#include "UnrealClasses/ModumateVertexActor_CPP.h"

FMOIMetaVertexImpl::FMOIMetaVertexImpl(FModumateObjectInstance *moi)
	: FMOIVertexImplBase(moi)
{
}

void FMOIMetaVertexImpl::GetStructuralPointsAndLines(TArray<FStructurePoint>& OutPoints, TArray<FStructureLine>& OutLines, bool bForSnapping, bool bForSelection) const
{
	FVector pointSnapTangent(ForceInitToZero);
	if (CachedConnectedMOIs.Num() > 0)
	{
		const FModumateObjectInstance* connectedMOI = CachedConnectedMOIs[0];
		if (connectedMOI && (connectedMOI->GetObjectType() == EObjectType::OTMetaEdge) && ensure(connectedMOI->GetNumCorners() == 2))
		{
			pointSnapTangent = (connectedMOI->GetCorner(1) - connectedMOI->GetCorner(0)).GetSafeNormal();
		}
	}

	OutPoints.Add(FStructurePoint(GetLocation(), pointSnapTangent, 0));
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

