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
	if ((GraphElementTarget == nullptr) || (GraphElementTarget->GetObjectType() != EObjectType::OTSurfacePolygon))
	{
		// If we are targeting a graph, then it should have a polygon that we can target, so don't create a new one.
		if (GraphTarget != nullptr)
		{
			return false;
		}

		// TODO: replace with implicit graph creation
		if (!Super::BeginUse())
		{
			return false;
		}

		for (FModumateObjectInstance *hostChild : HostTarget->GetChildObjects())
		{
			if (hostChild->GetObjectType() == EObjectType::OTSurfaceGraph)
			{
				GraphTarget = hostChild;
				break;
			}
		}

		if (GraphTarget == nullptr)
		{
			return false;
		}

		auto surfaceGraph = GameState->Document.FindSurfaceGraph(GraphTarget->ID);
		if (!surfaceGraph.IsValid())
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
	}

	if (GraphElementTarget == nullptr)
	{
		return false;
	}
	const FBIMAssemblySpec* assembly = GameState->Document.PresetManager.GetAssemblyByKey(EToolMode::VE_FINISH, AssemblyKey);

	if (!ensureAlways(assembly))
	{
		return false;
	}

	// If we're replacing an existing finish, just swap its assembly
	for (FModumateObjectInstance *child : GraphElementTarget->GetChildObjects())
	{
		if (child->GetObjectType() == EObjectType::OTFinish)
		{
			child->BeginPreviewOperation();
			child->SetAssembly(*assembly);

			TSharedPtr<FMOIDelta> swapAssemblyDelta = MakeShareable(new FMOIDelta({ child }));
			child->EndPreviewOperation();

			return GameState->Document.ApplyDeltas({ swapAssemblyDelta }, GetWorld());
		}
	}

	// Otherwise, create a new finish object on the target surface graph polygon
	FMOIStateData stateData;
	stateData.StateType = EMOIDeltaType::Create;
	stateData.ObjectType = EObjectType::OTFinish;
	stateData.ObjectAssemblyKey = AssemblyKey;
	stateData.ParentID = GraphElementTarget->ID;
	stateData.ObjectID = GameState->Document.GetNextAvailableID();

	TSharedPtr<FMOIDelta> createFinishDelta = MakeShareable(new FMOIDelta({ stateData }));
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
		const TArray<FVector> &surfacePolyCPs = GraphElementTarget->GetControlPoints();
		int32 numCPs = surfacePolyCPs.Num();
		for (int32 curPointIdx = 0; curPointIdx < numCPs; ++curPointIdx)
		{
			int32 nextPointIdx = (curPointIdx + 1) % numCPs;

			Controller->EMPlayerState->AffordanceLines.Add(FAffordanceLine(surfacePolyCPs[curPointIdx], surfacePolyCPs[nextPointIdx],
				AffordanceLineColor, AffordanceLineInterval, AffordanceLineThickness)
			);
		}
	}

	return true;
}
