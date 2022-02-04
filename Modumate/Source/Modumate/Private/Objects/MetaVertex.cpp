// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Objects/MetaVertex.h"

#include "DocumentManagement/ModumateDocument.h"
#include "Graph/Graph3D.h"
#include "Objects/ModumateObjectStatics.h"


AMOIMetaVertex::AMOIMetaVertex()
	: AMOIVertexBase()
{
}

bool AMOIMetaVertex::CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas)
{
	switch (DirtyFlag)
	{
	case EObjectDirtyFlags::Structure:
	{
		auto* graph = GetDocument()->FindVolumeGraph(ID);
		auto vertex = graph ? graph->FindVertex(ID) : nullptr;
		if (!ensure(vertex))
		{
			return false;
		}

		SetActorLocation(vertex->Position);
	}
	break;
	case EObjectDirtyFlags::Visuals:
	{
		return TryUpdateVisuals();
	}
	default:
		break;
	}

	return true;
}

void AMOIMetaVertex::GetTangents(TArray<FVector>& OutTangents) const
{
	FVector vertexLocation = GetLocation();

	for (AModumateObjectInstance* connectedMOI : CachedConnectedMOIs)
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

