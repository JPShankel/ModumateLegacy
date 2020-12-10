// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Objects/MetaVertex.h"

#include "DocumentManagement/ModumateDocument.h"
#include "Graph/Graph3D.h"
#include "ModumateCore/ModumateObjectStatics.h"
#include "UnrealClasses/VertexActor.h"

FMOIMetaVertexImpl::FMOIMetaVertexImpl()
	: FMOIVertexImplBase()
{
}

bool FMOIMetaVertexImpl::CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas)
{
	switch (DirtyFlag)
	{
	case EObjectDirtyFlags::Structure:
	{
		auto vertex = GetDocument()->GetVolumeGraph().FindVertex(ID);
		if (!ensure(vertex))
		{
			return false;
		}

		VertexActor->SetMOILocation(vertex->Position);
	}
	break;
	case EObjectDirtyFlags::Visuals:
	{
		UpdateVisuals();
	}
	break;
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

