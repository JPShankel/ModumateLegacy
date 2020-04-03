#include "EditModelMoveTool.h"
#include "EditModelPlayerController_CPP.h"
#include "EditModelPlayerState_CPP.h"
#include "EditModelGameState_CPP.h"
#include "ModumateCommands.h"
#include "EditModelGameMode_CPP.h"
#include "LineActor3D_CPP.h"

namespace Modumate
{
	FMoveObjectTool::FMoveObjectTool(AEditModelPlayerController_CPP *pc) :
		FEditModelToolBase(pc)
		, FSelectedObjectToolMixin(pc)
	{}

	FMoveObjectTool::~FMoveObjectTool() {}

	bool FMoveObjectTool::Activate()
	{
		FEditModelToolBase::Activate();
		Controller->EMPlayerState->SnappedCursor.MouseMode = EMouseMode::Location;
		return true;
	}

	bool FMoveObjectTool::Deactivate()
	{
		if (IsInUse())
		{
			AbortUse();
		}
		Controller->EMPlayerState->SnappedCursor.MouseMode = EMouseMode::Location;
		Controller->EMPlayerState->SnappedCursor.WantsVerticalAffordanceSnap = false;
		return FEditModelToolBase::Deactivate();
	}

	bool FMoveObjectTool::BeginUse()
	{
		FEditModelToolBase::BeginUse();
		Controller->EMPlayerState->SnappedCursor.WantsVerticalAffordanceSnap = true;

		AcquireSelectedObjects();

		if (Controller->EMPlayerState->SnappedCursor.Visible)
		{
			AnchorPoint = Controller->EMPlayerState->SnappedCursor.WorldPosition;
			Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(AnchorPoint, Controller->EMPlayerState->SnappedCursor.HitNormal, Controller->EMPlayerState->SnappedCursor.HitTangent);
			PendingMoveLine = Controller->GetWorld()->SpawnActor<ALineActor3D_CPP>(AEditModelGameMode_CPP::LineClass);
			return true;
		}
		return false;
	}

	bool FMoveObjectTool::FrameUpdate()
	{
		FEditModelToolBase::FrameUpdate();
		if (IsInUse() && Controller->EMPlayerState->SnappedCursor.Visible)
		{
			const FVector &hitLoc = Controller->EMPlayerState->SnappedCursor.WorldPosition;

			if (PendingMoveLine.IsValid())
			{
				PendingMoveLine->Point1 = AnchorPoint;
				PendingMoveLine->Point2 = hitLoc;
				PendingMoveLine->Color = FColor::Black;
				PendingMoveLine->Thickness = 3.0f;
				PendingMoveLine->AntiAliasing = false;
				PendingMoveLine->ScreenSize = false;
			}
			else
			{
				return false;
			}

			switch (Controller->EMPlayerState->SnappedCursor.SnapType)
			{
				case ESnapType::CT_WORLDSNAPX:
				case ESnapType::CT_WORLDSNAPY:
				case ESnapType::CT_WORLDSNAPZ:
				case ESnapType::CT_WORLDSNAPXY:
				case ESnapType::CT_USERPOINTSNAP:
				case ESnapType::CT_CUSTOMSNAPX:
				case ESnapType::CT_CUSTOMSNAPY:
				case ESnapType::CT_CUSTOMSNAPZ:
					PendingMoveLine->SetActorHiddenInGame(true);
					break;
				default:
					PendingMoveLine->SetActorHiddenInGame(false);
					break;
			};

			Controller->UpdateDimensionString(hitLoc, AnchorPoint, Controller->EMPlayerState->SnappedCursor.AffordanceFrame.Normal);

			for (auto &kvp : OriginalObjectData)
			{
				kvp.Key->SetFromDataRecordAndDisplacement(kvp.Value, hitLoc - AnchorPoint);
			}
		}
		return true;
	}

	bool FMoveObjectTool::HandleInputNumber(double n)
	{
		if (Controller->EMPlayerState->SnappedCursor.Visible)
		{
			FVector direction = (Controller->EMPlayerState->SnappedCursor.WorldPosition - AnchorPoint).GetSafeNormal();
			FVector oldPos = Controller->EMPlayerState->SnappedCursor.WorldPosition;
			Controller->EMPlayerState->SnappedCursor.WorldPosition = AnchorPoint + direction * n;
			EndUse();
			Controller->EMPlayerState->SnappedCursor.WorldPosition = oldPos;
		}
		return true;
	}

	bool FMoveObjectTool::EndUse()
	{
		Controller->EMPlayerState->SnappedCursor.WantsVerticalAffordanceSnap = false;
		Controller->EMPlayerState->SnappedCursor.ClearAffordanceFrame();
		Controller->EMPlayerState->SnappedCursor.MouseMode = EMouseMode::Location;

		if (Controller->EMPlayerState->SnappedCursor.Visible)
		{
			RestoreSelectedObjects();
			Controller->ClearArraybleCommand();

			TArray<int32> ids;
			ids.Reset(OriginalObjectData.Num());
			for (auto &kvp : OriginalObjectData)
			{
				ids.Add(kvp.Key->ID);
			}
			ReleaseSelectedObjects();

			FVector hitLoc = Controller->EMPlayerState->SnappedCursor.WorldPosition;
			Controller->ModumateCommand(
				FModumateCommand(Commands::kMoveObjects)
				.Param(Parameters::kDelta, hitLoc - AnchorPoint)
				.Param(Parameters::kObjectIDs, ids));
		}

		if (PendingMoveLine.IsValid())
		{
			PendingMoveLine->Destroy();
		}
		ReleaseSelectedObjects();

		return 	FEditModelToolBase::EndUse();
	}

	bool FMoveObjectTool::AbortUse()
	{
		RestoreSelectedObjects();
		ReleaseSelectedObjects();

		Controller->EMPlayerState->SnappedCursor.ClearAffordanceFrame();

		if (PendingMoveLine.IsValid())
		{
			PendingMoveLine->Destroy();
		}

		return FEditModelToolBase::AbortUse();
	}

	bool FMoveObjectTool::HandleControlKey(bool pressed)
	{
		return true;
	}

	IModumateEditorTool *MakeMoveObjectTool(AEditModelPlayerController_CPP *controller)
	{
		return new FMoveObjectTool(controller);
	}
}
