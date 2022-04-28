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

	static TSet<EObjectType> hostedTypes = { EObjectType::OTEdgeHosted, EObjectType::OTStructureLine, EObjectType::OTMullion };

	if (cursor.Actor)
	{
		hitMOI = GameState->Document->ObjectFromActor(cursor.Actor);

		if (hitMOI && (hostedTypes.Contains(hitMOI->GetObjectType())))
		{
			hitMOI = hitMOI->GetParentObject();
		}
	}

	if (hitMOI &&
		(hitMOI->GetObjectType() == EObjectType::OTMetaEdge ||
		hitMOI->GetObjectType() == EObjectType::OTMetaEdgeSpan) &&
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

		// Preview delta
		if (GetCreateObjectMode() == EToolCreateObjectMode::Apply ||
			GetCreateObjectMode() == EToolCreateObjectMode::Add)
		{
			if (GameState->Document->StartPreviewing() && GetObjectCreationDeltas(LastValidTargetID, CurDeltas))
			{
				GameState->Document->ApplyPreviewDeltas(CurDeltas, GetWorld());
			}
		}

		// Don't show the snap cursor if we're targeting an edge or existing structure line.
		Controller->EMPlayerState->bShowSnappedCursor = (LastValidTargetID == MOD_ID_NONE);
	}

	// Preview span edit on every frame
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

bool UEdgeHostedTool::GetObjectCreationDeltas(const int32 InTargetEdgeID, TArray<FDeltaPtr>& OutDeltaPtrs)
{
	OutDeltaPtrs.Reset();
	NewObjectIDs.Reset();
	// Make sure the pending line is valid
	TArray<int32> targetEdgeIDs = { InTargetEdgeID };

	bool bCreateNewSpan = false;
	bool bCreateNewEdgeHosted = false;
	bool bDestroyChildren = false;
	int32 spanParentID = MOD_ID_NONE;
	int32 nextID = GameState->Document->GetNextAvailableID();

	AModumateObjectInstance* inTargetEdgeMOI = GameState->Document->GetObjectById(InTargetEdgeID);
	if (!inTargetEdgeMOI)
	{
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
		bCreateNewSpan = true;
		bCreateNewEdgeHosted = true;
		spanParentID = nextID;
	}

	// Begin making delta
	TSharedPtr<FMOIDelta> delta = MakeShared<FMOIDelta>();
	OutDeltaPtrs.Add(delta);

	TArray<int32> inGraphMemberIDs;

	if (inTargetEdgeMOI && inTargetEdgeMOI->GetObjectType() == EObjectType::OTMetaEdge)
	{
		inGraphMemberIDs.Add(inTargetEdgeMOI->ID);
		bCreateNewSpan = true;
		bCreateNewEdgeHosted = true;
	}
	else if (inTargetEdgeMOI && inTargetEdgeMOI->GetObjectType() == EObjectType::OTMetaEdgeSpan)
	{
		spanParentID = inTargetEdgeMOI->ID;
		bCreateNewSpan = false;
		bCreateNewEdgeHosted = true;
		AMOIMetaEdgeSpan* span = Cast<AMOIMetaEdgeSpan>(inTargetEdgeMOI);
		inGraphMemberIDs = span->InstanceData.GraphMembers;
		if (CreateObjectMode == EToolCreateObjectMode::Apply)
		{
			//UE_LOG(LogTemp, Error, TEXT("SpanEdge, Apply"));
			bDestroyChildren = true;
		}
	}

	if (bDestroyChildren)
	{
		for (auto* child : inTargetEdgeMOI->GetChildObjects())
		{
			delta->AddCreateDestroyState(child->GetStateData(), EMOIDeltaType::Destroy);
		}
	}

	if (bCreateNewSpan)
	{
		FMOIStateData spanCreateState(nextID++, EObjectType::OTMetaEdgeSpan);
		FMOIMetaEdgeSpanData spanData;
		spanData.GraphMembers = inGraphMemberIDs;
		spanCreateState.CustomData.SaveStructData<FMOIMetaEdgeSpanData>(spanData);
		spanCreateState.ParentID = MOD_ID_NONE;
		delta->AddCreateDestroyState(spanCreateState, EMOIDeltaType::Create);
		NewObjectIDs.Add(spanCreateState.ID);
		spanParentID = spanCreateState.ID;
	}

	if (bCreateNewEdgeHosted)
	{
		NewMOIStateData.ID = nextID++;
		NewMOIStateData.ParentID = spanParentID;
		NewMOIStateData.AssemblyGUID = AssemblyGUID;
		delta->AddCreateDestroyState(NewMOIStateData, EMOIDeltaType::Create);
		NewObjectIDs.Add(NewMOIStateData.ID);
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
	NewMOIStateData.ID = newObjID++;
	NewMOIStateData.AssemblyGUID = AssemblyGUID;
	deltaPtr->AddCreateDestroyState(NewMOIStateData, EMOIDeltaType::Create);
	NewObjectIDs.Add(NewMOIStateData.ID);
	
	OutDeltaPtrs.Add(deltaPtr);
	return true;
}

void UEdgeHostedTool::ResetSpanIDs()
{
	PreviewSpanGraphMemberIDs.Empty();
}
