// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Common/ModumateSnappedCursor.h"
#include "UI/EditModelPlayerHUD.h"


FAffordanceFrame::FAffordanceFrame()
	: Origin(ForceInitToZero)
	, Normal(FVector::UpVector)
	, Tangent(ForceInitToZero)
{ }

bool FSnappedCursor::TryMakeAffordanceLineFromCursorToSketchPlane(FAffordanceLine &outAffordance, FVector &outHitPoint) const
{
	FVector projected = SketchPlaneProject(WorldPosition);

	if (!projected.Equals(WorldPosition,0.01f))
	{
		outAffordance.Color = FLinearColor::Blue;
		outAffordance.EndPoint = WorldPosition;
		outAffordance.StartPoint = projected;
		outAffordance.Interval = 4.0f;
		outHitPoint = outAffordance.StartPoint;
		return true;
	}
	return false;
}

FVector FSnappedCursor::SketchPlaneProject(const FVector &p) const
{
	return FVector::PointPlaneProject(p, FPlane(AffordanceFrame.Origin, AffordanceFrame.Normal));
}

bool FSnappedCursor::HasAffordanceSet() const
{
	return bHasCustomAffordance;
}

void FSnappedCursor::SetAffordanceFrame(const FVector &origin, const FVector &normal, const FVector &tangent, bool bInVerticalAffordanceSnap, bool bInSnapGlobalAxes)
{
	AffordanceFrame.Origin = origin;
	AffordanceFrame.Normal = normal.IsZero() ? FVector::UpVector : normal;	// Default to the up-vector so that we always have a valid sketch plane to project against
	AffordanceFrame.Tangent = tangent;
	bHasCustomAffordance = true;
	WantsVerticalAffordanceSnap = bInVerticalAffordanceSnap;
	bSnapGlobalAxes = bInSnapGlobalAxes;
}

void FSnappedCursor::ClearAffordanceFrame()
{
	AffordanceFrame = FAffordanceFrame();
	bHasCustomAffordance = false;
	WantsVerticalAffordanceSnap = false;
	bSnapGlobalAxes = true;
}

bool FSnappedCursor::TryGetRaySketchPlaneIntersection(const FVector &origin, const FVector &direction, FVector &outputPosition) const
{
	// RayPlaneIntersection is not safe, pre-reject parallel projection
	if (!AffordanceFrame.Normal.IsNormalized() || FVector::Orthogonal(direction, AffordanceFrame.Normal))
	{
		return false;
	}

	// First, compute the intersection between the input ray and the sketch plane
	outputPosition = FMath::RayPlaneIntersection(origin, direction, FPlane(AffordanceFrame.Origin,AffordanceFrame.Normal));

	// Then, compensate for division-by-dot-product floating point error by projecting on the plane explicitly
	outputPosition = SketchPlaneProject(outputPosition);

	return true;
}
