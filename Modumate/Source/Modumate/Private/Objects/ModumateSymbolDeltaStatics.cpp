// Copyright 2022 Modumate, Inc. All Rights Reserved.

#pragma once

#include "Objects/ModumateSymbolDeltaStatics.h"

#include "Objects/ModumateObjectStatics.h"
#include "DocumentManagement/ModumateDocument.h"
#include "Graph/Graph3DDelta.h"
#include "Objects/MOIDelta.h"
#include "Objects/MetaGraph.h"
#include "Objects/MetaEdgeSpan.h"
#include "Objects/MetaPlaneSpan.h"
#include "Objects/ModumateObjectDeltaStatics.h"
#include "BIMKernel/Presets/BIMSymbolPresetData.h"
#include "BIMKernel/Presets/BIMPresetDocumentDelta.h"
#include "Algo/ForEach.h"
#include "TransformTypes.h"


void FModumateSymbolDeltaStatics::GetDerivedDeltasFromDeltas(UModumateDocument* Doc, EMOIDeltaType DeltaType, const TArray<FDeltaPtr>& InDeltas, TArray<FDeltaPtr>& DerivedDeltas)
{
	for (const auto& delta : InDeltas)
	{
		delta->GetDerivedDeltas(Doc, DeltaType, DerivedDeltas);
	}
}

void FModumateSymbolDeltaStatics::CreateSymbolDerivedDeltasForMoi(UModumateDocument* Doc, const FMOIDeltaState& DeltaState, EMOIDeltaType DeltaType, TArray<FDeltaPtr>& OutDeltas)
{
	if (DeltaType != EMOIDeltaType::None && DeltaType != DeltaState.DeltaType)
	{
		return;
	}

	int32 id = DeltaState.OldState.ID;
	int32 groupId = UModumateObjectStatics::GetGroupIdForObject(Doc, id);
	if (groupId == MOD_ID_NONE)
	{	// Must be new MOI - assume created in current group.
		groupId = Doc->GetActiveVolumeGraphID();
	}

	// Return quickly if not in a symbol.
	if (groupId == Doc->GetRootVolumeGraphID() || UModumateTypeStatics::IsSpanObject(Doc->GetObjectById(id)))
	{
		return;
	}

	AMOIMetaGraph* groupMoi = Cast<AMOIMetaGraph>(Doc->GetObjectById(groupId));
	if (!groupMoi->InstanceData.SymbolID.IsValid())
	{
		return;
	}

	const FBIMPresetCollection& presets = Doc->GetPresetCollection();
	const FBIMPresetInstance* symbolPreset = presets.PresetFromGUID(groupMoi->InstanceData.SymbolID);
	if (!ensure(symbolPreset))
	{
		return;
	}

	switch (DeltaState.DeltaType)
	{
	case EMOIDeltaType::Mutate:
	{
		FBIMSymbolPresetData symbolData;
		if (ensure(symbolPreset->TryGetCustomData(symbolData)))
		{
			int primaryId = MOD_ID_NONE;
			auto newMoiDelta = MakeShared<FMOIDelta>();
			for (const auto& idMapping : symbolData.EquivalentIDs)
			{
				if (idMapping.Value.IDSet.Contains(id))
				{
					primaryId = idMapping.Key;
					for (int32 otherId : idMapping.Value.IDSet)
					{
						if (otherId != id)
						{
							const auto* otherMoi = Doc->GetObjectById(otherId);
							if (otherMoi)
							{
								FMOIDeltaState& otherState = newMoiDelta->States.Add_GetRef(DeltaState);
								otherState.OldState.ID = otherId;
								otherState.OldState.ParentID = otherMoi->GetParentID();
								otherState.NewState.ID = otherId;
								otherState.NewState.ParentID = otherState.OldState.ParentID;
							}
						}
					}

					break;
				}
			}

			if (newMoiDelta->IsValid())
			{
				OutDeltas.Add(newMoiDelta);
				// Update the Preset with the new State Data:
				FMOIStateData* newPresetData = symbolData.Members.Find(primaryId);
				if (ensure(newPresetData))
				{
					int32 refParent = newPresetData->ParentID;
					*newPresetData = DeltaState.NewState;
					// Restore original ID & parent
					newPresetData->ID = primaryId;
					newPresetData->ParentID = refParent;
					// JSON is used for serialization to Preset delta below.
					newPresetData->CustomData.SaveJsonFromCbor();
					TSharedPtr<FBIMPresetDelta> presetUpdateDelta(presets.MakeUpdateDelta(symbolPreset->GUID));
					presetUpdateDelta->NewState.SetCustomData(symbolData);
					OutDeltas.Add(presetUpdateDelta);
				}
			}

		}
		break;
	}

	case EMOIDeltaType::Create:
	{
		Doc->DirtySymbolGroup(groupId);  // Just re-duplicate group to other symbol instances.
		auto* moi = Doc->GetObjectById(groupId);
		if (ensure(moi))
		{   // Graph might've already been cleaned.
			moi->MarkDirty(EObjectDirtyFlags::Structure);
		}
		break;
	}

	case EMOIDeltaType::Destroy:
	{
		EObjectType objectType = DeltaState.OldState.ObjectType;
		if (objectType == EObjectType::OTMetaGraph)
		{
			FBIMSymbolPresetData symbolData;
			if (ensure(symbolPreset->TryGetCustomData(symbolData)))
			{
				TSet<AModumateObjectInstance*> groupMembers;
				UModumateObjectStatics::GetObjectsInGroups(Doc, { groupId }, groupMembers, true);
				TSet<int32> groupMemberIDs;
				Algo::ForEach(groupMembers, [&groupMemberIDs](const AModumateObjectInstance* o) { groupMemberIDs.Add(o->ID); });

				if (RemoveGroupMembersFromSymbol(groupMemberIDs, symbolData))
				{
					symbolData.EquivalentIDs.Compact();
					FBIMPresetInstance newSymbolPreset(*symbolPreset);
					newSymbolPreset.SetCustomData(symbolData);
					FDeltaPtr presetDelta(presets.MakeUpdateDelta(newSymbolPreset));
					OutDeltas.Add(presetDelta);
				}
			}

			Doc->ClearDirtySymbolGroup(groupId);
		}
		else if (objectType != EObjectType::OTMetaEdgeSpan && objectType != EObjectType::OTMetaPlaneSpan)
		{
			Doc->DirtySymbolGroup(groupId);  // Just re-duplicate group to other symbol instances.
			groupMoi->MarkDirty(EObjectDirtyFlags::Structure);
		}
		break;
	}

	default:
		break;
	}
}

