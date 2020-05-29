// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Tools/EditModelCountertopTool.h"

#include "UnrealClasses/LineActor.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "DocumentManagement/ModumateCommands.h"

using namespace Modumate;

UCountertopTool::UCountertopTool(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, State(Neutral)
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
		if ((State == NewSegmentPending) && PendingSegment.IsValid())
		{
			FVector dir = (PendingSegment->Point2 - PendingSegment->Point1).GetSafeNormal() * n;
			HandleClick(PendingSegment->Point1 + dir);
		}
		return true;
	case 1:
		// Project segment to degree defined by user input
		float currentSegmentLength = FMath::Max((PendingSegment->Point1 - PendingSegment->Point2).Size(), 100.f);

		FVector startPos = PendingSegment->Point1;
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

	PendingSegment = Controller->GetWorld()->SpawnActor<ALineActor>();
	PendingSegment->Point1 = hitLoc;
	PendingSegment->Point2 = hitLoc;
	PendingSegment->Color = FColor::Green;
	PendingSegment->Thickness = 2;

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
	AEditModelGameState_CPP *gameState = Controller->GetWorld()->GetGameState<AEditModelGameState_CPP>();
	Modumate::FModumateDocument &doc = gameState->Document;
	FMOIStateData state;
	state.StateType = EMOIDeltaType::Create;
	state.ControlPoints = { PendingSegment->Point1, p };
	state.ParentID = Controller->EMPlayerState->GetViewGroupObjectID();
	state.ObjectType = EObjectType::OTLineSegment;
	state.ObjectID = doc.GetNextAvailableID();

	TSharedPtr<FMOIDelta> delta = MakeShareable(new FMOIDelta({ state }));
	Controller->ModumateCommand(delta->AsCommand());

	if (Controller->TryMakePrismFromSegments(EObjectType::OTCountertop, Assembly.Key, Inverted))
	{
		EndUse();
	}
	else
	{
		AnchorPointDegree = PendingSegment->Point1;
		Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(p, FVector::UpVector, (PendingSegment->Point1 - PendingSegment->Point2).GetSafeNormal());
		PendingSegment->Point1 = p;
		PendingSegment->Point2 = p;
	}
	SegmentsConformInvert();
}

bool UCountertopTool::FrameUpdate()
{
	Super::FrameUpdate();

	if (!Controller->EMPlayerState->SnappedCursor.Visible)
	{
		return false;
	}

	FVector hitLoc = Controller->EMPlayerState->SnappedCursor.WorldPosition;

	if (State == NewSegmentPending && PendingSegment.IsValid())
	{
		FAffordanceLine affordance;
		if (Controller->EMPlayerState->SnappedCursor.TryMakeAffordanceLineFromCursorToSketchPlane(affordance, hitLoc))
		{
			Controller->EMPlayerState->AffordanceLines.Add(affordance);
		}

		PendingSegment->Point2 = hitLoc;
		// TODO: non-horizontal sketch
		Controller->UpdateDimensionString(PendingSegment->Point1, PendingSegment->Point2, FVector::UpVector);
	}
	if (PendingSegment.Get())
	{
		if ((PendingSegment->Point1 - PendingSegment->Point2).Size() > 25.f) // Add degree string when segment is greater than certain length
		{
			bool clockwise = FVector::CrossProduct(AnchorPointDegree - PendingSegment->Point1, PendingSegment->Point2 - PendingSegment->Point1).Z < 0.0f;
			UModumateFunctionLibrary::AddNewDegreeString(
				Controller,
				PendingSegment->Point1, //degree location
				AnchorPointDegree, // start
				PendingSegment->Point2, // end
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
	if (PendingSegment.IsValid())
	{
		PendingSegment->Destroy();
		PendingSegment.Reset();
	}

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
	SegmentsConformInvert();
	return true;
}

void UCountertopTool::SegmentsConformInvert()
{
	// TODO: UpdateVerticalPlane was taken out of line actor, countertops won't work until this is reimplemented

	AEditModelGameState_CPP *gameState = Controller->GetWorld()->GetGameState<AEditModelGameState_CPP>();
	Modumate::FModumateDocument *doc = &gameState->Document;
	TArray<FModumateObjectInstance*> segmentMoi = doc->GetObjectsOfType(EObjectType::OTLineSegment);
	for (auto curSegment : segmentMoi)
	{
		ALineActor* lineSegment = Cast<ALineActor>(curSegment->GetActor());
	}
}

