// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Tools/EditModelGroupTool.h"

#include "ModumateCore/ModumateObjectDeltaStatics.h"
#include "UnrealClasses/EditModelGameState.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"

UGroupTool::UGroupTool()
	: Super()
{ }

bool UGroupTool::Activate()
{
#if !UE_EDITOR
	return false;
#endif
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
	static const TSet<EObjectType> massingTypes = { EObjectType::OTMetaVertex, EObjectType::OTMetaEdge, EObjectType::OTMetaPlane,
		EObjectType::OTMetaGraph };

	const auto& selectedObjects = emPlayerState->SelectedObjects;
	for (auto* obj: selectedObjects)
	{
		while (obj && !massingTypes.Contains(obj->GetObjectType()))
		{
			obj = obj->GetParentObject();
		}
		if (obj)
		{
			massingObjects.Add(obj);
		}
	}

	TArray<FDeltaPtr> deltas;

	if (massingObjects.Num() > 0)
	{
		int32 nextID = doc->GetNextAvailableID();
		int32 newGroupID = nextID++;
		int32 oldGroupID = doc->FindGraph3DByObjID((*massingObjects.begin())->ID);
		FMOIStateData stateData(newGroupID, EObjectType::OTMetaGraph, doc->GetActiveVolumeGraphID());

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
		oldGraph->GetDeltaForDeleteObjects(metaItemsToMove, graphDeltas, nextID, true);
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

		bRetVal = doc->ApplyDeltas(deltas, GetWorld());
		UE_LOG(LogTemp, Warning, TEXT("Created new group from %d massing elements"), massingObjects.Num());
	}

	// Tool is single-use only
	Controller->ToolAbortUse();
	return bRetVal;
}