void FModumateSymbolDeltaStatics::GetDerivedDeltasForGraph3d(UModumateDocument* Doc, const FGraph3DDelta* GraphDelta, EMOIDeltaType DeltaType,
	TArray<FDeltaPtr>& OutDeltas)
{
	if (GraphDelta->DeltaType != EGraph3DDeltaType::Edit)
	{
		return;
	}

	// Return quickly if no movements or not in Symbol group.
	int32 graphId = GraphDelta->GraphID;

	const AMOIMetaGraph* groupObj = Cast<AMOIMetaGraph>(Doc->GetObjectById(graphId));
	if (!groupObj || !groupObj->InstanceData.SymbolID.IsValid())
	{
		return;
	}

	if (graphId == MOD_ID_NONE || graphId == Doc->GetRootVolumeGraphID())
	{
		return;
	}

	if (GraphDelta->VertexDeletions.Num() + GraphDelta->VertexAdditions.Num() + GraphDelta->EdgeAdditions.Num()
		+ GraphDelta->EdgeDeletions.Num() + GraphDelta->FaceAdditions.Num() + GraphDelta->FaceDeletions.Num()
		+ GraphDelta->EdgeReversals.Num() + GraphDelta->FaceReversals.Num() + GraphDelta->FaceVertexIDUpdates.Num()
		+ GraphDelta->FaceContainmentUpdates.Num() != 0)
	{
		Doc->DirtySymbolGroup(graphId);  // Just re-duplicate group to other symbol instances.
		auto* moi = Doc->GetObjectById(graphId);
		if (ensure(moi))
		{   // Graph might've already been cleaned.
			moi->MarkDirty(EObjectDirtyFlags::Structure);
		}
		return;
	}

	if (GraphDelta->VertexMovements.Num() == 0)
	{
		return;
	}

	const FBIMPresetCollection& presets = Doc->GetPresetCollection();
	const FBIMPresetInstance* symbolPreset = presets.PresetFromGUID(groupObj->InstanceData.SymbolID);
	if (!ensure(symbolPreset))
	{
		return;
	}

	FBIMSymbolPresetData symbolData;
	if (!ensure(symbolPreset->TryGetCustomData(symbolData)))
	{
		return;
	}

	auto graphDelta = MakeShared<FGraph3DDelta>();
TMap<int32, TSharedPtr<FGraph3DDelta>> newDeltas;  // Per other-group

const FTransform thisTransformInverse(groupObj->GetWorldTransform().Inverse());

for (const auto& moveDelta : GraphDelta->VertexMovements)
{
	const int32 vertId = moveDelta.Key;
	for (const auto& idMapping : symbolData.EquivalentIDs)
	{
		if (idMapping.Value.IDSet.Contains(vertId))
		{
			for (int32 otherVertId : idMapping.Value.IDSet)
			{
				if (otherVertId != vertId)
				{
					const AMOIMetaGraph* otherGroupMoi = Cast<AMOIMetaGraph>(Doc->GetObjectById(Doc->FindGraph3DByObjID(otherVertId)));
					if (ensure(otherGroupMoi))
					{
						const FTransform vertTransform(thisTransformInverse * otherGroupMoi->GetWorldTransform());
						const int32 otherGroupId = otherGroupMoi->ID;
						if (!newDeltas.Contains(otherGroupId))
						{
							newDeltas.Add(otherGroupId, MakeShared<FGraph3DDelta>(otherGroupId));
						}
						newDeltas[otherGroupId]->VertexMovements.Add(otherVertId,
							{ vertTransform.TransformPosition(moveDelta.Value.Key), vertTransform.TransformPosition(moveDelta.Value.Value) });
					}

				}
			}
		}
	}
}

for (auto& delta : newDeltas)
{
	OutDeltas.Add(delta.Value);
}

}

