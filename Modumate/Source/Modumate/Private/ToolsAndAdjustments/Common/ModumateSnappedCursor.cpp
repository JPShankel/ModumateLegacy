// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "ToolsAndAdjustments/Common/ModumateSnappedCursor.h"
#include "UI/EditModelPlayerHUD.h"

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
	return AffordanceFrame.bHasValue;
}

void FSnappedCursor::SetAffordanceFrame(const FVector &origin, const FVector &normal, const FVector &tangent)
{
	AffordanceFrame.Origin = origin;
	AffordanceFrame.Normal = normal;
	AffordanceFrame.Tangent = tangent;
	AffordanceFrame.bHasValue = true;
}

void FSnappedCursor::ClearAffordanceFrame()
{
	AffordanceFrame.Origin = FVector::ZeroVector;
	AffordanceFrame.Normal = FVector::UpVector;
	AffordanceFrame.Tangent = FVector::ZeroVector;
	AffordanceFrame.bHasValue = false;
}

bool FSnappedCursor::TryGetRaySketchPlaneIntersection(const FVector &origin, const FVector &direction, FVector &outputPosition) const
{
	// RayPlaneIntersection is not safe, pre-reject parallel projection
	if (FVector::Orthogonal(direction, AffordanceFrame.Normal))
	{
		return false;
	}

	// First, compute the intersection between the input ray and the sketch plane
	outputPosition = FMath::RayPlaneIntersection(origin, direction, FPlane(AffordanceFrame.Origin,AffordanceFrame.Normal));

	// Then, compensate for division-by-dot-product floating point error by projecting on the plane explicitly
	outputPosition = SketchPlaneProject(outputPosition);

	return true;
}
