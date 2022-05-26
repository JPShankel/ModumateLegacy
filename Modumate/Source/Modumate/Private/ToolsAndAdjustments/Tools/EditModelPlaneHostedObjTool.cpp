// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Tools/EditModelPlaneHostedObjTool.h"

#include "DocumentManagement/ModumateDocument.h"
#include "DocumentManagement/ModumateCommands.h"
#include "Objects/PlaneHostedObj.h"
#include "Objects/ModumateObjectStatics.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "UnrealClasses/EditModelGameState.h"
#include "UnrealClasses/EditModelGameMode.h"
#include "UnrealClasses/LineActor.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "UI/DimensionManager.h"
#include "UI/PendingSegmentActor.h"
#include "Objects/MetaPlaneSpan.h"
#include "Objects/ModumateObjectDeltaStatics.h"



UPlaneHostedObjTool::UPlaneHostedObjTool()
	: Super()
	, bInverted(false)
	, bRequireHoverMetaPlane(false)
	, ObjectType(EObjectType::OTNone)
	, LastValidTargetID(MOD_ID_NONE)
	, bWasShowingSnapCursor(true)
{
	//InstanceJustification = GetDefaultJustificationValue();
}

bool UPlaneHostedObjTool::Activate()
{
	if (!Super::Activate())
	{
		return false;
	}

	FMOIPlaneHostedObjData newMOICustomData(FMOIPlaneHostedObjData::CurrentVersion);
	newMOICustomData.FlipSigns.Y = GetAppliedInversionValue() ? -1.0 : 1.0f;
	//newMOICustomData.Justification_DEPRECATED = InstanceJustification;

	NewMOIStateData.ObjectType = ObjectType;
	NewMOIStateData.CustomData.SaveStructData(newMOICustomData);

	bWasShowingSnapCursor = Controller->EMPlayerState->bShowSnappedCursor;
	TargetSpanIndex = 0;
	ResetSpanIDs();
	return true;
}

bool UPlaneHostedObjTool::HandleInputNumber(double n)
{
	return Super::HandleInputNumber(n);
}

bool UPlaneHostedObjTool::Deactivate()
{
	if (Controller)
	{
		Controller->EMPlayerState->bShowSnappedCursor = bWasShowingSnapCursor;
	}

	return Super::Deactivate();
}

bool UPlaneHostedObjTool::BeginUse()
{
	if (GetCreateObjectMode() == EToolCreateObjectMode::SpanEdit && LastValidTargetID != MOD_ID_NONE)
	{
		// TODO: Check if legal before adding member
		// If click on a face that's already selected, remove it from member list
		// else add to member list
		if (PreviewSpanGraphMemberIDs.Remove(LastValidTargetID) == 0)
		{
			PreviewSpanGraphMemberIDs.AddUnique(LastValidTargetID);
		}
		return false;
	}

	// TODO: require assemblies for stairs, once they can be crafted
	if (!Controller || (!AssemblyGUID.IsValid() && (ObjectType != EObjectType::OTStaircase)))
	{
		return false;
	}

	if (LastValidTargetID != MOD_ID_NONE)
	{
		// Skip FMetaPlaneTool::BeginUse because we don't want to create any line segments in plane-application / "paint bucket" mode
		if (!UEditModelToolBase::BeginUse())
		{
			return false;
		}

		GameState->Document->ClearPreviewDeltas(GetWorld());
		TArray<FDeltaPtr> deltas;
		int32 newID = GameState->Document->GetNextAvailableID();
		if (GetObjectCreationDeltas({ LastValidTargetID }, newID, deltas, true) && 
			GameState->Document->ApplyDeltas(deltas, GetWorld()))
		{
			EndUse();
		}
	}
	else if (!bRequireHoverMetaPlane && GetCreateObjectMode() == EToolCreateObjectMode::Draw)
	{
		return Super::BeginUse();
	}

	return false;
}

bool UPlaneHostedObjTool::EnterNextStage()
{
	return Super::EnterNextStage();
}

