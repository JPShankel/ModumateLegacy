// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Tools/EditModelSplitTool.h"
#include "DocumentManagement/ModumateCommands.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/Modumate.h"
#include "Runtime/Engine/Classes/Components/LineBatchComponent.h"

using namespace Modumate;

USplitObjectTool::USplitObjectTool(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, FSelectedObjectToolMixin(Controller)
	, LastValidTarget(nullptr)
	, LastValidSplitStart(ForceInitToZero)
	, LastValidSplitEnd(ForceInitToZero)
	, LastValidStartCP1(-1)
	, LastValidStartCP2(-1)
	, LastValidEndCP1(-1)
	, LastValidEndCP2(-1)
	, ChosenCornerCP(-1)
	, Stage(Hovering)
{ }

bool USplitObjectTool::Activate()
{
	Super::Activate();
	Controller->EMPlayerState->SnappedCursor.MouseMode = EMouseMode::Location;
	Stage = Hovering;
	return true;
}

bool USplitObjectTool::Deactivate()
{
	Super::Deactivate();
	Controller->EMPlayerState->SnappedCursor.MouseMode = EMouseMode::Location;
	return true;
}

bool USplitObjectTool::BeginUse()
{
	Super::BeginUse();

	bool success = true;
	if (HasValidEdgeSplitTarget())
	{
		success = PerformSplit();
	}
	else if (HasValidCornerHoverTarget())
	{
		EnterNextStage();
		return true;
	}

	EndUse();
	return success;
}

bool USplitObjectTool::EnterNextStage()
{
	switch (Stage)
	{
	case ESplitStage::Hovering:
	{
		if (HasValidCornerHoverTarget())
		{
			Stage = ESplitStage::SplittingFromPoint;
			ChosenCornerCP = LastValidStartCP1;
			return true;
		}
		return false;
	}
	case ESplitStage::SplittingFromPoint:
	{
		if (HasValidCornerSplitTarget())
		{
			bool success = PerformSplit();
			EndUse();
			return success;
		}
		return false;
	}
	default:
		return false;
	}
}

bool USplitObjectTool::EndUse()
{
	Controller->EMPlayerState->SnappedCursor.MouseMode = EMouseMode::Location;
	Stage = ESplitStage::Hovering;

	if (IsInUse())
	{
		Super::EndUse();
		RestoreSelectedObjects();
		Controller->EMPlayerState->SnappedCursor.ClearAffordanceFrame();
	}
	return true;
}

bool USplitObjectTool::AbortUse()
{
	Super::AbortUse();
	Controller->EMPlayerState->SnappedCursor.MouseMode = EMouseMode::Location;
	RestoreSelectedObjects();
	if (IsInUse())
	{
		Controller->EMPlayerState->SnappedCursor.ClearAffordanceFrame();
	}

	return true;
}

bool USplitObjectTool::FrameUpdate()
{
	Super::FrameUpdate();

	// Invalidate the splitting ends, so that we don't perform a split with an outdated valid target
	LastValidEndCP1 = LastValidEndCP2 = -1;

	const FSnappedCursor &cursor = Controller->EMPlayerState->SnappedCursor;
	bool hasProjectedCornerSplitTarget = cursor.HasProjectedPosition && (Stage == ESplitStage::SplittingFromPoint);

	if (cursor.Visible && (cursor.Actor || hasProjectedCornerSplitTarget))
	{
		AEditModelGameState_CPP *gameState = Controller->GetWorld()->GetGameState<AEditModelGameState_CPP>();

		FModumateObjectInstance *moi = nullptr;
		FVector splitCursorPos(ForceInitToZero);

		if (hasProjectedCornerSplitTarget)
		{
			moi = LastValidTarget;
			splitCursorPos = cursor.ProjectedPosition;
		}
		else
		{
			moi = gameState->Document.ObjectFromActor(cursor.Actor);
			splitCursorPos = cursor.WorldPosition;
		}


		switch (Stage)
		{
		case ESplitStage::Hovering:
		{
			LastValidTarget = nullptr;
			LastValidSplitStart = LastValidSplitEnd = splitCursorPos;
			LastValidStartCP1 = LastValidStartCP2 = -1;
		}
		break;
		case ESplitStage::SplittingFromPoint:
		{
			if (moi != LastValidTarget)
			{
				return false;
			}
		}
		break;
		default:
			return false;
		}

		FVector splitStart2(splitCursorPos);
		FVector splitEnd2(splitCursorPos);

		if (moi && moi->CanBeSplit() &&
			FindSplitTarget(moi, cursor, Stage,
				LastValidSplitStart, LastValidSplitEnd, splitStart2, splitEnd2,
				LastValidStartCP1, LastValidStartCP2, LastValidEndCP1, LastValidEndCP2))
		{
			LastValidTarget = moi;

			if (HasValidEdgeSplitTarget() || HasValidCornerSplitTarget())
			{
				const auto& controlPoints = moi->GetControlPoints();
				auto& affordanceLines = Controller->EMPlayerState->AffordanceLines;

				FAffordanceLine splitPlaneLine;
				splitPlaneLine.Color = FLinearColor::Blue;
				splitPlaneLine.Interval = 4.0f;

				// Add four affordance lines to represent the entire splitting plane
				splitPlaneLine.StartPoint = LastValidSplitStart;
				splitPlaneLine.EndPoint = LastValidSplitEnd;
				affordanceLines.Add(splitPlaneLine);

				splitPlaneLine.StartPoint = splitStart2;
				splitPlaneLine.EndPoint = splitEnd2;
				affordanceLines.Add(splitPlaneLine);

				splitPlaneLine.StartPoint = LastValidSplitStart;
				splitPlaneLine.EndPoint = splitStart2;
				affordanceLines.Add(splitPlaneLine);

				splitPlaneLine.StartPoint = LastValidSplitEnd;
				splitPlaneLine.EndPoint = splitEnd2;
				affordanceLines.Add(splitPlaneLine);

				// Add affordance lines for the hovered object's control points / sketch
				// TODO: make more visually consistent with other effects,
				// and potentially replace with a general hover visual style
				FAffordanceLine controlPointLine;
				controlPointLine.Color = FLinearColor::Green;
				controlPointLine.Interval = 6.0f;

				switch (moi->GetObjectType())
				{
				case EObjectType::OTWallSegment:
				{
					controlPointLine.StartPoint = moi->GetCorner(0);
					controlPointLine.EndPoint = moi->GetCorner(1);
					affordanceLines.Add(controlPointLine);

					controlPointLine.StartPoint = moi->GetCorner(0);
					controlPointLine.EndPoint = moi->GetCorner(2);
					affordanceLines.Add(controlPointLine);

					controlPointLine.StartPoint = moi->GetCorner(1);
					controlPointLine.EndPoint = moi->GetCorner(3);
					affordanceLines.Add(controlPointLine);

					controlPointLine.StartPoint = moi->GetCorner(2);
					controlPointLine.EndPoint = moi->GetCorner(3);
					affordanceLines.Add(controlPointLine);
				}
				break;
				case EObjectType::OTRailSegment:
				{
					for (int32 i = 0; i < controlPoints.Num() - 1; i += 2)
					{
						controlPointLine.StartPoint = controlPoints[i];
						controlPointLine.EndPoint = controlPoints[i + 1];
						affordanceLines.Add(controlPointLine);
					}
				}
				break;
				case EObjectType::OTFloorSegment:
					//case OTRoofSegment:
				case EObjectType::OTCabinet:
				{
					for (int32 i = 0; i < controlPoints.Num(); ++i)
					{
						controlPointLine.StartPoint = controlPoints[i];
						controlPointLine.EndPoint = controlPoints[(i + 1) % controlPoints.Num()];
						affordanceLines.Add(controlPointLine);
					}
				}
				break;
				default:
					break;
				}
			}
		}
	}

	return true;
}

bool USplitObjectTool::PerformSplit() const
{
	UE_LOG(LogCallTrace, Display, TEXT("Splitting target: %s between edges %d-%d and %d-%d"),
		*LastValidTarget->GetActor()->GetName(),
		LastValidStartCP1, LastValidStartCP2, LastValidEndCP1, LastValidEndCP2);

	const TArray<FVector> &oldPoints = LastValidTarget->GetControlPoints();
	const TArray<int32> &oldIndices = LastValidTarget->GetControlPointIndices();

	TArray<FVector> newPointsA, newPointsB;
	TArray<int32> newIndicesA, newIndicesB;
	bool validPoints = false;

	switch (LastValidTarget->GetObjectType())
	{
	case EObjectType::OTWallSegment:
	{
		FVector segmentDelta = oldPoints[1] - oldPoints[0];
		FVector projectedSplitPosition = oldPoints[0] + (LastValidSplitStart - oldPoints[0]).ProjectOnTo(oldPoints[1] - oldPoints[0]);

		newPointsA = { oldPoints[0], projectedSplitPosition };
		newPointsB = { projectedSplitPosition, oldPoints[1] };
		newIndicesA = newIndicesB = oldIndices;

		validPoints = true;
	}
	break;
	case EObjectType::OTRailSegment:
	{
		// TODO: implement rails
	}
	break;
	case EObjectType::OTFloorSegment:
		//case OTRoofSegment:
	case EObjectType::OTCabinet:
	{
		bool hasFrontIndex = oldIndices.Num() > 0;
		int32 oldFrontIndex = hasFrontIndex ? oldIndices[0] : -1;

		int32 startCP1 = LastValidStartCP1, startCP2 = LastValidStartCP2;
		int32 endCP1 = LastValidEndCP1, endCP2 = LastValidEndCP2;
		FVector splitStartPoint = LastValidSplitStart, splitEndPoint = LastValidSplitEnd;

		if (startCP1 > endCP1)
		{
			Swap(startCP1, endCP1);
			Swap(startCP2, endCP2);
			Swap(splitStartPoint, splitEndPoint);
		}

		bool newSplitStart = (startCP2 >= 0);
		bool newSplitEnd = (endCP2 >= 0);
		const FVector &startPoint1 = oldPoints[startCP1];
		const FVector &startPoint2 = newSplitStart ? oldPoints[startCP2] : startPoint1;
		const FVector &endPoint1 = oldPoints[endCP1];
		const FVector &endPoint2 = newSplitEnd ? oldPoints[endCP2] : endPoint1;
		splitStartPoint = startPoint1 + (splitStartPoint - startPoint1).ProjectOnTo(startPoint2 - startPoint1);
		splitEndPoint = endPoint1 + (splitEndPoint - endPoint1).ProjectOnTo(endPoint2 - endPoint1);

		// Use a helper function to decide which new index should be used for each new cycle for the front index,
		// if the old cycle had one.
		auto checkAddFrontIndex = [hasFrontIndex, oldFrontIndex](int32 i, const TArray<FVector> &newPoints, TArray<int32> &newIndices)
		{
			if (hasFrontIndex && (i >= 0) && (i == oldFrontIndex) && (newIndices.Num() == 0))
			{
				newIndices.Add(newPoints.Num() - 1);
			}
		};

		// Fill the first half of the points, which loops around the control points starting at index 0
		for (int32 i = 0; i < oldPoints.Num(); ++i)
		{
			newPointsA.Add(oldPoints[i]);

			if (i == startCP1)
			{
				if (newSplitStart)
				{
					checkAddFrontIndex(i, newPointsA, newIndicesA);
					newPointsA.Add(splitStartPoint);
				}

				if (newSplitEnd)
				{
					newPointsA.Add(splitEndPoint);
				}

				if (newSplitEnd)
				{
					if (endCP2 == 0)
					{
						checkAddFrontIndex(newPointsA.Num() - 1, newPointsA, newIndicesA);
						break;
					}

					i = endCP2 - 1;
				}
				else
				{
					if (endCP1 == 0)
					{
						break;
					}

					i = endCP1 - 1;
				}
			}

			checkAddFrontIndex(i, newPointsA, newIndicesA);
		}

		// Fill the second half of the points, which loops around the control points starting at the split
		int32 nextStartIndex = newSplitStart ? startCP2 : startCP1;
		if (newSplitStart)
		{
			newPointsB.Add(splitStartPoint);
			checkAddFrontIndex(nextStartIndex - 1, newPointsB, newIndicesB);
		}
		for (int32 i = nextStartIndex; i < oldPoints.Num(); ++i)
		{
			newPointsB.Add(oldPoints[i]);

			if (i == endCP1)
			{
				if (newSplitEnd)
				{
					checkAddFrontIndex(i, newPointsB, newIndicesB);
					newPointsB.Add(splitEndPoint);
				}

				break;
			}

			checkAddFrontIndex(i, newPointsB, newIndicesB);
		}

		Controller->SetObjectSelected(LastValidTarget, false);

		int32 expectedTotalNewPoints = oldPoints.Num() + 2 +
			(newSplitStart ? 1 : 0) + (newSplitEnd ? 1 : 0);
		if (ensureAlways((((newPointsA.Num() + newPointsB.Num()) == expectedTotalNewPoints)) &&
			(newPointsA.Num() > 2) && (newPointsB.Num() > 2)))
		{
			validPoints = true;
		}
		else
		{
			UE_LOG(LogCallTrace, Error, TEXT("Split error; invalid resulting number of points!"));
		}
	}
	break;
	default:
		break;
	}

	if (validPoints)
	{
		Controller->ModumateCommand(
			FModumateCommand(Modumate::Commands::kSplit)
			.Param("id", LastValidTarget->ID)
			.Param("pointsA", newPointsA)
			.Param("pointsB", newPointsB)
			.Param("indicesA", newIndicesA)
			.Param("indicesB", newIndicesB)
		);

		return true;
	}

	return false;
}

bool USplitObjectTool::HasValidEdgeSplitTarget() const
{
	if (LastValidTarget)
	{
		switch (LastValidTarget->GetObjectType())
		{
		case EObjectType::OTWallSegment:
		case EObjectType::OTRailSegment:
		{
			return true;
		}
		case EObjectType::OTFloorSegment:
			//case OTRoofSegment:
		case EObjectType::OTCabinet:
		{
			return (LastValidStartCP1 >= 0) && (LastValidStartCP2 >= 0) &&
				(LastValidEndCP1 >= 0) && (LastValidEndCP2 >= 0);
		}
		default:
			return false;
		}
	}

	return false;
}

bool USplitObjectTool::HasValidCornerHoverTarget() const
{
	if (LastValidTarget)
	{
		switch (LastValidTarget->GetObjectType())
		{
		case EObjectType::OTWallSegment:
		case EObjectType::OTRailSegment:
		{
			return false;
		}
		case EObjectType::OTFloorSegment:
			//case OTRoofSegment:
		case EObjectType::OTCabinet:
		{
			return (LastValidStartCP1 >= 0) && (LastValidStartCP2 < 0);
		}
		default:
			return false;
		}
	}

	return false;
}

bool USplitObjectTool::HasValidCornerSplitTarget() const
{
	if (LastValidTarget)
	{
		switch (LastValidTarget->GetObjectType())
		{
		case EObjectType::OTWallSegment:
		case EObjectType::OTRailSegment:
		{
			return false;
		}
		case EObjectType::OTFloorSegment:
			//case OTRoofSegment:
		case EObjectType::OTCabinet:
		{
			return (LastValidStartCP1 >= 0) && (LastValidStartCP2 < 0) && (LastValidEndCP1 >= 0);
		}
		default:
			return false;
		}
	}

	return false;
}

bool USplitObjectTool::FindSplitTarget(const FModumateObjectInstance *moi, const FSnappedCursor &snapCursor, ESplitStage splitStage,
	FVector &refSplitStart, FVector &outSplitEnd, FVector &outSplitStart2, FVector &outSplitEnd2,
	int32 &refStartCP1, int32 &refStartCP2, int32 &outCP1, int32 &outCP2)
{
	if (!moi)
	{
		return false;
	}

	const TArray<FVector>& points = moi->GetControlPoints();
	int32 numPoints = points.Num();

	ESnapType snapType = snapCursor.SnapType;
	if (splitStage == ESplitStage::Hovering)
	{
		refStartCP1 = snapCursor.CP1 % numPoints;
		refStartCP2 = snapCursor.CP2 % numPoints;
	}

	switch (moi->GetObjectType())
	{
	case EObjectType::OTWallSegment:
	{
		if (splitStage == ESplitStage::SplittingFromPoint)
		{
			return false;
		}

		FVector wallHeightDelta = moi->GetExtents().Y * FVector::UpVector;

		if (snapType == ESnapType::CT_FACESELECT)
		{
			FVector wallDelta = points[1] - points[0];
			float wallLength = wallDelta.Size();
			if (wallLength > 0.0f)
			{
				FVector wallDir = wallDelta / wallLength;
				FVector splitStartFromSegmentStart = refSplitStart - points[0];
				float projectedSplitLength = splitStartFromSegmentStart | wallDir;
				if ((projectedSplitLength > 0.0f) && (projectedSplitLength < wallLength))
				{
					refSplitStart = points[0] + projectedSplitLength * wallDir;
					outSplitEnd = refSplitStart + (moi->CalculateThickness() * moi->GetWallDirection());

					outSplitStart2 = refSplitStart + wallHeightDelta;
					outSplitEnd2 = outSplitEnd + wallHeightDelta;

					refStartCP1 = outCP1 = 0;
					refStartCP2 = outCP2 = 1;
					return true;
				}
			}
		}
		else if ((refStartCP1 >= 0) && (refStartCP2 >= 0) &&
			((snapType == ESnapType::CT_EDGESNAP) || (snapType != ESnapType::CT_MIDSNAP)))
		{
			// Require splitting the long edge of a wall
			if (FMath::Abs(refStartCP1 - refStartCP2) != 1)
			{
				return false;
			}

			// Only support edge and midpoint snapping for now
			if (((snapType != ESnapType::CT_EDGESNAP) && (snapType != ESnapType::CT_MIDSNAP)) ||
				(refStartCP1 < 0) || (refStartCP2 < 0))
			{
				return false;
			}

			// Figure out which sides of the wall we snapped against for the split, so the preview will be correct
			int32 wallDirSidedness = ((refStartCP1 % 4) >= 2) ? -1 : 1;
			float wallHeightSidedness = (refStartCP1 >= 4) ? 1.0f : -1.0f;

			outSplitEnd = refSplitStart + (wallDirSidedness * moi->CalculateThickness() * moi->GetWallDirection());
			outSplitStart2 = refSplitStart - wallHeightSidedness * wallHeightDelta;
			outSplitEnd2 = outSplitEnd - wallHeightSidedness * wallHeightDelta;

			outCP1 = refStartCP1 + wallDirSidedness * 2;
			outCP2 = refStartCP2 + wallDirSidedness * 2;
			return true;
		}

		return false;
	}
	break;
	case EObjectType::OTRailSegment:
	{
		if (splitStage == ESplitStage::SplittingFromPoint)
		{
			return false;
		}

		if (//(snapType == ESnapType::CT_FACESELECT) ||
			((refStartCP1 >= 0) && (refStartCP2 >= 0) && (refStartCP2 == (refStartCP1 + 1)) &&
			((snapType == ESnapType::CT_EDGESNAP) || (snapType != ESnapType::CT_MIDSNAP))))
		{
			FVector railDelta = points[1] - points[0];
			float railLength = railDelta.Size();
			if (railLength > 0.0f)
			{
				FVector railDir = railDelta / railLength;
				FVector splitStartFromSegmentStart = refSplitStart - points[0];
				float projectedSplitLength = splitStartFromSegmentStart | railDir;
				if ((projectedSplitLength > 0.0f) && (projectedSplitLength < railLength))
				{
					outSplitEnd = refSplitStart;
					outCP1 = refStartCP1;
					outCP2 = refStartCP2;
					return true;
				}
			}
		}

		return false;
	}
	break;
	case EObjectType::OTFloorSegment:
		//case OTRoofSegment:
	case EObjectType::OTCabinet:
	{
		FPlane objectPlane(points[0], FVector::UpVector);
		FVector extrusionDelta = (moi->GetExtents().Y * objectPlane);
		FVector splitCursorPlaneProjected = FVector::PointPlaneProject(snapCursor.WorldPosition, objectPlane);
		FVector goalSplitDir(ForceInitToZero);

		if (splitStage == ESplitStage::Hovering)
		{
			refSplitStart = splitCursorPlaneProjected;

			// Allow starting corner splits from corner snaps
			if (snapType == ESnapType::CT_CORNERSNAP)
			{
				return true;
			}
		}

		// If we get a face hit against a polygonal shape, make sure it's against one of its sides
		// (orthogonal to its extrusion axis), and try to find the corresponding edge
		if ((splitStage == ESplitStage::Hovering) && (snapType == ESnapType::CT_FACESELECT) &&
			FVector::Orthogonal(snapCursor.HitNormal, objectPlane))
		{
			for (int32 i = 0; i < numPoints; ++i)
			{
				int32 nextI = (i + 1) % numPoints;
				const FVector &p1 = points[i];
				const FVector &p2 = points[nextI];
				FVector edgeDelta = p2 - p1;
				float edgeLength = edgeDelta.Size();
				if (edgeLength > 0.0f)
				{
					FVector edgeDir = edgeDelta / edgeLength;
					FVector planeProjectedDelta = splitCursorPlaneProjected - p1;
					float distAlongEdge = planeProjectedDelta | edgeDir;
					if ((distAlongEdge > 0) && (distAlongEdge < edgeLength) &&
						FVector::Parallel(planeProjectedDelta.GetSafeNormal(), edgeDir))
					{
						refStartCP1 = i;
						refStartCP2 = nextI;
						break;
					}
				}
			}
		}

		if (splitStage == ESplitStage::Hovering)
		{
			// Only support edge and midpoint snapping for polygonal shapes during edge hovering
			if (((snapType != ESnapType::CT_EDGESNAP) && (snapType != ESnapType::CT_MIDSNAP) && (snapType != ESnapType::CT_FACESELECT)) ||
				(refStartCP1 < 0) || (refStartCP2 < 0))
			{
				return false;
			}

			if (refStartCP1 < 0 || refStartCP1 >= numPoints || refStartCP2 < 0 || refStartCP2 >= numPoints || numPoints < 3)
			{
				return false;
			}

			if (!moi->GetTriInternalNormalFromEdge(refStartCP1, refStartCP2, goalSplitDir))
			{
				return false;
			}

			FVector edgeDir = (points[refStartCP2] - points[refStartCP1]).GetSafeNormal();

			if (FVector::Parallel(edgeDir, objectPlane))
			{
				return false;
			}

			if (!FVector::Parallel(goalSplitDir, edgeDir ^ objectPlane))
			{
				return false;
			}

			if (!objectPlane.IsNormalized() || !edgeDir.IsNormalized() || !goalSplitDir.IsNormalized())
			{
				return false;
			}
		}
		else if (splitStage == ESplitStage::SplittingFromPoint)
		{
			switch (snapType)
			{
			case ESnapType::CT_CORNERSNAP:
			case ESnapType::CT_EDGESNAP:
			case ESnapType::CT_MIDSNAP:
			case ESnapType::CT_FACESELECT:
			{
				// continue on to find the next best target edge based on the direction of the cursor
				goalSplitDir = (splitCursorPlaneProjected - refSplitStart).GetSafeNormal();

				int32 startNextCP = (refStartCP1 + 1) % numPoints;
				int32 startPrevCP = (refStartCP1 - 1 + numPoints) % numPoints;

				FVector dirToNextPoint = (points[startNextCP] - refSplitStart).GetSafeNormal();
				FVector dirToPrevPoint = (points[startPrevCP] - refSplitStart).GetSafeNormal();

				// Make sure that our goal direction doesn't go along one of its adjacent edges
				if (goalSplitDir.IsNearlyZero() || dirToNextPoint.IsNearlyZero() || dirToPrevPoint.IsNearlyZero() ||
					FVector::Coincident(goalSplitDir, dirToNextPoint) || FVector::Coincident(goalSplitDir, dirToPrevPoint))
				{
					return false;
				}

				// Make sure that the goal direction points inside the polygon

				FVector nextEdgeInternalNormal, prevEdgeInternalNormal;
				if (!moi->GetTriInternalNormalFromEdge(startPrevCP, refStartCP1, prevEdgeInternalNormal) ||
					!moi->GetTriInternalNormalFromEdge(refStartCP1, startNextCP, nextEdgeInternalNormal))
				{
					return false;
				}

				// If the next and previous edges overlap somehow, this whole thing is invalid
				float nextPrevDot = dirToPrevPoint | dirToNextPoint;
				if (nextPrevDot >= THRESH_NORMALS_ARE_PARALLEL)
				{
					return false;
				}
				// If they're anti-parallel, then just make sure the split dir is on the same side as their shared internal normal
				else if (nextPrevDot <= -THRESH_NORMALS_ARE_PARALLEL)
				{
					if (!FVector::Coincident(prevEdgeInternalNormal, nextEdgeInternalNormal) || ((goalSplitDir | nextEdgeInternalNormal) <= 0.0f))
					{
						return false;
					}
				}
				else
				{
					bool isCornerConvex = ((dirToPrevPoint | nextEdgeInternalNormal) > 0.0f) && ((dirToNextPoint | prevEdgeInternalNormal) > 0.0f);

					float prevToNextCrossAmount = (dirToPrevPoint ^ dirToNextPoint) | objectPlane;
					float prevToSplitCrossAmount = (dirToPrevPoint ^ goalSplitDir) | objectPlane;
					float nextToPrevCrossAmount = (dirToNextPoint ^ dirToPrevPoint) | objectPlane;
					float nextToSplitCrossAmount = (dirToNextPoint ^ goalSplitDir) | objectPlane;

					// If the corner is convex, then the split direction must be between the edge directions.
					// Otherwise, the split direction must be outside of the acute angle formed by the edge vectors.
					bool splitDirBetweenEdgesAcute = ((prevToNextCrossAmount * prevToSplitCrossAmount) >= 0) &&
						((nextToPrevCrossAmount * nextToSplitCrossAmount) >= 0);

					if (isCornerConvex != splitDirBetweenEdgesAcute)
					{
						return false;
					}
				}
			}
			break;
			default:
				return false;
			}
		}

		FVector bestSplitEnd(ForceInitToZero);
		int32 bestCP1 = -1;
		int32 bestCP2 = -1;
		float bestSplitLength = FLT_MAX;
		float lengthAlongOtherBestEdge = 0.0f;
		static float splitCornerSnapMargin = 0.5f;
		static float splitLengthEpsilon = 0.01f;

		for (int32 otherCP1 = 0; otherCP1 < numPoints; ++otherCP1)
		{
			// Compute the other edge against which we might split,
			// and make sure it is possible to compute the intersection
			int32 otherCP2 = (otherCP1 + 1) % numPoints;

			if ((otherCP1 == refStartCP1) && (otherCP2 == refStartCP2))
			{
				continue;
			}

			const FVector &otherPoint1 = points[otherCP1];
			const FVector &otherPoint2 = points[otherCP2];
			FVector otherEdgeDelta = otherPoint2 - otherPoint1;

			if (otherEdgeDelta.IsNearlyZero())
			{
				continue;
			}

			float otherEdgeLength = otherEdgeDelta.Size();
			FVector otherEdgeDir = otherEdgeDelta / otherEdgeLength;

			if (FVector::Parallel(otherEdgeDir, goalSplitDir))
			{
				continue;
			}

			FVector otherTriNormal;
			if (!moi->GetTriInternalNormalFromEdge(otherCP1, otherCP2, otherTriNormal))
			{
				continue;
			}

			// Compute the intersection
			FVector splitEnd = FMath::RayPlaneIntersection(splitCursorPlaneProjected, goalSplitDir, FPlane(otherPoint1, otherTriNormal));

			// Make sure the intersection is actually on the other edge, between its points
			float splitEndProjectedLength = (splitEnd - otherPoint1) | otherEdgeDir;
			if ((splitEndProjectedLength < -splitCornerSnapMargin) ||
				(splitEndProjectedLength >= (otherEdgeLength + splitCornerSnapMargin)))
			{
				continue;
			}

			// Make sure the split ends up hitting the interior edge of a triangle, so we're not splitting in midair
			FVector otherSplitDelta = splitEnd - refSplitStart;
			FVector otherSplitDir = otherSplitDelta.GetSafeNormal();
			if (((otherSplitDir | otherTriNormal) > 0.0f) || ((otherSplitDir | goalSplitDir) < 0.0f))
			{
				continue;
			}

			// Only keep the shortest split we've found so far
			float splitLength = otherSplitDelta.Size();
			if (!FMath::IsNearlyZero(splitLength, splitLengthEpsilon) &&
				(splitLength < bestSplitLength))
			{
				bestSplitEnd = splitEnd;
				bestCP1 = otherCP1;
				bestCP2 = otherCP2;
				bestSplitLength = splitLength;
				lengthAlongOtherBestEdge = splitEndProjectedLength;
			}
		}

		if ((bestCP1 >= 0) && (bestCP2 >= 0))
		{
			float bestSplitEdgeLength = FVector::Distance(points[bestCP1], points[bestCP2]);

			// If we're very close to a single other control point, just use that point rather than the entire edge
			if (FMath::Abs(lengthAlongOtherBestEdge) < splitCornerSnapMargin)
			{
				outSplitEnd = points[bestCP1];
				outCP1 = bestCP1;
				outCP2 = -1;
			}
			else if (FMath::Abs(lengthAlongOtherBestEdge - bestSplitEdgeLength) < splitCornerSnapMargin)
			{
				outSplitEnd = points[bestCP2];
				outCP1 = bestCP2;
				outCP2 = -1;
			}
			// Otherwise, store the entire edge
			else
			{
				outSplitEnd = bestSplitEnd;
				outCP1 = bestCP1;
				outCP2 = bestCP2;
			}

			outSplitStart2 = refSplitStart + extrusionDelta;
			outSplitEnd2 = outSplitEnd + extrusionDelta;

			return true;
		}
		else
		{
			return false;
		}
	}
	break;
	default:
		return false;
		break;
	}
}

