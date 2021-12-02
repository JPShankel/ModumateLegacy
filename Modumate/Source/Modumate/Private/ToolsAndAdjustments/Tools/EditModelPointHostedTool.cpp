// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Tools/EditModelPointHostedTool.h"
#include "DocumentManagement/ModumateCommands.h"
#include "DocumentManagement/ModumateDocument.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "UnrealClasses/EditModelGameState.h"

UPointHostedTool::UPointHostedTool()
	: Super()
{}

bool UPointHostedTool::Activate()
{
	if (!UEditModelToolBase::Activate())
	{
		return false;
	}

	Controller->DeselectAll();
	Controller->EMPlayerState->SetHoveredObject(nullptr);
	bWasShowingSnapCursor = Controller->EMPlayerState->bShowSnappedCursor;

	OriginalMouseMode = Controller->EMPlayerState->SnappedCursor.MouseMode;
	Controller->EMPlayerState->SnappedCursor.MouseMode = EMouseMode::Location;

	bWantedVerticalSnap = Controller->EMPlayerState->SnappedCursor.WantsVerticalAffordanceSnap;
	Controller->EMPlayerState->SnappedCursor.WantsVerticalAffordanceSnap = true;

	ResetState();

	return true;
}

bool UPointHostedTool::Deactivate()
{
	ResetState();

	if (Controller)
	{
		Controller->EMPlayerState->bShowSnappedCursor = bWasShowingSnapCursor;
		Controller->EMPlayerState->SnappedCursor.MouseMode = OriginalMouseMode;
		Controller->EMPlayerState->SnappedCursor.WantsVerticalAffordanceSnap = bWantedVerticalSnap;
	}

	return UEditModelToolBase::Deactivate();
}

bool UPointHostedTool::FrameUpdate()
{
	const FSnappedCursor& cursor = Controller->EMPlayerState->SnappedCursor;
	if (!cursor.Visible || !AssemblyGUID.IsValid())
	{
		return false;
	}

	int32 newTargetID = MOD_ID_NONE;
	const AModumateObjectInstance* hitMOI = nullptr;
	PointHostedPos = FVector::ZeroVector;

	if (cursor.Actor)
	{
		hitMOI = GameState->Document->ObjectFromActor(cursor.Actor);

		if (hitMOI && (hitMOI->GetObjectType() == EObjectType::OTPointHosted))
		{
			hitMOI = hitMOI->GetParentObject();
		}
	}

	if ((hitMOI != nullptr) && (hitMOI->GetObjectType() == EObjectType::OTMetaVertex))
	{
		newTargetID = hitMOI->ID;
	}

	SetTargetID(newTargetID);
	if (LastValidTargetID != MOD_ID_NONE)
	{
		PointHostedPos = hitMOI->GetLocation();

		// Preview the deltas to create a point hosted object
		if (GameState->Document->StartPreviewing() && GetObjectCreationDeltas(LastValidTargetID , CurDeltas))
		{
			GameState->Document->ApplyPreviewDeltas(CurDeltas, GetWorld());
		}
	}

	return true;
}

bool UPointHostedTool::BeginUse()
{
	if (!AssemblyGUID.IsValid())
	{
		return false;
	}

	Super::BeginUse();

	GameState->Document->ClearPreviewDeltas(GetWorld());

	TArray<FDeltaPtr> deltas;
	if (!GetObjectCreationDeltas( LastValidTargetID, deltas))
	{
		return false;
	}

	if (deltas.Num() == 0)
	{
		return false;
	}

	bool bSuccess = GameState->Document->ApplyDeltas(deltas, GetWorld());

	EndUse();

	return bSuccess;
}

void UPointHostedTool::ResetState()
{
	SetTargetID(MOD_ID_NONE);
	PointHostedPos = FVector::ZeroVector;
}

void UPointHostedTool::SetTargetID(int32 NewTargetID)
{
	LastValidTargetID = NewTargetID;
}

bool UPointHostedTool::GetObjectCreationDeltas(const int32 InTargetVertexID, TArray<FDeltaPtr>& OutDeltaPtrs)
{
	OutDeltaPtrs.Reset();
	NewObjectIDs.Reset();

	if (InTargetVertexID == MOD_ID_NONE)
	{
		return false;
	}

	TSharedPtr<FMOIDelta> delta;
	int32 nextID = GameState->Document->GetNextAvailableID();

	// Follows the pattern established in UPlaneHostedObjTool
	if (!delta.IsValid())
	{
		delta = MakeShared<FMOIDelta>();
		OutDeltaPtrs.Add(delta);
	}

	bool bCreateNewObject = true;
	AModumateObjectInstance* parentMOI = GameState->Document->GetObjectById(InTargetVertexID);

	// Check if there's already an existing object
	if (parentMOI && ensure(parentMOI->GetObjectType() == EObjectType::OTMetaVertex))
	{
		for (auto child : parentMOI->GetChildObjects())
		{
			if (child->GetObjectType() == EObjectType::OTPointHosted)
			{
				// Change assembly of a point hosted object
				FMOIStateData& newState = delta->AddMutationState(child);
				newState.AssemblyGUID = AssemblyGUID;
				bCreateNewObject = false;
			}
			else
			{
				// Destroy if it isn't a point hosted object
				delta->AddCreateDestroyState(child->GetStateData(), EMOIDeltaType::Destroy);
			}
		}
	}

	if (bCreateNewObject)
	{
		NewMOIStateData.ID = nextID++;
		NewMOIStateData.ObjectType = EObjectType::OTPointHosted;
		NewMOIStateData.ParentID = InTargetVertexID;
		NewMOIStateData.AssemblyGUID = AssemblyGUID;

		NewObjectIDs.Add(NewMOIStateData.ID);

		delta->AddCreateDestroyState(NewMOIStateData, EMOIDeltaType::Create);
	}

	return true;
}
