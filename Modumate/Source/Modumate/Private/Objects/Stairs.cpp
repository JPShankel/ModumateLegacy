// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "Objects/Stairs.h"

#include "Objects/ModumateObjectInstance.h"
#include "ModumateCore/ModumateStairStatics.h"
#include "ModumateCore/ModumateUnits.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "ToolsAndAdjustments/Handles/AdjustPolyEdgeHandle.h"
#include "Drafting/ModumateDraftingElements.h"
#include "Algo/Accumulate.h"

class AEditModelPlayerController;

AMOIStaircase::AMOIStaircase()
	: AModumateObjectInstance()
	, bCachedStartRiser(true)
	, bCachedEndRiser(true)
{
}

bool AMOIStaircase::CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas)
{
	switch (DirtyFlag)
	{
	case EObjectDirtyFlags::Structure:
		SetupDynamicGeometry();
		break;

	case EObjectDirtyFlags::Visuals:
		UpdateVisuals();
		break;

	default:
		break;
	}

	return true;
}

void AMOIStaircase::GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping, bool bForSelection) const
{
	// TODO: use the tread/riser polygons as structural points and lines

	const AModumateObjectInstance *planeParent = GetParentObject();
	if (planeParent)
	{
		planeParent->GetStructuralPointsAndLines(outPoints, outLines, true);
	}
}

void AMOIStaircase::SetupDynamicGeometry()
{
	AModumateObjectInstance* planeParent = GetParentObject();
	if (!(planeParent && (planeParent->GetObjectType() == EObjectType::OTMetaPlane)))
	{
		return;
	}

	TArray<FVector> runPlanePoints;
	for (int32 pointIdx = 0; pointIdx < planeParent->GetNumCorners(); ++pointIdx)
	{
		runPlanePoints.Add(planeParent->GetCorner(pointIdx));
	}

	const auto& assemblySpec = GetAssembly();
	CachedTreadDims.UpdateLayersFromAssembly(assemblySpec.TreadLayers);
	CachedRiserDims.UpdateLayersFromAssembly(assemblySpec.RiserLayers);
	bCachedUseRisers = assemblySpec.RiserLayers.Num() != 0;
	TreadRun = assemblySpec.TreadDepthCentimeters;

	// Tread 'depth' is horizontal run from nose to nose.
	float goalTreadDepth = TreadRun;

	// Calculate the polygons that make up the outer surfaces of each tread and riser of the linear stair run
	float stepRun, stepRise;
	FVector runDir(ForceInitToZero), stairOrigin(ForceInitToZero);
	if (!Modumate::FStairStatics::CalculateLinearRunPolysFromPlane(
		runPlanePoints, goalTreadDepth, bCachedUseRisers, bCachedStartRiser, bCachedEndRiser,
		stepRun, stepRise, runDir, stairOrigin,
		CachedTreadPolys, CachedRiserPolys))
	{
		return;
	}

	const float totalTreadThickness = CachedTreadDims.TotalUnfinishedWidth;
	const float totalRiserThickness = bCachedUseRisers ? CachedRiserDims.TotalUnfinishedWidth : OpenStairsOverhang;
	Modumate::FStairStatics::CalculateSetupStairPolysParams(
		assemblySpec,
		totalTreadThickness, totalRiserThickness, runDir,
		CachedTreadPolys, CachedRiserPolys,
		CachedRiserNormals, TreadLayers, RiserLayers);

	// Set up the triangulated staircase mesh by extruding each tread and riser polygon
	DynamicMeshActor->SetupStairPolys(stairOrigin, CachedTreadPolys, CachedRiserPolys, CachedRiserNormals, TreadLayers, RiserLayers, assemblySpec);
}

void AMOIStaircase::GetDraftingLines(const TSharedPtr<Modumate::FDraftingComposite>& ParentPage, const FPlane& Plane,
	const FVector& AxisX, const FVector& AxisY, const FVector& Origin, const FBox2D& BoundingBox,
	TArray<TArray<FVector>>& OutPerimeters) const
{
	bool bGetFarLines = ParentPage->lineClipping.IsValid();
	if (bGetFarLines)
	{
		GetBeyondLines(ParentPage, Plane, AxisX, AxisY, Origin, BoundingBox);
	}
	else
	{
		GetInPlaneLines(ParentPage, Plane, AxisX, AxisY, Origin, BoundingBox);
	}
}

