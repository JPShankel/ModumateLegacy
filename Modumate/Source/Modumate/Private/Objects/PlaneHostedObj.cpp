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
#include "ModumateCore/ModumateObjectStatics.h"
#include "ToolsAndAdjustments/Common/AdjustmentHandleActor.h"
#include "ToolsAndAdjustments/Handles/AdjustInvertHandle.h"
#include "ToolsAndAdjustments/Handles/AdjustPolyEdgeHandle.h"
#include "ToolsAndAdjustments/Handles/JustificationHandle.h"
#include "UnrealClasses/EditModelGameMode.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"


class AEditModelPlayerController;

using namespace Modumate::Mitering;


FMOIPlaneHostedObjData::FMOIPlaneHostedObjData()
{
}

FMOIPlaneHostedObjData::FMOIPlaneHostedObjData(int32 InVersion)
	: Version(InVersion)
{
}


AMOIPlaneHostedObj::AMOIPlaneHostedObj()
	: AModumateObjectInstance()
{
}

FQuat AMOIPlaneHostedObj::GetRotation() const
{
	const AModumateObjectInstance *parent = GetParentObject();
	if (ensure(parent && (parent->GetObjectType() == EObjectType::OTMetaPlane)))
	{
		return parent->GetRotation();
	}
	else
	{
		return FQuat::Identity;
	}
}

FVector AMOIPlaneHostedObj::GetLocation() const
{
	const AModumateObjectInstance *parent = GetParentObject();
	if (ensure(parent && (parent->GetObjectType() == EObjectType::OTMetaPlane)))
	{
		float thickness, startOffset;
		FVector normal;
		UModumateObjectStatics::GetPlaneHostedValues(this, thickness, startOffset, normal);

		FVector planeLocation = parent->GetLocation();
		return planeLocation + (startOffset + (0.5f * thickness)) * normal;
	}
	else
	{
		return FVector::ZeroVector;
	}
}

FVector AMOIPlaneHostedObj::GetCorner(int32 CornerIndex) const
{
	// Handle the meta plane host case which just returns the meta plane with this MOI's offset
	const AModumateObjectInstance *parent = GetParentObject();
	if (ensure(parent && (parent->GetObjectType() == EObjectType::OTMetaPlane)))
	{
		int32 numPlanePoints = parent->GetNumCorners();
		bool bOnStartingSide = (CornerIndex < numPlanePoints);
		int32 numLayers = LayerGeometries.Num();
		int32 pointIndex = CornerIndex % numPlanePoints;

		if (ensure((numLayers == GetAssembly().Layers.Num()) && numLayers > 0))
		{
			auto& layerPoints = bOnStartingSide ? LayerGeometries[0].OriginalPointsA : LayerGeometries[numLayers - 1].OriginalPointsB;

			if (ensure((3 * numPlanePoints) == layerPoints.Num()))
			{
				return parent->GetLocation() + layerPoints[3 * pointIndex];
			}
		}
	}

	return GetLocation();
}

FVector AMOIPlaneHostedObj::GetNormal() const
{
	const AModumateObjectInstance *planeParent = GetParentObject();
	if (ensureAlways(planeParent && (planeParent->GetObjectType() == EObjectType::OTMetaPlane)))
	{
		return planeParent->GetNormal();
	}
	else
	{
		return FVector::ZeroVector;
	}
}

void AMOIPlaneHostedObj::PreDestroy()
{
	MarkEdgesMiterDirty();
}