bool FModumateSymbolDeltaStatics::CreateDeltasForNewSymbol(UModumateDocument* Doc, const AModumateObjectInstance* SymbolGroup, TArray<FDeltaPtr>& OutDeltas)
{
	const AMOIMetaGraph* group = Cast<const AMOIMetaGraph>(SymbolGroup);
	if (!group)
	{
		return false;
	}

	FMOIMetaGraphData newGroupData = group->InstanceData;
	if (ensure(!newGroupData.SymbolID.IsValid()) && group->ID != Doc->GetRootVolumeGraphID())
	{
		static const FName symbolNodeType(TEXT("0Symbol"));
		static const FString symbolNcp(TEXT("Symbol"));

		FBIMPresetCollection& presets = Doc->GetPresetCollection();
		TArray<FGuid> symbols;
		presets.GetPresetsForNCP(FBIMTagPath(symbolNcp), symbols, true);
		TArray<const FText*> symbolNames;
		Algo::Transform(symbols, symbolNames, [&presets](const FGuid& id) { return &presets.PresetFromGUID(id)->DisplayName; });
		
		FBIMPresetInstance newSymbolPreset;
		newSymbolPreset.NodeType = symbolNodeType;
		newSymbolPreset.NodeScope = EBIMValueScope::Symbol;
		newSymbolPreset.MyTagPath.FromString(symbolNcp);

		int32 idNumber = 1;
		FText displayName;
		do
		{
			displayName = FText::FromString(FString::Printf(TEXT("Symbol %d"), idNumber++));
		} while (symbolNames.FindByPredicate([&displayName](const FText* s) {return displayName.EqualToCaseIgnored(*s); }));
		newSymbolPreset.DisplayName = displayName;

		FBIMSymbolPresetData symbolData;
		if (!CreatePresetDataForSymbol(Doc, group, FTransform::Identity, symbolData))
		{
			return false;
		}

		newSymbolPreset.SetCustomData(symbolData);
		FDeltaPtr presetDelta(presets.MakeCreateNewDelta(newSymbolPreset));

		newGroupData.SymbolID = newSymbolPreset.GUID;
		// Reset transform so Preset postion data is correct for this group (transform is identity above).
		newGroupData.Location = FVector::ZeroVector;
		newGroupData.Rotation = FQuat::Identity;
		FMOIStateData groupInstanceData(group->GetStateData());
		groupInstanceData.CustomData.SaveStructData(newGroupData, UE_EDITOR);

		auto groupDelta = MakeShared<FMOIDelta>();
		groupDelta->AddMutationState(group, group->GetStateData(), groupInstanceData);
		OutDeltas.Add(presetDelta);
		OutDeltas.Add(groupDelta);
		return true;
	}

	return false;
}

