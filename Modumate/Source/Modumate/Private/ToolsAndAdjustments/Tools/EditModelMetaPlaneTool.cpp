// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Tools/EditModelMetaPlaneTool.h"

#include "Components/EditableTextBox.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "Online/ModumateAnalyticsStatics.h"
#include "UI/DimensionManager.h"
#include "UI/PendingSegmentActor.h"
#include "UnrealClasses/DimensionWidget.h"
#include "UnrealClasses/DynamicMeshActor.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "UnrealClasses/LineActor.h"

using namespace Modumate;

UMetaPlaneTool::UMetaPlaneTool(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, State(Neutral)
	, AnchorPointDegree(ForceInitToZero)
	, bPendingSegmentValid(false)
	, bPendingPlaneValid(false)
	, PendingPlaneGeom(ForceInitToZero)
	, MinPlaneSize(1.0f)
	, PendingPlaneAlpha(0.4f)
{
	UWorld *world = Controller ? Controller->GetWorld() : nullptr;
	if (world)
	{
		GameMode = world->GetAuthGameMode<AEditModelGameMode_CPP>();
		GameState = world->GetGameState<AEditModelGameState_CPP>();
	}
}

void UMetaPlaneTool::Initialize()
{
	InstanceHeight = GetDefaultPlaneHeight();
}

bool UMetaPlaneTool::HandleInputNumber(double n)
{
	Super::HandleInputNumber(n);

	if (!Controller->EMPlayerState->SnappedCursor.Visible)
	{
		return false;
	}

	auto pendingSegment = DimensionManager->GetDimensionActor(PendingSegmentID)->GetLineActor();

	if ((State == NewSegmentPending) && pendingSegment != nullptr)
	{
		FVector dir = (pendingSegment->Point2 - pendingSegment->Point1).GetSafeNormal() * n;

		// TODO: until the UI flow can allow for separate X and Y distances, do not respond
		// to typing a number for XY planes
		if (AxisConstraint == EAxisConstraint::AxesXY)
		{
			return false;
		}

		return MakeObject(pendingSegment->Point1 + dir);
	}
	return true;
}

bool UMetaPlaneTool::Activate()
{
	Controller->DeselectAll();
	Controller->EMPlayerState->SetHoveredObject(nullptr);
	OriginalMouseMode = Controller->EMPlayerState->SnappedCursor.MouseMode;
	Controller->EMPlayerState->SnappedCursor.MouseMode = EMouseMode::Location;

	return UEditModelToolBase::Activate();
}

bool UMetaPlaneTool::Deactivate()
{
	State = Neutral;
	Controller->EMPlayerState->SnappedCursor.MouseMode = OriginalMouseMode;
	Controller->EMPlayerState->SnappedCursor.WantsVerticalAffordanceSnap = false;
	return UEditModelToolBase::Deactivate();
}

bool UMetaPlaneTool::BeginUse()
{
	Super::BeginUse();
	if (!Controller->EMPlayerState->SnappedCursor.Visible)
	{
		return false;
	}

	const FVector &hitLoc = Controller->EMPlayerState->SnappedCursor.WorldPosition;
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

	auto dimensionActor = DimensionManager->GetDimensionActor(PendingSegmentID);
	if (dimensionActor != nullptr)
	{
		auto pendingSegment = dimensionActor->GetLineActor();
		pendingSegment->Point1 = hitLoc;
		pendingSegment->Point2 = hitLoc;
		pendingSegment->Color = FColor::White;
		pendingSegment->Thickness = 2;
	}

	AnchorPointDegree = hitLoc + FVector(0.f, -1.f, 0.f); // Make north as AnchorPointDegree at new segment

	GameState->Document->StartPreviewing();

	PendingPlanePoints.Reset();

	return true;
}

bool UMetaPlaneTool::EnterNextStage()
{
	Super::EnterNextStage();

	if (!Controller->EMPlayerState->SnappedCursor.Visible)
	{
		return false;
	}
	if (State == NewSegmentPending)
	{
		return MakeObject(Controller->EMPlayerState->SnappedCursor.WorldPosition);
	}

	return false;
}

bool UMetaPlaneTool::GetMetaObjectCreationDeltas(const FVector& Location, bool bSplitAndUpdateFaces,
	FVector& OutConstrainedLocation, FVector& OutAffordanceNormal, FVector& OutAffordanceTangent, TArray<FVector>& OutSketchPoints,
	TArray<FDeltaPtr>& OutDeltaPtrs)
{
	OutConstrainedLocation = Location;
	OutAffordanceNormal = FVector::ZeroVector;
	OutAffordanceTangent = FVector::ZeroVector;
	OutSketchPoints = SketchPlanePoints;
	OutDeltaPtrs.Reset();

	CurAddedVertexIDs.Reset();
	CurAddedEdgeIDs.Reset();
	CurAddedFaceIDs.Reset();

	UModumateDocument* doc = GameState->Document;

	bool bSuccess = false;
	auto dimensionActor = DimensionManager->GetDimensionActor(PendingSegmentID);
	auto pendingSegment = dimensionActor->GetLineActor();

	FVector constrainedStartPoint = pendingSegment->Point1;
	OutConstrainedLocation = Location;
	ConstrainHitPoint(constrainedStartPoint);
	ConstrainHitPoint(OutConstrainedLocation);

	if (State == NewSegmentPending && pendingSegment != nullptr &&
		(FVector::Dist(constrainedStartPoint, OutConstrainedLocation) >= MinPlaneSize))
	{
		switch (AxisConstraint)
		{
			case EAxisConstraint::None:
			{
				// The segment-based plane updates the sketch plane on the first three clicks
				FVector segmentDirection = FVector(OutConstrainedLocation - constrainedStartPoint).GetSafeNormal();
				FVector currentSketchPlaneNormal(Controller->EMPlayerState->SnappedCursor.AffordanceFrame.Normal);

				OutAffordanceTangent = !FVector::Parallel(currentSketchPlaneNormal, segmentDirection) ? segmentDirection : Controller->EMPlayerState->SnappedCursor.AffordanceFrame.Tangent;
				bool bSetDefaultAffordance = true;

				if (OutSketchPoints.Num() == 0)
				{
					// If the two points are close together, ignore them and wait for the next segment
					if (!FMath::IsNearlyZero(segmentDirection.Size()))
					{
						// Otherwise, this is our first valid line segment, so adjust sketch plane normal to be perpendicular to this
						OutSketchPoints.Add(constrainedStartPoint);
						OutSketchPoints.Add(OutConstrainedLocation);

						// If we've selected a point along the sketch plane's Z, leave the sketch plane intact
						// Note: it is all right to add the sketch plane points above as a vertical line is a legitimate basis for a new sketch plane
						// which will be established with the next click, meantime a vertical click should just leave the original sketch plane in place
						if (!FVector::Orthogonal(currentSketchPlaneNormal, OutAffordanceTangent))
						{
							FVector transverse = FVector::CrossProduct(currentSketchPlaneNormal, segmentDirection);
							OutAffordanceNormal = FVector::CrossProduct(transverse, OutAffordanceTangent).GetSafeNormal();
							bSetDefaultAffordance = false;
						}
					}
				}
				// If we've clicked our third point, establish a sketch plane from the three points and then constrain all future points to that plane
				else if (OutSketchPoints.Num() < 3)
				{
					// If the third point is colinear, skip it and wait for the next one
					FVector firstSegmentDir = (OutSketchPoints[1] - OutSketchPoints[0]).GetSafeNormal();
					FVector nextPointDir = (OutConstrainedLocation - OutSketchPoints[0]).GetSafeNormal();

					if (!FVector::Parallel(firstSegmentDir, nextPointDir))
					{
						FVector sketchNormal = (firstSegmentDir ^ nextPointDir).GetSafeNormal();
						// we should not have gotten a degenerate point, ensure
						if (ensureAlways(sketchNormal.IsNormalized()))
						{
							OutAffordanceNormal = sketchNormal;
							OutSketchPoints.Add(OutConstrainedLocation);
							bSetDefaultAffordance = false;
						}
					}
				}

				if (bSetDefaultAffordance)
				{
					OutAffordanceNormal = currentSketchPlaneNormal;
				}

				TArray<FVector> points = { constrainedStartPoint, OutConstrainedLocation };

				bSuccess = doc->MakeMetaObject(GetWorld(), points, {}, EObjectType::OTMetaEdge, Controller->EMPlayerState->GetViewGroupObjectID(),
					CurAddedVertexIDs, CurAddedEdgeIDs, CurAddedFaceIDs, OutDeltaPtrs, bSplitAndUpdateFaces);

				break;
			}
			case EAxisConstraint::AxisZ:
			{
				// set end of the segment to the hit location
				pendingSegment->Point2 = OutConstrainedLocation;
				UpdatePendingPlane();

				if (bPendingPlaneValid)
				{
					bSuccess = doc->MakeMetaObject(GetWorld(), PendingPlanePoints, {}, EObjectType::OTMetaPlane, Controller->EMPlayerState->GetViewGroupObjectID(),
						CurAddedVertexIDs, CurAddedEdgeIDs, CurAddedFaceIDs, OutDeltaPtrs, bSplitAndUpdateFaces);

					OutAffordanceNormal = FVector::UpVector;
					OutAffordanceTangent = (pendingSegment->Point1 - pendingSegment->Point2).GetSafeNormal();
				}

				break;
			}
			case EAxisConstraint::AxesXY:
			{
				bSuccess = bPendingPlaneValid && doc->MakeMetaObject(GetWorld(), PendingPlanePoints, {}, EObjectType::OTMetaPlane, Controller->EMPlayerState->GetViewGroupObjectID(),
					CurAddedVertexIDs, CurAddedEdgeIDs, CurAddedFaceIDs, OutDeltaPtrs, bSplitAndUpdateFaces);

				break;
			}
		}
	}

	return bSuccess;
}

bool UMetaPlaneTool::MakeObject(const FVector& Location)
{
	UModumateDocument* doc = GameState ? GameState->Document : nullptr;
	if (doc == nullptr)
	{
		return false;
	}

	doc->ClearPreviewDeltas(GetWorld());

	FVector constrainedLocation, newAffordanceNormal, newAffordanceTangent;
	if (!GetMetaObjectCreationDeltas(Location, true, constrainedLocation, newAffordanceNormal, newAffordanceTangent, SketchPlanePoints, CurDeltas))
	{
		return false;
	}

	if (!doc->ApplyDeltas(CurDeltas, GetWorld()))
	{
		return false;
	}

	if (GetToolMode() == EToolMode::VE_METAPLANE)
	{
		EObjectType analyticsObjectType = (AxisConstraint == EAxisConstraint::None) ? EObjectType::OTMetaEdge : EObjectType::OTMetaPlane;
		UModumateAnalyticsStatics::RecordObjectCreation(this, analyticsObjectType);
	}

	// Decide whether to end the tool's use, or continue the chain, based on axis constraint
	switch (AxisConstraint)
	{
	case EAxisConstraint::AxesXY:
		EndUse();
		break;
	default:
	{
		Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(constrainedLocation, newAffordanceNormal, newAffordanceTangent);

		AnchorPointDegree = constrainedLocation;
		auto dimensionActor = DimensionManager->GetDimensionActor(PendingSegmentID);
		auto pendingSegment = dimensionActor ? dimensionActor->GetLineActor() : nullptr;
		if (pendingSegment)
		{
			pendingSegment->Point1 = constrainedLocation;
			pendingSegment->Point2 = constrainedLocation;
		}
	}
	break;
	}

	return true;
}

bool UMetaPlaneTool::UpdatePreview()
{
	CurDeltas.Reset();

	if (!Controller->EMPlayerState->SnappedCursor.Visible)
	{
		return false;
	}

	FVector hitLoc = Controller->EMPlayerState->SnappedCursor.WorldPosition;

	if (State != NewSegmentPending || !ensure(GameState && GameState->Document))
	{
		return false;
	}

	auto dimensionActor = DimensionManager->GetDimensionActor(PendingSegmentID);
	ALineActor* pendingSegment = nullptr;
	if (dimensionActor != nullptr)
	{
		pendingSegment = dimensionActor->GetLineActor();
	}

	if (pendingSegment != nullptr)
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

		pendingSegment->Point2 = hitLoc;
		pendingSegment->UpdateVisuals(false);
	}

	UpdatePendingPlane();
	GameState->Document->StartPreviewing();

	FVector previewAffordanceNormal, previewAffordanceTangent;
	TArray<FVector> tempSketchPoints;
	return GetMetaObjectCreationDeltas(hitLoc, false, hitLoc, previewAffordanceNormal, previewAffordanceTangent, tempSketchPoints, CurDeltas);
}

