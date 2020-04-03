// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "EditModelRailTool.h"
#include "LineActor3D_CPP.h"
#include "EditModelPlayerController_CPP.h"
#include "ModumateCommands.h"
#include "AdjustmentHandleActor_CPP.h"
#include "EditModelGameMode_CPP.h"
#include "ModumateObjectInstanceRails.h"

namespace Modumate
{
	FRailTool::FRailTool(AEditModelPlayerController_CPP *controller) :
		FEditModelToolBase(controller)
	{}


	FRailTool::~FRailTool()
	{}

	bool FRailTool::Activate()
	{
		Controller->DeselectAll();
		Controller->EMPlayerState->SetHoveredObject(nullptr);
		Controller->EMPlayerState->SnappedCursor.MouseMode = EMouseMode::Location;
		PendingSegment = nullptr;
		return FEditModelToolBase::Activate();
	}

	bool FRailTool::BeginUse()
	{
		FEditModelToolBase::BeginUse();

		if (!Controller->EMPlayerState->SnappedCursor.Visible)
		{
			return false;
		}

		FVector hitLoc = Controller->EMPlayerState->SnappedCursor.WorldPosition;

		Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(hitLoc, FVector::UpVector);

		PendingSegment = Controller->GetWorld()->SpawnActor<ALineActor3D_CPP>(AEditModelGameMode_CPP::LineClass);
		PendingSegment->Point1 = hitLoc;
		PendingSegment->Point2 = hitLoc;
		PendingSegment->Color = FColor::Green;
		PendingSegment->Thickness = 2;

		return true;
	}

	bool FRailTool::EnterNextStage()
	{
		FEditModelToolBase::EnterNextStage();

		if (!Controller->EMPlayerState->SnappedCursor.Visible)
		{
			return false;
		}
		if (PendingSegment != nullptr)
		{
			FVector hitLoc = Controller->EMPlayerState->SnappedCursor.SketchPlaneProject(Controller->EMPlayerState->SnappedCursor.WorldPosition);

			Controller->ModumateCommand(
				FModumateCommand(Modumate::Commands::kMakeLineSegment)
				.Param(Parameters::kPoint1, PendingSegment->Point1)
				.Param(Parameters::kPoint2, hitLoc)
				.Param(Parameters::kParent, Controller->EMPlayerState->GetViewGroupObjectID())
			);

			Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(hitLoc, (hitLoc - PendingSegment->Point1).GetSafeNormal());

			PendingSegment->Point1 = PendingSegment->Point2 = hitLoc;

			return true;
		}
		return false;
	}

	bool FRailTool::FrameUpdate()
	{
		if (!Controller->EMPlayerState->SnappedCursor.Visible)
		{
			return false;
		}

		FVector hitLoc = Controller->EMPlayerState->SnappedCursor.WorldPosition;

		if (PendingSegment != nullptr)
		{
			FAffordanceLine affordance;
			if (Controller->EMPlayerState->SnappedCursor.TryMakeAffordanceLineFromCursorToSketchPlane(affordance,hitLoc))
			{
				Controller->EMPlayerState->AffordanceLines.Add(affordance);
			}

			PendingSegment->Point2 = hitLoc;
			// TODO: non-horizontal dimensions
			Controller->UpdateDimensionString(PendingSegment->Point1, PendingSegment->Point2, FVector::UpVector);
		}

		return FEditModelToolBase::FrameUpdate();
	}

	bool FRailTool::EndUse()
	{
		if (PendingSegment != nullptr)
		{
			PendingSegment->Destroy();
			PendingSegment = nullptr;
		}
		Controller->EMPlayerState->SnappedCursor.ClearAffordanceFrame();
		Controller->MakeRailsFromSegments();
		return FEditModelToolBase::EndUse();
	}

	bool FRailTool::AbortUse()
	{
		if (PendingSegment != nullptr)
		{
			PendingSegment->Destroy();
			PendingSegment = nullptr;
		}
		Controller->EMPlayerState->SnappedCursor.ClearAffordanceFrame();
		return FEditModelToolBase::AbortUse();
	}

	bool FRailTool::HandleSpacebar()
	{
		if (!IsInUse())
		{
			return false;
		}
		EndUse();
		return true;
	}

	bool FRailTool::HandleControlKey(bool pressed)
	{
		return true;
	}

	bool FRailTool::HandleInputNumber(double n)
	{
		if (!Controller->EMPlayerState->SnappedCursor.Visible)
		{
			return false;
		}
		FVector dir = (PendingSegment->Point2 - PendingSegment->Point1).GetSafeNormal() * n;

		Controller->ModumateCommand(
			FModumateCommand(Modumate::Commands::kMakeLineSegment)
			.Param(Parameters::kPoint1, PendingSegment->Point1)
			.Param(Parameters::kPoint2, PendingSegment->Point1 + dir)
			.Param(Parameters::kParent, Controller->EMPlayerState->GetViewGroupObjectID())
		);

		Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(PendingSegment->Point1 + dir, dir.GetSafeNormal());

		PendingSegment->Point1 = PendingSegment->Point2 = PendingSegment->Point1 + dir;

		return true;
	}


	IModumateEditorTool *MakeRailTool(AEditModelPlayerController_CPP *controller)
	{
		return new FRailTool(controller);
	}
}