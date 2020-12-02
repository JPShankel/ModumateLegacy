// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "Objects/Finish.h"

#include "Algo/Accumulate.h"
#include "DocumentManagement/ModumateDocument.h"
#include "Drafting/ModumateDraftingElements.h"
#include "Graph/Graph2D.h"
#include "ModumateCore/LayerGeomDef.h"
#include "ModumateCore/ModumateGeometryStatics.h"
#include "ModumateCore/ModumateObjectStatics.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "Objects/ModumateObjectInstance.h"
#include "ToolsAndAdjustments/Handles/AdjustPolyPointHandle.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"


FMOIFinishImpl::FMOIFinishImpl(FModumateObjectInstance *moi)
	: FModumateObjectInstanceImplBase(moi)
{ }

FMOIFinishImpl::~FMOIFinishImpl()
{
}

void FMOIFinishImpl::Destroy()
{
	MarkConnectedEdgeChildrenDirty(EObjectDirtyFlags::Structure);
}

FVector FMOIFinishImpl::GetCorner(int32 index) const
{
	const FModumateObjectInstance *parentObj = MOI->GetParentObject();
	if (!ensure(parentObj) || (CachedPerimeter.Num() == 0))
	{
		return FVector::ZeroVector;
	}

	float thickness = MOI->CalculateThickness();
	int32 numPoints = CachedPerimeter.Num();
	FVector cornerOffset = (index < numPoints) ? FVector::ZeroVector : (GetNormal() * thickness);

	return CachedPerimeter[index % numPoints] + cornerOffset;
}

FVector FMOIFinishImpl::GetNormal() const
{
	return CachedGraphOrigin.GetRotation().GetAxisZ();
}

bool FMOIFinishImpl::CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas)
{
	switch (DirtyFlag)
	{
	case EObjectDirtyFlags::Structure:
	{
		// The finish requires a surface polygon parent from which to extrude its layered assembly
		FModumateObjectInstance *surfacePolyParent = MOI->GetParentObject();
		if ((surfacePolyParent == nullptr) || !ensure(surfacePolyParent->GetObjectType() == EObjectType::OTSurfacePolygon))
		{
			return false;
		}

		bool bInteriorPolygon, bInnerBoundsPolygon;
		if (!UModumateObjectStatics::GetGeometryFromSurfacePoly(MOI->GetDocument(), surfacePolyParent->ID,
			bInteriorPolygon, bInnerBoundsPolygon, CachedGraphOrigin, CachedPerimeter, CachedHoles))
		{
			return false;
		}

		// We shouldn't have been able to parent a finish to an invalid surface polygon, so don't bother trying to get geometry from it
		if (!ensure(bInteriorPolygon && !bInnerBoundsPolygon) || (CachedPerimeter.Num() < 3))
		{
			return false;
		}

		bool bToleratePlanarErrors = true;
		bool bLayerSetupSuccess = DynamicMeshActor->CreateBasicLayerDefs(CachedPerimeter, CachedGraphOrigin.GetRotation().GetAxisZ(), CachedHoles,
			MOI->GetAssembly(), 0.0f, FVector::ZeroVector, 0.0f, bToleratePlanarErrors);

		if (bLayerSetupSuccess)
		{
			DynamicMeshActor->UpdatePlaneHostedMesh(true, true, true);
		}

		MarkConnectedEdgeChildrenDirty(EObjectDirtyFlags::Structure);
	}
		break;
	case EObjectDirtyFlags::Visuals:
		MOI->UpdateVisibilityAndCollision();
		break;
	default:
		break;
	}

	return true;
}

