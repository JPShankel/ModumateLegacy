// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Tools/EditModelMetaPlaneTool.h"

#include "Components/EditableTextBox.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
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
	, PendingPlane(nullptr)
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

bool UMetaPlaneTool::HandleInputNumber(double n)
{
	Super::HandleInputNumber(n);

	if (!Controller->EMPlayerState->SnappedCursor.Visible)
	{
		return false;
	}
	NewObjIDs.Reset();

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

		return MakeObject(pendingSegment->Point1 + dir, NewObjIDs);
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

	auto dimensionActor = DimensionManager->AddDimensionActor(APendingSegmentActor::StaticClass());
	PendingSegmentID = dimensionActor->ID;

	auto dimensionWidget = dimensionActor->DimensionText;
	dimensionWidget->Measurement->SetIsReadOnly(false);
	dimensionWidget->Measurement->OnTextCommitted.AddDynamic(this, &UEditModelToolBase::OnTextCommitted);

	GameInstance->DimensionManager->SetActiveActorID(PendingSegmentID);

	auto pendingSegment = dimensionActor->GetLineActor();
	pendingSegment->Point1 = hitLoc;
	pendingSegment->Point2 = hitLoc;
	pendingSegment->Color = FColor::White;
	pendingSegment->Thickness = 2;

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

bool UMetaPlaneTool::EnterNextStage()
{
	Super::EnterNextStage();

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

bool UMetaPlaneTool::MakeObject(const FVector &Location, TArray<int32> &OutNewObjIDs)
{
	FModumateDocument &doc = GameState->Document;

	bool bSuccess = false;
	auto dimensionActor = DimensionManager->GetDimensionActor(PendingSegmentID);
	auto pendingSegment = dimensionActor->GetLineActor();

	FVector constrainedStartPoint = pendingSegment->Point1;
	FVector constrainedEndPoint = Location;
	ConstrainHitPoint(constrainedStartPoint);
	ConstrainHitPoint(constrainedEndPoint);
	OutNewObjIDs.Reset();

	if (State == NewSegmentPending && pendingSegment != nullptr &&
		(FVector::Dist(constrainedStartPoint, constrainedEndPoint) >= MinPlaneSize))
	{
		switch (AxisConstraint)
		{
			case EAxisConstraint::None:
			{
				// The segment-based plane updates the sketch plane on the first three clicks
				FVector segmentDirection = FVector(constrainedEndPoint - constrainedStartPoint).GetSafeNormal();
				FVector currentSketchPlaneNormal(Controller->EMPlayerState->SnappedCursor.AffordanceFrame.Normal);

				FVector tangentDir = !FVector::Parallel(currentSketchPlaneNormal, segmentDirection) ? segmentDirection : Controller->EMPlayerState->SnappedCursor.AffordanceFrame.Tangent;
				bool bSetDefaultAffordance = true;

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
							bSetDefaultAffordance = false;
						}
					}
				}
				// If we've clicked our third point, establish a sketch plane from the three points and then constrain all future points to that plane
				else if (SketchPlanePoints.Num() < 3)
				{
					// If the third point is colinear, skip it and wait for the next one
					FVector firstSegmentDir = (SketchPlanePoints[1] - SketchPlanePoints[0]).GetSafeNormal();
					FVector nextPointDir = (constrainedEndPoint - SketchPlanePoints[0]).GetSafeNormal();

					if (!FVector::Parallel(firstSegmentDir, nextPointDir))
					{
						FVector sketchNormal = (firstSegmentDir ^ nextPointDir).GetSafeNormal();
						// we should not have gotten a degenerate point, ensure
						if (ensureAlways(sketchNormal.IsNormalized()))
						{
							Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(constrainedEndPoint, sketchNormal, tangentDir);
							SketchPlanePoints.Add(constrainedEndPoint);
							bSetDefaultAffordance = false;
						}
					}
				}

				if (bSetDefaultAffordance)
				{
					Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(constrainedEndPoint, currentSketchPlaneNormal, tangentDir);
				}

				AnchorPointDegree = pendingSegment->Point1;
				pendingSegment->Point1 = constrainedEndPoint;
				pendingSegment->Point2 = constrainedEndPoint;

				TArray<FVector> points = { constrainedStartPoint, constrainedEndPoint };

				bSuccess = doc.MakeMetaObject(GetWorld(), points, {}, EObjectType::OTMetaEdge, Controller->EMPlayerState->GetViewGroupObjectID(), OutNewObjIDs);

				break;
			}
			case EAxisConstraint::AxisZ:
			{
				// set end of the segment to the hit location
				pendingSegment->Point2 = constrainedEndPoint;
				UpdatePendingPlane();

				if (bPendingPlaneValid)
				{
					bSuccess = doc.MakeMetaObject(GetWorld(), PendingPlanePoints, {}, EObjectType::OTMetaPlane, Controller->EMPlayerState->GetViewGroupObjectID(), OutNewObjIDs);

					// set up cursor affordance so snapping works on the next chained segment
					Controller->EMPlayerState->SnappedCursor.SetAffordanceFrame(constrainedEndPoint, FVector::UpVector, (pendingSegment->Point1 - pendingSegment->Point2).GetSafeNormal());

					// continue plane creation starting from the hit location
					AnchorPointDegree = pendingSegment->Point1;
					pendingSegment->Point1 = constrainedEndPoint;
				}

				break;
			}
			case EAxisConstraint::AxesXY:
			{
				if (bPendingPlaneValid)
				{
					bSuccess = doc.MakeMetaObject(GetWorld(), PendingPlanePoints, {}, EObjectType::OTMetaPlane, Controller->EMPlayerState->GetViewGroupObjectID(), OutNewObjIDs);

					// Don't chain horizontal planes, so end the tool's use here
					if (bSuccess)
					{
						EndUse();
					}
				}

				break;
			}
		}
	}

	return bSuccess;
}

