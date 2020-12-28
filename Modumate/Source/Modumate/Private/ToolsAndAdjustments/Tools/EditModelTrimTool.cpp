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
		FMOIStateData stateData(GameState->Document->GetNextAvailableID(), EObjectType::OTTrim, TargetEdgeID);
		stateData.AssemblyGUID = AssemblyGUID;
		stateData.CustomData.SaveStructData(PendingTrimData);

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
	PendingTrimData = FMOITrimData(FMOITrimData::CurrentVersion);

	if (!ensure(Controller && GameState))
	{
		return false;
	}

	const FSnappedCursor& cursor = Controller->EMPlayerState->SnappedCursor;
	const AModumateObjectInstance* targetMOI = GameState->Document->ObjectFromActor(cursor.Actor);
	if (targetMOI && (targetMOI->GetObjectType() == EObjectType::OTSurfaceEdge))
	{
		auto targetSurfaceGraph = GameState->Document->FindSurfaceGraphByObjID(targetMOI->ID);
		if (!targetSurfaceGraph.IsValid())
		{
			return false;
		}

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

		// Determine the desired flip/justification of the Trim based on the target SurfaceEdge
		PendingTrimData.UpJustification = 0.5f;
		PendingTrimData.FlipSigns = FVector2D::UnitVector;
		int32 adjacentPolyID = MOD_ID_NONE;

		auto boudingPoly = targetSurfaceGraph->FindPolygon(targetSurfaceGraph->GetOuterBoundingPolyID());
		if (boudingPoly)
		{
			bool bBoundsForwardEdge = boudingPoly->Edges.Contains(targetMOI->ID);
			bool bBoundsReverseEdge = boudingPoly->Edges.Contains(-targetMOI->ID);
			if (bBoundsForwardEdge || bBoundsReverseEdge)
			{
				adjacentPolyID = boudingPoly->ID;

				PendingTrimData.UpJustification = 1.0f;
				PendingTrimData.FlipSigns.Y = bBoundsForwardEdge ? 1.0f : -1.0f;
			}
		}

		if (adjacentPolyID == MOD_ID_NONE)
		{
			for (auto& innerBoundsKVP : targetSurfaceGraph->GetInnerBounds())
			{
				auto innerBoundingPoly = targetSurfaceGraph->FindPolygon(innerBoundsKVP.Key);
				if (innerBoundingPoly && (innerBoundingPoly->Edges.Contains(targetMOI->ID) || innerBoundingPoly->Edges.Contains(-targetMOI->ID)))
				{
					bool bBoundsForwardEdge = innerBoundingPoly->Edges.Contains(targetMOI->ID);
					bool bBoundsReverseEdge = innerBoundingPoly->Edges.Contains(-targetMOI->ID);
					if (bBoundsForwardEdge || bBoundsReverseEdge)
					{
						adjacentPolyID = innerBoundingPoly->ID;

						PendingTrimData.UpJustification = 1.0f;
						PendingTrimData.FlipSigns.Y = bBoundsForwardEdge ? -1.0f : 1.0f;
					}
				}
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