void AMOIStaircase::GetBeyondLines(const TSharedPtr<Modumate::FDraftingComposite>& ParentPage, const FPlane& Plane,
	const FVector& AxisX, const FVector& AxisY, const FVector& Origin, const FBox2D& BoundingBox) const
{
	static const ModumateUnitParams::FThickness stairLineThickness = ModumateUnitParams::FThickness::Points(0.15f);
	static const Modumate::FMColor lineColor(0.439f, 0.439f, 0.439f);  // Gray112
	static const Modumate::FModumateLayerType dwgLayerType = Modumate::FModumateLayerType::kSeparatorBeyondSurfaceEdges;

	float treadThickness = CachedTreadDims.TotalUnfinishedWidth;
	float riserThickness = bCachedUseRisers ? CachedRiserDims.TotalUnfinishedWidth : OpenStairsOverhang;

	TArray<FEdge> beyondPlaneLines;
	FVector location = DynamicMeshActor->GetActorLocation();

	bool bProcessingRisers = bCachedUseRisers;
	int32 riserNumber = 0;
	auto treadsList = { &CachedTreadPolys };
	auto treadsRisersList = { &CachedRiserPolys, &CachedTreadPolys };
	auto& stairItemsList = bCachedUseRisers ? treadsRisersList : treadsList;

	for (const auto* componentPolys: stairItemsList)
	{
		for (const auto& componentPoly: *componentPolys)
		{
			const int32 numCorners = componentPoly.Num();
			TArray<FVector> points = componentPoly;

			if (componentPoly.Num() == 4)
			{
				if (bProcessingRisers)
				{   // Don't overlap treads & risers;
					points[2] -= treadThickness * FVector::UpVector;
					points[3] -= treadThickness * FVector::UpVector;
				}
				else
				{
					FVector riserNormal = (points[2] - points[1]).GetUnsafeNormal();
					points[2] += riserThickness * riserNormal;
					points[3] += riserThickness * riserNormal;
				}
			}

			FVector thicknessDelta = bProcessingRisers ? riserThickness * CachedRiserNormals[riserNumber++] : treadThickness * -FVector::UpVector;
			for (int32 p = 0; p < numCorners; ++p)
			{
				beyondPlaneLines.Add({ points[p], points[(p + 1) % numCorners] });
				beyondPlaneLines.Add({ points[p] + thicknessDelta, points[(p + 1) % numCorners] + thicknessDelta });
				beyondPlaneLines.Add({ points[p], points[p] + thicknessDelta });
			}

		}
		bProcessingRisers = false;
	}

	for (const auto& line : beyondPlaneLines)
	{
		FVector v0 = line.Vertex[0] + location;
		FVector v1 = line.Vertex[1] + location;

		TArray<FEdge> clippedLineSections = ParentPage->lineClipping->ClipWorldLineToView({ v0, v1 });
		for (const auto& lineSection : clippedLineSections)
		{
			FVector2D vert0(lineSection.Vertex[0]);
			FVector2D vert1(lineSection.Vertex[1]);

			FVector2D boxClipped0;
			FVector2D boxClipped1;

			if (UModumateFunctionLibrary::ClipLine2DToRectangle(vert0, vert1, BoundingBox, boxClipped0, boxClipped1))
			{
				TSharedPtr<Modumate::FDraftingLine> draftingLine = MakeShared<Modumate::FDraftingLine>(
					FModumateUnitCoord2D::WorldCentimeters(boxClipped0),
					FModumateUnitCoord2D::WorldCentimeters(boxClipped1),
					stairLineThickness, lineColor);
				ParentPage->Children.Add(draftingLine);
				draftingLine->SetLayerTypeRecursive(dwgLayerType);
			}
		}
	}

}