void FMOIFinishImpl::GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping, bool bForSelection) const
{
	int32 numPoints = CachedPerimeter.Num();
	FVector extrusionDelta = MOI->CalculateThickness() * GetNormal();

	for (int32 i = 0; i < numPoints; ++i)
	{
		int32 edgeIdxA = i;
		int32 edgeIdxB = (i + 1) % numPoints;

		FVector cornerMinA = CachedPerimeter[edgeIdxA];
		FVector cornerMinB = CachedPerimeter[edgeIdxB];
		FVector edgeDir = (cornerMinB - cornerMinA).GetSafeNormal();

		FVector cornerMaxA = cornerMinA + extrusionDelta;
		FVector cornerMaxB = cornerMinB + extrusionDelta;

		outPoints.Add(FStructurePoint(cornerMinA, edgeDir, edgeIdxA));

		outLines.Add(FStructureLine(cornerMinA, cornerMinB, edgeIdxA, edgeIdxB));
		outLines.Add(FStructureLine(cornerMaxA, cornerMaxB, edgeIdxA + numPoints, edgeIdxB + numPoints));
		outLines.Add(FStructureLine(cornerMinA, cornerMaxA, edgeIdxA, edgeIdxA + numPoints));
	}
}

void FMOIFinishImpl::SetupAdjustmentHandles(AEditModelPlayerController_CPP* controller)
{
	// parent handles
	FModumateObjectInstance* parent = MOI->GetParentObject();
	if (!ensureAlways(parent && (parent->GetObjectType() == EObjectType::OTSurfacePolygon)))
	{
		return;
	}

	int32 numCorners = parent->GetNumCorners();
	for (int32 i = 0; i < numCorners; ++i)
	{
		auto cornerHandle = MOI->MakeHandle<AAdjustPolyPointHandle>();
		cornerHandle->SetTargetIndex(i);
		cornerHandle->SetTargetMOI(parent);

		auto edgeHandle = MOI->MakeHandle<AAdjustPolyPointHandle>();
		edgeHandle->SetTargetIndex(i);
		edgeHandle->SetAdjustPolyEdge(true);
		edgeHandle->SetTargetMOI(parent);
	}
}

