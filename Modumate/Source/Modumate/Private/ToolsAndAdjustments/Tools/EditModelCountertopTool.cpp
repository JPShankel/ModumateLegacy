// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Tools/EditModelCountertopTool.h"

#include "DocumentManagement/ModumateCommands.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/LineActor.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "UI/DimensionManager.h"
#include "UI/PendingSegmentActor.h"

using namespace Modumate;

UCountertopTool::UCountertopTool(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, State(Neutral)
	, PendingSegmentID(MOD_ID_NONE)
{}

bool UCountertopTool::HandleInputNumber(double n)
{
	if (!Controller->EMPlayerState->SnappedCursor.Visible)
	{
		return false;
	}

	switch (Controller->EMPlayerState->CurrentDimensionStringGroupIndex)
	{
	case 0:
		if ((State == NewSegmentPending) && PendingSegmentID != MOD_ID_NONE)
		{
			auto pendingSegment = GameInstance->DimensionManager->GetDimensionActor(PendingSegmentID)->GetLineActor();
			FVector dir = (pendingSegment->Point2 - pendingSegment->Point1).GetSafeNormal() * n;
			HandleClick(pendingSegment->Point1 + dir);
		}
		return true;
	case 1:
		if (PendingSegmentID != MOD_ID_NONE)
		{
			auto pendingSegment = GameInstance->DimensionManager->GetDimensionActor(PendingSegmentID)->GetLineActor();
			// Project segment to degree defined by user input
			float currentSegmentLength = FMath::Max((pendingSegment->Point1 - pendingSegment->Point2).Size(), 100.f);

			FVector startPos = pendingSegment->Point1;
			FVector degreeDir = (AnchorPointDegree - startPos).GetSafeNormal();
			degreeDir = degreeDir.RotateAngleAxis(n + 180.f, FVector::UpVector);

			FVector projectedInputPoint = startPos + degreeDir * currentSegmentLength;

			FVector2D projectedCursorScreenPos;
			if (Controller->ProjectWorldLocationToScreen(projectedInputPoint, projectedCursorScreenPos))
			{
				Controller->SetMouseLocation(projectedCursorScreenPos.X, projectedCursorScreenPos.Y);
				Controller->ClearUserSnapPoints();

				Controller->EMPlayerState->SnappedCursor.WorldPosition = projectedInputPoint;
			}
		}
		return true;
	}
	return true;
}

bool UCountertopTool::Activate()
{
	Controller->DeselectAll();
	Controller->EMPlayerState->SetHoveredObject(nullptr);
	Controller->EMPlayerState->SnappedCursor.MouseMode = EMouseMode::Location;
	Inverted = false;
	return UEditModelToolBase::Activate();
}

bool UCountertopTool::Deactivate()
{
	State = Neutral;
	return UEditModelToolBase::Deactivate();
}

bool UCountertopTool::BeginUse()
{
	Super::BeginUse();
	if (!Controller->EMPlayerState->SnappedCursor.Visible)
	{
		return false;
	}

	FVector hitLoc = Controller->EMPlayerState->SnappedCursor.WorldPosition;

	Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(hitLoc, FVector::UpVector);

	State = NewSegmentPending;

	auto dimensionActor = GameInstance->DimensionManager->AddDimensionActor(APendingSegmentActor::StaticClass());
	PendingSegmentID = dimensionActor->ID;

	auto pendingSegment = dimensionActor->GetLineActor();
	pendingSegment->Point1 = hitLoc;
	pendingSegment->Point2 = hitLoc;
	pendingSegment->Color = FColor::Green;
	pendingSegment->Thickness = 2;

	AnchorPointDegree = hitLoc + FVector(0.f, -1.f, 0.f); // Make north as AnchorPointDegree at new segment
	return true;
}

bool UCountertopTool::EnterNextStage()
{
	Super::EnterNextStage();

	if (!Controller->EMPlayerState->SnappedCursor.Visible)
	{
		return false;
	}
	if (State == NewSegmentPending)
	{
		FVector hitLoc = Controller->EMPlayerState->SnappedCursor.SketchPlaneProject(Controller->EMPlayerState->SnappedCursor.WorldPosition);
		HandleClick(hitLoc);
		return true;
	}

	return false;
}

void UCountertopTool::HandleClick(const FVector &p)
{
	//if (Controller->TryMakePrismFromSegments(EObjectType::OTCountertop, Assembly.Key, Inverted))
	auto pendingSegment = GameInstance->DimensionManager->GetDimensionActor(PendingSegmentID)->GetLineActor();
	if (false)
	{
		EndUse();
	}
	else
	{
		AnchorPointDegree = pendingSegment->Point1;
		Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(p, FVector::UpVector, (pendingSegment->Point1 - pendingSegment->Point2).GetSafeNormal());
		pendingSegment->Point1 = p;
		pendingSegment->Point2 = p;
	}
}

bool UCountertopTool::FrameUpdate()
{
	Super::FrameUpdate();

	if (!Controller->EMPlayerState->SnappedCursor.Visible)
	{
		return false;
	}

	FVector hitLoc = Controller->EMPlayerState->SnappedCursor.WorldPosition;

	if (PendingSegmentID == MOD_ID_NONE)
	{
		return false;
	}

	auto pendingSegment = GameInstance->DimensionManager->GetDimensionActor(PendingSegmentID)->GetLineActor();

	if (State == NewSegmentPending && pendingSegment != nullptr)
	{
		FAffordanceLine affordance;
		if (Controller->EMPlayerState->SnappedCursor.TryMakeAffordanceLineFromCursorToSketchPlane(affordance, hitLoc))
		{
			Controller->EMPlayerState->AffordanceLines.Add(affordance);
		}

		pendingSegment->Point2 = hitLoc;
	}
	if (pendingSegment != nullptr)
	{
		if ((pendingSegment->Point1 - pendingSegment->Point2).Size() > 25.f) // Add degree string when segment is greater than certain length
		{
			bool clockwise = FVector::CrossProduct(AnchorPointDegree - pendingSegment->Point1, pendingSegment->Point2 - pendingSegment->Point1).Z < 0.0f;
			UModumateFunctionLibrary::AddNewDegreeString(
				Controller,
				pendingSegment->Point1, //degree location
				AnchorPointDegree, // start
				pendingSegment->Point2, // end
				clockwise,
				FName(TEXT("PlayerController")),
				FName(TEXT("Degree")),
				1,
				Controller,
				EDimStringStyle::DegreeString,
				EEnterableField::NonEditableText,
				EAutoEditableBox::UponUserInput,
				true);
		}
	}
	return true;
}

bool UCountertopTool::EndUse()
{
	State = Neutral;

	GameInstance->DimensionManager->ReleaseDimensionActor(PendingSegmentID);
	PendingSegmentID = MOD_ID_NONE;

	return UEditModelToolBase::EndUse();
}

bool UCountertopTool::AbortUse()
{
	EndUse();
	return UEditModelToolBase::AbortUse();
}


bool UCountertopTool::HandleInvert()
{
	if (!IsInUse())
	{
		return false;
	}
	Inverted = !Inverted;
	return true;
}
