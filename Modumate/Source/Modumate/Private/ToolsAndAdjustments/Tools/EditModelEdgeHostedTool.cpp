// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Tools/EditModelEdgeHostedTool.h"
#include "DocumentManagement/ModumateCommands.h"
#include "DocumentManagement/ModumateDocument.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "UnrealClasses/EditModelGameState.h"
#include "Objects/EdgeHosted.h"
#include "Objects/MetaEdgeSpan.h"

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
	ResetSpanIDs();

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

	if (hitMOI && (hitMOI->GetObjectType() == EObjectType::OTMetaEdge) && IsObjectInActiveGroup(hitMOI))
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
		if (GetCreateObjectMode() == EToolCreateObjectMode::Apply)
		{
			if (GameState->Document->StartPreviewing() && GetObjectCreationDeltas({ LastValidTargetID }, CurDeltas))
			{
				GameState->Document->ApplyPreviewDeltas(CurDeltas, GetWorld());
			}
		}

		// Don't show the snap cursor if we're targeting an edge or existing structure line.
		Controller->EMPlayerState->bShowSnappedCursor = (LastValidTargetID == MOD_ID_NONE);
	}

	// Preview span edit on every flame
	if (GetCreateObjectMode() == EToolCreateObjectMode::SpanEdit)
	{
		if (GetSpanCreationDelta(CurDeltas))
		{
			GameState->Document->ApplyPreviewDeltas(CurDeltas, GetWorld());
		}
	}

	return true;
}

bool UEdgeHostedTool::BeginUse()
{
	if (GetCreateObjectMode() == EToolCreateObjectMode::SpanEdit && LastValidTargetID != MOD_ID_NONE)
	{
		// TODO: Check if legal before adding member
		// If click on a edge that's already selected, remove it from member list
		// else add to member list
		if (PreviewSpanGraphMemberIDs.Remove(LastValidTargetID) == 0)
		{
			PreviewSpanGraphMemberIDs.AddUnique(LastValidTargetID);
		}
		return false;
	}

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

void UEdgeHostedTool::CommitSpanEdit()
{
	GameState->Document->ClearPreviewDeltas(GetWorld());

	bool bSucess = GetSpanCreationDelta(CurDeltas);
	if (bSucess && GameState->Document->ApplyDeltas(CurDeltas, GetWorld()))
	{
		ResetSpanIDs();
		EndUse();
	}
}

void UEdgeHostedTool::CancelSpanEdit()
{
	ResetSpanIDs();

}

void UEdgeHostedTool::OnCreateObjectModeChanged()
{
	Super::OnCreateObjectModeChanged();
	ResetSpanIDs();
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
		AModumateObjectInstance* edgeOb = GameState->Document->GetObjectById(targetEdgeID);

		EObjectType objectType = EObjectType::OTEdgeHosted;

		TArray<int32> spans;
		UModumateObjectStatics::GetSpansForEdgeObject(GameState->Document, edgeOb, spans);

		const AMOIMetaEdgeSpan* spanOb = nullptr;

		int32 spanParentID = MOD_ID_NONE;
		if (spans.Num() == 0)
		{
			FMOIStateData spanCreateState(nextID++, EObjectType::OTMetaEdgeSpan);
			FMOIMetaEdgeSpanData spanData;
			spanData.GraphMembers.Add(targetEdgeID);
			spanCreateState.CustomData.SaveStructData(spanData);
			delta->AddCreateDestroyState(spanCreateState, EMOIDeltaType::Create);
			NewObjectIDs.Add(spanCreateState.ID);
			spanParentID = spanCreateState.ID;
		}
		else
		{
			spanOb = Cast<AMOIMetaEdgeSpan>(GameState->Document->GetObjectById(spans[0]));
			if (ensure(spanOb))
			{
				spanParentID = spanOb->ID;
			}
		}

		// TODO: use spanOb instead of edgeOb when parenting is put in.
		if (edgeOb && ensure(edgeOb->GetObjectType() == EObjectType::OTMetaEdge) &&
			CreateObjectMode != EToolCreateObjectMode::Add)
		{
			for (auto child : edgeOb->GetChildObjects())
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
			NewMOIStateData.ParentID = spanParentID;

			NewMOIStateData.ID = nextID++;
			NewMOIStateData.ObjectType = EObjectType::OTEdgeHosted;
			NewMOIStateData.AssemblyGUID = AssemblyGUID;
			FMOIEdgeHostedData newCustomData;
			NewMOIStateData.CustomData.SaveStructData<FMOIEdgeHostedData>(newCustomData);

			NewObjectIDs.Add(NewMOIStateData.ID);

			delta->AddCreateDestroyState(NewMOIStateData, EMOIDeltaType::Create);
		}
	}

	return true;
}

bool UEdgeHostedTool::GetSpanCreationDelta(TArray<FDeltaPtr>& OutDeltaPtrs)
{
	OutDeltaPtrs.Reset();
	auto deltaPtr = MakeShared<FMOIDelta>();
	if (PreviewSpanGraphMemberIDs.Num() == 0)
	{
		return false;
	}
	NewObjectIDs.Reset();
	int32 newObjID = GameState->Document->GetNextAvailableID();

	// Create new span that contains all preview graph members
	FMOIStateData spanCreateState(newObjID++, EObjectType::OTMetaEdgeSpan);
	FMOIMetaEdgeSpanData spanData;
	spanData.GraphMembers = PreviewSpanGraphMemberIDs;
	spanCreateState.CustomData.SaveStructData(spanData);
	deltaPtr->AddCreateDestroyState(spanCreateState, EMOIDeltaType::Create);

	// Create edge hosted object that will be child of the new preview span
	NewMOIStateData.ParentID = spanCreateState.ID;
	NewMOIStateData.ObjectType = EObjectType::OTEdgeHosted;
	NewMOIStateData.ID = newObjID++;
	NewMOIStateData.AssemblyGUID = AssemblyGUID;
	FMOIEdgeHostedData newCustomData;
	NewMOIStateData.CustomData.SaveStructData<FMOIEdgeHostedData>(newCustomData);
	deltaPtr->AddCreateDestroyState(NewMOIStateData, EMOIDeltaType::Create);
	NewObjectIDs.Add(NewMOIStateData.ID);
	
	OutDeltaPtrs.Add(deltaPtr);
	return true;
}

void UEdgeHostedTool::ResetSpanIDs()
{
	PreviewSpanGraphMemberIDs.Empty();
}
