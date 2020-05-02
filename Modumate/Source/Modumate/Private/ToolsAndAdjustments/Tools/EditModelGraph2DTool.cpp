// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "EditModelGraph2DTool.h"

#include "Algo/Transform.h"
#include "EditModelGameState_CPP.h"
#include "EditModelPlayerController_CPP.h"
#include "EditModelPlayerState_CPP.h"
#include "JsonObjectConverter.h"

#include "ModumateCommands.h"

using namespace Modumate;

UGraph2DTool::UGraph2DTool(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{

}

bool UGraph2DTool::Activate()
{
	UEditModelToolBase::Activate();

	FPlane selectedPlane;
	FGraph selectedGraph;
	AEditModelGameState_CPP *gameState = GetWorld()->GetGameState<AEditModelGameState_CPP>();
	const FGraph3D &volumeGraph = gameState->Document.GetVolumeGraph();

	TSet<int32> vertexIDs, edgeIDs, faceIDs;
	for (FModumateObjectInstance *selectedObj : Controller->EMPlayerState->SelectedObjects)
	{
		EObjectType objectType = selectedObj ? selectedObj->GetObjectType() : EObjectType::OTNone;
		int32 objectID = selectedObj ? selectedObj->ID : MOD_ID_NONE;
		switch (objectType)
		{
		case EObjectType::OTMetaVertex:
			vertexIDs.Add(objectID);
			break;
		case EObjectType::OTMetaEdge:
			edgeIDs.Add(objectID);
			break;
		case EObjectType::OTMetaPlane:
			faceIDs.Add(objectID);
			break;
		default:
			break;
		}
	}

	if (volumeGraph.Create2DGraph(vertexIDs, edgeIDs, faceIDs, selectedGraph, true, true))
	{
		FGraph2DRecord graphRecord;
		FName graphKey;
		if (selectedGraph.ToDataRecord(graphRecord) &&
			(gameState->Document.PresetManager.AddOrUpdateGraph2DRecord(NAME_None, graphRecord, graphKey) == ECraftingResult::Success))
		{
			UE_LOG(LogTemp, Log, TEXT("Added graph record \"%s\""), *graphKey.ToString());
		}
	}

	Deactivate();
	Controller->SetToolMode(EToolMode::VE_SELECT);

	return false;
}

bool UGraph2DTool::Deactivate()
{
	return UEditModelToolBase::Deactivate();
}
