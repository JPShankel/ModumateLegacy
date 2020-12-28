// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Tools/EditModelPlaneHostedObjTool.h"

#include "DocumentManagement/ModumateDocument.h"
#include "DocumentManagement/ModumateCommands.h"
#include "Objects/PlaneHostedObj.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/LineActor.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "UI/DimensionManager.h"
#include "UI/PendingSegmentActor.h"

using namespace Modumate;

UPlaneHostedObjTool::UPlaneHostedObjTool(const FObjectInitializer& ObjectInitializer)
	: UMetaPlaneTool(ObjectInitializer)
	, bInverted(false)
	, bRequireHoverMetaPlane(false)
	, ObjectType(EObjectType::OTNone)
	, LastValidTargetID(MOD_ID_NONE)
	, bWasShowingSnapCursor(true)
	, InstanceJustification(0.0f)
{
	InstanceJustification = GetDefaultJustificationValue();
}

bool UPlaneHostedObjTool::Activate()
{
	if (!Super::Activate())
	{
		return false;
	}

	FMOIPlaneHostedObjData newMOICustomData(FMOIPlaneHostedObjData::CurrentVersion);
	newMOICustomData.FlipSigns.Y = GetAppliedInversionValue() ? -1.0 : 1.0f;
	newMOICustomData.Justification = InstanceJustification;

	NewMOIStateData.ObjectType = ObjectType;
	NewMOIStateData.CustomData.SaveStructData(newMOICustomData);

	bWasShowingSnapCursor = Controller->EMPlayerState->bShowSnappedCursor;
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

		auto delta = GetObjectCreationDelta({ LastValidTargetID });
		if (delta.IsValid())
		{
			GameState->Document->ApplyDeltas({ delta }, GetWorld());
			EndUse();
		}
	}
	else if (!bRequireHoverMetaPlane)
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
	// Skip the direct parent UMetaPlaneTool::FrameUpdate, because we don't want to apply preview deltas yet
	if (!UEditModelToolBase::FrameUpdate())
	{
		return false;
	}

	CurDeltas.Reset();
	FDeltaPtr previewDelta;

	if (PendingSegmentID != MOD_ID_NONE)
	{
		if (!Super::UpdatePreview() || (CurAddedFaceIDs.Num() == 0))
		{
			return false;
		}

		previewDelta = GetObjectCreationDelta(CurAddedFaceIDs);
	}
	else
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
			int32 numCorners = hitMOI->GetNumCorners();
			for (int32 curCornerIdx = 0; curCornerIdx < numCorners; ++curCornerIdx)
			{
				int32 nextCornerIdx = (curCornerIdx + 1) % numCorners;

				FVector curCornerPos = hitMOI->GetCorner(curCornerIdx);
				FVector nextCornerPos = hitMOI->GetCorner(nextCornerIdx);

				Controller->EMPlayerState->AffordanceLines.Add(FAffordanceLine(
					curCornerPos, nextCornerPos, AffordanceLineColor, AffordanceLineInterval, AffordanceLineThickness, 1)
				);
			}

			previewDelta = GetObjectCreationDelta({ LastValidTargetID });
		}

		// Don't show the snap cursor if we're targeting a plane.
		Controller->EMPlayerState->bShowSnappedCursor = (LastValidTargetID == MOD_ID_NONE);
	}

	if (previewDelta.IsValid())
	{
		CurDeltas.Add(previewDelta);
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

	AModumateObjectInstance* newMOI = GameState->Document->GetObjectById(NewObjectIDs[0]);
	return newMOI && newMOI->GetFlippedState(FlipAxis, NewMOIStateData);
}

bool UPlaneHostedObjTool::HandleAdjustJustification(const FVector2D& ViewSpaceDirection)
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
	return newMOI && newMOI->GetJustifiedState(worldSpaceDirection, NewMOIStateData);
}

void UPlaneHostedObjTool::SetInstanceJustification(const float InJustification)
{
	InstanceJustification = InJustification;
}

