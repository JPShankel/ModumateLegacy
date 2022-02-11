// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Tools/EditModelFaceHostedTool.h"
#include "DocumentManagement/ModumateCommands.h"
#include "DocumentManagement/ModumateDocument.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "UnrealClasses/EditModelGameState.h"
#include "Objects/FaceHosted.h"

UFaceHostedTool::UFaceHostedTool()
	: Super()
{}

bool UFaceHostedTool::Activate()
{
	if (!UEditModelToolBase::Activate())
	{
		return false;
	}

	Controller->DeselectAll();
	Controller->EMPlayerState->SetHoveredObject(nullptr);
	bWasShowingSnapCursor = Controller->EMPlayerState->bShowSnappedCursor;

	OriginalMouseMode = Controller->EMPlayerState->SnappedCursor.MouseMode;
	Controller->EMPlayerState->SnappedCursor.MouseMode = EMouseMode::Object;

	bWantedVerticalSnap = Controller->EMPlayerState->SnappedCursor.WantsVerticalAffordanceSnap;
	Controller->EMPlayerState->SnappedCursor.WantsVerticalAffordanceSnap = true;

	ResetState();

	return true;
}

bool UFaceHostedTool::Deactivate()
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

bool UFaceHostedTool::FrameUpdate()
{
	TArray<EObjectType> compatibleObjectTypes;
	compatibleObjectTypes.Add(EObjectType::OTWallSegment);
	compatibleObjectTypes.Add(EObjectType::OTFloorSegment);
	compatibleObjectTypes.Add(EObjectType::OTRoofFace);
	compatibleObjectTypes.Add(EObjectType::OTCeiling);
	compatibleObjectTypes.Add(EObjectType::OTCountertop);
	compatibleObjectTypes.Add(EObjectType::OTSystemPanel);
	
	const FSnappedCursor& cursor = Controller->EMPlayerState->SnappedCursor;
	if (!AssemblyGUID.IsValid())
	{
		return false;
	}

	int32 newTargetID = MOD_ID_NONE;
	const AModumateObjectInstance* hitMOI = nullptr;

	if (cursor.Actor)
	{
		hitMOI = GameState->Document->ObjectFromActor(cursor.Actor);

		if (hitMOI && (hitMOI->GetObjectType() == EObjectType::OTFaceHosted) || (compatibleObjectTypes.Contains(hitMOI->GetObjectType())))
		{
			hitMOI = hitMOI->GetParentObject();
		}
	}

	if (hitMOI && (hitMOI->GetObjectType() == EObjectType::OTMetaPlane))
	{
		newTargetID = hitMOI->ID;
	}

	SetTargetID(newTargetID);
	if (LastValidTargetID != MOD_ID_NONE)
	{

		// Preview delta
		if (GameState->Document->StartPreviewing() && GetObjectCreationDeltas({ LastValidTargetID }, CurDeltas))
		{
			GameState->Document->ApplyPreviewDeltas(CurDeltas, GetWorld());
		}

		// Don't show the snap cursor if we're targeting an face or existing structure line.
		Controller->EMPlayerState->bShowSnappedCursor = (LastValidTargetID == MOD_ID_NONE);
	}

	return true;
}

bool UFaceHostedTool::BeginUse()
{
	if (!AssemblyGUID.IsValid())
	{
		return false;
	}

	Super::BeginUse();

	GameState->Document->ClearPreviewDeltas(GetWorld());

	TArray<FDeltaPtr> deltas;
	if (!GetObjectCreationDeltas({ LastValidTargetID }, deltas))
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

void UFaceHostedTool::ResetState()
{
	SetTargetID(MOD_ID_NONE);
}

void UFaceHostedTool::SetTargetID(int32 NewTargetID)
{
	LastValidTargetID = NewTargetID;
}

bool UFaceHostedTool::GetObjectCreationDeltas(const TArray<int32>& InTargetFaceIDs, TArray<FDeltaPtr>& OutDeltaPtrs)
{
	//reset any existing deltas
	OutDeltaPtrs.Reset();
	NewObjectIDs.Reset();

	//ensure valid faces
	TArray<int32> targetFaceIDs;
	for (int32 targetFaceID : InTargetFaceIDs)
	{
		if (targetFaceID != MOD_ID_NONE)
		{
			targetFaceIDs.Add(targetFaceID);
		}
	}

	TSharedPtr<FMOIDelta> delta;
	int32 nextID = GameState->Document->GetNextAvailableID();

	// Follows the pattern established in UPlaneHostedObjTool
	for (int32 targetFaceID : targetFaceIDs)
	{
		if (!delta.IsValid())
		{
			delta = MakeShared<FMOIDelta>();
			OutDeltaPtrs.Add(delta);
		}

		bool bCreateNewObject = true;
		AModumateObjectInstance* parentMOI = GameState->Document->GetObjectById(targetFaceID);

		EObjectType objectType = EObjectType::OTFaceHosted;

		if (parentMOI && ensure(parentMOI->GetObjectType() == EObjectType::OTMetaPlane) &&
			CreateObjectMode != EToolCreateObjectMode::Add) //currently only have one mode
		{
			for (auto child : parentMOI->GetChildObjects())
			{
				if (child->GetObjectType() == objectType)
				{
					// Only swap the obj that is being pointed at
					const FSnappedCursor& cursor = Controller->EMPlayerState->SnappedCursor;
					if (cursor.Actor && child == GameState->Document->ObjectFromActor(cursor.Actor))
					{
						FMOIStateData& newState = delta->AddMutationState(child);
						newState.AssemblyGUID = AssemblyGUID;
						bCreateNewObject = false;
					}
				}
				else
				{
					delta->AddCreateDestroyState(child->GetStateData(), EMOIDeltaType::Destroy);
				}
			}
		}

		if (bCreateNewObject)
		{
			NewMOIStateData.ID = nextID++;
			NewMOIStateData.ObjectType = EObjectType::OTFaceHosted;
			NewMOIStateData.ParentID = targetFaceID;
			NewMOIStateData.AssemblyGUID = AssemblyGUID;
			FMOIFaceHostedData newCustomData;
			NewMOIStateData.CustomData.SaveStructData<FMOIFaceHostedData>(newCustomData);

			NewObjectIDs.Add(NewMOIStateData.ID);

			delta->AddCreateDestroyState(NewMOIStateData, EMOIDeltaType::Create);
		}
	}

	return true;
}