bool UPlaneHostedObjTool::FrameUpdate()
{
	UEditModelToolBase::FrameUpdate();

	CurDeltas.Reset();
	TArray<FDeltaPtr> previewDeltas;
	int32 newID = GameState->Document->GetNextAvailableID();

	Controller->EMPlayerState->bShowSnappedCursor = GetCreateObjectMode() != EToolCreateObjectMode::Apply;
	if (GetCreateObjectMode() == EToolCreateObjectMode::Apply ||
		GetCreateObjectMode() == EToolCreateObjectMode::SpanEdit ||
		GetCreateObjectMode() == EToolCreateObjectMode::Add)
	{
		// Determine whether we can apply the plane hosted object to a plane targeted by the cursor
		const FSnappedCursor& cursor = Controller->EMPlayerState->SnappedCursor;
		LastValidTargetID = MOD_ID_NONE;
		const AModumateObjectInstance* hitMOI = nullptr;

		if ((cursor.SnapType == ESnapType::CT_FACESELECT) && cursor.Actor)
		{
			hitMOI = GameState->Document->ObjectFromActor(cursor.Actor);
			if (hitMOI && (hitMOI->GetObjectType() == ObjectType))
			{
				hitMOI = GameState->Document->GetObjectById(hitMOI->GetParentID());
			}

			if (ValidatePlaneTarget(hitMOI))
			{
				LastValidTargetID = hitMOI->ID;
			}
		}

		if (LastValidTargetID)
		{
			// Show affordance for current valid target on both Apply and SpanEdit mode
			DrawAffordanceForPlaneMOI(hitMOI, AffordanceLineInterval);
			// Preview apply delta only when there's a valid target ID
			if (GetCreateObjectMode() == EToolCreateObjectMode::Apply ||
				GetCreateObjectMode() == EToolCreateObjectMode::Add)
			{
				GetObjectCreationDeltas({ LastValidTargetID }, newID, previewDeltas, false);
			}
		}

		// Preview span edit on every flame
		if (GetCreateObjectMode() == EToolCreateObjectMode::SpanEdit)
		{
			int32 newSpanID;
			int32 newEdgeHostedObjID;
			FModumateObjectDeltaStatics::GetFaceSpanCreationDeltas(PreviewSpanGraphMemberIDs, newID, AssemblyGUID, NewMOIStateData, previewDeltas, newSpanID, newEdgeHostedObjID);
		}
		// Show preview affordance for all graph members in the new span
		for (auto curSpanID : PreviewSpanGraphMemberIDs)
		{
			auto curSpanObj = GameState->Document->GetObjectById(curSpanID);
			DrawAffordanceForPlaneMOI(curSpanObj, AffordanceLineInterval);
		}
	}
	// UpdatePreview() create MetaPlane for new span to be hosted on
	else if (GetCreateObjectMode() == EToolCreateObjectMode::Draw && UpdatePreview())
	{
		if (State == EState::NewPlanePending)
		{
			GetObjectCreationDeltas({ CurAddedFaceIDs }, newID, previewDeltas, false);
		}
	}
	CurDeltas.Append(previewDeltas);
	if (CurDeltas.Num() > 0)
	{
		GameState->Document->ApplyPreviewDeltas(CurDeltas, GetWorld());
	}

	return true;
}

bool UPlaneHostedObjTool::EndUse()
{
	return Super::EndUse();
}

bool UPlaneHostedObjTool::AbortUse()
{
	return Super::AbortUse();
}

bool UPlaneHostedObjTool::HandleInvert()
{
	if (!IsInUse())
	{
		return false;
	}

	bInverted = !bInverted;

	return true;
}

bool UPlaneHostedObjTool::HandleFlip(EAxis::Type FlipAxis)
{
	if (NewObjectIDs.Num() == 0)
	{
		return false;
	}

	// PlaneHosted tools create single new span, 
	// we handle flip by setting its child (PlaneHOstedObj) instance data
	AModumateObjectInstance* newMOI = GameState->Document->GetObjectById(NewObjectIDs[0]);
	AMOIMetaPlaneSpan* span = Cast<AMOIMetaPlaneSpan>(newMOI);
	if (span && span->GetChildIDs().Num() > 0)
	{
		newMOI = span->GetChildObjects()[0];
	}
	return newMOI && newMOI->GetFlippedState(FlipAxis, NewMOIStateData);
}

