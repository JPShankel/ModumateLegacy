// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Tools/EditModelCountertopTool.h"

#include "Database/ModumateObjectAssembly.h"
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
#include "UI/AngleDimensionActor.h"

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
			pendingSegment->Point2 = pendingSegment->Point1 + dir;
			return EnterNextStage();
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

	BasePoints.Push(hitLoc);

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
		if (MakeSegment(hitLoc))
		{
			FModumateDocument &doc = Controller->GetWorld()->GetGameState<AEditModelGameState_CPP>()->Document;

			FMOIStateData stateData;
			stateData.StateType = EMOIDeltaType::Create;
			stateData.ObjectType = EObjectType::OTCountertop;
			stateData.ObjectAssemblyKey = Assembly.Key;
			stateData.ControlPoints = BasePoints;
			stateData.ParentID = Controller->EMPlayerState->GetViewGroupObjectID();
			stateData.ObjectID = doc.GetNextAvailableID();
			stateData.bObjectInverted = Inverted;

			TSharedPtr<FMOIDelta> delta = MakeShareable(new FMOIDelta({ stateData }));
			Controller->ModumateCommand(delta->AsCommand());

			return false;
		}
		return true;
	}

	return false;
}

bool UCountertopTool::MakeSegment(const FVector &hitLoc)
{
	auto pendingSegment = GameInstance->DimensionManager->GetDimensionActor(PendingSegmentID)->GetLineActor();
	if (pendingSegment == nullptr)
	{
		return false;
	}

	auto baseSegment = GetWorld()->SpawnActor<ALineActor>();
	baseSegment->Point1 = pendingSegment->Point1;
	baseSegment->Point2 = pendingSegment->Point2;
	baseSegment->Color = pendingSegment->Color;
	baseSegment->Thickness = pendingSegment->Thickness;

	// update base stacks
	BaseSegs.Push(baseSegment);

	bool bClosedLoop = BaseSegs[0]->Point1.Equals(BaseSegs[BaseSegs.Num() - 1]->Point2);
	if (bClosedLoop && UModumateGeometryStatics::IsPolygonValid(BasePoints))
	{
		return true;
	}
	else
	{
		BasePoints.Push(baseSegment->Point2);
	}

	pendingSegment->Point1 = hitLoc;
	pendingSegment->Point2 = hitLoc;

	return false;
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

	for (auto &seg : BaseSegs)
	{
		seg->Destroy();
	}

	BaseSegs.Empty();
	BasePoints.Reset();

	return UEditModelToolBase::EndUse();
}

bool UCountertopTool::AbortUse()
{
	if (BaseSegs.Num() == 0)
	{
		return EndUse();
	}
	
	auto pendingSegment = GameInstance->DimensionManager->GetDimensionActor(PendingSegmentID)->GetLineActor();
	auto poppedSegment = BaseSegs.Pop();
	pendingSegment->Point1 = poppedSegment->Point1;
	pendingSegment->Point2 = poppedSegment->Point2;
	pendingSegment->Color = poppedSegment->Color;
	pendingSegment->Thickness = poppedSegment->Thickness;

	poppedSegment->Destroy();

	BasePoints.Pop();

	return true;
}

void UCountertopTool::GetSnappingPointsAndLines(TArray<FVector> &OutPoints, TArray<TPair<FVector, FVector>> &OutLines)
{
	OutPoints = BasePoints;

	for (int32 idx = 0; idx < BasePoints.Num() - 1; idx++)
	{
		OutLines.Add(TPair<FVector, FVector>(BasePoints[idx], BasePoints[idx + 1]));
	}
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
