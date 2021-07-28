// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "Objects/TerrainEdge.h"

#include "DocumentManagement/ModumateDocument.h"
#include "ModumateCore/ModumateObjectStatics.h"
#include "UnrealClasses/LineActor.h"


AMOITerrainEdge::AMOITerrainEdge()
{
	BaseColor = FColor(0x00, 0x35, 0xFF);
	HoveredColor = FColor(0x00, 0x35, 0xFF);
}

FVector AMOITerrainEdge::GetCorner(int32 Index) const
{
	if (ensure(Index == 0 || Index == 1))
	{
		return CachedPoints[Index];
	}
	else
	{
		return FVector::ZeroVector;
	}
}

bool AMOITerrainEdge::CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas)
{
	switch (DirtyFlag)
	{
	case EObjectDirtyFlags::Structure:
	{
		auto terrainMoi = GetParentObject();
		if (!ensure(terrainMoi))
		{
			return false;
		}

		FVector origin = terrainMoi->GetLocation();
		const auto graph2d = GetDocument()->FindSurfaceGraph(terrainMoi->ID);
		const FGraph2DEdge* graphEdge = graph2d->FindEdge(ID);
		if (ensure(graphEdge))
		{
			TArray<int32> vertexIDs;
			graphEdge->GetVertexIDs(vertexIDs);
			FVector2D startPosition(graph2d->FindVertex(vertexIDs[0])->Position);
			FVector2D endPosition(graph2d->FindVertex(vertexIDs[1])->Position);
			CachedPoints[0] = origin + startPosition.X * FVector::ForwardVector + startPosition.Y * FVector::RightVector;
			CachedPoints[1] = origin + endPosition.X * FVector::ForwardVector + endPosition.Y * FVector::RightVector;
			LineActor->Point1 = CachedPoints[0];
			LineActor->Point2 = CachedPoints[1];
			LineActor->UpdateTransform();
		}
		break;
	}

	case EObjectDirtyFlags::Visuals:
		return TryUpdateVisuals();

	default:
		break;
	}
		
	return true;
}
