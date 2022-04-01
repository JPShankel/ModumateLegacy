// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Tools/EditModelGroupTool.h"

#include "Objects/ModumateObjectDeltaStatics.h"
#include "UnrealClasses/EditModelGameState.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "Objects/MetaGraph.h"
#include "Objects/ModumateObjectStatics.h"
#include "Objects/MetaEdgeSpan.h"
#include "Objects/MetaPlaneSpan.h"

UGroupTool::UGroupTool()
	: Super()
{ }

bool UGroupTool::Activate()
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

	bool bRetVal = true;

	TSet<const AModumateObjectInstance*> massingObjects;

	const auto& selectedObjects = emPlayerState->SelectedObjects;
	for (auto* obj: selectedObjects)
	{
		while (obj && (UModumateTypeStatics::Graph3DObjectTypeFromObjectType(obj->GetObjectType()) == EGraph3DObjectType::None)
			&& !UModumateTypeStatics::IsSpanObject(obj))
		{
			obj->ClearAdjustmentHandles();
			obj = obj->GetParentObject();
		}

		switch (obj ? obj->GetObjectType() : EObjectType::OTNone)
		{
		case EObjectType::OTMetaEdgeSpan:
			for (int32 id : Cast<AMOIMetaEdgeSpan>(obj)->InstanceData.GraphMembers)
			{
				massingObjects.Add(doc->GetObjectById(id));
			}
			break;

		case EObjectType::OTMetaPlaneSpan:
			for (int32 id : Cast<AMOIMetaPlaneSpan>(obj)->InstanceData.GraphMembers)
			{
				massingObjects.Add(doc->GetObjectById(id));
			}
			break;

		case EObjectType::OTNone:
			break;

		default:
			massingObjects.Add(obj);
			break;
		}
	}

	const auto& selectedGroups = emPlayerState->SelectedGroupObjects;

	TArray<FDeltaPtr> deltas;

	if (massingObjects.Num() + selectedGroups.Num() > 0)
	{
		int32 nextID = doc->GetNextAvailableID();
		int32 newGroupID = nextID++;

		int32 oldGroupID = doc->GetActiveVolumeGraphID();
		FMOIStateData stateData(newGroupID, EObjectType::OTMetaGraph, oldGroupID);
		stateData.CustomData.SaveStructData<FMOIMetaGraphData>(FMOIMetaGraphData());

		auto delta = MakeShared<FMOIDelta>();
		delta->AddCreateDestroyState(stateData, EMOIDeltaType::Create);
		deltas.Add(delta);
		FGraph3DDelta addGraph;
		addGraph.DeltaType = EGraph3DDeltaType::Add;
		addGraph.GraphID = newGroupID;
		deltas.Add(MakeShared<FGraph3DDelta>(addGraph));

		FGraph3D* oldGraph = doc->GetVolumeGraph(oldGroupID);
		FGraph3D tempGraph(newGroupID);
		TArray<int32> metaItemsToMove;

		for (auto* object: massingObjects)
		{
			EObjectType objectType = object->GetObjectType();

			if (doc->FindGraph3DByObjID(object->ID) == oldGroupID &&
				(objectType == EObjectType::OTMetaPlane || objectType == EObjectType::OTMetaEdge))
			{
				metaItemsToMove.Add(object->ID);
			}
		}
		
		TArray<FGraph3DDelta> graphDeltas;
		// Regular delete has bGatherEdgesFromFaces = false, bAttemptJoin = true. This can change
		// behaviour slightly (esp. bGatherEdgesFromFaces).
		oldGraph->GetDeltaForDeleteObjects(metaItemsToMove, graphDeltas, nextID, true /* bGatherEdgesFromFaces */, false /* bAttemptJoin */);
		TArray<FGraph3DDelta> createGraphDeltas;

		FModumateObjectDeltaStatics::ConvertGraphDeleteToMove(graphDeltas, oldGraph, nextID, createGraphDeltas);
		for (auto& graphDelta: graphDeltas)
		{
			graphDelta.GraphID = oldGroupID;
			deltas.Add(MakeShared<FGraph3DDelta>(MoveTemp(graphDelta)));
		}

		for (auto& createGraphDelta: createGraphDeltas)
		{
			createGraphDelta.GraphID = newGroupID;
			deltas.Add(MakeShared<FGraph3DDelta>(MoveTemp(createGraphDelta)) );
		}

		for (auto& childGroup: selectedGroups)
		{
			FMOIStateData newState(childGroup->GetStateData());
			newState.ParentID = newGroupID;
			auto subgroupDelta = MakeShared<FMOIDelta>();
			subgroupDelta->AddMutationState(childGroup, childGroup->GetStateData(), newState);
			deltas.Add(subgroupDelta);
		}

		bRetVal = doc->ApplyDeltas(deltas, GetWorld());
		emPlayerState->DeselectAll(false);
		auto* newGroupObject = doc->GetObjectById(newGroupID);
		if (newGroupID)
		{
			emPlayerState->SetGroupObjectSelected(doc->GetObjectById(newGroupID), true, true);
		}
	}

	// Tool is single-use only
	Controller->ToolAbortUse();
	return bRetVal;
}