bool UPlaneHostedObjTool::HandleOffset(const FVector2D& ViewSpaceDirection)
{
	if (NewObjectIDs.Num() == 0)
	{
		return false;
	}

	FQuat cameraRotation = Controller->PlayerCameraManager->GetCameraRotation().Quaternion();
	FVector worldSpaceDirection =
		(ViewSpaceDirection.X * cameraRotation.GetRightVector()) +
		(ViewSpaceDirection.Y * cameraRotation.GetUpVector());
	
	// PlaneHosted tools create single new span, 
	// we handle offset by setting its child (PlaneHOstedObj) instance data
	AModumateObjectInstance* newMOI = GameState->Document->GetObjectById(NewObjectIDs[0]);
	AMOIMetaPlaneSpan* span = Cast<AMOIMetaPlaneSpan>(newMOI);
	if (span && span->GetChildIDs().Num() > 0)
	{
		newMOI = span->GetChildObjects()[0];
	}
	return newMOI && newMOI->GetOffsetState(worldSpaceDirection, NewMOIStateData);
}

void UPlaneHostedObjTool::CommitSpanEdit()
{
	GameState->Document->ClearPreviewDeltas(GetWorld());

	int32 newSpanID;
	int32 newPlaneHostedObjID;
	TArray<FDeltaPtr> deltas;
	int32 newID = GameState->Document->GetNextAvailableID();
	bool bSuccess = FModumateObjectDeltaStatics::GetFaceSpanCreationDeltas(PreviewSpanGraphMemberIDs, newID, AssemblyGUID, NewMOIStateData, deltas, newSpanID, newPlaneHostedObjID);
	if (bSuccess && GameState->Document->ApplyDeltas(deltas, GetWorld()))
	{
		NewObjectIDs = { newSpanID, newPlaneHostedObjID };
		ResetSpanIDs();
	}
	EndUse();
}

void UPlaneHostedObjTool::CancelSpanEdit()
{
	ResetSpanIDs();
}

void UPlaneHostedObjTool::OnAssemblyChanged()
{
	Super::OnAssemblyChanged();

	if (Active)
	{
		FrameUpdate();
	}
}

void UPlaneHostedObjTool::OnCreateObjectModeChanged()
{
	Super::OnCreateObjectModeChanged();
	ResetSpanIDs();
}

bool UPlaneHostedObjTool::GetObjectCreationDeltas(const TArray<int32>& InTargetFaceIDs, int32& NewID, TArray<FDeltaPtr>& OutDeltaPtrs, bool bSplitFaces)
{
	OutDeltaPtrs.Reset();
	NewObjectIDs.Reset();

	TArray<int32> targetFaceIDs;
	for (int32 targetFaceID : InTargetFaceIDs)
	{
		if (targetFaceID != MOD_ID_NONE)
		{
			targetFaceIDs.Add(targetFaceID);
		}
	}

	if (targetFaceIDs.Num() == 0)
	{
		return false;
	}

	// We need to keep track of next ID because Document->MakeMetaObject can make 1+ new meta objects
	NewID = GameState->Document->GetNextAvailableID();

	return FModumateObjectDeltaStatics::GetObjectCreationDeltasForFaceSpans(GameState->Document, GetCreateObjectMode(),
		targetFaceIDs, NewID, TargetSpanIndex, AssemblyGUID, NewMOIStateData, OutDeltaPtrs, NewObjectIDs);
}

bool UPlaneHostedObjTool::MakeObject(const FVector& Location)
{
	GameState->Document->BeginUndoRedoMacro();

	bool bSuccess = Super::MakeObject(Location);

	if (bSuccess && (CurAddedFaceIDs.Num() > 0))
	{
		int32 newID = GameState->Document->GetNextAvailableID();
		TArray<FDeltaPtr> deltas;
		GetObjectCreationDeltas({ CurAddedFaceIDs }, newID, deltas, false);
		bSuccess = deltas.Num() > 0 && GameState->Document->ApplyDeltas(deltas, GetWorld());
	}

	GameState->Document->EndUndoRedoMacro();

	return bSuccess;
}