bool FModumateSymbolDeltaStatics::CreatePresetDataForSymbol(UModumateDocument* Doc, const AModumateObjectInstance* SymbolGroup, const FTransform& Transform,
	FBIMSymbolPresetData& OutPreset)
{
	const AMOIMetaGraph* group = Cast<const AMOIMetaGraph>(SymbolGroup);
	if (!ensure(group))
	{
		return false;
	}

	TSet<AModumateObjectInstance*> groupMembers;
	UModumateObjectStatics::GetObjectsInGroups(Doc, { group->ID }, groupMembers, true);
	for (const auto* moi : groupMembers)
	{
		if (UModumateTypeStatics::Graph3DObjectTypeFromObjectType(moi->GetObjectType()) == EGraph3DObjectType::None
			&& UModumateTypeStatics::Graph2DObjectTypeFromObjectType(moi->GetObjectType()) == EGraphObjectType::None)
		{
			OutPreset.Members.Add(moi->ID, moi->GetStateData()).CustomData.SaveJsonFromCbor();
			if (moi->GetObjectType() == EObjectType::OTSurfaceGraph)
			{
				const TSharedPtr<FGraph2D> graph2d = Doc->FindSurfaceGraph(moi->ID);
				if (ensure(graph2d))
				{
					graph2d->ToDataRecord(&OutPreset.SurfaceGraphs.FindOrAdd(moi->ID), true, true);
				}
			}
		}
		OutPreset.EquivalentIDs.Add(moi->ID, { {moi->ID} });
	}

	const FGraph3D* graph3d = Doc->GetVolumeGraph(group->ID);
	if (ensure(graph3d))
	{
		FTransform3d transform(Transform);
		graph3d->Save(&OutPreset.Graph3d);
		for (auto& vert : OutPreset.Graph3d.Vertices)
		{
			vert.Value.Position = transform.TransformPosition(vert.Value.Position);
		}
		return true;
	}

	return false;
}

