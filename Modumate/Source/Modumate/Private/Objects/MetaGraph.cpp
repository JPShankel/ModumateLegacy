// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "Objects/MetaGraph.h"

#include "DocumentManagement/ModumateDocument.h"
#include "Graph/Graph3D.h"
#include "Objects/ModumateObjectStatics.h"
#include "Objects/ModumateSymbolDeltaStatics.h"
#include "BIMKernel/Presets/BIMPresetDocumentDelta.h"

const FString AMOIMetaGraph::PropertyName(TEXT("Name"));

AMOIMetaGraph::AMOIMetaGraph()
{
	FWebMOIProperty prop;

	prop.name = PropertyName;
	prop.type = EWebMOIPropertyType::text;
	prop.displayName = TEXT("Name");
	prop.isEditable = true;
	prop.isVisible = true;
	WebProperties.Add(prop.name, prop);

	prop.name = TEXT("SymbolGuid");
	prop.type = EWebMOIPropertyType::text;
	prop.displayName = TEXT("Symbol Guid");
	prop.isEditable = false;
	prop.isVisible = false;
	WebProperties.Add(prop.name, prop);
}

void AMOIMetaGraph::PostCreateObject(bool bNewObject)
{
	if (Document)
	{
		// Prevent assembly getting default Symbol GUID:
		CachedAssembly.ObjectType = StateData.ObjectType;
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

		CachedCorners.Empty();
		UModumateGeometryStatics::GetBoxCorners(CachedBounds, CachedCorners);
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

		CachedCorners.Empty();
		UModumateGeometryStatics::GetBoxCorners(CachedBounds, CachedCorners);
	}

	return true;
}

void AMOIMetaGraph::GetStructuralPointsAndLines(TArray<FStructurePoint>& outPoints, TArray<FStructureLine>& outLines, bool bForSnapping /*= false*/, bool bForSelection /*= false*/) const
{
	if (!bForSnapping && !bForSelection && CachedBounds.IsValid)
	{
		TArray<FEdge> lines;
		if (!ensure(CachedCorners.Num() == 8))
		{
			return;
		}

		lines.Emplace(CachedCorners[0], CachedCorners[1]);
		lines.Emplace(CachedCorners[1], CachedCorners[3]);
		lines.Emplace(CachedCorners[3], CachedCorners[2]);
		lines.Emplace(CachedCorners[2], CachedCorners[0]);
		lines.Emplace(CachedCorners[4], CachedCorners[5]);
		lines.Emplace(CachedCorners[5], CachedCorners[7]);
		lines.Emplace(CachedCorners[7], CachedCorners[6]);
		lines.Emplace(CachedCorners[6], CachedCorners[4]);
		lines.Emplace(CachedCorners[0], CachedCorners[4]);
		lines.Emplace(CachedCorners[1], CachedCorners[5]);
		lines.Emplace(CachedCorners[2], CachedCorners[6]);
		lines.Emplace(CachedCorners[3], CachedCorners[7]);

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
	if (DirtyFlag == EObjectDirtyFlags::Structure)
	{
		CachedAssembly.PresetGUID = StateData.AssemblyGUID;  // For CS tool
		if (!SetupBoundingBox())
		{
			return false;
		}

		if (OutSideEffectDeltas && Document->IsSymbolGroupDirty(ID))
		{
			int32 nextID = Document->GetNextAvailableID();
			FModumateSymbolDeltaStatics::PropagateChangedSymbolInstance(Document, nextID, ID, *OutSideEffectDeltas);
			Document->ClearDirtySymbolGroups();
		}
	}

	return true;
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
		const FGuid& symbolID = StateData.AssemblyGUID;
		const FWebMOIProperty* formPropUpdateSymbolGuid = WebProperties.Find(TEXT("SymbolGuid"));
		FWebMOIProperty webPropSymbolGuid = *formPropUpdateSymbolGuid;
		webPropSymbolGuid.valueArray.Empty();
		webPropSymbolGuid.valueArray.Add(symbolID.ToString());
		OutMOI.properties.Add(TEXT("SymbolGuid"), webPropSymbolGuid);

		if (symbolID.IsValid())
		{
			const auto * preset = Document->GetPresetCollection().PresetFromGUID(symbolID);
			if (ensure(preset))
			{
				const FWebMOIProperty* formPropUpdateSymbolName = WebProperties.Find(PropertyName);
				FWebMOIProperty webPropName = *formPropUpdateSymbolName;
				webPropName.valueArray.Empty();
				webPropName.valueArray.Add(preset->DisplayName.ToString());
				OutMOI.properties.Add(formPropUpdateSymbolName->name, webPropName);
			}
		}
		return true;
	}
	return false;

}

bool AMOIMetaGraph::FromWebMOI(const FString& InJson)
{
	if (Super::FromWebMOI(InJson) && StateData.AssemblyGUID.IsValid())
	{
		FWebMOI webMOI;
		if (!ReadJsonGeneric<FWebMOI>(InJson, &webMOI))
		{
			return false;
		}
		const auto* nameProp = webMOI.properties.Find(PropertyName);

		if (nameProp && nameProp->type == EWebMOIPropertyType::text && nameProp->valueArray.Num() == 1)
		{
			const FString& symbolName = nameProp->valueArray[0];
			const auto* preset = Document->GetPresetCollection().PresetFromGUID(StateData.AssemblyGUID);
			if (preset && preset->DisplayName.ToString() != symbolName)
			{
				auto deltaPtr = Document->GetPresetCollection().MakeUpdateDelta(preset->GUID);
				deltaPtr->NewState.DisplayName = FText::FromString(symbolName);
				Document->ApplyDeltas({ deltaPtr }, GetWorld());
			}
		}

		return true;
	}

	return false;
}

FVector AMOIMetaGraph::GetCorner(int32 Index) const
{
	return CachedCorners[Index];
}
