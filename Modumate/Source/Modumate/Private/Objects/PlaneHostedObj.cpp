// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "Objects/PlaneHostedObj.h"

#include "Algo/Transform.h"
#include "Algo/Accumulate.h"

#include "DocumentManagement/ModumateDocument.h"
#include "Objects/ModumateObjectInstance.h"
#include "Drafting/ModumateDraftingElements.h"
#include "Graph/Graph3D.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "ModumateCore/ModumateMitering.h"
#include "ToolsAndAdjustments/Common/AdjustmentHandleActor.h"
#include "ToolsAndAdjustments/Handles/AdjustInvertHandle.h"
#include "ToolsAndAdjustments/Handles/AdjustPolyPointHandle.h"
#include "ToolsAndAdjustments/Handles/JustificationHandle.h"
#include "ToolsAndAdjustments/Handles/EditModelPortalAdjustmentHandles.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"


class AEditModelPlayerController_CPP;

using namespace Modumate::Mitering;

FMOIPlaneHostedObjImpl::FMOIPlaneHostedObjImpl(FModumateObjectInstance *InMOI)
	: FModumateObjectInstanceImplBase(InMOI)
{
	CachedLayerDims.UpdateLayersFromAssembly(MOI->GetAssembly());
	CachedLayerDims.UpdateFinishFromObject(MOI);
}

FMOIPlaneHostedObjImpl::~FMOIPlaneHostedObjImpl()
{
}

FQuat FMOIPlaneHostedObjImpl::GetRotation() const
{
	const FModumateObjectInstance *parent = MOI->GetParentObject();
	if (ensure(parent && (parent->GetObjectType() == EObjectType::OTMetaPlane)))
	{
		return parent->GetObjectRotation();
	}
	else
	{
		return FQuat::Identity;
	}
}

FVector FMOIPlaneHostedObjImpl::GetLocation() const
{
	const FModumateObjectInstance *parent = MOI->GetParentObject();
	if (ensure(parent && (parent->GetObjectType() == EObjectType::OTMetaPlane)))
	{
		float thickness = MOI->CalculateThickness();
		FVector planeNormal = parent->GetNormal();
		FVector planeLocation = parent->GetObjectLocation();
		float offsetPct = MOI->GetExtents().X;
		return planeLocation + ((0.5f - offsetPct) * thickness * planeNormal);
	}
	else
	{
		return FVector::ZeroVector;
	}
}

FVector FMOIPlaneHostedObjImpl::GetCorner(int32 CornerIndex) const
{
	// Handle the meta plane host case which just returns the meta plane with this MOI's offset
	const FModumateObjectInstance *parent = MOI->GetParentObject();
	if (ensure(parent && (parent->GetObjectType() == EObjectType::OTMetaPlane)))
	{
		int32 numPlanePoints = parent->GetNumCorners();
		bool bOnStartingSide = (CornerIndex < numPlanePoints);
		int32 numLayers = LayerGeometries.Num();
		int32 pointIndex = CornerIndex % numPlanePoints;

		if (ensure((numLayers == MOI->GetAssembly().Layers.Num()) && numLayers > 0))
		{
			auto& layerPoints = bOnStartingSide ? LayerGeometries[0].PointsA : LayerGeometries[numLayers - 1].PointsB;

			if (ensure(numPlanePoints == layerPoints.Num()))
			{
				return parent->GetObjectLocation() + layerPoints[pointIndex];
			}
		}
	}

	return GetLocation();
}

FVector FMOIPlaneHostedObjImpl::GetNormal() const
{
	const FModumateObjectInstance *planeParent = MOI->GetParentObject();
	if (ensureAlways(planeParent && (planeParent->GetObjectType() == EObjectType::OTMetaPlane)))
	{
		return planeParent->GetNormal();
	}
	else
	{
		return FVector::ZeroVector;
	}
}

void FMOIPlaneHostedObjImpl::Destroy()
{
	MarkEdgesMiterDirty();
}

