// Copyright 2022 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Tools/EditModelUngroupTool.h"

#include "ModumateCore/ModumateObjectDeltaStatics.h"
#include "UnrealClasses/EditModelGameState.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "Objects/MetaGraph.h"

UUngroupTool::UUngroupTool()
	: Super()
{ }

bool UUngroupTool::Activate()
{
	if (!Super::Activate())
	{
		return false;
	}

	AEditModelPlayerState* emPlayerState = Controller->GetPlayerState<AEditModelPlayerState>();
	UModumateDocument* doc = GameState->Document;

	if (!ensure(emPlayerState && doc))
	{
		return false;
	}

	// Tool is single-use
	Controller->ToolAbortUse();

	const auto& selectedGroups = emPlayerState->SelectedGroupObjects;
	ReselectionItems.Empty();

	TArray<FDeltaPtr> deltas;

	int32 nextID = doc->GetNextAvailableID();

	for (auto* obj: selectedGroups)
	{
		if (!ExplodeGroup(doc, obj, nextID, deltas))
		{
			return false;
		}
	}
	
	TSet<AModumateObjectInstance*> reselectObjects(emPlayerState->SelectedObjects);  // Reselect simple objects afterwards
	TSet<AModumateObjectInstance*> reselectGroups;

	emPlayerState->DeselectAll(false);

	if (doc->ApplyDeltas(deltas, GetWorld()))
	{
		for (int32 item : ReselectionItems)
		{
			AModumateObjectInstance* object = doc->GetObjectById(item);
			if (object)
			{
				if (object->GetObjectType() == EObjectType::OTMetaGraph)
				{
					reselectGroups.Add(object);
				}
				else
				{
					object->ClearAdjustmentHandles();
					reselectObjects.Add(object);
				}
			}
		}

		ReselectionItems.Empty();
		emPlayerState->SetObjectsSelected(reselectObjects, true, false);
		emPlayerState->SetGroupObjectsSelected(reselectGroups, true, false);

		return true;
	}

	return false;
}

bool UUngroupTool::ExplodeGroup(UModumateDocument* Doc, AModumateObjectInstance* GroupObject, int32& NextID, TArray<FDeltaPtr>& OutDeltas)
{
	const int32 currentGroupID = GroupObject->ID;

	const int32 parentGroup = GroupObject->GetParentID();
	if (parentGroup == MOD_ID_NONE)
	{   // Can't explode root group
		return true;
	}

	// Hoist up any child groups.
	const TArray<int32> childIDs(GroupObject->GetChildIDs());
	for (int32 childID: childIDs)
	{
		auto* childObject = Cast<AMOIMetaGraph>(Doc->GetObjectById(childID));
		if (ensure(childObject))
		{
			auto delta = MakeShared<FMOIDelta>();
			const FMOIStateData& oldState = childObject->GetStateData();
			FMOIStateData newState(oldState);
			newState.ParentID = parentGroup;
			delta->AddMutationState(childObject, oldState, newState);
			OutDeltas.Add(delta);
			ReselectionItems.Add(childID);
		}
	}

	FGraph3D* graph = Doc->GetVolumeGraph(currentGroupID);
	if (!ensureAlways(graph))
	{
		return false;
	}

	TArray<int32> allGraphElements;
	graph->GetAllObjects().GenerateKeyArray(allGraphElements);
	TArray<FGraph3DDelta> deleteGraphDeltas;
	graph->GetDeltaForDeleteObjects(allGraphElements, deleteGraphDeltas, NextID, true);

	// Note all members of group for selection after ungrouping.
	for (int32 graphItem: allGraphElements)
	{
		AModumateObjectInstance* metaMOI = Doc->GetObjectById(graphItem);
		if (metaMOI)
		{
			auto& children = metaMOI->GetChildIDs();
			if (children.Num() > 0)
			{
				ReselectionItems.Append(children);
			}
			else
			{
				ReselectionItems.Add(graphItem);
			}

		}
	}

	// Remove old graph elements:
	for (auto& graphDelta: deleteGraphDeltas)
	{
		OutDeltas.Add(MakeShared<FGraph3DDelta>(MoveTemp(graphDelta)));
	}

	// Remove old graph:
	auto removeGraphDelta = MakeShared<FGraph3DDelta>(currentGroupID);
	removeGraphDelta->DeltaType = EGraph3DDeltaType::Remove;
	OutDeltas.Add(removeGraphDelta);
	auto removeObjectDelta = MakeShared<FMOIDelta>();
	removeObjectDelta->AddCreateDestroyState(GroupObject->GetStateData(), EMOIDeltaType::Destroy);
	OutDeltas.Add(removeObjectDelta);
	
	FModumateObjectDeltaStatics::MergeGraphToCurrentGraph(Doc, graph, NextID, OutDeltas);

	return true;
}
