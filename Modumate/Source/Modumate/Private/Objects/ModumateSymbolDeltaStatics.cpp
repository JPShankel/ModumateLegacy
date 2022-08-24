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
#include "BIMKernel/Presets/BIMSymbolPresetData.h"
#include "BIMKernel/Presets/BIMPresetDocumentDelta.h"
#include "Algo/ForEach.h"



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

	// Return quickly if not in a symbol.
	if (groupId == MOD_ID_NONE || groupId == Doc->GetRootVolumeGraphID() || UModumateTypeStatics::IsSpanObject(Doc->GetObjectById(id)))
	{
		return;
	}

	const AMOIMetaGraph* groupMoi = Cast<AMOIMetaGraph>(Doc->GetObjectById(groupId));
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
		break;
	}

	case EMOIDeltaType::Destroy:
	{
		FBIMSymbolPresetData symbolData;
		if (ensure(symbolPreset->TryGetCustomData(symbolData)))
		{
			int32 primaryId = MOD_ID_NONE;
			for (const auto& idMapping : symbolData.EquivalentIDs)
			{
				if (idMapping.Value.IDSet.Contains(id))
				{
					primaryId = idMapping.Key;
					auto newDelta = MakeShared<FMOIDelta>();
					for (int32 otherId : idMapping.Value.IDSet)
					{
						if (otherId != id)
						{
							const auto* otherMoi = Doc->GetObjectById(otherId);
							if (otherMoi)
							{
								newDelta->AddCreateDestroyState(otherMoi->GetStateData(), EMOIDeltaType::Destroy);
							}
						}
					}

					if (newDelta->IsValid())
					{
						OutDeltas.Add(newDelta);
					}

					break;
				}

			}

			if (ensure(primaryId != MOD_ID_NONE))
			{
				symbolData.EquivalentIDs.Remove(primaryId);
				symbolData.Members.Remove(primaryId);
				symbolData.EquivalentIDs.Compact();
				symbolData.Members.Compact();

				// Update the Preset with the new State Data:
				TSharedPtr<FBIMPresetDelta> presetUpdateDelta(presets.MakeUpdateDelta(symbolPreset->GUID));
				presetUpdateDelta->NewState.SetCustomData(symbolData);
				OutDeltas.Add(presetUpdateDelta);
			}
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
	// Return quickly if no movements or not in Symbol group.
	int32 graphId = GraphDelta->GraphID;
	if (GraphDelta->VertexMovements.Num() == 0 || graphId == MOD_ID_NONE || graphId == Doc->GetRootVolumeGraphID())
	{
		return;
	}

	const AMOIMetaGraph* groupObj = Cast<AMOIMetaGraph>(Doc->GetObjectById(graphId));
	if (!ensure(groupObj) || !groupObj->InstanceData.SymbolID.IsValid())
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
		static const FString symbolNcp(TEXT("Part_0FlexDims3Fixed_Symbol"));

		FBIMPresetCollection& presets = Doc->GetPresetCollection();
		FBIMPresetInstance newSymbolPreset;
		newSymbolPreset.DisplayName = FText::FromString(TEXT("ExampleSymbol"));
		newSymbolPreset.NodeType = symbolNodeType;
		newSymbolPreset.MyTagPath.FromString(symbolNcp);

		FBIMSymbolPresetData symbolData;
		if (!CreatePresetDataForNewSymbol(Doc, group, symbolData))
		{
			return false;
		}

		newSymbolPreset.SetCustomData(symbolData);
		FDeltaPtr presetDelta(presets.MakeCreateNewDelta(newSymbolPreset));

		newGroupData.SymbolID = newSymbolPreset.GUID;
		// Reset transform so frozen Preset data is absolute:
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

bool FModumateSymbolDeltaStatics::CreatePresetDataForNewSymbol(UModumateDocument* Doc, const AModumateObjectInstance* SymbolGroup, FBIMSymbolPresetData& OutPreset)
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
		graph3d->Save(&OutPreset.Graph3d);
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