bool FMOIPlaneHostedObjImpl::CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas)
{
	switch (DirtyFlag)
	{
	case EObjectDirtyFlags::Structure:
	{
		// TODO: as long as the assembly is not stored inside of the data state, and its layers can be reversed,
		// then this is the centralized opportunity to match up the reversal of layers with whatever the intended inversion state is,
		// based on preview/current state changing, assembly changing, object creation, etc.
		MOI->SetAssemblyLayersReversed(MOI->GetObjectInverted());

		CachedLayerDims.UpdateLayersFromAssembly(MOI->GetAssembly());
		CachedLayerDims.UpdateFinishFromObject(MOI);

		// When structure (assembly, offset, or plane structure) changes, mark neighboring
		// edges as miter-dirty, so they can re-evaluate mitering with our new structure.
		MOI->MarkDirty(EObjectDirtyFlags::Mitering);
		MarkEdgesMiterDirty();
	}
	break;
	case EObjectDirtyFlags::Mitering:
	{
		// Make sure all connected edges have resolved mitering,
		// so we can generate our own mitered mesh correctly.
		for (FModumateObjectInstance *connectedEdge : CachedConnectedEdges)
		{
			if (connectedEdge->IsDirty(EObjectDirtyFlags::Mitering))
			{
				return false;
			}
		}

		SetupDynamicGeometry();
	}
	break;
	case EObjectDirtyFlags::Visuals:
	{
		MOI->UpdateVisibilityAndCollision();
		UpdateMeshWithLayers(false, false);
	}
	break;
	}

	return true;
}

void FMOIPlaneHostedObjImpl::SetupDynamicGeometry()
{
	bGeometryDirty = true;
	InternalUpdateGeometry();
}

void FMOIPlaneHostedObjImpl::UpdateDynamicGeometry()
{
	// If our parent metaplane is being adjusted, then just update our own mesh to match rather than derive points from the doc graph
	const FModumateObjectInstance *parent = MOI->GetParentObject();
	if (ensure(parent != nullptr) && parent->GetIsInPreviewMode())
	{
		UpdateMeshWithLayers(false, true);
	}
	else
	{
		InternalUpdateGeometry();
	}
}

void FMOIPlaneHostedObjImpl::GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping, bool bForSelection) const
{
	const FModumateObjectInstance *parent = MOI->GetParentObject();

	if (ensure(parent && (parent->GetObjectType() == EObjectType::OTMetaPlane)))
	{
		int32 numPlanePoints = parent->GetNumCorners();

		for (int32 i = 0; i < numPlanePoints; ++i)
		{
			int32 edgeIdxA = i;
			int32 edgeIdxB = (i + 1) % numPlanePoints;

			FVector cornerMinA = GetCorner(edgeIdxA);
			FVector cornerMinB = GetCorner(edgeIdxB);
			FVector edgeDir = (cornerMinB - cornerMinA).GetSafeNormal();

			FVector cornerMaxA = GetCorner(edgeIdxA + numPlanePoints);
			FVector cornerMaxB = GetCorner(edgeIdxB + numPlanePoints);

			outPoints.Add(FStructurePoint(cornerMinA, edgeDir, edgeIdxA));

			outLines.Add(FStructureLine(cornerMinA, cornerMinB, edgeIdxA, edgeIdxB));
			outLines.Add(FStructureLine(cornerMaxA, cornerMaxB, edgeIdxA + numPlanePoints, edgeIdxB + numPlanePoints));
			outLines.Add(FStructureLine(cornerMinA, cornerMaxA, edgeIdxA, edgeIdxA + numPlanePoints));
		}
	}
}

