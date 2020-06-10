// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Common/EditModelAdjustmentHandleBase.h"

#include "UnrealClasses/AdjustmentHandleActor_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "ToolsAndAdjustments/Interface/EditModelToolInterface.h"
#include "DocumentManagement/ModumateCommands.h"

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

	void FEditModelAdjustmentHandleBase::Tick(float DeltaTime)
	{
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
}
