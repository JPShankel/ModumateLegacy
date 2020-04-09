// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "EditModelMetaPlaneTool.h"

#include "DynamicMeshActor.h"
#include "LineActor3D_CPP.h"
#include "ModumateFunctionLibrary.h"
#include "EditModelPlayerController_CPP.h"
#include "EditModelGameState_CPP.h"
#include "EditModelGameMode_CPP.h"
#include "ModumateCommands.h"
#include "EditModelPlayerState_CPP.h"

namespace Modumate
{

	FMetaPlaneTool::FMetaPlaneTool(AEditModelPlayerController_CPP *InController, EAxisConstraint InAxisConstraint)
		: FEditModelToolBase(InController)
		, State(Neutral)
		, PendingSegment(nullptr)
		, PendingPlane(nullptr)
		, AnchorPointDegree(ForceInitToZero)
		, MinPlaneSize(1.0f)
		, PendingPlaneAlpha(0.4f)
	{
		UWorld *world = Controller.IsValid() ? Controller->GetWorld() : nullptr;
		if (ensureAlways(world))
		{
			GameMode = world->GetAuthGameMode<AEditModelGameMode_CPP>();
			GameState = world->GetGameState<AEditModelGameState_CPP>();
		}

		SetAxisConstraint(InAxisConstraint);
	}

	FMetaPlaneTool::~FMetaPlaneTool() {}

