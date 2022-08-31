// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "Objects/MetaGraph.h"

#include "DocumentManagement/ModumateDocument.h"
#include "Graph/Graph3D.h"
#include "Objects/ModumateObjectStatics.h"

AMOIMetaGraph::AMOIMetaGraph()
{
	FWebMOIProperty prop;

	prop.Name = TEXT("SymbolGuid");
	prop.Type = EWebMOIPropertyType::text;
	prop.DisplayName = TEXT("Symbol Guid");
	prop.isEditable = false;
	prop.isVisible = false;
	WebProperties.Add(prop.Name, prop);
}

void AMOIMetaGraph::PostCreateObject(bool bNewObject)
{
	if (Document)
	{
		Super::PostCreateObject(bNewObject);
	}
}

// Called for structural dirty flag.
bool AMOIMetaGraph::SetupBoundingBox()
{
	// Don't calculate bounding box for root graph:
	if (IsDirty(EObjectDirtyFlags::Structure) && ID != GetDocument()->GetRootVolumeGraphID())
	{
		FBox oldBox(CachedBounds);
		CachedBounds.Init();
		FBox objectBox;

		TArray<AModumateObjectInstance*> childObjects;
		for (int32 childID : GetChildIDs())
		{
			auto* graphObject = GetDocument()->GetObjectById(childID);
			if (ensure(graphObject))
			{
				if (graphObject->IsDirty(EObjectDirtyFlags::Structure) || graphObject->IsDirty(EObjectDirtyFlags::Visuals))
				{
					return false;
				}
				childObjects.Add(graphObject);
			}
		}

		TSet<AModumateObjectInstance*> groupMembers;
		UModumateObjectStatics::GetObjectsInGroups(Document, { ID }, groupMembers);

		for (auto* groupMember : groupMembers)
		{
			if (groupMember->IsDirty(EObjectDirtyFlags::Structure) || groupMember->IsDirty(EObjectDirtyFlags::Visuals))
			{
				return false;
			}
		}

		FGraph3D* myGraph = GetDocument()->GetVolumeGraph(ID);
		if (ensure(myGraph))
		{
			objectBox.Init();
			myGraph->GetBoundingBox(objectBox);
			CachedBounds += objectBox;
		}

		TArray<FStructurePoint> structurePoints;  // unused
		TArray<FStructureLine> structureLines;

		GetStructuralPointsAndLines(structurePoints, structureLines, false, false);

		for (const auto* graphObject: childObjects)
		{
			graphObject->GetStructuralPointsAndLines(structurePoints, structureLines, false, false);
		}

		for (const auto* groupMember : groupMembers)
		{
			if (UModumateTypeStatics::Graph3DObjectTypeFromObjectType(groupMember->GetObjectType()) == EGraph3DObjectType::None)
			{
				groupMember->GetStructuralPointsAndLines(structurePoints, structureLines, false, true);
			}
		}

		for (const auto& l : structureLines)
		{
			CachedBounds += l.P1;
			CachedBounds += l.P2;
		}

		if (!(CachedBounds == oldBox) && GetParentObject())
		{
			GetParentObject()->MarkDirty(EObjectDirtyFlags::Structure);
		}
	}

	return true;
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

bool AMOIMetaGraph::CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas)
{
	Super::CleanObject(DirtyFlag, OutSideEffectDeltas);
	return DirtyFlag == EObjectDirtyFlags::Structure ? SetupBoundingBox() : true;
}

bool AMOIMetaGraph::GetUpdatedVisuals(bool& bOutVisible, bool& bOutCollisionEnabled)
{
	bOutVisible = false; bOutCollisionEnabled = false;
	return true;
}

FTransform AMOIMetaGraph::GetWorldTransform() const
{
	return FTransform(InstanceData.Rotation, InstanceData.Location);
}

bool AMOIMetaGraph::ToWebMOI(FWebMOI& OutMOI) const
{
	if (AModumateObjectInstance::ToWebMOI(OutMOI))
	{
		const FWebMOIProperty* formPropUpdateSymbolGuid = WebProperties.Find(TEXT("SymbolGuid"));
		FWebMOIProperty webPropSymbolGuid = *formPropUpdateSymbolGuid;
		webPropSymbolGuid.ValueArray.Empty();
		webPropSymbolGuid.ValueArray.Add(InstanceData.SymbolID.ToString());
		OutMOI.Properties.Add(TEXT("SymbolGuid"), webPropSymbolGuid);

		return true;
	}
	return false;

}
