// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "Objects/MetaGraph.h"

#include "DocumentManagement/ModumateDocument.h"
#include "Graph/Graph3D.h"
#include "ModumateCore/ModumateObjectStatics.h"

AMOIMetaGraph::AMOIMetaGraph()
{ }

void AMOIMetaGraph::PostCreateObject(bool bNewObject)
{
	if (Document)
	{
		Super::PostCreateObject(bNewObject);
	}
}

// Called for structural dirty flag.
void AMOIMetaGraph::SetupDynamicGeometry()
{
	if (IsDirty(EObjectDirtyFlags::Structure))
	{
		CachedBounds.Init();
		TArray<int32> subtreeIDs;
		subtreeIDs.Push(ID);
		FBox objectBox;
		while (subtreeIDs.Num() > 0)
		{
			int32 graphID = subtreeIDs.Pop();
			const auto* graphObject = GetDocument()->GetObjectById(graphID);
			if (ensure(graphObject))
			{
				subtreeIDs.Append(graphObject->GetChildIDs());
			}
			FGraph3D* graph = GetDocument()->GetVolumeGraph(graphID);
			if (ensure(graph))
			{
				objectBox.Init();
				graph->GetBoundingBox(objectBox);
				CachedBounds += objectBox;
			}
		}
	}
}

void AMOIMetaGraph::GetStructuralPointsAndLines(TArray<FStructurePoint>& outPoints, TArray<FStructureLine>& outLines, bool bForSnapping /*= false*/, bool bForSelection /*= false*/) const
{
	if (!bForSnapping && !bForSelection && CachedBounds.IsValid)
	{
		TArray<FEdge> lines;
		TArray<FVector> corners;
		UModumateGeometryStatics::GetBoxCorners(CachedBounds, corners);
		if (!ensure(corners.Num() == 8))
		{
			return;
		}

		lines.Emplace(corners[0], corners[1]);
		lines.Emplace(corners[1], corners[3]);
		lines.Emplace(corners[3], corners[2]);
		lines.Emplace(corners[2], corners[0]);
		lines.Emplace(corners[4], corners[5]);
		lines.Emplace(corners[5], corners[7]);
		lines.Emplace(corners[7], corners[6]);
		lines.Emplace(corners[6], corners[4]);
		lines.Emplace(corners[0], corners[4]);
		lines.Emplace(corners[1], corners[5]);
		lines.Emplace(corners[2], corners[6]);
		lines.Emplace(corners[3], corners[7]);

		for (const auto& line : lines)
		{
			outLines.Emplace(line.Vertex[0], line.Vertex[1], 0, 0);
		}
	}
}

bool AMOIMetaGraph::ShowStructureOnSelection() const
{
	return true;
}