void FMOIPlaneHostedObjImpl::InternalUpdateGeometry()
{
	FModumateObjectInstance *parent = MOI->GetParentObject();
	if (!(parent && (parent->GetObjectType() == EObjectType::OTMetaPlane)))
	{
		return;
	}

	if (bGeometryDirty)
	{
		UpdateMeshWithLayers(false, true);

		TArray<FModumateObjectInstance*> children = MOI->GetChildObjects();
		for (auto *child : children)
		{
			child->MarkDirty(EObjectDirtyFlags::Structure);
		}

		CachedLayerDims.UpdateFinishFromObject(MOI);
	}
	else
	{
		UpdateMeshWithLayers(false, true);
	}

	bGeometryDirty = false;
}

void FMOIPlaneHostedObjImpl::SetupAdjustmentHandles(AEditModelPlayerController_CPP *controller)
{
	FModumateObjectInstance *parent = MOI->GetParentObject();
	if (!ensureAlways(parent && (parent->GetObjectType() == EObjectType::OTMetaPlane)))
	{
		return;
	}

	// Make the polygon adjustment handles, for modifying the parent plane's polygonal shape
	int32 numCorners = parent->GetNumCorners();
	for (int32 i = 0; i < numCorners; ++i)
	{
		// Don't allow adjusting wall corners, since they're more likely to be edited edge-by-edge.
		if (MOI->GetObjectType() != EObjectType::OTWallSegment)
		{
			auto cornerHandle = MOI->MakeHandle<AAdjustPolyPointHandle>();
			cornerHandle->SetTargetIndex(i);
			cornerHandle->SetTargetMOI(parent);
		}

		auto edgeHandle = MOI->MakeHandle<AAdjustPolyPointHandle>();
		edgeHandle->SetTargetIndex(i);
		edgeHandle->SetAdjustPolyEdge(true);
		edgeHandle->SetTargetMOI(parent);
	}

	// Make justification handles
	TArray<AAdjustmentHandleActor*> rootHandleChildren;

	auto frontJustificationHandle = MOI->MakeHandle<AJustificationHandle>();
	frontJustificationHandle->SetJustification(0.0f);
	rootHandleChildren.Add(frontJustificationHandle);

	auto backJustificationHandle = MOI->MakeHandle<AJustificationHandle>();
	backJustificationHandle->SetJustification(1.0f);
	rootHandleChildren.Add(backJustificationHandle);

	// Make the invert handle, as a child of the justification handle root
	auto invertHandle = MOI->MakeHandle<AAdjustInvertHandle>();
	rootHandleChildren.Add(invertHandle);

	auto rootJustificationHandle = MOI->MakeHandle<AJustificationHandle>();
	rootJustificationHandle->SetJustification(0.5f);
	rootJustificationHandle->HandleChildren = rootHandleChildren;
	for (auto& child : rootHandleChildren)
	{
		child->HandleParent = rootJustificationHandle;
	}
}

void FMOIPlaneHostedObjImpl::OnSelected(bool bNewSelected)
{
	FModumateObjectInstanceImplBase::OnSelected(bNewSelected);

	if (const FModumateObjectInstance *parent = MOI ? MOI->GetParentObject() : nullptr)
	{
		TArray<FModumateObjectInstance *> connectedMOIs;
		parent->GetConnectedMOIs(connectedMOIs);
		for (FModumateObjectInstance *connectedMOI : connectedMOIs)
		{
			connectedMOI->UpdateVisibilityAndCollision();
		}
	}
}

void FMOIPlaneHostedObjImpl::GetPlaneIntersections(TArray<FVector> &OutIntersections, const TArray<FVector> &InPoints, const FPlane &InPlane) const
{
	const FModumateObjectInstance *parent = MOI->GetParentObject();
	const FVector parentLocation = parent->GetObjectLocation();

	int32 numPoints = InPoints.Num();
	for (int32 pointIdx = 0; pointIdx < numPoints; pointIdx++)
	{
		FVector layerPoint1 = InPoints[pointIdx] + parentLocation;
		FVector layerPoint2 = InPoints[(pointIdx + 1) % numPoints] + parentLocation;

		FVector point;
		if (FMath::SegmentPlaneIntersection(layerPoint1, layerPoint2, InPlane, point))
		{
			OutIntersections.Add(point);
		}
	}
}

