// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "Objects/MetaGraph.h"

#include "DocumentManagement/ModumateDocument.h"
#include "Graph/Graph3D.h"
#include "Objects/ModumateObjectStatics.h"
#include "Objects/ModumateSymbolDeltaStatics.h"
#include "Objects/ModumateObjectDeltaStatics.h"
#include "BIMKernel/Presets/BIMPresetDocumentDelta.h"
#include "BIMKernel/Presets/BIMSymbolPresetData.h"
#include "Algo/ForEach.h"

AMOIMetaGraph::AMOIMetaGraph()
{
	FWebMOIProperty prop;

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
		GetAssembly().ObjectType = StateData.ObjectType;

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
		if (OutSideEffectDeltas && CachedSymbolGuid != StateData.AssemblyGUID && CachedSymbolGuid.IsValid()
			&& StateData.AssemblyGUID.IsValid())
		{
			SwapSymbol(OutSideEffectDeltas);
		}
		CachedSymbolGuid = StateData.AssemblyGUID;

		GetAssembly().PresetGUID = StateData.AssemblyGUID;  // For CS tool

		if (!SetupBoundingBox())
		{
			return false;
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
		FGuid symbolID;
		UModumateObjectStatics::IsObjectInSymbol(Document, ID, &symbolID);
		const FWebMOIProperty* formPropUpdateSymbolGuid = WebProperties.Find(TEXT("SymbolGuid"));
		FWebMOIProperty webPropSymbolGuid = *formPropUpdateSymbolGuid;
		webPropSymbolGuid.valueArray.Empty();
		webPropSymbolGuid.valueArray.Add(symbolID.ToString());
		OutMOI.presetId = symbolID;
		OutMOI.properties.Add(TEXT("SymbolGuid"), webPropSymbolGuid);

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
		
		return true;
	}

	return false;
}

FVector AMOIMetaGraph::GetCorner(int32 Index) const
{
	return CachedCorners[Index];
}

void AMOIMetaGraph::SwapSymbol(TArray<FDeltaPtr>* OutSideEffectDeltas)
{
	const FBIMPresetCollection& presets = Document->GetPresetCollection();
	const FBIMPresetInstance* oldSymbolPreset = presets.PresetFromGUID(CachedSymbolGuid);

	if (!ensure(oldSymbolPreset))
	{
		return;
	}

	FBIMSymbolCollectionProxy symbolCollection(&presets);

	// TODO: old symbol data to use collection proxy also.
	FBIMSymbolPresetData oldSymbolData;
	if (!ensure(oldSymbolPreset->TryGetCustomData(oldSymbolData) && symbolCollection.PresetDataFromGUID(StateData.AssemblyGUID)) )
	{
		return;
	}


	const FVector oldAnchor = oldSymbolData.Anchor;
	const FVector newAnchor = symbolCollection.PresetDataFromGUID(StateData.AssemblyGUID)->Anchor;
	const FVector deltaAnchor(oldAnchor - newAnchor);

	// Gut the group:
	FModumateObjectDeltaStatics::GetDeltasForGraphDelete(Document, ID, *OutSideEffectDeltas, false);

	TSet<AModumateObjectInstance*> groupMembers;
	UModumateObjectStatics::GetObjectsInGroups(Document, { ID }, groupMembers, true);
	TSet<int32> groupMemberIDs;
	Algo::ForEach(groupMembers, [&groupMemberIDs](const AModumateObjectInstance* o) { groupMemberIDs.Add(o->ID); });
	FModumateSymbolDeltaStatics::RemoveGroupMembersFromSymbol(groupMemberIDs, oldSymbolData);

	// Add new Symbol contents:
	FTransform transform(InstanceData.Rotation, InstanceData.Rotation.RotateVector(deltaAnchor) + InstanceData.Location);
	int32 nextID = Document->GetNextAvailableID();
	
	FModumateSymbolDeltaStatics::CreateDeltasForNewSymbolInstance(Document, ID, nextID, StateData.AssemblyGUID, symbolCollection,
		transform, *OutSideEffectDeltas, { StateData.AssemblyGUID });

	// New transform for the group
	auto groupDelta = MakeShared<FMOIDelta>();
	FMOIStateData& newGroupState = groupDelta->AddMutationState(this);
	FMOIMetaGraphData newGroupData(InstanceData);
	newGroupData.Location = transform.GetLocation();
	newGroupData.Rotation = transform.GetRotation();
	newGroupState.CustomData.SaveStructData(newGroupData);
	OutSideEffectDeltas->Add(groupDelta);

	// Update equivalence lists of old & new Symbol Presets:
	auto oldPresetDelta = presets.MakeUpdateDelta(CachedSymbolGuid);
	oldPresetDelta->NewState.SetCustomData(oldSymbolData);
	OutSideEffectDeltas->Add(oldPresetDelta);

	symbolCollection.GetPresetDeltas(*OutSideEffectDeltas);
}
