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
			MOI->BeginPreviewOperation();
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
		// TODO: subclass handles to support meta and non-meta objects separately
		TArray<FVector> metaControlPoints;

		switch (MOI->GetObjectType())
		{
		case EObjectType::OTMetaPlane:
			metaControlPoints = MOI->GetControlPoints();
			break;
		};

		if (OnEndOrAbort())
		{
			bool bCommandSuccess;

			// If we are a meta-object, use the kSetGeometry command which creates graph deltas
			if (metaControlPoints.Num() > 0)
			{
				bCommandSuccess = Controller->ModumateCommand(
					FModumateCommand(Modumate::Commands::kSetGeometry)
					.Param(Parameters::kObjectID, MOI->ID)
					.Param(Parameters::kControlPoints, metaControlPoints)
				).GetValue(Parameters::kSuccess);
			}
			else
			{
				TSharedPtr<FMOIDelta> delta = MakeShareable(new FMOIDelta(TArray<FModumateObjectInstance*>({ MOI })));
				bCommandSuccess = Controller->ModumateCommand(delta->AsCommand()).GetValue(Parameters::kSuccess);
			}

			if (!bCommandSuccess)
			{
				UpdateTargetGeometry();
			}

			ADynamicMeshActor* dynMeshActor = Cast<ADynamicMeshActor>(MOI->GetActor());
			if (dynMeshActor != nullptr)
			{
				dynMeshActor->AdjustHandleFloor.Empty();
			}

			Controller->EMPlayerState->SnappedCursor.ClearAffordanceFrame();

			if (!MOI->IsDestroyed())
			{
				MOI->ShowAdjustmentHandles(Controller.Get(), true);
			}

		}
		return true;
	}

	bool FEditModelAdjustmentHandleBase::OnAbortUse()
	{
		if (OnEndOrAbort())
		{
			// Tell mesh it is done editing at handle
			ADynamicMeshActor* dynMeshActor = Cast<ADynamicMeshActor>(MOI->GetActor());
			if (dynMeshActor != nullptr)
			{
				dynMeshActor->AdjustHandleFloor.Empty();
			}

			UpdateTargetGeometry();
			MOI->ShowAdjustmentHandles(Controller.Get(), true);

			Controller->EMPlayerState->SnappedCursor.ClearAffordanceFrame();
		}
		return true;
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
			MOI->EndPreviewOperation();
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
		OriginalP = MOI->GetControlPoint(CP);
	}

	bool FAdjustLineSegmentHandle::OnBeginUse()
	{
		if (!FEditModelAdjustmentHandleBase::OnBeginUse())
		{
			return false;
		}

		OriginalP = MOI->GetControlPoint(CP);
		FVector lineDirection = (MOI->GetControlPoint(1) - MOI->GetControlPoint(0)).GetSafeNormal();
		Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(MOI->GetControlPoint((CP + 1) % 2), FVector::UpVector, lineDirection);

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

		// TODO: allow this handle to be grabbed from the tool mode that created the line, rather than just from the Select tool
		EToolMode curToolMode = Controller->GetToolMode();
		EObjectType curObjectType = UModumateTypeStatics::ObjectTypeFromToolMode(curToolMode);
		Controller->TryMakePrismFromSegments(curObjectType, Controller->CurrentTool->GetAssembly().Key, MOI->GetObjectInverted(), MOI);

		return true;
	}

	bool FAdjustLineSegmentHandle::OnUpdateUse()
	{
		FEditModelAdjustmentHandleBase::OnUpdateUse();

		if (Controller.IsValid() && Controller->EMPlayerState->SnappedCursor.Visible)
		{
			FVector hitLoc = Controller->EMPlayerState->SnappedCursor.WorldPosition;
			FVector newCP = Controller->EMPlayerState->SnappedCursor.WorldPosition;
			newCP.Z = OriginalP.Z;
			MOI->SetControlPoint(CP, newCP);
			MOI->UpdateGeometry();

			Controller->UpdateDimensionString(MOI->GetControlPoint((CP + 1) % 2), MOI->GetControlPoint(CP), FVector::UpVector);
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
			MOI->SetControlPoint(CP,OriginalP);
			MOI->UpdateGeometry();
			Controller->EMPlayerState->SnappedCursor.ClearAffordanceFrame();
		}

		return true;
	}

	FVector FAdjustLineSegmentHandle::GetAttachmentPoint()
	{
		return MOI->GetControlPoint(CP) + FVector(0, 0, 0.25f);
	}
}