bool FMOIPlaneHostedObjImpl::GetRangesForHolesOnPlane(TArray<TPair<float, float>> &OutRanges, TPair<FVector, FVector> &Intersection, const FLayerGeomDef &Layer, const float CurrentThickness, const FPlane &Plane, const FVector &AxisX, const FVector &AxisY, const FVector &Origin) const
{
	FVector intersectionStart = Intersection.Key;
	FVector intersectionEnd = Intersection.Value;

	// TODO: only works when the plane goes through the object once (2 intersections)
	FVector2D start = UModumateGeometryStatics::ProjectPoint2D(Origin, AxisX, AxisY, intersectionStart);
	FVector2D end = UModumateGeometryStatics::ProjectPoint2D(Origin, AxisX, AxisY, intersectionEnd);
	float length = (end - start).Size();

	TArray<TPair<float, float>> holeRanges;
	TArray<TPair<float, float>> mergedRanges;
	int32 idx = 0;

	// intersect holes with the plane, and figure out the ranges along the original line where there is a valid hole
	for (auto& hole : Layer.Holes3D)
	{
		if (!Layer.HolesValid[idx])
		{
			continue;
		}
		TArray<FVector> projectedHolePoints;
		for (auto& point : hole.Points)
		{
			// TODO: cache this value
			projectedHolePoints.Add(point + GetNormal() * CurrentThickness);
		}

		// TODO: once there is potential for concave holes, generalize the sorter in GetDraftingLines and sort here as well
		TArray<FVector> holeIntersections;
		GetPlaneIntersections(holeIntersections, projectedHolePoints, Plane);

		if (holeIntersections.Num() != 2)
		{
			continue;
		}
		FVector2D holeStart = UModumateGeometryStatics::ProjectPoint2D(Origin, AxisX, AxisY, holeIntersections[0]);
		FVector2D holeEnd = UModumateGeometryStatics::ProjectPoint2D(Origin, AxisX, AxisY, holeIntersections[1]);

		float hs = (holeStart - start).Size() / length;
		float he = (holeEnd - start).Size() / length;
		if (hs > he)
		{
			Swap(hs, he);
		}
		holeRanges.Add(TPair<float, float>(hs, he));
		idx++;
	}

	holeRanges.Sort();

	//TODO: merging the ranges may be unnecessary because it seems like if holes overlap, one of them will be invalid
	for (auto& range : holeRanges)
	{
		if (mergedRanges.Num() == 0)
		{
			mergedRanges.Push(range);
			continue;
		}
		auto& currentRange = mergedRanges.Top();
		// if the sorted ranges overlap, combine them
		if (range.Value > currentRange.Value && range.Key < currentRange.Value)
		{
			mergedRanges.Top() = TPair<float, float>(currentRange.Key, range.Value);
		}
		else if (range.Key > currentRange.Value)
		{
			mergedRanges.Push(range);
		}
	}

	// lines for drafting are drawn in the areas where there aren't holes, 
	// so OutRanges inverts the merged ranges (which is where there are holes)
	if (mergedRanges.Num() == 0)
	{
		OutRanges.Add(TPair<float, float>(0, 1));
	}
	else
	{
		OutRanges.Add(TPair<float, float>(0, mergedRanges[0].Key));
		for (int32 rangeIdx = 0; rangeIdx < mergedRanges.Num() - 1; rangeIdx++)
		{
			OutRanges.Add(TPair<float, float>(mergedRanges[rangeIdx].Value, mergedRanges[rangeIdx + 1].Key));
		}
		OutRanges.Add(TPair<float, float>(mergedRanges[mergedRanges.Num()-1].Value, 1));
	}

	return true;
}

