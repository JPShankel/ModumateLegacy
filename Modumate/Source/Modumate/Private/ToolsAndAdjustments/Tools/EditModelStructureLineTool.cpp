// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Tools/EditModelStructureLineTool.h"

#include "DocumentManagement/ModumateCommands.h"
#include "DocumentManagement/ModumateDocument.h"
#include "Graph/Graph3D.h"
#include "ModumateCore/ModumateStairStatics.h"
#include "Objects/MetaEdgeSpan.h"
#include "Objects/StructureLine.h"
#include "UnrealClasses/DynamicMeshActor.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "UnrealClasses/EditModelGameMode.h"
#include "UnrealClasses/EditModelGameState.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "UnrealClasses/LineActor.h"
#include "UI/DimensionManager.h"
#include "UI/PendingSegmentActor.h"
#include "Objects/ModumateObjectDeltaStatics.h"



UStructureLineTool::UStructureLineTool()
	: Super()
	, bWasShowingSnapCursor(false)
	, OriginalMouseMode(EMouseMode::Location)
	, bWantedVerticalSnap(false)
	, LastValidTargetID(MOD_ID_NONE)
	, LastTargetStructureLineID(MOD_ID_NONE)
	, LineStartPos(ForceInitToZero)
	, LineEndPos(ForceInitToZero)
	, LineDir(ForceInitToZero)
	, ObjNormal(ForceInitToZero)
	, ObjUp(ForceInitToZero)
{
}

