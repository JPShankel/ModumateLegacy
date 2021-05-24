// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Tools/EditModelFinishTool.h"

#include "Database/ModumateObjectDatabase.h"
#include "DocumentManagement/ModumateCommands.h"
#include "DocumentManagement/ModumateDocument.h"
#include "DocumentManagement/ObjIDReservationHandle.h"
#include "ModumateCore/ModumateObjectStatics.h"
#include "Objects/Finish.h"
#include "Objects/ModumateObjectInstance.h"
#include "UnrealClasses/EditModelGameMode.h"
#include "UnrealClasses/EditModelGameState.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"



UFinishTool::UFinishTool()
	: Super()
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

	GameState->Document->ClearPreviewDeltas(GetWorld());

	return Super::Deactivate();
}

bool UFinishTool::BeginUse()
{
	GameState->Document->ClearPreviewDeltas(GetWorld());

	TArray<FDeltaPtr> deltas;
	bool bSuccess = GetFinishCreationDeltas(deltas) && GameState->Document->ApplyDeltas(deltas, GetWorld());

	return bSuccess;
}

bool UFinishTool::FrameUpdate()
{
	if (!Super::FrameUpdate())
	{
		return false;
	}

	if (HitGraphElementMOI && (HitGraphElementMOI->GetObjectType() == EObjectType::OTSurfacePolygon))
	{
		int32 numCorners = HitGraphElementMOI->GetNumCorners();
		for (int32 curPointIdx = 0; curPointIdx < numCorners; ++curPointIdx)
		{
			int32 nextPointIdx = (curPointIdx + 1) % numCorners;

			Controller->EMPlayerState->AffordanceLines.Add(FAffordanceLine(HitGraphElementMOI->GetCorner(curPointIdx), HitGraphElementMOI->GetCorner(nextPointIdx),
				AffordanceLineColor, AffordanceLineInterval, AffordanceLineThickness)
			);
		}
	}

	GameState->Document->StartPreviewing();

	TArray<FDeltaPtr> deltas;
	if (GetFinishCreationDeltas(deltas))
	{
		GameState->Document->ApplyPreviewDeltas(deltas, GetWorld());
	}

	return true;
}

bool UFinishTool::GetFinishCreationDeltas(TArray<FDeltaPtr>& OutDeltas)
{
	if ((HitGraphHostMOI == nullptr) || (HitGraphHostMOI->ID == MOD_ID_NONE))
	{
		return false;
	}

	// If we're replacing an existing finish, just swap its assembly
	if (HitGraphElementMOI)
	{
		for (const AModumateObjectInstance* child : HitGraphElementMOI->GetChildObjects())
		{
			if (child->GetObjectType() == EObjectType::OTFinish)
			{
				if (child->GetAssembly().UniqueKey() != AssemblyGUID)
				{
					auto swapAssemblyDelta = MakeShared<FMOIDelta>();
					auto& newState = swapAssemblyDelta->AddMutationState(child);
					newState.AssemblyGUID = AssemblyGUID;

					OutDeltas.Add(swapAssemblyDelta);
				}
				else
				{
					return false;
				}
			}
		}
	}

	FObjIDReservationHandle objIDReservation(GameState->Document, HitGraphHostMOI->ID);
	int32& nextID = objIDReservation.NextID;
	int32 surfaceGraphPolyID = MOD_ID_NONE;

	// If we're targeting an existing surface polygon, then try to use it as the parent for the finish
	if (HitGraphMOI)
	{
		if ((HitGraphElementMOI == nullptr) || (HitGraphElementMOI->GetObjectType() != EObjectType::OTSurfacePolygon))
		{
			return false;
		}

		surfaceGraphPolyID = HitGraphElementMOI->ID;
	}
	// Otherwise, if we aren't already targeting a surface graph or polygon, then we'll try to create a graph on the target host.
	else
	{
		// Use the base class SurfaceGraphTool to get deltas to create a graph, and find the polygon that would host this finish object
		int32 newSurfaceGraphID;
		if (!CreateGraphFromFaceTarget(nextID, newSurfaceGraphID, surfaceGraphPolyID, OutDeltas))
		{
			return false;
		}
	}

	// Now, create a new finish object on the target surface graph polygon
	FMOIStateData newFinishState(nextID, EObjectType::OTFinish, surfaceGraphPolyID);
	newFinishState.AssemblyGUID = AssemblyGUID;
	FMOIFinishData newFinishInstanceData;
	newFinishState.CustomData.SaveStructData(newFinishInstanceData);

	auto createFinishDelta = MakeShared<FMOIDelta>();
	createFinishDelta->AddCreateDestroyState(newFinishState, EMOIDeltaType::Create);
	OutDeltas.Add(createFinishDelta);

	return true;
}
