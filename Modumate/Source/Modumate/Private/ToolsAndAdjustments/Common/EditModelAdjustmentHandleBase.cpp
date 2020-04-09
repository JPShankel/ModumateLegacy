// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "EditModelAdjustmentHandleBase.h"

#include "AdjustmentHandleActor_CPP.h"
#include "EditModelPlayerState_CPP.h"
#include "EditModelPlayerController_CPP.h"
#include "EditModelToolInterface.h"
#include "ModumateCommands.h"

namespace Modumate
{
	const FName FEditModelAdjustmentHandleBase::StateRequestTag(TEXT("AdjustmentHandleBase"));

	FEditModelAdjustmentHandleBase::FEditModelAdjustmentHandleBase(FModumateObjectInstance *moi) : MOI(moi)
	{
	}

	bool FEditModelAdjustmentHandleBase::OnBeginUse()
	{
		UWorld *world = Handle.IsValid() ? Handle->GetWorld() : nullptr;
		Controller = world ? world->GetFirstPlayerController<AEditModelPlayerController_CPP>() : nullptr;
		if (!Controller.IsValid())
		{
			return false;
		}

		OriginalMouseMode = Controller->EMPlayerState->SnappedCursor.MouseMode;
		Controller->EMPlayerState->SnappedCursor.MouseMode = EMouseMode::Location;

		if (MOI)
		{
			MOI->RequestCollisionDisabled(StateRequestTag, true);

			auto descendents = MOI->GetAllDescendents();
			for (auto *descendent : descendents)
			{
				descendent->RequestCollisionDisabled(StateRequestTag, true);
			}
		}

		bIsInUse = true;

		return true;
	}

	bool FEditModelAdjustmentHandleBase::OnUpdateUse()
	{
		return false;
	}

	bool FEditModelAdjustmentHandleBase::OnEndUse()
	{
		return OnEndOrAbort();
	}

	bool FEditModelAdjustmentHandleBase::OnAbortUse()
	{
		return OnEndOrAbort();
	}

	bool FEditModelAdjustmentHandleBase::OnEndOrAbort()
	{
		bool bSuccess = false;

		if (Controller.IsValid())
		{
			Controller->InteractionHandle = nullptr;
			Controller->EMPlayerState->SnappedCursor.MouseMode = OriginalMouseMode;
			bSuccess = true;
		}

		if (MOI)
		{
			MOI->RequestCollisionDisabled(StateRequestTag, false);

			auto descendents = MOI->GetAllDescendents();
			for (auto *descendent : descendents)
			{
				descendent->RequestCollisionDisabled(StateRequestTag, false);
			}
		}

		if (ensureAlways(bIsInUse))
		{
			bIsInUse = false;
		}

		return bSuccess;
	}

	void FEditModelAdjustmentHandleBase::UpdateTargetGeometry(bool bIncremental)
	{
		if (MOI)
		{
			MOI->MarkDirty(EObjectDirtyFlags::Structure);
		}
	}

	FAdjustLineSegmentHandle::FAdjustLineSegmentHandle(FModumateObjectInstance *moi, int cp) : FEditModelAdjustmentHandleBase(moi)
	{
		CP = cp;
		OriginalP = MOI->ControlPoints[CP];
	}

	bool FAdjustLineSegmentHandle::OnBeginUse()
	{
		if (!FEditModelAdjustmentHandleBase::OnBeginUse())
		{
			return false;
		}

		OriginalP = MOI->ControlPoints[CP];
		FVector lineDirection = (MOI->ControlPoints[1] - MOI->ControlPoints[0]).GetSafeNormal();
		Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(MOI->ControlPoints[(CP + 1) % 2], FVector::UpVector, lineDirection);

		MOI->ShowAdjustmentHandles(Controller.Get(), false);

		return true;
	}

	bool FAdjustLineSegmentHandle::OnEndUse()
	{
		if (!FEditModelAdjustmentHandleBase::OnEndUse())
		{
			return false;
		}

		Controller->EMPlayerState->SnappedCursor.ClearAffordanceFrame();
		MOI->ShowAdjustmentHandles(Controller.Get(), true);

		TArray<FVector> points = MOI->ControlPoints;
		MOI->ControlPoints[CP] = OriginalP;

		Controller->ModumateCommand(FModumateCommand(Commands::kBeginUndoRedoMacro));

		Controller->ModumateCommand(FModumateCommand(Commands::kSetLineSegmentPoints)
			.Param(Parameters::kObjectID, MOI->ID)
			.Param(Parameters::kPoint1, points[0])
			.Param(Parameters::kPoint2, points[1])
		);

		// TODO: allow this handle to be grabbed from the tool mode that created the line, rather than just from the Select tool
		EToolMode curToolMode = Controller->GetToolMode();
		EObjectType curObjectType = UModumateTypeStatics::ObjectTypeFromToolMode(curToolMode);
		Controller->TryMakePrismFromSegments(curObjectType, Controller->CurrentTool->GetAssembly().Key, MOI->ObjectInverted, MOI);

		Controller->ModumateCommand(FModumateCommand(Commands::kEndUndoRedoMacro));

		return true;
	}

	bool FAdjustLineSegmentHandle::OnUpdateUse()
	{
		FEditModelAdjustmentHandleBase::OnUpdateUse();

		if (Controller.IsValid() && Controller->EMPlayerState->SnappedCursor.Visible)
		{
			FVector hitLoc = Controller->EMPlayerState->SnappedCursor.WorldPosition;
			MOI->ControlPoints[CP] = Controller->EMPlayerState->SnappedCursor.WorldPosition;
			MOI->ControlPoints[CP].Z = OriginalP.Z;
			MOI->UpdateGeometry();

			Controller->UpdateDimensionString(MOI->ControlPoints[(CP + 1) % 2], MOI->ControlPoints[CP], FVector::UpVector);
			FAffordanceLine affordance;
			if (Controller->EMPlayerState->SnappedCursor.TryMakeAffordanceLineFromCursorToSketchPlane(affordance, hitLoc))
			{
				Controller->EMPlayerState->AffordanceLines.Add(affordance);
			}
		}

		return true;
	}

	bool FAdjustLineSegmentHandle::OnAbortUse()
	{
		if (!FEditModelAdjustmentHandleBase::OnAbortUse())
		{
			return false;
		}

		if (Controller.IsValid())
		{
			MOI->ShowAdjustmentHandles(Controller.Get(), true);
			MOI->ControlPoints[CP] = OriginalP;
			MOI->UpdateGeometry();
			Controller->EMPlayerState->SnappedCursor.ClearAffordanceFrame();
		}

		return true;
	}

	FVector FAdjustLineSegmentHandle::GetAttachmentPoint()
	{
		return MOI->ControlPoints[CP] + FVector(0, 0, 0.25f);
	}
}