bool UMetaPlaneTool::FrameUpdate()
{
	Super::FrameUpdate();

	if (UpdatePreview())
	{
		GameState->Document->ApplyPreviewDeltas(CurDeltas, GetWorld());
	}

	return true;
}

bool UMetaPlaneTool::EndUse()
{
	State = Neutral;

	Controller->EMPlayerState->SnappedCursor.WantsVerticalAffordanceSnap = false;

	return Super::EndUse();
}

bool UMetaPlaneTool::AbortUse()
{
	EndUse();
	return Super::AbortUse();
}

bool UMetaPlaneTool::PostEndOrAbort()
{
	if (GameState && GameState->Document)
	{
		GameState->Document->ClearPreviewDeltas(GetWorld(), false);
	}

	return Super::PostEndOrAbort();
}

void UMetaPlaneTool::SetInstanceHeight(const float InHeight)
{
	InstanceHeight = InHeight;

	UpdatePendingPlane();
}

float UMetaPlaneTool::GetInstanceHeight() const
{
	return InstanceHeight;
}

void UMetaPlaneTool::OnAxisConstraintChanged()
{
	Super::OnAxisConstraintChanged();
	UpdatePendingPlane();
}

void UMetaPlaneTool::UpdatePendingPlane()
{
	bPendingSegmentValid = false;
	bPendingPlaneValid = false;

	if (!InUse)
	{
		return;
	}

	auto pendingSegment = DimensionManager ? DimensionManager->GetDimensionActor(PendingSegmentID)->GetLineActor() : nullptr;
	if (State == NewSegmentPending && pendingSegment != nullptr &&
		(FVector::Dist(pendingSegment->Point1, pendingSegment->Point2) >= MinPlaneSize))
	{
		bPendingSegmentValid = true;

		switch (AxisConstraint)
		{
		case EAxisConstraint::None:
			break;
		case EAxisConstraint::AxisZ:
		{
			bPendingPlaneValid = true;
			FVector verticalRectOffset = FVector::UpVector * InstanceHeight;

			PendingPlanePoints = {
				pendingSegment->Point1,
				pendingSegment->Point2,
				pendingSegment->Point2 + verticalRectOffset,
				pendingSegment->Point1 + verticalRectOffset
			};

			break;
		}
		case EAxisConstraint::AxesXY:
		{
			bPendingPlaneValid = !FMath::IsNearlyEqual(pendingSegment->Point1.X, pendingSegment->Point2.X) &&
				!FMath::IsNearlyEqual(pendingSegment->Point1.Y, pendingSegment->Point2.Y) &&
				FMath::IsNearlyEqual(pendingSegment->Point1.Z, pendingSegment->Point2.Z);

			if (bPendingPlaneValid)
			{
				FBox2D pendingRect(ForceInitToZero);
				pendingRect += FVector2D(pendingSegment->Point1);
				pendingRect += FVector2D(pendingSegment->Point2);
				float planeHeight = pendingSegment->Point1.Z;

				PendingPlanePoints = {
					FVector(pendingRect.Min.X, pendingRect.Min.Y, planeHeight),
					FVector(pendingRect.Min.X, pendingRect.Max.Y, planeHeight),
					FVector(pendingRect.Max.X, pendingRect.Max.Y, planeHeight),
					FVector(pendingRect.Max.X, pendingRect.Min.Y, planeHeight),
				};
			}

			break;
		}
		}

		if (bPendingPlaneValid)
		{
			bPendingPlaneValid = UModumateGeometryStatics::GetPlaneFromPoints(PendingPlanePoints, PendingPlaneGeom);
		}
	}

	if (!bPendingPlaneValid)
	{
		PendingPlanePoints.Reset();
	}
}

bool UMetaPlaneTool::ConstrainHitPoint(FVector &hitPoint)
{
	// Free plane sketching is not limited to the sketch plane for the first three points (which establish the sketch plane for subsequent clicks)
	if (AxisConstraint == EAxisConstraint::None && SketchPlanePoints.Num() < 3)
	{
		return false;
	}

	if (Controller != nullptr)
	{
		FVector sketchPlanePoint = Controller->EMPlayerState->SnappedCursor.SketchPlaneProject(hitPoint);
		bool returnValue = !sketchPlanePoint.Equals(hitPoint);
		hitPoint = sketchPlanePoint;
		return returnValue;
	}

	return false;
}

float UMetaPlaneTool::GetDefaultPlaneHeight() const
{
	return Controller->GetDefaultWallHeightFromDoc();
}