float UPlaneHostedObjTool::GetInstanceJustification() const
{
	return InstanceJustification;
}

void UPlaneHostedObjTool::OnAssemblyChanged()
{
	Super::OnAssemblyChanged();

	if (Active)
	{
		FrameUpdate();
	}
}

FDeltaPtr UPlaneHostedObjTool::GetObjectCreationDelta(const TArray<int32>& TargetFaceIDs)
{
	NewObjectIDs.Reset();
	TSharedPtr<FMOIDelta> delta;
	int32 nextID = GameState->Document->GetNextAvailableID();

	for (int32 targetFaceID : TargetFaceIDs)
	{
		if (!delta.IsValid())
		{
			delta = MakeShared<FMOIDelta>();
		}

		bool bCreateNewObject = true;
		AModumateObjectInstance* parentMOI = GameState->Document->GetObjectById(targetFaceID);

		if (parentMOI && ensure(parentMOI->GetObjectType() == EObjectType::OTMetaPlane))
		{
			AModumateObjectInstance* existingLayeredObj = nullptr;
			for (auto child : parentMOI->GetChildObjects())
			{
				if ((child->GetLayeredInterface() != nullptr) && ensureAlways(existingLayeredObj == nullptr))
				{
					existingLayeredObj = child;

					if (child->GetObjectType() == ObjectType)
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
		}

		if (bCreateNewObject)
		{
			/*FMOIPlaneHostedObjData newMOICustomData(FMOIPlaneHostedObjData::CurrentVersion);
			newMOICustomData.FlipSigns.Y = GetAppliedInversionValue() ? -1.0 : 1.0f;
			newMOICustomData.Justification = InstanceJustification;*/

			NewMOIStateData.ID = nextID++;
			NewMOIStateData.ParentID = targetFaceID;
			NewMOIStateData.AssemblyGUID = AssemblyGUID;
			//NewMOIStateData.CustomData.SaveStructData(newMOICustomData);
			delta->AddCreateDestroyState(NewMOIStateData, EMOIDeltaType::Create);

			NewObjectIDs.Add(NewMOIStateData.ID);
		}
	}

	return delta;
}

bool UPlaneHostedObjTool::MakeObject(const FVector& Location)
{
	Controller->ModumateCommand(FModumateCommand(Modumate::Commands::kBeginUndoRedoMacro));

	bool bSuccess = Super::MakeObject(Location);

	if (bSuccess && (CurAddedFaceIDs.Num() > 0))
	{
		auto delta = GetObjectCreationDelta(CurAddedFaceIDs);
		if (delta.IsValid())
		{
			GameState->Document->ApplyDeltas({ delta }, GetWorld());
		}
	}

	Controller->ModumateCommand(FModumateCommand(Modumate::Commands::kEndUndoRedoMacro));

	return bSuccess;
}

bool UPlaneHostedObjTool::ValidatePlaneTarget(const AModumateObjectInstance *PlaneTarget)
{
	return (PlaneTarget != nullptr) && (PlaneTarget->GetObjectType() == EObjectType::OTMetaPlane);
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


UWallTool::UWallTool(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ObjectType = EObjectType::OTWallSegment;
	SetAxisConstraint(EAxisConstraint::AxisZ);
}

UFloorTool::UFloorTool(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ObjectType = EObjectType::OTFloorSegment;
	SetAxisConstraint(EAxisConstraint::AxesXY);
}

URoofFaceTool::URoofFaceTool(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ObjectType = EObjectType::OTRoofFace;
	SetAxisConstraint(EAxisConstraint::AxesXY);
}

UCeilingTool::UCeilingTool(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ObjectType = EObjectType::OTCeiling;
	SetAxisConstraint(EAxisConstraint::AxesXY);
}

UCountertopTool::UCountertopTool(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ObjectType = EObjectType::OTCountertop;
	SetAxisConstraint(EAxisConstraint::AxesXY);
}

UPanelTool::UPanelTool(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ObjectType = EObjectType::OTSystemPanel;
	SetAxisConstraint(EAxisConstraint::AxisZ);
}