// Create Deltas for new Symbol instance from Symbol Preset. Assumes Group MOI & Graph3d are created elsewhere.
bool FModumateSymbolDeltaStatics::CreateDeltasForNewSymbolInstance(UModumateDocument* Doc, int32 GroupID, int32& NextID, FBIMSymbolPresetData& Preset,
	const FTransform& Transform, TArray<FDeltaPtr>& OutDeltas)
{
	TMap<int32, int32> oldIDToNewID;
	oldIDToNewID.Add(MOD_ID_NONE, MOD_ID_NONE);

	// First, pre-allocate new IDs for all MOIs.
	for (const auto& kvp : Preset.Members)
	{
		oldIDToNewID.Add(kvp.Key, NextID++);
	}

	// Similar to FModumateObjectDeltaStatics::DuplicateGroups.
	// Duplicate Preset Graph3d:
	const FGraph3DRecordV1& graph = Preset.Graph3d;
	auto newElementsDelta = MakeShared<FGraph3DDelta>();
	newElementsDelta->GraphID = GroupID;
	for (const auto& kvp : graph.Vertices)
	{
		newElementsDelta->VertexAdditions.Add(NextID, Transform.TransformPosition(FVector(kvp.Value.Position)) );
		oldIDToNewID.Add(kvp.Key, NextID++);
	}

	for (const auto& kvp : graph.Edges)
	{
		FGraph3DObjDelta newEdge(FGraphVertexPair(oldIDToNewID[kvp.Value.StartVertexID], oldIDToNewID[kvp.Value.EndVertexID]));
		newElementsDelta->EdgeAdditions.Add(NextID, MoveTemp(newEdge));
		oldIDToNewID.Add(kvp.Key, NextID++);
	}

	for (const auto& kvp : graph.Faces)
	{
		TArray<int32> newVertices;
		Algo::ForEach(kvp.Value.VertexIDs, [&](int32 v)
			{ newVertices.Add(oldIDToNewID[v]); });

		newElementsDelta->FaceAdditions.Add(NextID, FGraph3DObjDelta(newVertices));
		oldIDToNewID.Add(kvp.Key, NextID++);
	}

	// Map old contained/containing face IDs:
	for (auto& kvp : newElementsDelta->FaceAdditions)
	{
		kvp.Value.ContainingObjID = oldIDToNewID[kvp.Value.ContainingObjID];
		TSet<int32> containedFaces;
		for (int32 oldVert : kvp.Value.ContainedObjIDs)
		{
			containedFaces.Add(oldIDToNewID[oldVert]);
		}
		kvp.Value.ContainedObjIDs = containedFaces;
	}

	OutDeltas.Add(newElementsDelta);

	// Duplicate surface-graph MOIs before actual surface graphs:
	auto surfaceGraphMoiDelta = MakeShared<FMOIDelta>();
	for (const auto& kvp : Preset.Members)
	{
		const auto& moiDef = kvp.Value;
		if (moiDef.ObjectType == EObjectType::OTSurfaceGraph)
		{
			FMOIStateData newState(moiDef);
			newState.ID = oldIDToNewID[newState.ID];
			newState.ParentID = oldIDToNewID[newState.ParentID];
			surfaceGraphMoiDelta->AddCreateDestroyState(newState, EMOIDeltaType::Create);
		}
	}
	if (surfaceGraphMoiDelta->IsValid())
	{
		OutDeltas.Add(surfaceGraphMoiDelta);
	}

	// Duplicate surface graphs:
	for (const auto& graph2dKvp: Preset.SurfaceGraphs)
	{
		int32 newGraphID = oldIDToNewID[graph2dKvp.Key];
		auto makeGraph2dDelta = MakeShared<FGraph2DDelta>(newGraphID, EGraph2DDeltaType::Add);
		auto graph2dDelta = MakeShared<FGraph2DDelta>(newGraphID);
		const FGraph2DRecord& graph2d = graph2dKvp.Value;

		for (const auto& kvp: graph2d.Vertices)
		{
			oldIDToNewID.Add(kvp.Key, NextID);
			graph2dDelta->AddNewVertex(kvp.Value, NextID);
		}

		for (const auto& kvp: graph2d.Edges)
		{
			oldIDToNewID.Add(kvp.Key, NextID);
			graph2dDelta->AddNewEdge(FGraphVertexPair(oldIDToNewID[kvp.Value.VertexIDs[0]], oldIDToNewID[kvp.Value.VertexIDs[1]]), NextID);
		}

		for (const auto& kvp: graph2d.Polygons)
		{
			TArray<int32> newVertices;
			Algo::ForEach(kvp.Value.VertexIDs, [&](int32 v)
				{ newVertices.Add(oldIDToNewID[v]); });
			oldIDToNewID.Add(kvp.Key, NextID);
			graph2dDelta->AddNewPolygon(newVertices, NextID);
		}

		OutDeltas.Add(makeGraph2dDelta);
		OutDeltas.Add(graph2dDelta);
	}

	TArray<FMOIStateData> newStates;

	// Duplicate MOIs:
	for (const auto& kvp : Preset.Members)
	{
		const auto& moiDef = kvp.Value;
		if (moiDef.ObjectType != EObjectType::OTSurfaceGraph && 
			UModumateTypeStatics::Graph3DObjectTypeFromObjectType(moiDef.ObjectType) == EGraph3DObjectType::None)
		{
			FMOIStateData& newState = newStates.Emplace_GetRef(moiDef);
			newState.ID = oldIDToNewID[newState.ID];
			newState.ParentID = oldIDToNewID[newState.ParentID];

			// Fix-up ID references.
			switch (newState.ObjectType)
			{
			case EObjectType::OTMetaEdgeSpan:
			{
				FMOIMetaEdgeSpanData spanInstanceData;
				newState.CustomData.LoadStructData(spanInstanceData);
				Algo::ForEach(spanInstanceData.GraphMembers, [&oldIDToNewID](int32& S)
					{ S = oldIDToNewID[S]; });
				newState.CustomData.SaveStructData(spanInstanceData, UE_EDITOR);
				break;
			}

			case EObjectType::OTMetaPlaneSpan:
			{
				FMOIMetaPlaneSpanData spanInstanceData;
				newState.CustomData.LoadStructData(spanInstanceData);
				Algo::ForEach(spanInstanceData.GraphMembers, [&oldIDToNewID](int32& S)
					{ S = oldIDToNewID[S]; });
				newState.CustomData.SaveStructData(spanInstanceData, UE_EDITOR);
				break;
			}

			default:
				break;
			}

		}
	}

	auto delta = MakeShared<FMOIDelta>();
	delta->AddCreateDestroyStates(newStates, EMOIDeltaType::Create);
	OutDeltas.Add(delta);

	// Update Preset equivalency sets with new IDs.
	for (const auto& kvp : oldIDToNewID)
	{
		if (kvp.Key != MOD_ID_NONE && ensure(Preset.EquivalentIDs.Contains(kvp.Key)))
		{
			Preset.EquivalentIDs[kvp.Key].IDSet.Add(kvp.Value);
		}
	}

	return true;
}