void FMOIFinishImpl::GetDraftingLines(const TSharedPtr<Modumate::FDraftingComposite>& ParentPage, const FPlane& Plane,
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

void FMOIFinishImpl::UpdateConnectedEdges()
{
	CachedConnectedEdgeIDs.Reset();
	FModumateDocument* doc = MOI->GetDocument();

	auto grandParentGraph = doc->FindSurfaceGraphByObjID(MOI->ID);
	auto parentSurfacePoly = grandParentGraph ? grandParentGraph->FindPolygon(MOI->GetParentID()) : nullptr;

	if (parentSurfacePoly == nullptr)
	{
		return;
	}

	for (FGraphSignedID edgeID : parentSurfacePoly->Edges)
	{
		CachedConnectedEdgeIDs.Add(FMath::Abs(edgeID));
	}
	for (int32 containedPolyID : parentSurfacePoly->ContainedPolyIDs)
	{
		if (auto containedSurfacePoly = grandParentGraph->FindPolygon(containedPolyID))
		{
			for (FGraphSignedID edgeID : containedSurfacePoly->Edges)
			{
				CachedConnectedEdgeIDs.Add(FMath::Abs(edgeID));
			}
		}
	}

	CachedConnectedEdgeChildren.Reset();
	for (int32 connectedEdgeID : CachedConnectedEdgeIDs)
	{
		FModumateObjectInstance* connectedEdgeMOI = doc->GetObjectById(connectedEdgeID);
		if (connectedEdgeMOI && (connectedEdgeMOI->GetObjectType() == EObjectType::OTSurfaceEdge))
		{
			CachedConnectedEdgeChildren.Append(connectedEdgeMOI->GetChildObjects());
		}
	}
}

void FMOIFinishImpl::MarkConnectedEdgeChildrenDirty(EObjectDirtyFlags DirtyFlags)
{
	UpdateConnectedEdges();

	for (FModumateObjectInstance* connectedEdgeChild : CachedConnectedEdgeChildren)
	{
		connectedEdgeChild->MarkDirty(DirtyFlags);
	}
}

void FMOIFinishImpl::GetInPlaneLines(const TSharedPtr<Modumate::FDraftingComposite>& ParentPage, const FPlane& Plane,
	const FVector& AxisX, const FVector& AxisY, const FVector& Origin, const FBox2D& BoundingBox) const
{
	static const Modumate::Units::FThickness lineThickness = Modumate::Units::FThickness::Points(0.05f);
	static const Modumate::FMColor lineColor = Modumate::FMColor::Gray144;
	static const Modumate::FModumateLayerType dwgLayerType = Modumate::FModumateLayerType::kSeparatorCutMinorLayer;

	const ADynamicMeshActor* actor = CastChecked<ADynamicMeshActor>(MOI->GetActor());
	if (actor == nullptr)
	{
		return;
	}

	const TArray<FLayerGeomDef>& layers = actor->LayerGeometries;
	const int32 numLayers = layers.Num();

	TArray<FVector2D> previousLinePoints;
	float currentThickness = 0.0f;

	for (int32 layerIdx = 0; layerIdx <= numLayers; layerIdx++)
	{

		bool usePointsA = layerIdx < numLayers;
		const auto& layer = usePointsA ? layers[layerIdx] : layers[layerIdx - 1];
		Modumate::FModumateLayerType dwgTypeThisLayer = usePointsA ? dwgLayerType : Modumate::FModumateLayerType::kSeparatorCutOuterSurface;

		TArray<FVector> intersections;
		UModumateGeometryStatics::GetPlaneIntersections(intersections, usePointsA ? layer.PointsA : layer.PointsB, Plane);
		intersections.Sort(UModumateGeometryStatics::Points3dSorter);

		int32 linePoint = 0;
		for (int32 idx = 0; idx < intersections.Num() - 1; idx += 2)
		{

			TArray<TPair<float, float>> lineRanges;
			FVector intersectionStart = intersections[idx];
			FVector intersectionEnd = intersections[idx + 1];
			TPair<FVector, FVector> currentIntersection = TPair<FVector, FVector>(intersectionStart, intersectionEnd);
			layer.GetRangesForHolesOnPlane(lineRanges, currentIntersection, currentThickness * layer.Normal,
				Plane, -AxisX, -AxisY, Origin);

			// TODO: unclear why the axes need to be flipped here, could be because of the different implementation of finding intersections
			FVector2D start = UModumateGeometryStatics::ProjectPoint2D(Origin, -AxisX, -AxisY, intersectionStart);
			FVector2D end = UModumateGeometryStatics::ProjectPoint2D(Origin, -AxisX, -AxisY, intersectionEnd);
			FVector2D delta = end - start;

			for (auto& range: lineRanges)
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
					line->SetLayerTypeRecursive(dwgTypeThisLayer);
				}
				if (previousLinePoints.Num() > linePoint)
				{
					if (UModumateFunctionLibrary::ClipLine2DToRectangle(previousLinePoints[linePoint], rangeStart, BoundingBox, clippedStart, clippedEnd))
					{
						TSharedPtr<Modumate::FDraftingLine> line = MakeShared<Modumate::FDraftingLine>(
							Modumate::Units::FCoordinates2D::WorldCentimeters(clippedStart),
							Modumate::Units::FCoordinates2D::WorldCentimeters(clippedEnd),
							Modumate::Units::FThickness::Points(0.25f),
							Modumate::FMColor::Gray96);
						ParentPage->Children.Add(line);
						line->SetLayerTypeRecursive(Modumate::FModumateLayerType::kSeparatorCutOuterSurface);
					}
					if (UModumateFunctionLibrary::ClipLine2DToRectangle(previousLinePoints[linePoint + 1], rangeEnd, BoundingBox, clippedStart, clippedEnd))
					{
						TSharedPtr<Modumate::FDraftingLine> line = MakeShared<Modumate::FDraftingLine>(
							Modumate::Units::FCoordinates2D::WorldCentimeters(clippedStart),
							Modumate::Units::FCoordinates2D::WorldCentimeters(clippedEnd),
							Modumate::Units::FThickness::Points(0.25f),
							Modumate::FMColor::Gray96);
						ParentPage->Children.Add(line);
						line->SetLayerTypeRecursive(Modumate::FModumateLayerType::kSeparatorCutOuterSurface);
					}
				}
				previousLinePoints.SetNum(FMath::Max(linePoint + 2, previousLinePoints.Num()));
				previousLinePoints[linePoint] = rangeStart;
				previousLinePoints[linePoint + 1] = rangeEnd;
				linePoint += 2;
			}

		}
		currentThickness += layer.Thickness;
	}
	
}