void FMOIPlaneHostedObjImpl::GetDraftingLines(const TSharedPtr<Modumate::FDraftingComposite> &ParentPage, const FPlane &Plane, const FVector &AxisX, const FVector &AxisY, const FVector &Origin, const FBox2D &BoundingBox, TArray<TArray<FVector>> &OutPerimeters) const
{
	OutPerimeters.Reset();

	Modumate::Units::FThickness innerThickness = Modumate::Units::FThickness::Points(0.125f);
	Modumate::Units::FThickness structureThickness = Modumate::Units::FThickness::Points(0.375f);
	Modumate::Units::FThickness outerThickness = Modumate::Units::FThickness::Points(0.25f);

	Modumate::FMColor innerColor = Modumate::FMColor::Gray64;
	Modumate::FMColor outerColor = Modumate::FMColor::Gray32;
	Modumate::FMColor structureColor = Modumate::FMColor::Black;

	// this sorter is used to sort the intersections between a layer's points and a plane.
	// assuming the points in a layer are planar, all intersections will fall on the same line.
	// since the intersections are on a line, we can sort the vectors simply and know the intersections 
	// are ordered such that adjacent pairs of intersections represent a line segment going through the layer polygon
	auto sorter = [this](const FVector &rhs, const FVector &lhs) {
		if (!FMath::IsNearlyEqual(rhs.X, lhs.X, KINDA_SMALL_NUMBER))
		{
			return rhs.X > lhs.X;
		}
		else if (!FMath::IsNearlyEqual(rhs.Y, lhs.Y, KINDA_SMALL_NUMBER))
		{
			return rhs.Y > lhs.Y;
		}
		else if (!FMath::IsNearlyEqual(rhs.Z, lhs.Z, KINDA_SMALL_NUMBER))
		{
			return rhs.Z > lhs.Z;
		}
		return true;
	};

	bool bGetFarLines = ParentPage->lineClipping.IsValid();
	if (!bGetFarLines)
	{
		const FModumateObjectInstance *parent = MOI->GetParentObject();
		FVector parentLocation = parent->GetObjectLocation();

		float currentThickness = 0.0f;
		TArray<FVector2D> previousLinePoints;
		int32 numLines = LayerGeometries.Num() + 1;
		for (int32 layerIdx = 0; layerIdx < numLines; layerIdx++)
		{
			bool usePointsA = layerIdx < LayerGeometries.Num();
			auto& layer = usePointsA ? LayerGeometries[layerIdx] : LayerGeometries[layerIdx - 1];

			auto dwgLayerType = Modumate::FModumateLayerType::kSeparatorCutMinorLayer;
			if (layerIdx == 0 || layerIdx == LayerGeometries.Num())
			{
				dwgLayerType = Modumate::FModumateLayerType::kSeparatorCutOuterSurface;
			}

			TArray<FVector> intersections;
			GetPlaneIntersections(intersections, usePointsA ? layer.PointsA : layer.PointsB, Plane);

			intersections.Sort(sorter);
			// we can make mask perimeters when there are an even amount of intersection between a simple polygon and a plane
			bool bMakeMaskPerimeter = (intersections.Num() % 2 == 0);

			Modumate::Units::FThickness lineThickness;
			Modumate::FMColor lineColor = innerColor;
			if (FMath::IsNearlyEqual(currentThickness, CachedLayerDims.StructureWidthStart) ||
				FMath::IsNearlyEqual(currentThickness, CachedLayerDims.StructureWidthEnd, KINDA_SMALL_NUMBER))
			{
				lineThickness = structureThickness;
				lineColor = structureColor;
			}
			// TODO: currently, finishes are not rendered in drawings.  Given finish lines are implemented,
			// the wall needs to check whether there is a finish on the outside before choosing to use the
			// outer layer weight as opposed to the inner layer weight.
			// Finish lines are not considered structural, they are all inner except for the outermost layer
			else if (layerIdx == 0 || layerIdx == LayerGeometries.Num())
			{
				lineThickness = outerThickness;
				lineColor = outerColor;
			}
			else
			{
				lineThickness = innerThickness;
				lineColor = innerColor;
			}

			for (int32 idx = 0; idx < intersections.Num() - 1; idx += 2)
			{
				// find vertices for the stencil masks, to hide geometry behind the object from the edge detection custom shader
				if (bMakeMaskPerimeter)
				{
					if (layerIdx == 0)
					{
						OutPerimeters.Add(TArray<FVector>());
						OutPerimeters[idx / 2].Add(intersections[idx]);
						OutPerimeters[idx / 2].Add(intersections[idx + 1]);
					}
					else if (layerIdx == numLines - 1)
					{
						// TODO: remove this constraint, potentially need to figure out where the ray would intersect with the plane
						// sometimes only some of the layers of a wall intersect the plane, resulting in a discrepancy in the amount 
						// of intersection points.  Currently, the mask geometry expects 4 points
						if (intersections.Num() / 2 == OutPerimeters.Num())
						{
							OutPerimeters[idx / 2].Add(intersections[idx + 1]);
							OutPerimeters[idx / 2].Add(intersections[idx]);
						}
					}
				}

				TArray<TPair<float, float>> lineRanges;
				FVector intersectionStart = intersections[idx];
				FVector intersectionEnd = intersections[idx + 1];
				TPair<FVector, FVector> currentIntersection = TPair<FVector, FVector>(intersectionStart, intersectionEnd);
				GetRangesForHolesOnPlane(lineRanges, currentIntersection, layer, currentThickness, Plane, -AxisX, -AxisY, Origin);

				// TODO: unclear why the axes need to be flipped here, could be because of the different implementation of finding intersections
				FVector2D start = UModumateGeometryStatics::ProjectPoint2D(Origin, -AxisX, -AxisY, intersectionStart);
				FVector2D end = UModumateGeometryStatics::ProjectPoint2D(Origin, -AxisX, -AxisY, intersectionEnd);
				FVector2D delta = end - start;

				ParentPage->inPlaneLines.Emplace(FVector(start, 0), FVector(end, 0));

				int32 linePoint = 0;
				for (auto& range : lineRanges)
				{
					FVector2D clippedStart, clippedEnd;
					FVector2D rangeStart = start + delta * range.Key;
					FVector2D rangeEnd = start + delta * range.Value;

					if (UModumateFunctionLibrary::ClipLine2DToRectangle(rangeStart, rangeEnd, BoundingBox, clippedStart, clippedEnd))
					{
						TSharedPtr<Modumate::FDraftingLine> line = MakeShared<Modumate::FDraftingLine>(
							Modumate::Units::FCoordinates2D::WorldCentimeters(clippedStart),
							Modumate::Units::FCoordinates2D::WorldCentimeters(clippedEnd),
							lineThickness, lineColor);
						ParentPage->Children.Add(line);
						line->SetLayerTypeRecursive(dwgLayerType);
					}
					if (previousLinePoints.Num() > linePoint)
					{
						if (UModumateFunctionLibrary::ClipLine2DToRectangle(previousLinePoints[linePoint], rangeStart, BoundingBox, clippedStart, clippedEnd))
						{
							TSharedPtr<Modumate::FDraftingLine> line = MakeShared<Modumate::FDraftingLine>(
								Modumate::Units::FCoordinates2D::WorldCentimeters(clippedStart),
								Modumate::Units::FCoordinates2D::WorldCentimeters(clippedEnd),
								lineThickness, lineColor);
							ParentPage->Children.Add(line);
							line->SetLayerTypeRecursive(Modumate::FModumateLayerType::kSeparatorCutOuterSurface);
						}
						if (UModumateFunctionLibrary::ClipLine2DToRectangle(previousLinePoints[linePoint+1], rangeEnd, BoundingBox, clippedStart, clippedEnd))
						{
							TSharedPtr<Modumate::FDraftingLine> line = MakeShared<Modumate::FDraftingLine>(
								Modumate::Units::FCoordinates2D::WorldCentimeters(clippedStart),
								Modumate::Units::FCoordinates2D::WorldCentimeters(clippedEnd),
								lineThickness, lineColor);
							ParentPage->Children.Add(line);
							line->SetLayerTypeRecursive(Modumate::FModumateLayerType::kSeparatorCutOuterSurface);
						}
					}
					previousLinePoints.SetNum(FMath::Max(linePoint + 2, previousLinePoints.Num()) );
					previousLinePoints[linePoint] = rangeStart;
					previousLinePoints[linePoint+1] = rangeEnd;
					linePoint += 2;
				}

			}
			currentThickness += layer.Thickness;

		}
	}
	else  // Get far lines.
	{
		GetBeyondDraftingLines(ParentPage, Plane, BoundingBox);
	}
}

