// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "EditModelCountertopTool.h"

#include "LineActor3D_CPP.h"
#include "ModumateFunctionLibrary.h"
#include "EditModelPlayerController_CPP.h"
#include "EditModelGameState_CPP.h"
#include "EditModelGameMode_CPP.h"
#include "ModumateCommands.h"
#include "EditModelPlayerState_CPP.h"

namespace Modumate
{

	FCountertopTool::FCountertopTool(AEditModelPlayerController_CPP *controller) :
		FEditModelToolBase(controller)
		, State(Neutral)
	{}

	FCountertopTool::~FCountertopTool() {}

	bool FCountertopTool::HandleInputNumber(double n)
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

	bool FCountertopTool::Activate()
	{
		Controller->DeselectAll();
		Controller->EMPlayerState->SetHoveredObject(nullptr);
		Controller->EMPlayerState->SnappedCursor.MouseMode = EMouseMode::Location;
		Inverted = false;
		return FEditModelToolBase::Activate();
	}

	bool FCountertopTool::Deactivate()
	{
		State = Neutral;
		return FEditModelToolBase::Deactivate();
	}

	bool FCountertopTool::BeginUse()
	{
		FEditModelToolBase::BeginUse();
		if (!Controller->EMPlayerState->SnappedCursor.Visible)
		{
			return false;
		}

		FVector hitLoc = Controller->EMPlayerState->SnappedCursor.WorldPosition;

		Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(hitLoc, FVector::UpVector);

		State = NewSegmentPending;

		PendingSegment = Controller->GetWorld()->SpawnActor<ALineActor3D_CPP>(AEditModelGameMode_CPP::LineClass);
		PendingSegment->Point1 = hitLoc;
		PendingSegment->Point2 = hitLoc;
		PendingSegment->Color = FColor::Green;
		PendingSegment->Thickness = 2;
		PendingSegment->Inverted = Inverted;

		AnchorPointDegree = hitLoc + FVector(0.f, -1.f, 0.f); // Make north as AnchorPointDegree at new segment
		return true;
	}

	bool FCountertopTool::EnterNextStage()
	{
		FEditModelToolBase::EnterNextStage();

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

	void FCountertopTool::HandleClick(const FVector &p)
	{
		AEditModelGameState_CPP *gameState = Controller->GetWorld()->GetGameState<AEditModelGameState_CPP>();
		Modumate::FModumateDocument &doc = gameState->Document;

		if (State == NewSegmentPending && PendingSegment.IsValid())
		{
			Controller->ModumateCommand(
				FModumateCommand(Commands::kMakeLineSegment)
				.Param(Parameters::kPoint1, PendingSegment->Point1)
				.Param(Parameters::kPoint2, p)
				.Param(Parameters::kParent, Controller->EMPlayerState->GetViewGroupObjectID()));

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
		}
		SegmentsConformInvert();
	}

	bool FCountertopTool::FrameUpdate()
	{
		FEditModelToolBase::FrameUpdate();

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
					Controller.Get(),
					PendingSegment->Point1, //degree location
					AnchorPointDegree, // start
					PendingSegment->Point2, // end
					clockwise,
					FName(TEXT("PlayerController")),
					FName(TEXT("Degree")),
					1,
					Controller.Get(),
					EDimStringStyle::DegreeString,
					EEnterableField::NonEditableText,
					EAutoEditableBox::UponUserInput,
					true);
			}
		}
		return true;
	}

	bool FCountertopTool::EndUse()
	{
		State = Neutral;
		if (PendingSegment.IsValid())
		{
			PendingSegment->Destroy();
			PendingSegment.Reset();
		}

		return FEditModelToolBase::EndUse();
	}

	bool FCountertopTool::AbortUse()
	{
		EndUse();
		return FEditModelToolBase::AbortUse();
	}


	bool FCountertopTool::HandleSpacebar()
	{
		if (!IsInUse())
		{
			return false;
		}
		Inverted = !Inverted;
		SegmentsConformInvert();
		return true;
	}

	void FCountertopTool::SegmentsConformInvert()
	{
		if (PendingSegment.IsValid())
		{
			PendingSegment->Inverted = Inverted;
			PendingSegment->UpdateVerticalPlaneFromToolModeShopItem();
		}
		AEditModelGameState_CPP *gameState = Controller->GetWorld()->GetGameState<AEditModelGameState_CPP>();
		Modumate::FModumateDocument *doc = &gameState->Document;
		TArray<FModumateObjectInstance*> segmentMoi = doc->GetObjectsOfType(EObjectType::OTLineSegment);
		for (auto curSegment : segmentMoi)
		{
			ALineActor3D_CPP* lineSegment = Cast<ALineActor3D_CPP>(curSegment->GetActor());
			lineSegment->Inverted = Inverted;
			lineSegment->UpdateVerticalPlaneFromToolModeShopItem();
		}
	}

	IModumateEditorTool *MakeCountertopTool(AEditModelPlayerController_CPP *controller)
	{
		return new FCountertopTool(controller);
	}
}