	bool FMetaPlaneTool::HandleInputNumber(double n)
	{
		if (!Controller->EMPlayerState->SnappedCursor.Visible)
		{
			return false;
		}
		NewObjIDs.Reset();

		switch (Controller->EMPlayerState->CurrentDimensionStringGroupIndex)
		{
		case 0:
			if ((State == NewSegmentPending) && PendingSegment.IsValid())
			{
				FVector dir = (PendingSegment->Point2 - PendingSegment->Point1).GetSafeNormal() * n;

				// TODO: until the UI flow can allow for separate X and Y distances, do not respond
				// to typing a number for XY planes
				if (AxisConstraint == EAxisConstraint::AxesXY)
				{
					return false;
				}

				return MakeObject(PendingSegment->Point1 + dir, NewObjIDs);
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

	bool FMetaPlaneTool::Activate()
	{
		Controller->DeselectAll();
		Controller->EMPlayerState->SetHoveredObject(nullptr);
		OriginalMouseMode = Controller->EMPlayerState->SnappedCursor.MouseMode;
		Controller->EMPlayerState->SnappedCursor.MouseMode = EMouseMode::Location;

		return FEditModelToolBase::Activate();
	}

	bool FMetaPlaneTool::Deactivate()
	{
		State = Neutral;
		Controller->EMPlayerState->SnappedCursor.MouseMode = OriginalMouseMode;
		Controller->EMPlayerState->SnappedCursor.WantsVerticalAffordanceSnap = false;
		return FEditModelToolBase::Deactivate();
	}

	bool FMetaPlaneTool::BeginUse()
	{
		FEditModelToolBase::BeginUse();
		if (!Controller->EMPlayerState->SnappedCursor.Visible)
		{
			return false;
		}

		const FVector &hitLoc = Controller->EMPlayerState->SnappedCursor.WorldPosition;
		PendingPlaneEdgeIDs.Reset();
		SketchPlanePoints.Reset();

		const FVector &tangent = Controller->EMPlayerState->SnappedCursor.HitTangent;
		if (AxisConstraint == EAxisConstraint::None)
		{
			Controller->EMPlayerState->SnappedCursor.WantsVerticalAffordanceSnap = true;

			const FVector &normal = Controller->EMPlayerState->SnappedCursor.HitNormal;
			if (!FMath::IsNearlyZero(normal.Size()))
			{
				Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(hitLoc, normal, tangent);
			}
			else
			{
				Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(hitLoc, FVector::UpVector, tangent);
			}
		}
		else
		{
			Controller->EMPlayerState->SnappedCursor.WantsVerticalAffordanceSnap = false;
			Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(hitLoc, FVector::UpVector, tangent);
		}

		State = NewSegmentPending;

		PendingSegment = Controller->GetWorld()->SpawnActor<ALineActor3D_CPP>(AEditModelGameMode_CPP::LineClass);
		PendingSegment->Point1 = hitLoc;
		PendingSegment->Point2 = hitLoc;
		PendingSegment->Color = FColor::White;
		PendingSegment->Thickness = 2;
		PendingSegment->Inverted = false;
		PendingSegment->bDrawVerticalPlane = false;

		AnchorPointDegree = hitLoc + FVector(0.f, -1.f, 0.f); // Make north as AnchorPointDegree at new segment

		if (GameMode.IsValid())
		{
			PendingPlane = Controller->GetWorld()->SpawnActor<ADynamicMeshActor>(GameMode->DynamicMeshActorClass.Get());
			PendingPlane->SetActorHiddenInGame(true);
			PendingPlaneMaterial.EngineMaterial = GameMode->MetaPlaneMaterial;
		}

		PendingPlanePoints.Reset();

		return true;
	}

	bool FMetaPlaneTool::EnterNextStage()
	{
		FEditModelToolBase::EnterNextStage();

		if (!Controller->EMPlayerState->SnappedCursor.Visible)
		{
			return false;
		}
		if (State == NewSegmentPending)
		{
			return MakeObject(Controller->EMPlayerState->SnappedCursor.WorldPosition, NewObjIDs);
		}

		return false;
	}

	bool FMetaPlaneTool::MakeObject(const FVector &Location, TArray<int32> &OutNewObjIDs)
	{
		Modumate::FModumateDocument &doc = GameState->Document;

		int32 newObjectID = MOD_ID_NONE;
		FVector constrainedStartPoint = PendingSegment->Point1;
		FVector constrainedEndPoint = Location;
		ConstrainHitPoint(constrainedStartPoint);
		ConstrainHitPoint(constrainedEndPoint);
		NewObjIDs.Reset();

		if (State == NewSegmentPending && PendingSegment.IsValid() &&
			(FVector::Dist(constrainedStartPoint, constrainedEndPoint) >= MinPlaneSize))
		{
			switch (AxisConstraint)
			{
			case EAxisConstraint::None:
			{
				Controller->ModumateCommand(FModumateCommand(Commands::kBeginUndoRedoMacro));

				auto commandResult = Controller->ModumateCommand(
					FModumateCommand(Commands::kMakeMetaVertex)
					.Param(Parameters::kLocation, constrainedStartPoint)
					.Param(Parameters::kParent, Controller->EMPlayerState->GetViewGroupObjectID()));
				TArray<int32> objIDs = commandResult.GetValue(Parameters::kObjectIDs);
				int32 startVertexID = objIDs[0];

				commandResult = Controller->ModumateCommand(
					FModumateCommand(Commands::kMakeMetaVertex)
					.Param(Parameters::kLocation, constrainedEndPoint)
					.Param(Parameters::kParent, Controller->EMPlayerState->GetViewGroupObjectID()));
				objIDs = commandResult.GetValue(Parameters::kObjectIDs);
				int32 endVertexID = objIDs[0];

				if ((endVertexID != MOD_ID_NONE) && (startVertexID != MOD_ID_NONE))
				{
					TArray<int32> edgeVertexIDs;
					edgeVertexIDs.Add(startVertexID);
					edgeVertexIDs.Add(endVertexID);

					// TODO: there will be situations where we neither create nor find an edge, and that needs to be handled here
					// potentially the MakeObject functions should return true if they find an object as opposed to make an object
					TArray<int32> outEdgeIDs;
					outEdgeIDs = Controller->ModumateCommand(
						FModumateCommand(Commands::kMakeMetaEdge)
						.Param(Parameters::kObjectIDs, edgeVertexIDs)
						.Param(Parameters::kParent, Controller->EMPlayerState->GetViewGroupObjectID())
					).GetValue(Parameters::kObjectIDs);

					for (auto id : outEdgeIDs)
					{
						PendingPlaneEdgeIDs.Add(id);
					}
					int32 edgeID = MOD_ID_NONE;
					if (outEdgeIDs.Num() > 0)
					{
						edgeID = outEdgeIDs[0];
					}
				}

				// The segment-based plane updates the sketch plane on the first three clicks
				FVector segmentDirection = FVector(constrainedEndPoint - constrainedStartPoint).GetSafeNormal();
				FVector currentSketchPlaneNormal(Controller->EMPlayerState->SnappedCursor.AffordanceFrame.Normal);

				FVector tangentDir = !FVector::Parallel(currentSketchPlaneNormal, segmentDirection) ? segmentDirection : Controller->EMPlayerState->SnappedCursor.AffordanceFrame.Tangent;

				if (SketchPlanePoints.Num() == 0)
				{
					// If the two points are close together, ignore them and wait for the next segment
					if (!FMath::IsNearlyZero(segmentDirection.Size()))
					{
						// Otherwise, this is our first valid line segment, so adjust sketch plane normal to be perpendicular to this
						SketchPlanePoints.Add(constrainedStartPoint);
						SketchPlanePoints.Add(constrainedEndPoint);

						// If we've selected a point along the sketch plane's Z, leave the sketch plane intact
						// Note: it is all right to add the sketch plane points above as a vertical line is a legitimate basis for a new sketch plane
						// which will be established with the next click, meantime a vertical click should just leave the original sketch plane in place
						if (!FVector::Orthogonal(currentSketchPlaneNormal, tangentDir))
						{
							FVector transverse = FVector::CrossProduct(currentSketchPlaneNormal, segmentDirection);
							Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(constrainedEndPoint, FVector::CrossProduct(transverse, tangentDir).GetSafeNormal(), tangentDir);
						}
						else
						{
							Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(constrainedEndPoint, currentSketchPlaneNormal, tangentDir);
						}
					}
				}
				// If we've clicked our third point, establish a sketch plane from the three points and then constrain all future points to that plane
				else if (SketchPlanePoints.Num() < 3)
				{
					// If the third point is colinear, skip it and wait for the next one
					FVector firstSegmentDir = (SketchPlanePoints[1] - SketchPlanePoints[0]).GetSafeNormal();
					if (!FMath::IsNearlyZero(FMath::PointDistToLine(SketchPlanePoints[0], firstSegmentDir, constrainedEndPoint)))
					{
						FVector thirdPointDir = constrainedEndPoint - SketchPlanePoints[0];
						FVector n = FVector::CrossProduct(thirdPointDir, firstSegmentDir);
						// we should not have gotten a degenerate point, ensure
						if (ensureAlways(!FMath::IsNearlyZero(n.Size())))
						{
							Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(constrainedEndPoint, n.GetSafeNormal(), tangentDir);
							SketchPlanePoints.Add(constrainedEndPoint);
							constrainedEndPoint = Controller->EMPlayerState->SnappedCursor.SketchPlaneProject(constrainedEndPoint);
						}
					}
				}
				else
				{
					Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(constrainedEndPoint, currentSketchPlaneNormal, tangentDir);
				}

				AnchorPointDegree = PendingSegment->Point1;
				PendingSegment->Point1 = constrainedEndPoint;
				PendingSegment->Point2 = constrainedEndPoint;

				Controller->ModumateCommand(FModumateCommand(Commands::kEndUndoRedoMacro));
				break;
			}
			case EAxisConstraint::AxisZ:
			{
				// set end of the segment to the hit location
				PendingSegment->Point2 = constrainedEndPoint;
				UpdatePendingPlane();

				// create meta plane
				auto commandResult = Controller->ModumateCommand(
					FModumateCommand(Commands::kMakeMetaPlane)
					.Param(Parameters::kControlPoints, PendingPlanePoints)
					.Param(Parameters::kParent, Controller->EMPlayerState->GetViewGroupObjectID()));
				if (!commandResult.GetValue(Parameters::kSuccess))
				{
					return false;
				}
				NewObjIDs = commandResult.GetValue(Parameters::kObjectIDs);

				// set up cursor affordance so snapping works on the next chained segment
				Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(constrainedEndPoint, FVector::UpVector, (PendingSegment->Point1 - PendingSegment->Point2).GetSafeNormal());

				// continue plane creation starting from the hit location
				AnchorPointDegree = PendingSegment->Point1;
				PendingSegment->Point1 = constrainedEndPoint;
				break;
			}
			case EAxisConstraint::AxesXY:
			{
				auto commandResult = Controller->ModumateCommand(
					FModumateCommand(Commands::kMakeMetaPlane)
					.Param(Parameters::kControlPoints, PendingPlanePoints)
					.Param(Parameters::kParent, Controller->EMPlayerState->GetViewGroupObjectID()));
				if (!commandResult.GetValue(Parameters::kSuccess))
				{
					return false;
				}
				NewObjIDs = commandResult.GetValue(Parameters::kObjectIDs);

				EndUse();
				break;
			}
			}
		}

		OutNewObjIDs = NewObjIDs;

		return true;
	}

	bool FMetaPlaneTool::FrameUpdate()
	{
		FEditModelToolBase::FrameUpdate();

		if (!Controller->EMPlayerState->SnappedCursor.Visible)
		{
			return false;
		}

		FVector hitLoc = Controller->EMPlayerState->SnappedCursor.WorldPosition;

		if (State == NewSegmentPending && PendingSegment.IsValid())
		{
			if (ConstrainHitPoint(hitLoc))
			{
				FAffordanceLine affordance;
				affordance.Color = FLinearColor::Blue;
				affordance.EndPoint = Controller->EMPlayerState->SnappedCursor.WorldPosition;
				affordance.StartPoint = hitLoc;
				affordance.Interval = 4.0f;
				Controller->EMPlayerState->AffordanceLines.Add(affordance);
			}

			PendingSegment->Point2 = hitLoc;
			PendingSegment->UpdateMetaEdgeVisuals(false);
			// TODO: handle non-horizontal dimensions
			Controller->UpdateDimensionString(PendingSegment->Point1, PendingSegment->Point2, FVector::UpVector);
			UpdatePendingPlane();
		}

		if (PendingSegment.Get())
		{
			if ((PendingSegment->Point1 - PendingSegment->Point2).Size() > 25.f) // Add degree string when segment is greater than certain length
			{
				// TODO: refactor for non-horizontal sketching
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

	bool FMetaPlaneTool::EndUse()
	{
		State = Neutral;
		if (PendingSegment.IsValid())
		{
			PendingSegment->Destroy();
			PendingSegment.Reset();
		}

		if (PendingPlane.IsValid())
		{
			PendingPlane->Destroy();
			PendingPlane.Reset();
		}

		Controller->EMPlayerState->SnappedCursor.WantsVerticalAffordanceSnap = false;

		return FEditModelToolBase::EndUse();
	}

	bool FMetaPlaneTool::AbortUse()
	{
		EndUse();
		return FEditModelToolBase::AbortUse();
	}

	bool FMetaPlaneTool::HandleSpacebar()
	{
		return false;
	}

	void FMetaPlaneTool::SetAxisConstraint(EAxisConstraint InAxisConstraint)
	{
		FEditModelToolBase::SetAxisConstraint(InAxisConstraint);

		UpdatePendingPlane();
	}

	void FMetaPlaneTool::UpdatePendingPlane()
	{
		if (PendingPlane.IsValid())
		{
			if (State == NewSegmentPending && PendingSegment.IsValid() &&
				(FVector::Dist(PendingSegment->Point1, PendingSegment->Point2) >= MinPlaneSize))
			{
				switch (AxisConstraint)
				{
				case EAxisConstraint::None:
					PendingPlane->SetActorHiddenInGame(true);
					break;
				case EAxisConstraint::AxisZ:
				{
					FVector verticalRectOffset = FVector::UpVector * Controller->GetDefaultWallHeightFromDoc();

					PendingPlanePoints = {
						PendingSegment->Point1,
						PendingSegment->Point2,
						PendingSegment->Point2 + verticalRectOffset,
						PendingSegment->Point1 + verticalRectOffset
					};

					PendingPlane->SetActorHiddenInGame(false);
					PendingPlane->SetupMetaPlaneGeometry(PendingPlanePoints, PendingPlaneMaterial, PendingPlaneAlpha, true, false);

					break;
				}
				case EAxisConstraint::AxesXY:
				{
					FBox2D pendingRect(ForceInitToZero);
					pendingRect += FVector2D(PendingSegment->Point1);
					pendingRect += FVector2D(PendingSegment->Point2);
					float planeHeight = PendingSegment->Point1.Z;

					PendingPlanePoints = {
						FVector(pendingRect.Min.X, pendingRect.Min.Y, planeHeight),
						FVector(pendingRect.Min.X, pendingRect.Max.Y, planeHeight),
						FVector(pendingRect.Max.X, pendingRect.Max.Y, planeHeight),
						FVector(pendingRect.Max.X, pendingRect.Min.Y, planeHeight),
					};
					PendingPlane->SetActorHiddenInGame(false);
					PendingPlane->SetupMetaPlaneGeometry(PendingPlanePoints, PendingPlaneMaterial, PendingPlaneAlpha, true, false);
					
					break;
				}
				}
			}
			else
			{
				PendingPlanePoints.Reset();
				PendingPlane->SetActorHiddenInGame(true);
			}
		}
	}

	bool FMetaPlaneTool::ConstrainHitPoint(FVector &hitPoint)
	{
		// Free plane sketching is not limited to the sketch plane for the first three points (which establish the sketch plane for subsequent clicks)
		if (AxisConstraint == EAxisConstraint::None && SketchPlanePoints.Num() < 3)
		{
			return false;
		}

		if (Controller.IsValid() && PendingSegment.IsValid())
		{
			FVector sketchPlanePoint = Controller->EMPlayerState->SnappedCursor.SketchPlaneProject(hitPoint);
			bool returnValue = !sketchPlanePoint.Equals(hitPoint);
			hitPoint = sketchPlanePoint;
			return returnValue;
		}

		return false;
	}

	IModumateEditorTool *MakeMetaPlaneTool(AEditModelPlayerController_CPP *controller)
	{
		return new FMetaPlaneTool(controller, EAxisConstraint::None);
	}
}