void FModumateSymbolDeltaStatics::PropagateChangedSymbolInstances(UModumateDocument* Doc, int32& NextID, const TSet<int32>& GroupIDs,
	TArray<FDeltaPtr>& OutDeltas)
{
	for (int32 group: GroupIDs)
	{
		PropagateChangedSymbolInstance(Doc, NextID, group, OutDeltas);
	}
}

void FModumateSymbolDeltaStatics::PropagateChangedSymbolInstance(UModumateDocument* Doc, int32& NextID, int32 GroupID, TArray<FDeltaPtr>& OutDeltas)
{
	const FBIMPresetCollection& presets = Doc->GetPresetCollection();
	const AMOIMetaGraph* groupMoi = Cast<AMOIMetaGraph>(Doc->GetObjectById(GroupID));
	const FBIMPresetInstance* symbolPreset = groupMoi ? presets.PresetFromGUID(groupMoi->InstanceData.SymbolID) : nullptr;

	if (!ensure(symbolPreset != nullptr))
	{
		return;
	}

	FBIMSymbolPresetData symbolData;
	if (!ensure(symbolPreset->TryGetCustomData(symbolData) && symbolData.EquivalentIDs.Num() != 0))
	{
		return;
	}

	TSet<int32> allGroupIDs;
	for (int32 itemId : symbolData.EquivalentIDs.begin()->Value.IDSet)
	{
		allGroupIDs.Add(UModumateObjectStatics::GetGroupIdForObject(Doc, itemId));
	}

	FTransform masterTransform(groupMoi->GetWorldTransform().Inverse());

	FBIMSymbolPresetData newSymbolData;
	if (!ensure(CreatePresetDataForSymbol(Doc, groupMoi, masterTransform, newSymbolData)))
	{
		return;
	}

	FBIMPresetInstance newSymbolPreset(*symbolPreset);

	for (int32 otherGroup : allGroupIDs)
	{
		if (otherGroup == GroupID)
		{
			continue;
		}

		AModumateObjectInstance* otherGroupMoi = Doc->GetObjectById(otherGroup);
		if (ensure(otherGroupMoi))
		{
			FModumateObjectDeltaStatics::GetDeltasForGraphDelete(Doc, otherGroup, OutDeltas, false);
			FTransform transform(otherGroupMoi->GetWorldTransform());
			ensure(CreateDeltasForNewSymbolInstance(Doc, otherGroup, NextID, newSymbolData, transform, OutDeltas));
		}
	}

	newSymbolPreset.SetCustomData(newSymbolData);
	FDeltaPtr presetDelta(presets.MakeUpdateDelta(newSymbolPreset));
	OutDeltas.Add(presetDelta);
}

