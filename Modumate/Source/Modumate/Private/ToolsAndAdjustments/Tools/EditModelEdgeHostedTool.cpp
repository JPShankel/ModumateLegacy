// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Tools/EditModelEdgeHostedTool.h"
#include "DocumentManagement/ModumateCommands.h"
#include "DocumentManagement/ModumateDocument.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "UnrealClasses/EditModelGameState.h"

UEdgeHostedTool::UEdgeHostedTool()
	: Super()
{}

bool UEdgeHostedTool::Activate()
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

bool UEdgeHostedTool::Deactivate()
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

bool UEdgeHostedTool::FrameUpdate()
{
	const FSnappedCursor& cursor = Controller->EMPlayerState->SnappedCursor;
	if (!AssemblyGUID.IsValid())
	{
		return false;
	}

	int32 newTargetID = MOD_ID_NONE;
	const AModumateObjectInstance* hitMOI = nullptr;
	LineStartPos = LineEndPos = FVector::ZeroVector;

	if (cursor.Actor)
	{
		hitMOI = GameState->Document->ObjectFromActor(cursor.Actor);

		if (hitMOI && (hitMOI->GetObjectType() == EObjectType::OTEdgeHosted))
		{
			hitMOI = hitMOI->GetParentObject();
		}
	}

	if (hitMOI && (hitMOI->GetObjectType() == EObjectType::OTMetaEdge))
	{
		newTargetID = hitMOI->ID;
	}

	SetTargetID(newTargetID);
	if (LastValidTargetID != MOD_ID_NONE)
	{
		// Show an affordance for the preview object
		LineStartPos = hitMOI->GetCorner(0);
		LineEndPos = hitMOI->GetCorner(1);
		LineDir = (LineEndPos - LineStartPos).GetSafeNormal();
		UModumateGeometryStatics::FindBasisVectors(ObjNormal, ObjUp, LineDir);

		Controller->EMPlayerState->AffordanceLines.Add(FAffordanceLine(
			LineStartPos, LineEndPos,
			AffordanceLineColor, AffordanceLineInterval, AffordanceLineThickness, 1)
		);

		// Preview delta
		if (GameState->Document->StartPreviewing() && GetObjectCreationDeltas({ LastValidTargetID }, CurDeltas))
		{
			GameState->Document->ApplyPreviewDeltas(CurDeltas, GetWorld());
		}

		// Don't show the snap cursor if we're targeting an edge or existing structure line.
		Controller->EMPlayerState->bShowSnappedCursor = (LastValidTargetID == MOD_ID_NONE);
	}

	return true;
}

bool UEdgeHostedTool::BeginUse()
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

void UEdgeHostedTool::ResetState()
{
	SetTargetID(MOD_ID_NONE);
	LineStartPos = FVector::ZeroVector;
	LineEndPos = FVector::ZeroVector;
	LineDir = FVector::ZeroVector;
	ObjNormal = FVector::ZeroVector;
	ObjUp = FVector::ZeroVector;
}

void UEdgeHostedTool::SetTargetID(int32 NewTargetID)
{
	LastValidTargetID = NewTargetID;
}

bool UEdgeHostedTool::GetObjectCreationDeltas(const TArray<int32>& InTargetEdgeIDs, TArray<FDeltaPtr>& OutDeltaPtrs)
{
	OutDeltaPtrs.Reset();
	NewObjectIDs.Reset();

	TArray<int32> targetEdgeIDs;
	for (int32 targetEdgeID : InTargetEdgeIDs)
	{
		if (targetEdgeID != MOD_ID_NONE)
		{
			targetEdgeIDs.Add(targetEdgeID);
		}
	}

	if (targetEdgeIDs.Num() == 0)
	{
		// Make sure the pending line is valid
		FVector lineDelta = LineEndPos - LineStartPos;
		float lineLength = lineDelta.Size();
		if (FMath::IsNearlyZero(lineLength))
		{
			return false;
		}

		TArray<FVector> points({ LineStartPos, LineEndPos });
		if (!GameState->Document->MakeMetaObject(Controller->GetWorld(), points, targetEdgeIDs, OutDeltaPtrs))
		{
			return false;
		}
	}

	TSharedPtr<FMOIDelta> delta;
	int32 nextID = GameState->Document->GetNextAvailableID();

	// Follows the pattern established in UPlaneHostedObjTool
	for (int32 targetEdgeID : targetEdgeIDs)
	{
		if (!delta.IsValid())
		{
			delta = MakeShared<FMOIDelta>();
			OutDeltaPtrs.Add(delta);
		}

		bool bCreateNewObject = true;
		AModumateObjectInstance* parentMOI = GameState->Document->GetObjectById(targetEdgeID);

		EObjectType objectType = EObjectType::OTEdgeHosted;

		if (parentMOI && ensure(parentMOI->GetObjectType() == EObjectType::OTMetaEdge))
		{
			for (auto child : parentMOI->GetChildObjects())
			{
				if (child->GetObjectType() == objectType)
				{
					FMOIStateData& newState = delta->AddMutationState(child);
					newState.AssemblyGUID = AssemblyGUID;
					bCreateNewObject = false;
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
			NewMOIStateData.ObjectType = EObjectType::OTEdgeHosted;
			NewMOIStateData.ParentID = targetEdgeID;
			NewMOIStateData.AssemblyGUID = AssemblyGUID;

			NewObjectIDs.Add(NewMOIStateData.ID);

			delta->AddCreateDestroyState(NewMOIStateData, EMOIDeltaType::Create);
		}
	}

	return true;
}