bool AMOIPlaneHostedObj::CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas)
{
	switch (DirtyFlag)
	{
	case EObjectDirtyFlags::Structure:
	{
		// TODO: as long as the assembly is not stored inside of the data state, and its layers can be reversed,
		// then this is the centralized opportunity to match up the reversal of layers with whatever the intended inversion state is,
		// based on preview/current state changing, assembly changing, object creation, etc.
		SetAssemblyLayersReversed(InstanceData.FlipSigns.Y < 0);

		CachedLayerDims.UpdateLayersFromAssembly(GetAssembly());
		CachedLayerDims.UpdateFinishFromObject(this);

		// When structure (assembly, offset, or plane structure) changes, mark neighboring
		// edges as miter-dirty, so they can re-evaluate mitering with our new structure.
		MarkDirty(EObjectDirtyFlags::Mitering);
		MarkEdgesMiterDirty();
	}
	break;
	case EObjectDirtyFlags::Mitering:
	{
		// Make sure all connected edges have resolved mitering,
		// so we can generate our own mitered mesh correctly.
		for (AModumateObjectInstance *connectedEdge : CachedConnectedEdges)
		{
			if (connectedEdge->IsDirty(EObjectDirtyFlags::Mitering))
			{
				return false;
			}
		}

		UpdateMeshWithLayers(false, true);
	}
	break;
	case EObjectDirtyFlags::Visuals:
	{
		UpdateVisuals();
	}
	break;
	}

	return true;
}

void AMOIPlaneHostedObj::SetupDynamicGeometry()
{
}

void AMOIPlaneHostedObj::UpdateDynamicGeometry()
{
}

void AMOIPlaneHostedObj::GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping, bool bForSelection) const
{
	const AModumateObjectInstance *parent = GetParentObject();

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

void AMOIPlaneHostedObj::SetupAdjustmentHandles(AEditModelPlayerController *controller)
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
		// Don't allow adjusting wall corners, since they're more likely to be edited edge-by-edge.
		if (GetObjectType() != EObjectType::OTWallSegment)
		{
			auto cornerHandle = MakeHandle<AAdjustPolyEdgeHandle>();
			cornerHandle->SetTargetIndex(i);
			cornerHandle->SetTargetMOI(parent);
		}

		auto edgeHandle = MakeHandle<AAdjustPolyEdgeHandle>();
		edgeHandle->SetTargetIndex(i);
		edgeHandle->SetTargetMOI(parent);
	}

	// Make justification handles
	TArray<AAdjustmentHandleActor*> rootHandleChildren;

	auto frontJustificationHandle = MakeHandle<AJustificationHandle>();
	frontJustificationHandle->SetJustification(0.0f);
	rootHandleChildren.Add(frontJustificationHandle);

	auto backJustificationHandle = MakeHandle<AJustificationHandle>();
	backJustificationHandle->SetJustification(1.0f);
	rootHandleChildren.Add(backJustificationHandle);

	auto invertHandle = MakeHandle<AAdjustInvertHandle>();

	auto rootJustificationHandle = MakeHandle<AJustificationHandle>();
	rootJustificationHandle->SetJustification(0.5f);
	rootJustificationHandle->HandleChildren = rootHandleChildren;
	for (auto& child : rootHandleChildren)
	{
		child->HandleParent = rootJustificationHandle;
	}
}

bool AMOIPlaneHostedObj::OnSelected(bool bIsSelected)
{
	if (!AModumateObjectInstance::OnSelected(bIsSelected))
	{
		return false;
	}

	if (const AModumateObjectInstance *parent = GetParentObject())
	{
		TArray<AModumateObjectInstance *> connectedMOIs;
		parent->GetConnectedMOIs(connectedMOIs);
		for (AModumateObjectInstance *connectedMOI : connectedMOIs)
		{
			connectedMOI->UpdateVisuals();
		}
	}

	return true;
}

bool AMOIPlaneHostedObj::GetInvertedState(FMOIStateData& OutState) const
{
	return GetFlippedState(EAxis::Y, OutState);
}

bool AMOIPlaneHostedObj::GetFlippedState(EAxis::Type FlipAxis, FMOIStateData& OutState) const
{
	OutState = GetStateData();

	FMOIPlaneHostedObjData modifiedPlaneHostedObjData = InstanceData;

	float curFlipSign = modifiedPlaneHostedObjData.FlipSigns.GetComponentForAxis(FlipAxis);
	modifiedPlaneHostedObjData.FlipSigns.SetComponentForAxis(FlipAxis, -curFlipSign);

	return OutState.CustomData.SaveStructData(modifiedPlaneHostedObjData);
}