void FMOIPlaneHostedObjImpl::UpdateMeshWithLayers(bool bRecreateMesh, bool bRecalculateEdgeExtensions)
{
	const FModumateDocument *doc = MOI ? MOI->GetDocument() : nullptr;
	if (!ensureAlwaysMsgf(doc, TEXT("Tried to update invalid plane-hosted object!")))
	{
		return;
	}

	int32 parentID = MOI->GetParentID();
	const Modumate::FGraph3DFace *planeFace = doc->GetVolumeGraph().FindFace(parentID);
	const FModumateObjectInstance *parentPlane = doc->GetObjectById(parentID);
	if (!ensureMsgf(parentPlane, TEXT("Plane-hosted object (ID %d) is missing parent object (ID %d)!"), MOI->ID, parentID) ||
		!ensureMsgf(planeFace, TEXT("Plane-hosted object (ID %d) is missing parent graph face (ID %d)!"), MOI->ID, parentID))
	{
		return;
	}

	DynamicMeshActor->SetActorLocation(parentPlane->GetObjectLocation());
	DynamicMeshActor->SetActorRotation(FQuat::Identity);

	CachedHoles.Reset();
	for (auto& hole : planeFace->CachedHoles)
	{
		TempHoleRelativePoints.Reset();
		Algo::Transform(hole.Points, TempHoleRelativePoints, [planeFace](const FVector &worldPoint) { return worldPoint - planeFace->CachedCenter; });
		CachedHoles.Add(FPolyHole3D(TempHoleRelativePoints));
	}

	if (!FMiterHelpers::UpdateMiteredLayerGeoms(MOI, planeFace, &CachedHoles, LayerGeometries))
	{
		return;
	}

	DynamicMeshActor->Assembly = MOI->GetAssembly();
	DynamicMeshActor->LayerGeometries = LayerGeometries;

	bool bEnableCollision = !MOI->GetIsInPreviewMode() && !parentPlane->GetIsInPreviewMode();
	DynamicMeshActor->UpdatePlaneHostedMesh(bRecreateMesh, bEnableCollision, bEnableCollision);
}

