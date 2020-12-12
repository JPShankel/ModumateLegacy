// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Tools/EditModelTrimTool.h"

#include "UnrealClasses/DynamicMeshActor.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "DocumentManagement/ModumateCommands.h"
#include "ModumateCore/ModumateGeometryStatics.h"
#include "Objects/Trim.h"

using namespace Modumate;

UTrimTool::UTrimTool(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, TargetEdgeID(MOD_ID_NONE)
	, TargetEdgeStartPos(ForceInitToZero)
	, TargetEdgeEndPos(ForceInitToZero)
{
}

bool UTrimTool::Activate()
{
	Super::Activate();

	TargetEdgeID = MOD_ID_NONE;

	if (Controller)
	{
		Controller->DeselectAll();
	}

	return true;
}

bool UTrimTool::HandleInputNumber(double n)
{
	return false;
}

bool UTrimTool::Deactivate()
{
	TargetEdgeID = MOD_ID_NONE;

	return UEditModelToolBase::Deactivate();
}

bool UTrimTool::BeginUse()
{
	if (TargetEdgeID != MOD_ID_NONE)
	{
		FMOITrimData trimCustomData;
		FMOIStateData stateData(GameState->Document->GetNextAvailableID(), EObjectType::OTTrim, TargetEdgeID);
		stateData.AssemblyKey = AssemblyKey;
		stateData.CustomData.SaveStructData(trimCustomData);

		auto delta = MakeShared<FMOIDelta>();
		delta->AddCreateDestroyState(stateData, EMOIDeltaType::Create);
		GameState->Document->ApplyDeltas({ delta }, GetWorld());

		EndUse();
	}
	else
	{
		AbortUse();
	}

	return false;
}

bool UTrimTool::FrameUpdate()
{
	Super::FrameUpdate();

	TargetEdgeID = MOD_ID_NONE;

	if (!ensure(Controller && GameState))
	{
		return false;
	}

	const FSnappedCursor& cursor = Controller->EMPlayerState->SnappedCursor;
	const AModumateObjectInstance* targetMOI = GameState->Document->ObjectFromActor(cursor.Actor);
	if (targetMOI && (targetMOI->GetObjectType() == EObjectType::OTSurfaceEdge))
	{
		int32 existingTargetTrimID = MOD_ID_NONE;

		auto targetChildren = targetMOI->GetChildObjects();
		for (auto targetChild : targetChildren)
		{
			if (targetChild && (targetChild->GetObjectType() == EObjectType::OTTrim))
			{
				existingTargetTrimID = targetChild->ID;
				break;
			}
		}

		// TODO: support replacing existing trim with the current trim assembly
		if (existingTargetTrimID == MOD_ID_NONE)
		{
			TargetEdgeID = targetMOI->ID;
			TargetEdgeStartPos = targetMOI->GetCorner(0);
			TargetEdgeEndPos = targetMOI->GetCorner(1);

			Controller->EMPlayerState->AffordanceLines.Add(FAffordanceLine(TargetEdgeStartPos, TargetEdgeEndPos, AffordanceLineColor, AffordanceLineInterval, AffordanceLineThickness));
		}
	}

	return true;
}
