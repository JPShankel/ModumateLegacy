// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Tools/EditModelEdgeHostedTool.h"
#include "DocumentManagement/ModumateCommands.h"
#include "DocumentManagement/ModumateDocument.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "UnrealClasses/EditModelGameState.h"
#include "Objects/EdgeHosted.h"
#include "Objects/MetaEdgeSpan.h"
#include "Objects/ModumateObjectDeltaStatics.h"

UEdgeHostedTool::UEdgeHostedTool()
	: Super()
{}

bool UEdgeHostedTool::Activate()
{
	if (!UEditModelToolBase::Activate())
	{
		return false;
	}

	// Set default settings for NewMOIStateData
	FMOIEdgeHostedData newEdgeHostedCustomData;
	NewMOIStateData.ObjectType = EObjectType::OTEdgeHosted;
	NewMOIStateData.CustomData.SaveStructData(newEdgeHostedCustomData);

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

	// Find cursor hit actor, will only get MetaObj per called from AEditModelPlayerController::UpdateMouseTraceParams()
	if (cursor.Actor)
	{
		hitMOI = GameState->Document->ObjectFromActor(cursor.Actor);
	}

	// Only MetaEdges in active group are valid target
	if (hitMOI && hitMOI->GetObjectType() == EObjectType::OTMetaEdge &&
		IsObjectInActiveGroup(hitMOI))
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
		TArray<FDeltaPtr> deltas;
		if (GetObjectCreationDeltas(LastValidTargetID, deltas))
		{
			GameState->Document->ApplyPreviewDeltas(deltas, GetWorld());
		}
	}

	// Preview span edit on every frame
	if (GetCreateObjectMode() == EToolCreateObjectMode::SpanEdit)
	{
		int32 newSpanID;
		int32 newEdgeHostedObjID;
		TArray<FDeltaPtr> deltas;
		int32 newID = GameState->Document->GetNextAvailableID();
		bool bSpanEdit = FModumateObjectDeltaStatics::GetEdgeSpanCreationDeltas(PreviewSpanGraphMemberIDs, newID, AssemblyGUID, NewMOIStateData, deltas, newSpanID, newEdgeHostedObjID);
		if (bSpanEdit)
		{
			NewObjectIDs = { newSpanID, newEdgeHostedObjID };
			GameState->Document->ApplyPreviewDeltas(deltas, GetWorld());
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
	if (!GetObjectCreationDeltas(LastValidTargetID, deltas))
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

bool UEdgeHostedTool::HandleOffset(const FVector2D& ViewSpaceDirection)
{
	if (NewObjectIDs.Num() == 0)
	{
		return false;
	}

	FQuat cameraRotation = Controller->PlayerCameraManager->GetCameraRotation().Quaternion();
	FVector worldSpaceDirection =
		(ViewSpaceDirection.X * cameraRotation.GetRightVector()) +
		(ViewSpaceDirection.Y * cameraRotation.GetUpVector());

	// EdgeHostedTool creates single edge span, 
	// we handle offset by setting its child (EdgeHosted) instance data
	AModumateObjectInstance* newMOI = GameState->Document->GetObjectById(NewObjectIDs[0]);
	AMOIMetaEdgeSpan* span = Cast<AMOIMetaEdgeSpan>(newMOI);
	if (span && span->GetChildIDs().Num() > 0)
	{
		newMOI = span->GetChildObjects()[0];
	}
	return newMOI && newMOI->GetOffsetState(worldSpaceDirection, NewMOIStateData);
}

void UEdgeHostedTool::CommitSpanEdit()
{
	GameState->Document->ClearPreviewDeltas(GetWorld());

	int32 newSpanID;
	int32 newEdgeHostedObjID;
	TArray<FDeltaPtr> deltas;
	int32 newID = GameState->Document->GetNextAvailableID();
	bool bSuccess = FModumateObjectDeltaStatics::GetEdgeSpanCreationDeltas(PreviewSpanGraphMemberIDs, newID, AssemblyGUID, NewMOIStateData, deltas, newSpanID, newEdgeHostedObjID);
	if (bSuccess && GameState->Document->ApplyDeltas(deltas, GetWorld()))
	{
		NewObjectIDs = { newSpanID, newEdgeHostedObjID };
		ResetSpanIDs();
	}
	EndUse();
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

bool UEdgeHostedTool::GetObjectCreationDeltas(const int32 InTargetEdgeID, TArray<FDeltaPtr>& OutDeltaPtrs)
{
	OutDeltaPtrs.Reset();
	NewObjectIDs.Reset();

	int32 nextID = GameState->Document->GetNextAvailableID();

	return FModumateObjectDeltaStatics::GetObjectCreationDeltasForEdgeSpans(GameState->Document, GetCreateObjectMode(),
		{ InTargetEdgeID }, nextID, TargetSpanIndex, AssemblyGUID, NewMOIStateData, OutDeltaPtrs, NewObjectIDs);
}

void UEdgeHostedTool::ResetSpanIDs()
{
	PreviewSpanGraphMemberIDs.Empty();
}
