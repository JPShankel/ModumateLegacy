// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Tools/EditModelFaceHostedTool.h"
#include "DocumentManagement/ModumateCommands.h"
#include "DocumentManagement/ModumateDocument.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "UnrealClasses/EditModelGameState.h"
#include "Objects/FaceHosted.h"
#include "Objects/MetaPlaneSpan.h"
#include "ModumateCore/EnumHelpers.h"

UFaceHostedTool::UFaceHostedTool()
	: Super()
{}

bool UFaceHostedTool::Activate()
{
	if (!UEditModelToolBase::Activate())
	{
		return false;
	}

	// Set default settings for NewMOIStateData
	FMOIFaceHostedData newFaceHostedCustomData;
	NewMOIStateData.ObjectType = EObjectType::OTFaceHosted;
	NewMOIStateData.CustomData.SaveStructData(newFaceHostedCustomData);

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
		if (hitMOI && (hitMOI->GetObjectType() == EObjectType::OTMetaPlaneSpan || hitMOI->GetObjectType() == EObjectType::OTMetaPlane))
		{
			newTargetID = hitMOI->ID;
		}

		if (hitMOI && (hitMOI->GetObjectType() == EObjectType::OTFaceHosted || compatibleObjectTypes.Contains(hitMOI->GetObjectType())))
		{
			newTargetID = hitMOI->GetParentID();
		}
	}

	SetTargetID(newTargetID);
	if (LastValidTargetID != MOD_ID_NONE)
	{
		if (GetCreateObjectMode() == EToolCreateObjectMode::Apply ||
			GetCreateObjectMode() == EToolCreateObjectMode::Add)
		{
			// Preview delta
			if (GameState->Document->StartPreviewing() && GetObjectCreationDeltas({ LastValidTargetID }, CurDeltas, MOD_ID_NONE))
			{
				GameState->Document->ApplyPreviewDeltas(CurDeltas, GetWorld());
			}
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

bool UFaceHostedTool::HandleFlip(EAxis::Type FlipAxis)
{
	if (NewObjectIDs.Num() == 0)
	{
		return false;
	}

	// FaceHosted tools create single new span, 
	// we handle flip by setting its child (FaceHosted) instance data
	AModumateObjectInstance* newMOI = GameState->Document->GetObjectById(NewObjectIDs[0]);
	AMOIMetaPlaneSpan* span = Cast<AMOIMetaPlaneSpan>(newMOI);
	if (span && span->GetChildIDs().Num() > 0)
	{
		newMOI = span->GetChildObjects()[0];
	}
	return newMOI && newMOI->GetFlippedState(FlipAxis, NewMOIStateData);
}

bool UFaceHostedTool::HandleOffset(const FVector2D& ViewSpaceDirection)
{
	if (NewObjectIDs.Num() == 0)
	{
		return false;
	}

	FQuat cameraRotation = Controller->PlayerCameraManager->GetCameraRotation().Quaternion();
	FVector worldSpaceDirection =
		(ViewSpaceDirection.X * cameraRotation.GetRightVector()) +
		(ViewSpaceDirection.Y * cameraRotation.GetUpVector());

	// FaceHostedTool creates single plane span, 
	// we handle offset by setting its child (FaceHostedObject) instance data
	AModumateObjectInstance* newMOI = GameState->Document->GetObjectById(NewObjectIDs[0]);
	AMOIMetaPlaneSpan* span = Cast<AMOIMetaPlaneSpan>(newMOI);
	if (span && span->GetChildIDs().Num() > 0)
	{
		newMOI = span->GetChildObjects()[0];
	}
	return newMOI && newMOI->GetOffsetState(worldSpaceDirection, NewMOIStateData);
}

void UFaceHostedTool::ResetState()
{
	SetTargetID(MOD_ID_NONE);
}

void UFaceHostedTool::SetTargetID(int32 NewTargetID)
{
	LastValidTargetID = NewTargetID;
}

bool UFaceHostedTool::GetObjectCreationDeltas(const TArray<int32>& InTargetFaceIDs, TArray<FDeltaPtr>& OutDeltaPtrs, int32 HitFaceHostedID)
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

		AModumateObjectInstance* targetMOI = GameState->Document->GetObjectById(targetFaceID);

		if (!targetMOI)
		{
			continue;
		}

		int32 parentID = MOD_ID_NONE;

		if (targetMOI->GetObjectType() == EObjectType::OTMetaPlane)
		{
			TArray<int32> spanIDs;
			UModumateObjectStatics::GetSpansForFaceObject(GameState->Document, targetMOI, spanIDs);
			if (spanIDs.Num() > 0)
			{
				// TODO: cycle through multiple spans
				parentID = spanIDs[0];
				targetMOI = GameState->Document->GetObjectById(spanIDs[0]);
				if (!ensure(targetMOI))
				{
					continue;
				}
			}
			else
			{
				FMOIStateData newSpan;
				newSpan.ID = nextID++;
				newSpan.ObjectType = EObjectType::OTMetaPlaneSpan;

				FMOIMetaPlaneSpanData instanceData;
				instanceData.GraphMembers.Add(targetMOI->ID);
				newSpan.CustomData.SaveStructData(instanceData);

				delta->AddCreateDestroyState(newSpan, EMOIDeltaType::Create);

				parentID = newSpan.ID;
			}
		}

		if (targetMOI->GetObjectType() == EObjectType::OTMetaPlaneSpan)
		{
			parentID = targetMOI->ID;
			
			// If we're in Add mode, retain underlying children, otherwise replace
			if (GetCreateObjectMode() == EToolCreateObjectMode::Apply)
			{
				for (auto spanChild : targetMOI->GetChildObjects())
				{
					delta->AddCreateDestroyState(spanChild->GetStateData(), EMOIDeltaType::Destroy);
				}
			}
		}

		NewMOIStateData.ObjectType = EObjectType::OTFaceHosted;
		NewMOIStateData.ID = nextID++;
		NewMOIStateData.ParentID = parentID;
		NewMOIStateData.AssemblyGUID = AssemblyGUID;
		delta->AddCreateDestroyState(NewMOIStateData, EMOIDeltaType::Create);
		NewObjectIDs.Add(NewMOIStateData.ID);

	}

	return true;
}