bool UPlaneHostedObjTool::ValidatePlaneTarget(const AModumateObjectInstance *PlaneTarget)
{
	return (PlaneTarget != nullptr) && (PlaneTarget->GetObjectType() == EObjectType::OTMetaPlane)
		&& IsObjectInActiveGroup(PlaneTarget);
}

bool UPlaneHostedObjTool::IsTargetFacingDown()
{
	if (LastValidTargetID != MOD_ID_NONE)
	{
		AModumateObjectInstance *parentMOI = GameState->Document->GetObjectById(LastValidTargetID);
		return (parentMOI && (parentMOI->GetNormal().Z < 0.0f));
	}
	else if (bPendingPlaneValid && PendingPlaneGeom.IsNormalized())
	{
		return (PendingPlaneGeom.Z < 0.0f);
	}

	return false;
}

float UPlaneHostedObjTool::GetDefaultJustificationValue()
{
	if (GameState == nullptr)
	{
		return 0.5f;
	}

	// For walls, use the default justification for vertically-oriented objects
	if (ObjectType == EObjectType::OTWallSegment)
	{
		return GameState->Document->GetDefaultJustificationZ();
	}

	// Otherwise, start with the default justification for horizontally-oriented objects
	float defaultJustificationXY = GameState->Document->GetDefaultJustificationXY();

	// Now flip the justification if the target plane is not facing downwards
	if (!IsTargetFacingDown())
	{
		defaultJustificationXY = (1.0f - defaultJustificationXY);
	}

	return defaultJustificationXY;
}

bool UPlaneHostedObjTool::GetAppliedInversionValue()
{
	// When applying walls to meta planes, keep the original inversion value
	if (ObjectType == EObjectType::OTWallSegment)
	{
		return bInverted;
	}

	// Otherwise, use the same criteria as justification values, where if the target is not facing down,
	// flip the inversion value so that it matches the intent for object types that lie flat by default.
	bool bInversionValue = bInverted;

	if (!IsTargetFacingDown())
	{
		bInversionValue = !bInversionValue;
	}

	return bInversionValue;
}


void UPlaneHostedObjTool::ResetSpanIDs()
{
	PreviewSpanGraphMemberIDs.Empty();
}

void UPlaneHostedObjTool::DrawAffordanceForPlaneMOI(const AModumateObjectInstance* PlaneMOI, float Interval)
{
	int32 numCorners = PlaneMOI->GetNumCorners();
	for (int32 curCornerIdx = 0; curCornerIdx < numCorners; ++curCornerIdx)
	{
		int32 nextCornerIdx = (curCornerIdx + 1) % numCorners;

		FVector curCornerPos = PlaneMOI->GetCorner(curCornerIdx);
		FVector nextCornerPos = PlaneMOI->GetCorner(nextCornerIdx);

		Controller->EMPlayerState->AffordanceLines.Add(FAffordanceLine(
			curCornerPos, nextCornerPos, AffordanceLineColor, AffordanceLineInterval, AffordanceLineThickness, 1)
		);
	}
}

UWallTool::UWallTool()
	: Super()
{
	ObjectType = EObjectType::OTWallSegment;
}

UFloorTool::UFloorTool()
	: Super()
{
	ObjectType = EObjectType::OTFloorSegment;
}

URoofFaceTool::URoofFaceTool()
	: Super()
{
	ObjectType = EObjectType::OTRoofFace;
}

UCeilingTool::UCeilingTool()
	: Super()
{
	ObjectType = EObjectType::OTCeiling;
}

UCountertopTool::UCountertopTool()
	: Super()
{
	ObjectType = EObjectType::OTCountertop;
}

UPanelTool::UPanelTool()
	: Super()
{
	ObjectType = EObjectType::OTSystemPanel;
}
