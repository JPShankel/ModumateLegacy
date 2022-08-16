// Copyright 2022 Modumate, Inc. All Rights Reserved.

#pragma once

#include "Objects/ModumateDerivedDeltaStatics.h"

#include "Objects/ModumateObjectStatics.h"
#include "DocumentManagement/ModumateDocument.h"
#include "Graph/Graph3DDelta.h"
#include "Objects/MOIDelta.h"
#include "Objects/MetaGraph.h"
#include "BIMKernel/Presets/BIMSymbolPresetData.h"
#include "BIMKernel/Presets/BIMPresetDocumentDelta.h"



void FModumateDerivedDeltaStatics::GetDerivedDeltasFromDeltas(UModumateDocument* Doc, EMOIDeltaType DeltaType, const TArray<FDeltaPtr>& InDeltas, TArray<FDeltaPtr>& DerivedDeltas)
{
	for (const auto& delta : InDeltas)
	{
		delta->GetDerivedDeltas(Doc, DeltaType, DerivedDeltas);
	}
}

void FModumateDerivedDeltaStatics::CreateSymbolDerivedDeltasForMoi(UModumateDocument* Doc, const FMOIDeltaState& DeltaState, EMOIDeltaType DeltaType, TArray<FDeltaPtr>& OutDeltas)
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

void FModumateDerivedDeltaStatics::GetDerivedDeltasForGraph3d(UModumateDocument* Doc, const FGraph3DDelta* GraphDelta, EMOIDeltaType DeltaType,
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

bool FModumateDerivedDeltaStatics::CreateDeltasForNewSymbol(UModumateDocument* Doc, const AModumateObjectInstance* SymbolGroup, TArray<FDeltaPtr>& OutDeltas)
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
		TSet<AModumateObjectInstance*> groupMembers;
		UModumateObjectStatics::GetObjectsInGroups(Doc, { group->ID }, groupMembers);
		for (const auto* moi : groupMembers)
		{
			symbolData.Members.Add(moi->ID, moi->GetStateData()).CustomData.SaveJsonFromCbor();
			symbolData.EquivalentIDs.Add(moi->ID, { {moi->ID} });
		}
		newSymbolPreset.SetCustomData(symbolData);
		FDeltaPtr presetDelta(presets.MakeCreateNewDelta(newSymbolPreset));

		newGroupData.SymbolID = newSymbolPreset.GUID;
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
