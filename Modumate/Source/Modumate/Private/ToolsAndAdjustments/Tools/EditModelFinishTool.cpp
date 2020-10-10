// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Tools/EditModelFinishTool.h"

#include "Objects/ModumateObjectInstance.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "Database/ModumateObjectDatabase.h"
#include "ModumateCore/ModumateObjectStatics.h"
#include "DocumentManagement/ModumateDocument.h"
#include "DocumentManagement/ModumateCommands.h"

using namespace Modumate;

UFinishTool::UFinishTool(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{ }

bool UFinishTool::Activate()
{
	if (!Super::Activate())
	{
		return false;
	}

	OriginalMouseMode = Controller->EMPlayerState->SnappedCursor.MouseMode;
	Controller->EMPlayerState->SnappedCursor.MouseMode = EMouseMode::Object;

	return true;
}

bool UFinishTool::Deactivate()
{
	if (Controller)
	{
		Controller->EMPlayerState->SnappedCursor.MouseMode = OriginalMouseMode;
	}

	return Super::Deactivate();
}

bool UFinishTool::BeginUse()
{
	if ((HostTarget == nullptr) || (HostTarget->ID == MOD_ID_NONE))
	{
		return false;
	}

	// If we aren't already targeting a surface polygon, then we'll try to create an implicit one and a graph on the target host.
	if (GraphElementTarget == nullptr)
	{
		// Use the base class SurfaceGraphTool to create a graph, if it doesn't already exist
		int32 newSurfaceGraphID;
		if (!CreateGraphFromFaceTarget(newSurfaceGraphID))
		{
			return false;
		}

		GraphTarget = GameState->Document.GetObjectById(newSurfaceGraphID);
		auto surfaceGraph = GameState->Document.FindSurfaceGraph(newSurfaceGraphID);

		if (!ensure(GraphTarget && (GraphTarget->GetObjectType() == EObjectType::OTSurfaceGraph) && surfaceGraph.IsValid()))
		{
			return false;
		}

		for (auto &kvp : surfaceGraph->GetPolygons())
		{
			const FGraph2DPolygon &surfacePolygon = kvp.Value;
			if (surfacePolygon.bInterior && (surfacePolygon.ContainingPolyID == MOD_ID_NONE))
			{
				GraphElementTarget = GameState->Document.GetObjectById(surfacePolygon.ID);
				break;
			}
		}

		if (GraphElementTarget == nullptr)
		{
			return false;
		}
	}

	// If we're replacing an existing finish, just swap its assembly
	for (FModumateObjectInstance *child : GraphElementTarget->GetChildObjects())
	{
		if (child->GetObjectType() == EObjectType::OTFinish)
		{
			if (child->GetAssembly().UniqueKey() != AssemblyKey)
			{
				auto swapAssemblyDelta = MakeShared<FMOIDelta>();
				auto& newState = swapAssemblyDelta->AddMutationState(child);
				newState.AssemblyKey = AssemblyKey;

				return GameState->Document.ApplyDeltas({ swapAssemblyDelta }, GetWorld());
			}
			else
			{
				return false;
			}
		}
	}

	// Otherwise, create a new finish object on the target surface graph polygon
	FMOIStateData newFinishState(GameState->Document.GetNextAvailableID(), EObjectType::OTFinish, GraphElementTarget->ID);
	newFinishState.AssemblyKey = AssemblyKey;

	auto createFinishDelta = MakeShared<FMOIDelta>();
	createFinishDelta->AddCreateDestroyState(newFinishState, EMOIDeltaType::Create);

	return GameState->Document.ApplyDeltas({ createFinishDelta }, GetWorld());
}

bool UFinishTool::FrameUpdate()
{
	if (!Super::FrameUpdate())
	{
		return false;
	}

	if (GraphElementTarget && (GraphElementTarget->GetObjectType() == EObjectType::OTSurfacePolygon))
	{
		int32 numCorners = GraphElementTarget->GetNumCorners();
		for (int32 curPointIdx = 0; curPointIdx < numCorners; ++curPointIdx)
		{
			int32 nextPointIdx = (curPointIdx + 1) % numCorners;

			Controller->EMPlayerState->AffordanceLines.Add(FAffordanceLine(GraphElementTarget->GetCorner(curPointIdx), GraphElementTarget->GetCorner(nextPointIdx),
				AffordanceLineColor, AffordanceLineInterval, AffordanceLineThickness)
			);
		}
	}

	return true;
}

TArray<EEditViewModes> UFinishTool::GetRequiredEditModes() const
{
	return { EEditViewModes::ObjectEditing, EEditViewModes::SurfaceGraphs };
}