bool AMOIPlaneHostedObj::GetOffsetState(const FVector& AdjustmentDirection, FMOIStateData& OutState) const
{
	float projectedAdjustment = AdjustmentDirection | GetNormal();
	if (FMath::IsNearlyZero(projectedAdjustment, THRESH_NORMALS_ARE_ORTHOGONAL))
	{
		projectedAdjustment = 0.0f;
	}

	float projectedAdjustmentSign = FMath::Sign(projectedAdjustment);
	EDimensionOffsetType nextOffsetType = InstanceData.Offset.GetNextType(projectedAdjustmentSign, InstanceData.FlipSigns.Y);

	FMOIPlaneHostedObjData modifiedPlaneHostedObjData = InstanceData;
	modifiedPlaneHostedObjData.Offset.Type = nextOffsetType;
	OutState = GetStateData();

	return OutState.CustomData.SaveStructData(modifiedPlaneHostedObjData);
}

void AMOIPlaneHostedObj::GetDraftingLines(const TSharedPtr<Modumate::FDraftingComposite> &ParentPage, const FPlane &Plane, const FVector &AxisX, const FVector &AxisY, const FVector &Origin, const FBox2D &BoundingBox, TArray<TArray<FVector>> &OutPerimeters) const
{
	OutPerimeters.Reset();

	ModumateUnitParams::FThickness innerThickness = ModumateUnitParams::FThickness::Points(0.125f);
	ModumateUnitParams::FThickness structureThickness = ModumateUnitParams::FThickness::Points(0.375f);
	ModumateUnitParams::FThickness outerThickness = ModumateUnitParams::FThickness::Points(0.25f);

	Modumate::FMColor innerColor = Modumate::FMColor::Gray64;
	Modumate::FMColor outerColor = Modumate::FMColor::Gray32;
	Modumate::FMColor structureColor = Modumate::FMColor::Black;

	bool bGetFarLines = ParentPage->lineClipping.IsValid();
	if (!bGetFarLines)
	{
		const AModumateObjectInstance *parent = GetParentObject();
		FVector parentLocation = parent->GetLocation();

		Modumate::FModumateLayerType layerTypeOuterSurface;
		Modumate::FModumateLayerType layerTypeMinorSurface;

		switch (GetObjectType())
		{
		case EObjectType::OTCountertop:
			layerTypeOuterSurface = Modumate::FModumateLayerType::kCountertopCut;
			layerTypeMinorSurface = layerTypeOuterSurface;
			break;

		case EObjectType::OTSystemPanel:
			layerTypeOuterSurface = Modumate::FModumateLayerType::kSystemPanelCut;
			layerTypeMinorSurface = layerTypeOuterSurface;
			break;

		default:
			layerTypeOuterSurface = Modumate::FModumateLayerType::kSeparatorCutOuterSurface;
			layerTypeMinorSurface = Modumate::FModumateLayerType::kSeparatorCutMinorLayer;
			break;
		}

		float currentThickness = 0.0f;
		TArray<FVector2D> previousLinePoints;
		int32 numLines = LayerGeometries.Num() + 1;
		for (int32 layerIdx = 0; layerIdx < numLines; layerIdx++)
		{
			bool usePointsA = layerIdx < LayerGeometries.Num();
			auto& layer = usePointsA ? LayerGeometries[layerIdx] : LayerGeometries[layerIdx - 1];

			auto dwgLayerType = layerTypeMinorSurface;
			if (layerIdx == 0 || layerIdx == LayerGeometries.Num())
			{
				dwgLayerType = layerTypeOuterSurface;
			}

			TArray<FVector> intersections;
			UModumateGeometryStatics::GetPlaneIntersections(intersections, usePointsA ? layer.UniquePointsA : layer.UniquePointsB, Plane, parentLocation);

			intersections.Sort(UModumateGeometryStatics::Points3dSorter);
			// we can make mask perimeters when there are an even amount of intersection between a simple polygon and a plane
			bool bMakeMaskPerimeter = (intersections.Num() % 2 == 0);

			ModumateUnitParams::FThickness lineThickness;
			Modumate::FMColor lineColor = innerColor;
			if (FMath::IsNearlyEqual(currentThickness, CachedLayerDims.StructureWidthStart) ||
				FMath::IsNearlyEqual(currentThickness, CachedLayerDims.StructureWidthEnd, KINDA_SMALL_NUMBER))
			{
				lineThickness = structureThickness;
				lineColor = structureColor;
			}
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

			int32 linePoint = 0;

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
				// Hole coords are on the parent meta-plane so project.
				FVector samplePoint = usePointsA ? layer.OriginalPointsA[0] : layer.OriginalPointsB[0];
				FVector hole3DDisplacement = (samplePoint | layer.Normal) * layer.Normal;
				layer.GetRangesForHolesOnPlane(lineRanges, currentIntersection,
					parentLocation + hole3DDisplacement, Plane, -AxisX, -AxisY, Origin);

				// TODO: unclear why the axes need to be flipped here, could be because of the different implementation of finding intersections
				FVector2D start = UModumateGeometryStatics::ProjectPoint2D(Origin, -AxisX, -AxisY, intersectionStart);
				FVector2D end = UModumateGeometryStatics::ProjectPoint2D(Origin, -AxisX, -AxisY, intersectionEnd);
				FVector2D delta = end - start;

				ParentPage->inPlaneLines.Emplace(FVector(start, 0), FVector(end, 0));

				for (auto& range : lineRanges)
				{
					FVector2D clippedStart, clippedEnd;
					FVector2D rangeStart = start + delta * range.Key;
					FVector2D rangeEnd = start + delta * range.Value;

					if (UModumateFunctionLibrary::ClipLine2DToRectangle(rangeStart, rangeEnd, BoundingBox, clippedStart, clippedEnd))
					{
						TSharedPtr<Modumate::FDraftingLine> line = MakeShared<Modumate::FDraftingLine>(
							FModumateUnitCoord2D::WorldCentimeters(clippedStart),
							FModumateUnitCoord2D::WorldCentimeters(clippedEnd),
							lineThickness, lineColor);
						ParentPage->Children.Add(line);
						line->SetLayerTypeRecursive(dwgLayerType);
					}
					if (previousLinePoints.Num() > linePoint)
					{
						if (UModumateFunctionLibrary::ClipLine2DToRectangle(previousLinePoints[linePoint], rangeStart, BoundingBox, clippedStart, clippedEnd))
						{
							TSharedPtr<Modumate::FDraftingLine> line = MakeShared<Modumate::FDraftingLine>(
								FModumateUnitCoord2D::WorldCentimeters(clippedStart),
								FModumateUnitCoord2D::WorldCentimeters(clippedEnd),
								lineThickness, lineColor);
							ParentPage->Children.Add(line);
							line->SetLayerTypeRecursive(layerTypeOuterSurface);
						}
						if (UModumateFunctionLibrary::ClipLine2DToRectangle(previousLinePoints[linePoint+1], rangeEnd, BoundingBox, clippedStart, clippedEnd))
						{
							TSharedPtr<Modumate::FDraftingLine> line = MakeShared<Modumate::FDraftingLine>(
								FModumateUnitCoord2D::WorldCentimeters(clippedStart),
								FModumateUnitCoord2D::WorldCentimeters(clippedEnd),
								lineThickness, lineColor);
							ParentPage->Children.Add(line);
							line->SetLayerTypeRecursive(layerTypeOuterSurface);
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

void AMOIPlaneHostedObj::PostLoadInstanceData()
{
	bool bFixedInstanceData = false;

	if (InstanceData.Version < InstanceData.CurrentVersion)
	{
		if (InstanceData.Version < 1)
		{
			if (InstanceData.bLayersInverted_DEPRECATED)
			{
				InstanceData.FlipSigns.Y = -1.0f;
			}
		}
		if (InstanceData.Version < 2)
		{
			float flippedJustification = (-InstanceData.FlipSigns.Y * (InstanceData.Justification_DEPRECATED - 0.5f)) + 0.5f;
			if (FMath::IsNearlyEqual(flippedJustification, 0.0f))
			{
				InstanceData.Offset.Type = EDimensionOffsetType::Negative;
			}
			else if (FMath::IsNearlyEqual(flippedJustification, 1.0f))
			{
				InstanceData.Offset.Type = EDimensionOffsetType::Positive;
			}
			else 
			{
				InstanceData.Offset.Type = EDimensionOffsetType::Centered;
			}
		}

		InstanceData.Version = InstanceData.CurrentVersion;
		bFixedInstanceData = true;
	}

	// Check for invalid flip signs regardless of version numbers, due to non-serialization-version bugs
	for (int32 axisIdx = 0; axisIdx < 3; ++axisIdx)
	{
		float& flipSign = InstanceData.FlipSigns[axisIdx];
		if (FMath::Abs(flipSign) != 1.0f)
		{
			flipSign = 1.0f;
			bFixedInstanceData = true;
		}
	}

	if (bFixedInstanceData)
	{
		StateData.CustomData.SaveStructData(InstanceData);
	}
}

void AMOIPlaneHostedObj::UpdateMeshWithLayers(bool bRecreateMesh, bool bRecalculateEdgeExtensions)
{
	const UModumateDocument* doc = GetDocument();
	if (!ensureAlwaysMsgf(doc, TEXT("Tried to update invalid plane-hosted object!")))
	{
		return;
	}

	int32 parentID = GetParentID();
	const Modumate::FGraph3DFace *planeFace = doc->GetVolumeGraph().FindFace(parentID);
	const AModumateObjectInstance *parentPlane = doc->GetObjectById(parentID);
	if (!ensureMsgf(parentPlane, TEXT("Plane-hosted object (ID %d) is missing parent object (ID %d)!"), ID, parentID) ||
		!ensureMsgf(planeFace, TEXT("Plane-hosted object (ID %d) is missing parent graph face (ID %d)!"), ID, parentID))
	{
		return;
	}

	DynamicMeshActor->SetActorLocation(parentPlane->GetLocation());
	DynamicMeshActor->SetActorRotation(FQuat::Identity);

	CachedHoles.Reset();
	for (auto& hole : planeFace->CachedHoles)
	{
		TempHoleRelativePoints.Reset();
		Algo::Transform(hole.Points, TempHoleRelativePoints, [planeFace](const FVector &worldPoint) { return worldPoint - planeFace->CachedCenter; });
		CachedHoles.Add(FPolyHole3D(TempHoleRelativePoints));
	}

	if (!FMiterHelpers::UpdateMiteredLayerGeoms(this, planeFace, &CachedHoles, LayerGeometries))
	{
		return;
	}

	DynamicMeshActor->Assembly = GetAssembly();
	DynamicMeshActor->LayerGeometries = LayerGeometries;

	// TODO: now that preview deltas have replaced object-specific preview mode, this change might be too overreaching and affect too many objects.
	// We should reevaluate the tradeoffs between all updating objects consistently not updating their collision during preview deltas vs
	// keeping track of which objects are affected by preview deltas and only preserving old collision for those.
	// In addition, this logic shouldn't even be necessary if cursor raycast results could be correctly read from physics geometry
	// modified during the same frame before generating preview deltas in tools,
	// but relying on PhysX collision cooking and deferred actor/mesh flag updating may prevent that.
	bool bEnableCollision = true;
	bool bUpdateCollision = !doc->IsPreviewingDeltas();
	FVector2D uvFlip(InstanceData.FlipSigns.X, InstanceData.FlipSigns.Z);
	DynamicMeshActor->UpdatePlaneHostedMesh(bRecreateMesh, bUpdateCollision, bEnableCollision, FVector::ZeroVector, uvFlip);
}

void AMOIPlaneHostedObj::UpdateConnectedEdges()
{
	CachedParentConnectedMOIs.Reset();

	const AModumateObjectInstance *planeParent = GetParentObject();
	if (planeParent)
	{
		planeParent->GetConnectedMOIs(CachedParentConnectedMOIs);
	}

	CachedConnectedEdges.Reset();
	for (AModumateObjectInstance *planeConnectedMOI : CachedParentConnectedMOIs)
	{
		if (planeConnectedMOI && (planeConnectedMOI->GetObjectType() == EObjectType::OTMetaEdge))
		{
			CachedConnectedEdges.Add(planeConnectedMOI);
		}
	}
}

void AMOIPlaneHostedObj::MarkEdgesMiterDirty()
{
	UpdateConnectedEdges();
	for (AModumateObjectInstance *connectedEdge : CachedConnectedEdges)
	{
		connectedEdge->MarkDirty(EObjectDirtyFlags::Mitering);
	}
}

void AMOIPlaneHostedObj::GetBeyondDraftingLines(const TSharedPtr<Modumate::FDraftingComposite>& ParentPage, const FPlane& Plane,
	const FBox2D& BoundingBox) const
{
	static const ModumateUnitParams::FThickness outerThickness = ModumateUnitParams::FThickness::Points(0.25f);

	const AModumateObjectInstance *parent = GetParentObject();
	FVector parentLocation = parent->GetLocation();
	Modumate::FModumateLayerType layerType;

	switch (GetObjectType())
	{
	case EObjectType::OTCountertop:
		layerType = Modumate::FModumateLayerType::kCountertopBeyond;
		break;

	case EObjectType::OTSystemPanel:
		layerType = Modumate::FModumateLayerType::kSystemPanelBeyond;
		break;

	default:
		layerType = Modumate::FModumateLayerType::kSeparatorBeyondSurfaceEdges;
		break;
	}

	TArray<TPair<FEdge, Modumate::FModumateLayerType>> backgroundLines;

	const int numLayers = LayerGeometries.Num();

	if (numLayers > 0)
	{
		const float totalThickness = Algo::Accumulate(LayerGeometries, 0.0f,
			[](float s, const auto& layer) { return s + layer.Thickness; });
		const FVector planeOffset = totalThickness * LayerGeometries[0].Normal;

		auto addPerimeterLines = [&backgroundLines, parentLocation, layerType](const TArray<FVector>& points)
		{
			int numPoints = points.Num();
			for (int i = 0; i < numPoints; ++i)
			{
				FEdge line(points[i] + parentLocation,
					points[(i + 1) % numPoints] + parentLocation);
				backgroundLines.Emplace(line, layerType);

			}

		};

		addPerimeterLines(LayerGeometries[0].OriginalPointsA);
		addPerimeterLines(LayerGeometries[numLayers - 1].OriginalPointsB);

		auto addOpeningLines = [&backgroundLines](const FLayerGeomDef& layer, FVector parentLocation, bool bSideA)
		{
			const auto& holes = layer.Holes3D;
			for (const auto& hole: holes)
			{
				int32 n = hole.Points.Num();
				for (int32 i = 0; i < n; ++i)
				{
					FVector v1 = layer.ProjectToPlane(hole.Points[i], bSideA);
					FVector v2 = layer.ProjectToPlane(hole.Points[(i + 1) % n], bSideA);
					FEdge line(v1 + parentLocation, v2 + parentLocation);
					backgroundLines.Emplace(line, Modumate::FModumateLayerType::kSeparatorBeyondSurfaceEdges);
				}
			}

		};

		addOpeningLines(LayerGeometries[0], parentLocation, true);
		addOpeningLines(LayerGeometries[numLayers - 1], parentLocation, false);

		for (const auto& hole : LayerGeometries[0].Holes3D)
		{
			for (const auto& p : hole.Points)
			{
				FVector holePoint = LayerGeometries[0].ProjectToPlane(p, true) + parentLocation;
				backgroundLines.Emplace(FEdge(holePoint, holePoint + planeOffset),
					Modumate::FModumateLayerType::kSeparatorBeyondSurfaceEdges);
			}
		}

		// Corner lines.
		int32 numPoints = LayerGeometries[0].OriginalPointsA.Num();
		for (int32 p = 0; p < numPoints; ++p)
		{
			FEdge line(LayerGeometries[0].OriginalPointsA[p] + parentLocation,
				LayerGeometries[numLayers - 1].OriginalPointsB[p] + parentLocation);
			backgroundLines.Emplace(line, layerType);
		}

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
							FModumateUnitCoord2D::WorldCentimeters(boxClipped0),
							FModumateUnitCoord2D::WorldCentimeters(boxClipped1),
							outerThickness, Modumate::FMColor::Black);
						ParentPage->Children.Add(draftingLine);
						draftingLine->SetLayerTypeRecursive(line.Value);
					}
				}
			}
		}
	}

}