bool UStructureLineTool::Activate()
{
	if (!UEditModelToolBase::Activate())
	{
		return false;
	}

	// Set default settings for NewMOIStateData
	FMOIStructureLineData newStructureLineCustomData(FMOIStructureLineData::CurrentVersion);
	NewMOIStateData.ObjectType = UModumateTypeStatics::ObjectTypeFromToolMode(GetToolMode());
	NewMOIStateData.CustomData.SaveStructData(newStructureLineCustomData);

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

bool UStructureLineTool::HandleInputNumber(double n)
{
	FVector direction = LineEndPos - LineStartPos;
	direction.Normalize();

	LineEndPos = LineStartPos + direction * n;

	if (MakeStructureLine())
	{
		EndUse();
	}

	return true;
}

bool UStructureLineTool::Deactivate()
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

bool UStructureLineTool::BeginUse()
{
	if (!AssemblyGUID.IsValid())
	{
		return false;
	}

	switch (CreateObjectMode)
	{
	case EToolCreateObjectMode::SpanEdit:
		if (LastValidTargetID != MOD_ID_NONE)
		{
			if (PreviewSpanGraphMemberIDs.Remove(LastValidTargetID) == 0)
			{
				PreviewSpanGraphMemberIDs.AddUnique(LastValidTargetID);
			}
		}
		return false;
	case EToolCreateObjectMode::Draw:
	{
		Super::BeginUse();
		LineStartPos = Controller->EMPlayerState->SnappedCursor.WorldPosition;
		LineEndPos = LineStartPos;

		auto dimensionActor = DimensionManager->GetDimensionActor(PendingSegmentID);

		if (dimensionActor != nullptr)
		{
			auto pendingSegment = dimensionActor->GetLineActor();
			pendingSegment->Point1 = LineStartPos;
			pendingSegment->Point2 = LineStartPos;
			pendingSegment->Color = FColor::Black;
			pendingSegment->Thickness = 3;
		}

	}
	break;
	case EToolCreateObjectMode::Apply:
	case EToolCreateObjectMode::Add:
	{
		if (LastValidTargetID != MOD_ID_NONE)
		{
			MakeStructureLine(LastValidTargetID);
		}
	}
	break;
	}


	return true;
}

bool UStructureLineTool::EnterNextStage()
{
	FSnappedCursor &cursor = Controller->EMPlayerState->SnappedCursor;

	auto pendingSegment = DimensionManager->GetDimensionActor(PendingSegmentID)->GetLineActor();
	if ((CreateObjectMode == EToolCreateObjectMode::Draw) &&
		pendingSegment != nullptr && !pendingSegment->Point1.Equals(pendingSegment->Point2))
	{
		bool bCreationSuccess = MakeStructureLine();
		return false;
	}

	return true;
}

bool UStructureLineTool::FrameUpdate()
{
	const FSnappedCursor &cursor = Controller->EMPlayerState->SnappedCursor;

	if (!cursor.Visible || !AssemblyGUID.IsValid())
	{
		return false;
	}

	int32 newID = GameState->Document->GetNextAvailableID();

	if (CreateObjectMode == EToolCreateObjectMode::Draw)
	{
		auto dimensionActor = DimensionManager->GetDimensionActor(PendingSegmentID);
		if (IsInUse() && dimensionActor != nullptr)
		{
			auto pendingSegment = dimensionActor->GetLineActor();
			LineEndPos = cursor.WorldPosition;
			pendingSegment->Point2 = LineEndPos;
			LineDir = (LineEndPos - LineStartPos).GetSafeNormal();
			UModumateGeometryStatics::FindBasisVectors(ObjNormal, ObjUp, LineDir);

			// Preview the deltas to create a structureline
			if (GameState->Document->StartPreviewing() && GetObjectCreationDeltas({}, newID, CurDeltas, false))
			{
				GameState->Document->ApplyPreviewDeltas(CurDeltas, GetWorld());
			}
		}
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
		if (GetObjectCreationDeltas({ LastValidTargetID }, newID, deltas, false))
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
		bool bSpanEdit = FModumateObjectDeltaStatics::GetEdgeSpanCreationDeltas(PreviewSpanGraphMemberIDs, newID, AssemblyGUID, NewMOIStateData, deltas, newSpanID, newEdgeHostedObjID);
		if (bSpanEdit)
		{
			NewObjectIDs = { newSpanID, newEdgeHostedObjID };
			GameState->Document->ApplyPreviewDeltas(deltas, GetWorld());
		}
	}

	return true;
}

bool UStructureLineTool::EndUse()
{
	ResetState();
	return UEditModelToolBase::EndUse();
}

bool UStructureLineTool::AbortUse()
{
	ResetState();
	return UEditModelToolBase::AbortUse();
}

bool UStructureLineTool::HandleFlip(EAxis::Type FlipAxis)
{
	if (NewObjectIDs.Num() == 0)
	{
		return false;
	}

	AModumateObjectInstance* newMOI = GameState->Document->GetObjectById(NewObjectIDs[0]);
	return newMOI && newMOI->GetFlippedState(FlipAxis, NewMOIStateData);
}

bool UStructureLineTool::HandleOffset(const FVector2D& ViewSpaceDirection)
{
	if (NewObjectIDs.Num() == 0)
	{
		return false;
	}

	FQuat cameraRotation = Controller->PlayerCameraManager->GetCameraRotation().Quaternion();
	FVector worldSpaceDirection =
		(ViewSpaceDirection.X * cameraRotation.GetRightVector()) +
		(ViewSpaceDirection.Y * cameraRotation.GetUpVector());
	
	// StructureLineTool creates single edge span, 
	// we handle offset by setting its child (StructureLine) instance data
	AModumateObjectInstance* newMOI = GameState->Document->GetObjectById(NewObjectIDs[0]);
	AMOIMetaEdgeSpan* span = Cast<AMOIMetaEdgeSpan>(newMOI);
	if (span && span->GetChildIDs().Num() > 0)
	{
		newMOI = span->GetChildObjects()[0];
	}
	return newMOI && newMOI->GetOffsetState(worldSpaceDirection, NewMOIStateData);
}

void UStructureLineTool::OnCreateObjectModeChanged()
{
	Super::OnCreateObjectModeChanged();

	if ((CreateObjectMode == EToolCreateObjectMode::Apply) && IsInUse())
	{
		AbortUse();
	}
	else
	{
		ResetState();
	}
}

void UStructureLineTool::SetTargetID(int32 NewTargetID)
{
	LastValidTargetID = NewTargetID;
}

bool UStructureLineTool::MakeStructureLine(int32 TargetEdgeID)
{
	GameState->Document->ClearPreviewDeltas(GetWorld());

	TArray<FDeltaPtr> deltas;
	int32 newID = GameState->Document->GetNextAvailableID();
	if (!GetObjectCreationDeltas({ TargetEdgeID }, newID, deltas, true))
	{
		return false;
	}

	if (deltas.Num() == 0)
	{
		return false;
	}

	bool bSuccess = GameState->Document->ApplyDeltas(deltas, GetWorld());

	return bSuccess;
}

void UStructureLineTool::ResetState()
{
	SetTargetID(MOD_ID_NONE);
	LineStartPos = FVector::ZeroVector;
	LineEndPos = FVector::ZeroVector;
	LineDir = FVector::ZeroVector;
	ObjNormal = FVector::ZeroVector;
	ObjUp = FVector::ZeroVector;
	PreviewSpanGraphMemberIDs.Empty();

	FMOIStructureLineData newMOICustomData(FMOIStructureLineData::CurrentVersion);
	NewMOIStateData.CustomData.SaveStructData(newMOICustomData);
}

void UStructureLineTool::CommitSpanEdit() 
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
		ResetState();
	}
	EndUse();
}

void UStructureLineTool::CancelSpanEdit()
{
	ResetState();
}

bool UStructureLineTool::GetObjectCreationDeltas(const TArray<int32>& InTargetEdgeIDs, int32& NewID, TArray<FDeltaPtr>& OutDeltaPtrs, bool bSplitFaces)
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

	// If there are no targeted edges, ex: Draw Mode, make new edges as target
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
		if (!GameState->Document->MakeMetaObject(Controller->GetWorld(), points, targetEdgeIDs, OutDeltaPtrs, bSplitFaces))
		{
			return false;
		}
	}
	// By this point we should have at least make an edge, but check it anyway
	if (targetEdgeIDs.Num() == 0)
	{
		return false;
	}

	// We need to keep track of next ID because Document->MakeMetaObject can make 1+ new meta objects
	NewID = GameState->Document->GetNextAvailableID();

	return FModumateObjectDeltaStatics::GetObjectCreationDeltasForEdgeSpans(GameState->Document, GetCreateObjectMode(),
		targetEdgeIDs, NewID, TargetSpanIndex, AssemblyGUID, NewMOIStateData, OutDeltaPtrs, NewObjectIDs);
}
