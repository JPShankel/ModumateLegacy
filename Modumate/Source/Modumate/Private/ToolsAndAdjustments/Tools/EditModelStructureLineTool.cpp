// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Tools/EditModelStructureLineTool.h"

#include "DocumentManagement/ModumateCommands.h"
#include "DocumentManagement/ModumateDocument.h"
#include "Graph/Graph3D.h"
#include "ModumateCore/ModumateStairStatics.h"
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

using namespace Modumate;

UStructureLineTool::UStructureLineTool(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
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

	Super::BeginUse();

	switch (CreateObjectMode)
	{
	case EToolCreateObjectMode::Draw:
	{
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
	{
		if (LastValidTargetID != MOD_ID_NONE)
		{
			MakeStructureLine(LastValidTargetID);
		}

		EndUse();
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

	switch (CreateObjectMode)
	{
	case EToolCreateObjectMode::Draw:
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
			if (GameState->Document->StartPreviewing() && GetObjectCreationDeltas({}, CurDeltas, false))
			{
				GameState->Document->ApplyPreviewDeltas(CurDeltas, GetWorld());
			}
		}
	}
	break;
	case EToolCreateObjectMode::Apply:
	{
		int32 newTargetID = MOD_ID_NONE;
		const AModumateObjectInstance *hitMOI = nullptr;
		LineStartPos = LineEndPos = FVector::ZeroVector;

		if (cursor.Actor)
		{
			hitMOI = GameState->Document->ObjectFromActor(cursor.Actor);

			if (hitMOI && (hitMOI->GetObjectType() == UModumateTypeStatics::ObjectTypeFromToolMode(GetToolMode()) ))
			{
				hitMOI = hitMOI->GetParentObject();
			}
		}

		if ((hitMOI != nullptr) && (hitMOI->GetObjectType() == EObjectType::OTMetaEdge))
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

			// Preview the deltas to create a structureline
			if (GameState->Document->StartPreviewing() && GetObjectCreationDeltas({ LastValidTargetID }, CurDeltas, false))
			{
				GameState->Document->ApplyPreviewDeltas(CurDeltas, GetWorld());
			}
		}

		// Don't show the snap cursor if we're targeting an edge or existing structure line.
		Controller->EMPlayerState->bShowSnappedCursor = (LastValidTargetID == MOD_ID_NONE);
	}
	break;
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

	AModumateObjectInstance* newMOI = GameState->Document->GetObjectById(NewObjectIDs[0]);
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
	if (LastValidTargetID != NewTargetID)
	{
		int32 newTargetStructureLineID = MOD_ID_NONE;

		AModumateObjectInstance *targetObj = GameState->Document->GetObjectById(NewTargetID);
		if (targetObj && (targetObj->GetObjectType() == EObjectType::OTMetaEdge))
		{
			// Find a child structure line on the target (that this tool didn't just create)
			TArray<AModumateObjectInstance*> children = targetObj->GetChildObjects();
			for (AModumateObjectInstance *child : children)
			{
				if (child && (child->GetObjectType() == UModumateTypeStatics::ObjectTypeFromToolMode(GetToolMode()) ))
				{
					newTargetStructureLineID = child->ID;
					break;
				}
			}
		}

		// Hide it, so we can preview the new line in its place,
		// and unhide the previous target structure line (if there was one)
		if (LastTargetStructureLineID != newTargetStructureLineID)
		{
			SetStructureLineHidden(LastTargetStructureLineID, false);
			SetStructureLineHidden(newTargetStructureLineID, true);
			LastTargetStructureLineID = newTargetStructureLineID;
		}

		LastValidTargetID = NewTargetID;
	}
}

bool UStructureLineTool::SetStructureLineHidden(int32 StructureLineID, bool bHidden)
{
	if (GameState && (StructureLineID != MOD_ID_NONE))
	{
		AModumateObjectInstance *structureLineObj = GameState->Document->GetObjectById(StructureLineID);
		if (structureLineObj)
		{
			static const FName hideRequestTag(TEXT("StructureLineTool"));
			structureLineObj->RequestHidden(hideRequestTag, bHidden);
			return true;
		}
	}

	return false;
}

bool UStructureLineTool::MakeStructureLine(int32 TargetEdgeID)
{
	GameState->Document->ClearPreviewDeltas(GetWorld());

	TArray<FDeltaPtr> deltas;
	if (!GetObjectCreationDeltas({ TargetEdgeID }, deltas, true))
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

	FMOIStructureLineData newMOICustomData(FMOIStructureLineData::CurrentVersion);
	NewMOIStateData.CustomData.SaveStructData(newMOICustomData);
}

bool UStructureLineTool::GetObjectCreationDeltas(const TArray<int32>& InTargetEdgeIDs, TArray<FDeltaPtr>& OutDeltaPtrs, bool bSplitFaces)
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
		TArray<int32> addedVertexIDs, addedFaceIDs;
		if (!GameState->Document->MakeMetaObject(Controller->GetWorld(), points, {}, EObjectType::OTMetaEdge, Controller->EMPlayerState->GetViewGroupObjectID(),
			addedVertexIDs, targetEdgeIDs, addedFaceIDs, OutDeltaPtrs, bSplitFaces, true))
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

		EObjectType objectType = UModumateTypeStatics::ObjectTypeFromToolMode(GetToolMode());

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
			NewMOIStateData.ObjectType = UModumateTypeStatics::ObjectTypeFromToolMode(GetToolMode());
			NewMOIStateData.ParentID = targetEdgeID;
			NewMOIStateData.AssemblyGUID = AssemblyGUID;

			NewObjectIDs.Add(NewMOIStateData.ID);

			delta->AddCreateDestroyState(NewMOIStateData, EMOIDeltaType::Create);
		}
	}

	return true;
}
