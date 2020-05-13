// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "EditModelGraph2DTool.h"

#include "EditModelGameState_CPP.h"
#include "EditModelPlayerController_CPP.h"
#include "EditModelPlayerState_CPP.h"
#include "ModumateObjectStatics.h"

using namespace Modumate;

UGraph2DTool::UGraph2DTool(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UGraph2DTool::Activate()
{
	Super::Activate();

	AEditModelGameState_CPP *gameState = GetWorld()->GetGameState<AEditModelGameState_CPP>();
	const FGraph3D &volumeGraph = gameState->Document.GetVolumeGraph();

	TSet<FTypedGraphObjID> graphObjIDs, connectedGraphIDs;
	UModumateObjectStatics::GetGraphIDsFromMOIs(Controller->EMPlayerState->SelectedObjects, graphObjIDs);

	FGraph selectedGraph;
	FPlane graphPlane;
	if (volumeGraph.Create2DGraph(graphObjIDs, connectedGraphIDs, selectedGraph, graphPlane, true, true))
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
