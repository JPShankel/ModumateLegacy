// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "EditModelRoofTool.h"

#include "EditModelPlayerController_CPP.h"
#include "EditModelPlayerState_CPP.h"
#include "EditModelGameState_CPP.h"
#include "EditModelGameMode_CPP.h"
#include "LineActor3D_CPP.h"
#include "ModumateCommands.h"
#include "ModumateFunctionLibrary.h"
#include "ModumateNetworkView.h"
#include "ModumateObjectInstanceCabinets.h"

using namespace Modumate;

URoofTool::URoofTool(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, State(Neutral)
	, PendingSegment(nullptr)
	, LastPendingSegmentLoc(ForceInitToZero)
	, bLastPendingSegmentLocValid(false)
	, DefaultEdgeSlope(0.5f)
{
}

bool URoofTool::Activate()
{
	Super::Activate();
	Controller->DeselectAll();
	Controller->EMPlayerState->SnappedCursor.MouseMode = EMouseMode::Location;

	return true;
}

bool URoofTool::BeginUse()
{
	Super::BeginUse();

	if (!Controller->EMPlayerState->SnappedCursor.Visible)
	{
		return false;
	}

	FVector hitLoc = Controller->EMPlayerState->SnappedCursor.WorldPosition;

	Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(hitLoc, FVector::UpVector);

	State = NewSegmentPending;

	PendingSegment = Controller->GetWorld()->SpawnActor<ALineActor3D_CPP>(Controller->EMPlayerState->GetEditModelGameMode()->LineClass);
	PendingSegment->Point1 = hitLoc;
	PendingSegment->Point2 = hitLoc;
	PendingSegment->Color = FColor::Green;
	PendingSegment->Thickness = 2;

	LastPendingSegmentLoc = hitLoc;
	bLastPendingSegmentLocValid = true;

	return true;
}

bool URoofTool::FrameUpdate()
{
	Super::FrameUpdate();
	if (!Controller->EMPlayerState->SnappedCursor.Visible)
	{
		return false;
	}

	const FSnappedCursor& snappedCursor = Controller->EMPlayerState->SnappedCursor;
	FVector hitLoc = snappedCursor.WorldPosition;

	if (State == NewSegmentPending && PendingSegment != nullptr)
	{
		FAffordanceLine affordance;
		if (Controller->EMPlayerState->SnappedCursor.TryMakeAffordanceLineFromCursorToSketchPlane(affordance, hitLoc))
		{
			Controller->EMPlayerState->AffordanceLines.Add(affordance);
		}

		LastPendingSegmentLoc = hitLoc;
		bLastPendingSegmentLocValid = true;

		PendingSegment->Point2 = hitLoc;
		// TODO: non-horizontal dimensions
		Controller->UpdateDimensionString(PendingSegment->Point1, PendingSegment->Point2, FVector::UpVector);
	}

	return true;
}

bool URoofTool::HandleInputNumber(double n)
{
	if ((State == NewSegmentPending) && (PendingSegment != nullptr))
	{
		FVector direction = (PendingSegment->Point2 - PendingSegment->Point1).GetSafeNormal();
		FVector origin = PendingSegment->Point1 + direction * n;
		HandleClick(origin);
		Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(origin, FVector::UpVector, direction);
	}

	return true;
}

bool URoofTool::AbortUse()
{
	Super::AbortUse();

	if (PendingSegment != nullptr)
	{
		PendingSegment->Destroy();
		PendingSegment = nullptr;
	}

	bLastPendingSegmentLocValid = false;

	return true;
}

bool URoofTool::EndUse()
{
	Super::EndUse();

	if (PendingSegment != nullptr)
	{
		PendingSegment->Destroy();
		PendingSegment = nullptr;
	}

	bLastPendingSegmentLocValid = false;

	return true;
}

void URoofTool::HandleClick(const FVector &p)
{
	AEditModelGameState_CPP *gameState = Controller->GetWorld()->GetGameState<AEditModelGameState_CPP>();
	const Modumate::FModumateDocument &doc = gameState->Document;

	if ((State == NewSegmentPending) && PendingSegment.IsValid())
	{
		Controller->ModumateCommand(FModumateCommand(Commands::kBeginUndoRedoMacro));

		// Make the new line segment
		int32 newLineID = doc.GetNextAvailableID();

		FMOIStateData state;

		state.ControlPoints = { PendingSegment->Point1, p };
		state.ParentID = Controller->EMPlayerState->GetViewGroupObjectID();
		state.ObjectType = EObjectType::OTLineSegment;
		state.ObjectID = newLineID;

		FMOIDelta delta = FMOIDelta::MakeCreateObjectDelta(state);

		Controller->ModumateCommand(delta.AsCommand());

		const FModumateObjectInstance *newLineObj = doc.GetObjectById(newLineID);

		if (newLineObj)
		{
			TArray<const FModumateObjectInstance *> allSegments = doc.GetObjectsOfType(EObjectType::OTLineSegment);
			TArray<FVector> polyPoints;
			TArray<int32> connectedSegmentIDs;
			int32 newRoofID = 0;
			bool bStartedMacro = false;

			// Try to connect this line segment as a polygon with other line segments
			if (FModumateNetworkView::TryMakePolygon(allSegments, polyPoints, newLineObj, &connectedSegmentIDs))
			{
				int32 numPoints = polyPoints.Num();
				if (numPoints > 2)
				{
					bStartedMacro = true;

					TArray<float> edgeSlopes;
					TArray<bool> edgesHaveFaces;
					for (int32 i = 0; i < numPoints; ++i)
					{
						// TODO: allow editing the default edge slope, and modifying them afterwards with the tool.
						// In the meantime, for testing correctness, make random roof slopes.
						static bool bUseRandomSlope = false;
						static FVector2D randomSlopeRange(0.5f, 2.0f);
						float edgeSlope = bUseRandomSlope ? FMath::GetRangeValue(randomSlopeRange, FMath::FRand()) : DefaultEdgeSlope;
						edgeSlopes.Add(edgeSlope);
						edgesHaveFaces.Add(true);
					}

					// Try to make a roof out of the polygon of line segments
					newRoofID = Controller->ModumateCommand(
						FModumateCommand(Commands::kMakeRoof)
						.Param(Parameters::kControlPoints, polyPoints)
						.Param(Parameters::kAssembly, Assembly.Key)
						.Param(Parameters::kObjectIDs, connectedSegmentIDs)
						.Param(Parameters::kValues, edgeSlopes)
						.Param(Parameters::kEdgesHaveFaces, edgesHaveFaces)
						.Param(Parameters::kParent, Controller->EMPlayerState->GetViewGroupObjectID())
					).GetValue(Parameters::kObjectID);

					if (newRoofID != 0)
					{
						Controller->DeselectAll();
						EndUse();
					}
				}
			}
			// Otherwise, continue making another line segment that starts at the previous line's end
			else
			{
				Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(p, FVector::UpVector, (PendingSegment->Point1 - PendingSegment->Point2).GetSafeNormal());

				PendingSegment->Point1 = p;
				PendingSegment->Point2 = p;
			}
		}

		Controller->ModumateCommand(FModumateCommand(Commands::kEndUndoRedoMacro));
	}
}

bool URoofTool::EnterNextStage()
{
	Super::EnterNextStage();

	if (!Controller->EMPlayerState->SnappedCursor.Visible)
	{
		return false;
	}

	if ((State == NewSegmentPending) && PendingSegment.IsValid())
	{
		FVector hitLoc = Controller->EMPlayerState->SnappedCursor.SketchPlaneProject(Controller->EMPlayerState->SnappedCursor.WorldPosition);
		Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(hitLoc, (PendingSegment->Point1 - PendingSegment->Point2).GetSafeNormal());
		HandleClick(hitLoc);
		return true;
	}

	return false;
}

