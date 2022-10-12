// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Tools/EditModelGraph2DTool.h"

#include "Graph/Graph2D.h"
#include "Graph/Graph3D.h"
#include "Objects/ModumateObjectStatics.h"
#include "UnrealClasses/EditModelGameState.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"



UGraph2DTool::UGraph2DTool()
	: Super()
{
	SelectedGraph = MakeShared<FGraph2D>();
}

bool UGraph2DTool::Activate()
{
	Super::Activate();

	AEditModelGameState *gameState = GetWorld()->GetGameState<AEditModelGameState>();
	const FGraph3D& volumeGraph = *gameState->Document->GetVolumeGraph();

	TSet<int32> graphObjIDs, connectedGraphIDs;
	UModumateObjectStatics::GetGraphIDsFromMOIs(Controller->EMPlayerState->SelectedObjects, graphObjIDs);

#if 0 //TODO: deprecate or refactor 
	/*FPlane graphPlane;
	if (volumeGraph.Create2DGraph(graphObjIDs, connectedGraphIDs, SelectedGraph, graphPlane, true, true))
	{
		FGraph2DRecord graphRecord;
		FBIMKey graphKey;
		if (SelectedGraph->ToDataRecord(&graphRecord) &&
			(gameState->Document->PresetManager.AddOrUpdateGraph2DRecord(FBIMKey(), graphRecord, graphKey) == EBIMResult::Success))
		{
			UE_LOG(LogTemp, Log, TEXT("Added graph record \"%s\""), *graphKey.ToString());
		}
	}*/
#endif

	Deactivate();
	Controller->SetToolMode(EToolMode::VE_SELECT);

	return false;
}

bool UGraph2DTool::Deactivate()
{
	return UEditModelToolBase::Deactivate();
}