void FMOIPlaneHostedObjImpl::UpdateConnectedEdges()
{
	CachedParentConnectedMOIs.Reset();

	const FModumateObjectInstance *planeParent = MOI ? MOI->GetParentObject() : nullptr;
	if (planeParent)
	{
		planeParent->GetConnectedMOIs(CachedParentConnectedMOIs);
	}

	CachedConnectedEdges.Reset();
	for (FModumateObjectInstance *planeConnectedMOI : CachedParentConnectedMOIs)
	{
		if (planeConnectedMOI && (planeConnectedMOI->GetObjectType() == EObjectType::OTMetaEdge))
		{
			CachedConnectedEdges.Add(planeConnectedMOI);
		}
	}
}

void FMOIPlaneHostedObjImpl::MarkEdgesMiterDirty()
{
	UpdateConnectedEdges();
	for (FModumateObjectInstance *connectedEdge : CachedConnectedEdges)
	{
		connectedEdge->MarkDirty(EObjectDirtyFlags::Mitering);
	}
}

void FMOIPlaneHostedObjImpl::GetBeyondDraftingLines(const TSharedPtr<Modumate::FDraftingComposite>& ParentPage, const FPlane& Plane,
	const FBox2D& BoundingBox) const
{
	static const Modumate::Units::FThickness outerThickness = Modumate::Units::FThickness::Points(0.25f);

	const FModumateObjectInstance *parent = MOI->GetParentObject();
	FVector parentLocation = parent->GetObjectLocation();

	TArray<TPair<FEdge, Modumate::FModumateLayerType>> backgroundLines;

	const int numLayers = LayerGeometries.Num();

	if (numLayers > 0)
	{
		auto addPerimeterLines = [&backgroundLines, parentLocation](const TArray<FVector>& points)
		{
			int numPoints = points.Num();
			for (int i = 0; i < numPoints; ++i)
			{
				FEdge line(points[i] + parentLocation,
					points[(i + 1) % numPoints] + parentLocation);
				backgroundLines.Emplace(line, Modumate::FModumateLayerType::kSeparatorBeyondSurfaceEdges);

			}

		};

		addPerimeterLines(LayerGeometries[0].PointsA);
		addPerimeterLines(LayerGeometries[numLayers - 1].PointsB);

		auto addOpeningLines = [&backgroundLines](const FLayerGeomDef& layer, FVector parentLocation, bool bSideA)
		{
			const auto& holes = layer.CachedHoles2D;
			for (const auto& hole: holes)
			{
				int32 n = hole.Points.Num();
				for (int32 i = 0; i < n; ++i)
				{
					FVector v1 { layer.Deproject2DPoint(hole.Points[i], bSideA) + parentLocation};
					FVector v2 { layer.Deproject2DPoint(hole.Points[(i + 1) % n], bSideA) + parentLocation};
					FEdge line(v1, v2);
					backgroundLines.Emplace(line, Modumate::FModumateLayerType::kOpeningSystemBeyond);
				}
			}

		};

		addOpeningLines(LayerGeometries[0], parentLocation, true);
		addOpeningLines(LayerGeometries[numLayers - 1], parentLocation, false);

		FVector2D boxClipped0;
		FVector2D boxClipped1;
		for (const auto& line : backgroundLines)
		{
			FVector v0 { line.Key.Vertex[0] };
			FVector v1 { line.Key.Vertex[1] };

			bool bV1Infront = Plane.PlaneDot(v0) >= 0.0f;
			bool bV2Infront = Plane.PlaneDot(v1) >= 0.0f;
			if (bV1Infront || bV2Infront)
			{
				if (!bV1Infront || !bV2Infront)
				{   // Intersect with Plane:
					FVector intersect { FMath::LinePlaneIntersection(v0, v1, Plane) };
					(bV1Infront ? v1 : v0) = intersect;
				}

				TArray<FEdge> clippedLineSections = ParentPage->lineClipping->ClipWorldLineToView({ v0, v1 });
				for (const auto& lineSection: clippedLineSections)
				{
					FVector2D vert0(lineSection.Vertex[0]);
					FVector2D vert1(lineSection.Vertex[1]);

					if (UModumateFunctionLibrary::ClipLine2DToRectangle(vert0, vert1, BoundingBox, boxClipped0, boxClipped1))
					{
						TSharedPtr<Modumate::FDraftingLine> draftingLine = MakeShared<Modumate::FDraftingLine>(
							Modumate::Units::FCoordinates2D::WorldCentimeters(boxClipped0),
							Modumate::Units::FCoordinates2D::WorldCentimeters(boxClipped1),
							outerThickness, Modumate::FMColor::Black);
						ParentPage->Children.Add(draftingLine);
						draftingLine->SetLayerTypeRecursive(line.Value);
					}
				}
			}
		}
	}

}
