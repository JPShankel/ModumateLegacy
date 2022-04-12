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
	int32 hitFaceHostedID = MOD_ID_NONE;
	if (cursor.Actor)
	{
		hitMOI = GameState->Document->ObjectFromActor(cursor.Actor);
		if (hitMOI->GetObjectType() == EObjectType::OTFaceHosted)
		{
			hitFaceHostedID = hitMOI->ID;
		}
		if (hitMOI && (hitMOI->GetObjectType() == EObjectType::OTFaceHosted || compatibleObjectTypes.Contains(hitMOI->GetObjectType())))
		{
			hitMOI = hitMOI->GetParentObject();
		}
	}

	if (hitMOI && (hitMOI->GetObjectType() == EObjectType::OTMetaPlane))
	{
		newTargetID = hitMOI->ID;
	}
	if (hitMOI && (hitMOI->GetObjectType() == EObjectType::OTMetaPlaneSpan))
	{
		newTargetID = hitMOI->GetParentID();
	}

	SetTargetID(newTargetID);
	if (LastValidTargetID != MOD_ID_NONE)
	{
		if (GetCreateObjectMode() == EToolCreateObjectMode::Apply ||
			GetCreateObjectMode() == EToolCreateObjectMode::Add)
		{
			// Preview delta
			if (GameState->Document->StartPreviewing() && GetObjectCreationDeltas({ LastValidTargetID }, CurDeltas, hitFaceHostedID))
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

		AModumateObjectInstance* parentMOI = GameState->Document->GetObjectById(targetFaceID);

		EObjectType objectType = EObjectType::OTFaceHosted;
		TArray<int32> inGraphMemberIDs;
		int32 spanParentID = MOD_ID_NONE;
		bool bCreateNewSpan = true;
		bool bCreateNewFaceHosted = true;
		if (parentMOI->GetObjectType() == EObjectType::OTMetaPlane)
		{
			inGraphMemberIDs.Add(parentMOI->ID);
			if (CreateObjectMode == EToolCreateObjectMode::Add)
			{
				//UE_LOG(LogTemp, Error, TEXT("MetaFace, Add"));
				bCreateNewSpan = true;
				bCreateNewFaceHosted = true;
			}
			else if (CreateObjectMode == EToolCreateObjectMode::Apply)
			{
				//UE_LOG(LogTemp, Error, TEXT("MetaFace, Apply"));
				bCreateNewSpan = parentMOI->GetChildObjects().Num() == 0;
				bCreateNewFaceHosted = parentMOI->GetChildObjects().Num() == 0;
			}
		}
		else if (parentMOI->GetObjectType() == EObjectType::OTMetaPlaneSpan)
		{
			AMOIMetaPlaneSpan* span = Cast<AMOIMetaPlaneSpan>(parentMOI);
			inGraphMemberIDs = span->InstanceData.GraphMembers;
			if (CreateObjectMode == EToolCreateObjectMode::Add)
			{
				//UE_LOG(LogTemp, Error, TEXT("SpanFace, Add"));
				bCreateNewSpan = true;
				bCreateNewFaceHosted = true;
			}
			else if (CreateObjectMode == EToolCreateObjectMode::Apply)
			{
				//UE_LOG(LogTemp, Error, TEXT("SpanFace, Apply"));
				bCreateNewSpan = false;
				bCreateNewFaceHosted = false;
				spanParentID = parentMOI->ID;
			}
		}
		if (bCreateNewSpan)
		{
			FMOIStateData spanCreateState(nextID++, EObjectType::OTMetaPlaneSpan);
			FMOIMetaPlaneSpanData spanData;
			spanData.GraphMembers = inGraphMemberIDs;
			spanCreateState.CustomData.SaveStructData<FMOIMetaPlaneSpanData>(spanData);
			spanCreateState.ParentID = parentMOI->ID;
			delta->AddCreateDestroyState(spanCreateState, EMOIDeltaType::Create);
			NewObjectIDs.Add(spanCreateState.ID);
			spanParentID = spanCreateState.ID;
		}

		if (bCreateNewFaceHosted)
		{
			FMOIStateData faceHostedCreateState(nextID++, EObjectType::OTFaceHosted, spanParentID);
			faceHostedCreateState.AssemblyGUID = AssemblyGUID;
			FMOIFaceHostedData faceHostedData;
			faceHostedCreateState.CustomData.SaveStructData<FMOIFaceHostedData>(faceHostedData);
			delta->AddCreateDestroyState(faceHostedCreateState, EMOIDeltaType::Create);
			NewObjectIDs.Add(faceHostedCreateState.ID);
		}
		else
		{
			//replace existing mesh
			//mutate one of the existing meshes and then destroy any others

			bool bMutationFound = false;
			for (auto child : parentMOI->GetChildObjects())
			{
				//if the child is a span, loop through it's children to find the facehosted object to replace.
				if (child->GetObjectType() == EObjectType::OTMetaPlaneSpan)
				{
					bool bSpanMutated = false;
					for (auto spanChild : child->GetChildObjects())
					{
						if (spanChild->GetObjectType() == EObjectType::OTFaceHosted)
						{
							if (bMutationFound)
							{
								delta->AddCreateDestroyState(spanChild->GetStateData(), EMOIDeltaType::Destroy);
							}
							else {
								FMOIStateData& newState = delta->AddMutationState(spanChild);
								newState.ParentID = child->ID;
								newState.AssemblyGUID = AssemblyGUID;
								bMutationFound = true;
								bSpanMutated = true;
							}
						}
					}
					if(!bSpanMutated)
						delta->AddCreateDestroyState(child->GetStateData(), EMOIDeltaType::Destroy);
				}
				if (child->GetObjectType() == EObjectType::OTFaceHosted)
				{
					if (bMutationFound)
					{
						delta->AddCreateDestroyState(child->GetStateData(), EMOIDeltaType::Destroy);
					}
					else {
						FMOIStateData& newState = delta->AddMutationState(child);
						newState.ParentID = parentMOI->ID;
						newState.AssemblyGUID = AssemblyGUID;
						bMutationFound = true;
					}
				}
			}
		}
	}

	return true;
}