bool UMetaPlaneTool::FrameUpdate()
{
	Super::FrameUpdate();

	if (!Controller->EMPlayerState->SnappedCursor.Visible)
	{
		return false;
	}

	FVector hitLoc = Controller->EMPlayerState->SnappedCursor.WorldPosition;

	if (State == NewSegmentPending)
	{
		auto dimensionActor = DimensionManager->GetDimensionActor(PendingSegmentID);
		ALineActor *pendingSegment = nullptr;
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
			pendingSegment->UpdateMetaEdgeVisuals(false);
		}

		UpdatePendingPlane();
	}

	return true;
}

bool UMetaPlaneTool::EndUse()
{
	State = Neutral;
	DimensionManager->ReleaseDimensionActor(PendingSegmentID);
	PendingSegmentID = MOD_ID_NONE;

	if (PendingPlane.IsValid())
	{
		PendingPlane->Destroy();
		PendingPlane.Reset();
	}

	Controller->EMPlayerState->SnappedCursor.WantsVerticalAffordanceSnap = false;

	return UEditModelToolBase::EndUse();
}

bool UMetaPlaneTool::AbortUse()
{
	EndUse();
	return UEditModelToolBase::AbortUse();
}

void UMetaPlaneTool::SetAxisConstraint(EAxisConstraint InAxisConstraint)
{
	Super::SetAxisConstraint(InAxisConstraint);

	UpdatePendingPlane();
}

void UMetaPlaneTool::UpdatePendingPlane()
{
	bPendingSegmentValid = false;
	bPendingPlaneValid = false;

	if (PendingPlane.IsValid())
	{
		auto pendingSegment = DimensionManager->GetDimensionActor(PendingSegmentID)->GetLineActor();
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

				FVector verticalRectOffset = FVector::UpVector * Controller->GetDefaultWallHeightFromDoc();

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

		PendingPlane->SetActorHiddenInGame(!bPendingPlaneValid);

		if (bPendingPlaneValid)
		{
			PendingPlane->SetupMetaPlaneGeometry(PendingPlanePoints, PendingPlaneMaterial, PendingPlaneAlpha, true, false);
		}
		else
		{
			PendingPlanePoints.Reset();
		}
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

