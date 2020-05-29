// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Tools/EditModelRailTool.h"
#include "UnrealClasses/LineActor.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "DocumentManagement/ModumateCommands.h"
#include "UnrealClasses/AdjustmentHandleActor_CPP.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "DocumentManagement/ModumateObjectInstanceRails.h"

using namespace Modumate;

URailTool::URailTool(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{}

bool URailTool::Activate()
{
	Controller->DeselectAll();
	Controller->EMPlayerState->SetHoveredObject(nullptr);
	Controller->EMPlayerState->SnappedCursor.MouseMode = EMouseMode::Location;
	PendingSegment = nullptr;
	return UEditModelToolBase::Activate();
}

bool URailTool::BeginUse()
{
	Super::BeginUse();

	if (!Controller->EMPlayerState->SnappedCursor.Visible)
	{
		return false;
	}

	FVector hitLoc = Controller->EMPlayerState->SnappedCursor.WorldPosition;

	Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(hitLoc, FVector::UpVector);

	PendingSegment = Controller->GetWorld()->SpawnActor<ALineActor>();
	PendingSegment->Point1 = hitLoc;
	PendingSegment->Point2 = hitLoc;
	PendingSegment->Color = FColor::Green;
	PendingSegment->Thickness = 2;

	return true;
}

bool URailTool::EnterNextStage()
{
	Super::EnterNextStage();

	if (!Controller->EMPlayerState->SnappedCursor.Visible)
	{
		return false;
	}
	if (PendingSegment != nullptr)
	{
		FVector hitLoc = Controller->EMPlayerState->SnappedCursor.SketchPlaneProject(Controller->EMPlayerState->SnappedCursor.WorldPosition);

		// TODO: refactor for MOI delta when rail tool is reactivated
#if 0
		Controller->ModumateCommand(
			FModumateCommand(Modumate::Commands::kMakeLineSegment)
			.Param(Parameters::kPoint1, PendingSegment->Point1)
			.Param(Parameters::kPoint2, hitLoc)
			.Param(Parameters::kParent, Controller->EMPlayerState->GetViewGroupObjectID())
		);
#endif

		Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(hitLoc, (hitLoc - PendingSegment->Point1).GetSafeNormal());

		PendingSegment->Point1 = PendingSegment->Point2 = hitLoc;

		return true;
	}
	return false;
}

bool URailTool::FrameUpdate()
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

	return UEditModelToolBase::FrameUpdate();
}

bool URailTool::EndUse()
{
	if (PendingSegment != nullptr)
	{
		PendingSegment->Destroy();
		PendingSegment = nullptr;
	}
	Controller->EMPlayerState->SnappedCursor.ClearAffordanceFrame();
	Controller->MakeRailsFromSegments();
	return UEditModelToolBase::EndUse();
}

bool URailTool::AbortUse()
{
	if (PendingSegment != nullptr)
	{
		PendingSegment->Destroy();
		PendingSegment = nullptr;
	}
	Controller->EMPlayerState->SnappedCursor.ClearAffordanceFrame();
	return UEditModelToolBase::AbortUse();
}

bool URailTool::HandleInvert()
{
	if (!IsInUse())
	{
		return false;
	}
	EndUse();
	return true;
}

bool URailTool::HandleControlKey(bool pressed)
{
	return true;
}

bool URailTool::HandleInputNumber(double n)
{
	if (!Controller->EMPlayerState->SnappedCursor.Visible)
	{
		return false;
	}
	FVector dir = (PendingSegment->Point2 - PendingSegment->Point1).GetSafeNormal() * n;

	// TODO: refactor for MOI delta when rail tool is reactivated
#if 0
	Controller->ModumateCommand(
		FModumateCommand(Modumate::Commands::kMakeLineSegment)
		.Param(Parameters::kPoint1, PendingSegment->Point1)
		.Param(Parameters::kPoint2, PendingSegment->Point1 + dir)
		.Param(Parameters::kParent, Controller->EMPlayerState->GetViewGroupObjectID())
	);
#endif

	Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(PendingSegment->Point1 + dir, dir.GetSafeNormal());

	PendingSegment->Point1 = PendingSegment->Point2 = PendingSegment->Point1 + dir;

	return true;
}