bool FModumateSymbolDeltaStatics::CreateNewSymbol(UModumateDocument* Doc, const AModumateObjectInstance* Group)
{
	if (Group->GetObjectType() == EObjectType::OTMetaGraph)
	{
		TArray<FDeltaPtr> symbolDeltas;
		const AMOIMetaGraph* group = Cast<const AMOIMetaGraph>(Group);
		if (!group->InstanceData.SymbolID.IsValid() && ensure(CreateDeltasForNewSymbol(Doc, group, symbolDeltas)) )
		{
			return Doc->ApplyDeltas(symbolDeltas, Doc->GetWorld());
		}
	}

	return false;
}

bool FModumateSymbolDeltaStatics::DetachSymbol(UModumateDocument* Doc, const AModumateObjectInstance* Group)
{
	if (Group->GetObjectType() == EObjectType::OTMetaGraph)
	{
		TArray<FDeltaPtr> symbolDeltas;
		const AMOIMetaGraph* group = Cast<const AMOIMetaGraph>(Group);
		if (group->InstanceData.SymbolID.IsValid())
		{
			FBIMPresetCollection& presets = Doc->GetPresetCollection();
			const FBIMPresetInstance* symbolPreset = presets.PresetFromGUID(group->InstanceData.SymbolID);
			if (!ensure(symbolPreset))
			{
				return false;
			}
			FBIMSymbolPresetData symbolData;
			if (!ensure(symbolPreset->TryGetCustomData(symbolData)) )
			{
				return false;
			}

			TSet<AModumateObjectInstance*> groupMembers;
			UModumateObjectStatics::GetObjectsInGroups(Doc, { group->ID }, groupMembers, true);
			TSet<int32> groupMemberIDs;
			Algo::ForEach(groupMembers, [&groupMemberIDs](const AModumateObjectInstance* o) { groupMemberIDs.Add(o->ID); });
			RemoveGroupMembersFromSymbol(groupMemberIDs, symbolData);

			// Take out Symbol GUID from Group:
			FMOIMetaGraphData groupData(group->InstanceData);
			groupData.SymbolID.Invalidate();
			FMOIStateData newState(group->GetStateData());
			newState.CustomData.SaveStructData(groupData, UE_EDITOR);
			auto moiDelta = MakeShared<FMOIDelta>();
			moiDelta->AddMutationState(group, group->GetStateData(), newState);
			symbolDeltas.Add(moiDelta);

			// Update Symbol Preset:
			FBIMPresetInstance newSymbolPreset(*symbolPreset);
			newSymbolPreset.SetCustomData(symbolData);
			symbolDeltas.Add(presets.MakeUpdateDelta(newSymbolPreset));

			return Doc->ApplyDeltas(symbolDeltas, Doc->GetWorld());
		}
	}

	return false;
}

bool FModumateSymbolDeltaStatics::RemoveGroupMembersFromSymbol(const TSet<int32> GroupMembers, FBIMSymbolPresetData& SymbolData)
{
	bool bReturn = false;
	for (auto& idSet : SymbolData.EquivalentIDs)
	{
		for (auto id = idSet.Value.IDSet.CreateIterator(); id; ++id)
		{
			if (GroupMembers.Contains(*id))
			{
				id.RemoveCurrent();
				bReturn = true;
				idSet.Value.IDSet.Compact();  // Unclear why this is necessary.
				break;
			}
		}
	}

	return bReturn;
}
