#include "DocumentManagement/ModumateSnappingView.h"

#include "DocumentManagement/ModumateDocument.h"
#include "DocumentManagement/ModumateObjectInstance.h"
#include "ModumateCore/ModumateStats.h"
#include "ToolsAndAdjustments/Interface/EditModelToolInterface.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"

DECLARE_CYCLE_STAT(TEXT("Snap-points"), STAT_ModumateSnapPoints, STATGROUP_Modumate)

FModumateSnappingView::FModumateSnappingView(FModumateDocument *document, AEditModelPlayerController_CPP *controller)
	: Document(document)
	, Controller(controller)
{

}

FModumateSnappingView::~FModumateSnappingView()
{

}

void FModumateSnappingView::UpdateSnapPoints(const TSet<int32> &idsToIgnore, int32 collisionChannelMask, bool bForSnapping, bool bForSelection)
{
	SCOPE_CYCLE_COUNTER(STAT_ModumateSnapPoints);
	Corners.Reset();
	LineSegments.Reset();
	SnapIndicesByObjectID.Reset();

	const auto &objects = Document->GetObjectInstances();

	for (const auto *object : objects)
	{
		ECollisionChannel objectCollisionType = UModumateTypeStatics::CollisionTypeFromObjectType(object->GetObjectType());
		bool bObjectInMouseQuery = (collisionChannelMask & ECC_TO_BITFIELD(objectCollisionType)) != 0;

		if (object && object->IsCollisionEnabled() && bObjectInMouseQuery && !idsToIgnore.Contains(object->ID))
		{
			FSnapIndices objectSnapIndices;

			objectSnapIndices.CornerStartIndex = Corners.Num();
			objectSnapIndices.LineStartIndex = LineSegments.Num();

			CurObjCorners.Reset();
			CurObjLineSegments.Reset();

			object->GetStructuralPointsAndLines(CurObjCorners, CurObjLineSegments, bForSnapping, bForSelection);

			objectSnapIndices.NumCorners = CurObjCorners.Num();
			objectSnapIndices.NumLines = CurObjLineSegments.Num();
			Corners.Append(CurObjCorners);
			LineSegments.Append(CurObjLineSegments);

			SnapIndicesByObjectID.Add(object->ID, MoveTemp(objectSnapIndices));
		}
	}

	for (const FStructureLine &ls : LineSegments)
	{
		Corners.Add(FStructurePoint((ls.P1 + ls.P2) * 0.5f, (ls.P2 - ls.P1).GetSafeNormal(), ls.CP1, ls.CP2, ls.ObjID));
	}

	if (!bForSelection)
	{
		// Also ask the current tool
		if (auto tool = Controller->CurrentTool)
		{
			CurrentToolPoints.Reset();
			CurrentToolLines.Reset();
			tool->GetSnappingPointsAndLines(CurrentToolPoints, CurrentToolLines);

			for (FVector point : CurrentToolPoints)
			{
				Corners.Add(FStructurePoint(point, FVector::ZeroVector, INDEX_NONE, INDEX_NONE));
			}

			for (auto& line : CurrentToolLines)
			{
				LineSegments.Add(FStructureLine(line.Key, line.Value, INDEX_NONE, INDEX_NONE));
			}
		}
	}
}

void FModumateSnappingView::GetBoundingBoxPointsAndLines(const FVector &center, const FQuat &rot, const FVector &halfExtents,
	TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, const FVector &OverrideAxis)
{
	FVector forward = rot.GetForwardVector();
	FVector right = rot.GetRightVector();
	FVector up = rot.GetUpVector();

	FVector boxPoints[8] = {
		center + halfExtents.X * forward + halfExtents.Y * right - halfExtents.Z * up,
		center - halfExtents.X * forward + halfExtents.Y * right - halfExtents.Z * up,
		center - halfExtents.X * forward - halfExtents.Y * right - halfExtents.Z * up,
		center + halfExtents.X * forward - halfExtents.Y * right - halfExtents.Z * up,
		center + halfExtents.X * forward + halfExtents.Y * right + halfExtents.Z * up,
		center - halfExtents.X * forward + halfExtents.Y * right + halfExtents.Z * up,
		center - halfExtents.X * forward - halfExtents.Y * right + halfExtents.Z * up,
		center + halfExtents.X * forward - halfExtents.Y * right + halfExtents.Z * up,
	};

	// If supplied an override axis, don't return all of the box's points; just return one side.
	EAxis::Type overrideAxisType;
	float overrideAxisSign;
	if (!OverrideAxis.IsNearlyZero() &&
		UModumateGeometryStatics::GetAxisForVector(OverrideAxis, overrideAxisType, overrideAxisSign))
	{
		// Use fixed indices for each axis:
		static const int32 boxIndicesPerSide[6][4] = {
			{0, 3, 7, 4},	// +X
			{1, 2, 6, 5},	// -X
			{0, 1, 5, 4},	// +Y
			{2, 3, 7, 6},	// -Y
			{4, 5, 6, 7},	// +Z
			{0, 1, 2, 3},	// -Z
		};

		int32 axisIdx = 2 * ((int32)overrideAxisType - 1) + ((overrideAxisSign > 0.0f) ? 0 : 1);

		for (int32 i = 0; i < 4; ++i)
		{
			int32 curBoxIndex = boxIndicesPerSide[axisIdx][i];
			int32 nextBoxIndex = boxIndicesPerSide[axisIdx][(i + 1) % 4];
			const FVector &curBoxPoint = boxPoints[curBoxIndex];
			const FVector &nextBoxPoint = boxPoints[nextBoxIndex];

			outPoints.Add(FStructurePoint(curBoxPoint, FVector::ZeroVector, -1));
			outLines.Add(FStructureLine(curBoxPoint, nextBoxPoint, -1, -1));
		}
	}
	else
	{
		for (const FVector &boxPoint : boxPoints)
		{
			outPoints.Add(FStructurePoint(boxPoint, FVector::ZeroVector, -1));
		}

		for (int32 i = 0; i < 4; ++i)
		{
			int32 iBottom1 = i;
			int32 iBottom2 = (i + 1) % 4;
			int32 iTop1 = iBottom1 + 4;
			int32 iTop2 = iBottom2 + 4;

			outLines.Add(FStructureLine(boxPoints[iBottom1], boxPoints[iBottom2], -1, -1));
			outLines.Add(FStructureLine(boxPoints[iTop1], boxPoints[iTop2], -1, -1));
			outLines.Add(FStructureLine(boxPoints[iBottom1], boxPoints[iTop1], -1, -1));
		}
	}
}