void FMOIFinishImpl::GetBeyondLines(const TSharedPtr<Modumate::FDraftingComposite>& ParentPage, const FPlane& Plane,
	const FVector& AxisX, const FVector& AxisY, const FVector& Origin, const FBox2D& BoundingBox) const
{
	static const Modumate::Units::FThickness lineThickness = Modumate::Units::FThickness::Points(0.15f);
	static const Modumate::FMColor lineColor(0.439f, 0.439f, 0.439f);  // Gray112
	static const Modumate::FModumateLayerType dwgOuterType = Modumate::FModumateLayerType::kFinishBeyond;
	static const Modumate::FModumateLayerType dwgHoleType = Modumate::FModumateLayerType::kFinishBeyond;

	const ADynamicMeshActor* actor = CastChecked<ADynamicMeshActor>(MOI->GetActor());
	if (actor == nullptr)
	{
		return;
	}

	const TArray<FLayerGeomDef>& layers = actor->LayerGeometries;
	const int32 numLayers = layers.Num();
	const float totalThickness = Algo::Accumulate(layers, 0.0f, [](float s, const auto& layer) { return s + layer.Thickness; });

	TArray<TPair<FEdge, Modumate::FModumateLayerType>> beyondLines;

	if (numLayers > 0)
	{
		const FLayerGeomDef& layer = layers.Last();
		const FVector finishOffset = totalThickness * layer.Normal;

		const int numPoints = layer.PointsB.Num();
		for (int i = 0; i < numPoints; ++i)
		{
			FVector point(layer.PointsB[i]);
			beyondLines.Emplace( FEdge(point, layer.PointsB[(i + 1) % numPoints]), dwgOuterType );
			// Lines along finish thickness.
			beyondLines.Emplace( FEdge(point, point - finishOffset), dwgOuterType );
		}

		for (const auto& hole: layer.Holes3D)
		{
			const int numHolePoints = hole.Points.Num();
			for (int i = 0; i < numHolePoints; ++i)
			{
				beyondLines.Emplace( FEdge(hole.Points[i] + finishOffset, hole.Points[(i + 1) % numHolePoints] + finishOffset),
					dwgHoleType );
			}
		}

		FVector2D boxClipped0;
		FVector2D boxClipped1;
		for (const auto& line: beyondLines)
		{
			TArray<FEdge> clippedLineSections = ParentPage->lineClipping->ClipWorldLineToView(line.Key);

			for (const auto& lineSection: clippedLineSections)
			{
				FVector2D vert0(lineSection.Vertex[0]);
				FVector2D vert1(lineSection.Vertex[1]);

				if (UModumateFunctionLibrary::ClipLine2DToRectangle(vert0, vert1, BoundingBox, boxClipped0, boxClipped1))
				{
					TSharedPtr<Modumate::FDraftingLine> draftingLine = MakeShared<Modumate::FDraftingLine>(
						Modumate::Units::FCoordinates2D::WorldCentimeters(boxClipped0),
						Modumate::Units::FCoordinates2D::WorldCentimeters(boxClipped1),
						lineThickness, lineColor);
					ParentPage->Children.Add(draftingLine);
					draftingLine->SetLayerTypeRecursive(line.Value);
				}
			}
		}

	}
}