void AMOIStaircase::GetInPlaneLines(const TSharedPtr<Modumate::FDraftingComposite>& ParentPage, const FPlane& Plane,
	const FVector& AxisX, const FVector& AxisY, const FVector& Origin, const FBox2D& BoundingBox) const
{
	static const ModumateUnitParams::FThickness lineThickness = ModumateUnitParams::FThickness::Points(0.25f);
	static const Modumate::FMColor lineColor = Modumate::FMColor::Gray96;
	static const Modumate::FModumateLayerType dwgLayerType = Modumate::FModumateLayerType::kSeparatorCutOuterSurface;

	float treadThickness = CachedTreadDims.TotalUnfinishedWidth;
	float riserThickness = bCachedUseRisers ? CachedRiserDims.TotalUnfinishedWidth : OpenStairsOverhang;

	TArray<FEdge> inPlaneLines;
	FVector location = DynamicMeshActor->GetActorLocation();

	bool bProcessingRisers = bCachedUseRisers;
	float treadExtension = bCachedUseRisers ? riserThickness : OpenStairsOverhang;
	int32 riserNumber = 0;
	auto treadsList = { &CachedTreadPolys };
	auto treadsRisersList = { &CachedRiserPolys, &CachedTreadPolys };
	auto& stairItemsList = bCachedUseRisers ? treadsRisersList : treadsList;

	for (const auto* componentPolys: stairItemsList)
	{
		for (const auto& componentPoly: *componentPolys)
		{
			TArray<FVector> pointsA = componentPoly;
			TArray<FVector> pointsB;

			FVector thicknessDelta = bProcessingRisers ? riserThickness * CachedRiserNormals[riserNumber++] : treadThickness * -FVector::UpVector;

			if (componentPoly.Num() == 4)
			{
				if (bProcessingRisers)
				{   // Don't overlap treads & risers;
					pointsA[2] -= treadThickness * FVector::UpVector;
					pointsA[3] -= treadThickness * FVector::UpVector;
				}
				else
				{
					FVector riserNormal = (pointsA[2] - pointsA[1]).GetUnsafeNormal();
					pointsA[2] += treadExtension * riserNormal;
					pointsA[3] += treadExtension * riserNormal;
				}
			}

			for (auto& p: pointsA)
			{
				p += location;
				pointsB.Add(p + thicknessDelta);
			}

			FBox itemBounds(pointsA);
			itemBounds += pointsB;
			if (FMath::PlaneAABBIntersection(Plane, itemBounds))
			{
				TArray<FVector> intersectPoints;
				const int32 numPoints = pointsA.Num();
				for (int32 p = 0; p < numPoints; ++p)
				{
					FVector intersection;
					if (FMath::SegmentPlaneIntersection(pointsA[p], pointsB[p], Plane, intersection))
					{
						intersectPoints.Add(intersection);
					}
					if (FMath::SegmentPlaneIntersection(pointsA[p], pointsA[(p + 1) % numPoints], Plane, intersection))
					{
						intersectPoints.Add(intersection);
					}
					if (FMath::SegmentPlaneIntersection(pointsB[p], pointsB[(p + 1) % numPoints], Plane, intersection))
					{
						intersectPoints.Add(intersection);
					}
				}
				if (intersectPoints.Num() != 0)
				{
					TArray<FVector2D> projectedPoints;
					for (const auto& point3d: intersectPoints)
					{
						projectedPoints.Add(UModumateGeometryStatics::ProjectPoint2D(Origin, -AxisX, -AxisY, point3d));
					}
					TArray<int32> indices;
					ConvexHull2D::ComputeConvexHull(projectedPoints, indices);
					const int32 numHullPoints = indices.Num();
					for (int32 p = 0; p < numHullPoints; ++p)
					{
						inPlaneLines.Add(FEdge(FVector(projectedPoints[indices[p]], 0),
							FVector(projectedPoints[indices[(p + 1) % numHullPoints]], 0)));
					}
				}
			}
		}
		bProcessingRisers = false;
	}

	for (const auto& line : inPlaneLines)
	{
		FVector2D start(line.Vertex[0]);
		FVector2D end(line.Vertex[1]);
		FVector2D clippedStart, clippedEnd;

		if (UModumateFunctionLibrary::ClipLine2DToRectangle(start, end, BoundingBox, clippedStart, clippedEnd))
		{
			TSharedPtr<Modumate::FDraftingLine> draftingLine = MakeShared<Modumate::FDraftingLine>(
				FModumateUnitCoord2D::WorldCentimeters(clippedStart),
				FModumateUnitCoord2D::WorldCentimeters(clippedEnd),
				lineThickness, lineColor);
			ParentPage->Children.Add(draftingLine);
			draftingLine->SetLayerTypeRecursive(dwgLayerType);
		}
	}
}

void AMOIStaircase::SetupAdjustmentHandles(AEditModelPlayerController *controller)
{
	AModumateObjectInstance *parent = GetParentObject();
	if (!ensureAlways(parent && (parent->GetObjectType() == EObjectType::OTMetaPlane)))
	{
		return;
	}

	// Make the polygon adjustment handles, for modifying the parent plane's polygonal shape
	int32 numCorners = parent->GetNumCorners();
	for (int32 i = 0; i < numCorners; ++i)
	{
		auto edgeHandle = MakeHandle<AAdjustPolyEdgeHandle>();
		edgeHandle->SetTargetIndex(i);
		edgeHandle->SetTargetMOI(parent);
	}
}
