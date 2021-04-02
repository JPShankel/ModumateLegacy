// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Tools/EditModelTrimTool.h"

#include "DocumentManagement/ModumateCommands.h"
#include "ModumateCore/ModumateGeometryStatics.h"
#include "Objects/Trim.h"
#include "UnrealClasses/DynamicMeshActor.h"
#include "UnrealClasses/EditModelGameMode.h"
#include "UnrealClasses/EditModelGameState.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"

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
	GameState->Document->ClearPreviewDeltas(Controller->GetWorld());

	int32 nextID = GameState->Document->GetNextAvailableID();
	if (GetTrimCreationDeltas(TargetEdgeID, nextID, CurDeltas))
	{
		GameState->Document->ApplyDeltas(CurDeltas, GetWorld());
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

	if (!ensure(Controller && Controller->EMPlayerState && GameState && GameState->Document))
	{
		return false;
	}

	GameState->Document->StartPreviewing();

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
		PendingTrimData.OffsetUp = FDimensionOffset::Centered;
		PendingTrimData.FlipSigns = FVector2D::UnitVector;

		TArray<int32> boundingPolyIDs;
		boundingPolyIDs.Add(targetSurfaceGraph->GetOuterBoundingPolyID());
		boundingPolyIDs.Append(targetSurfaceGraph->GetInnerBounds());

		for (int32 boundingPolyID : boundingPolyIDs)
		{
			auto boundingPoly = targetSurfaceGraph->FindPolygon(boundingPolyID);
			if (boundingPoly == nullptr)
			{
				continue;
			}

			bool bBoundsForwardEdge = boundingPoly->Edges.Contains(targetMOI->ID);
			bool bBoundsReverseEdge = boundingPoly->Edges.Contains(-targetMOI->ID);
			if (bBoundsForwardEdge || bBoundsReverseEdge)
			{
				// The trim's normal (Y) needs to be flipped in order to either:
				// - Flip to be the opposite of an exterior outer bounding polygon's edge, whose normal faces outward
				// - Flip to be the opposite of an interior inner bounding polygon's edge, whose normal faces inward
				PendingTrimData.OffsetUp = FDimensionOffset::Positive;
				PendingTrimData.FlipSigns.Y = bBoundsForwardEdge ? -1.0f : 1.0f;
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

	int32 nextID = GameState->Document->GetNextAvailableID();
	if (GetTrimCreationDeltas(TargetEdgeID, nextID, CurDeltas))
	{
		GameState->Document->ApplyPreviewDeltas(CurDeltas, Controller->GetWorld());
	}

	return true;
}

bool UTrimTool::GetTrimCreationDeltas(int32 InTargetEdgeID, int32& NextID, TArray<FDeltaPtr>& OutDeltas) const
{
	OutDeltas.Reset();

	if (InTargetEdgeID == MOD_ID_NONE)
	{
		return false;
	}

	FMOIStateData stateData(NextID++, EObjectType::OTTrim, InTargetEdgeID);
	stateData.AssemblyGUID = AssemblyGUID;
	stateData.CustomData.SaveStructData(PendingTrimData);

	auto delta = MakeShared<FMOIDelta>();
	delta->AddCreateDestroyState(stateData, EMOIDeltaType::Create);
	OutDeltas.Add(delta);

	return true;
}
