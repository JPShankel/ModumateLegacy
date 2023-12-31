// Copyright 2019 Modumate, Inc. All Rights Reserved.

#include "Objects/Finish.h"

#include "Algo/Accumulate.h"
#include "DocumentManagement/ModumateDocument.h"
#include "Drafting/ModumateDraftingElements.h"
#include "Graph/Graph2D.h"
#include "ModumateCore/LayerGeomDef.h"
#include "ModumateCore/ModumateGeometryStatics.h"
#include "Objects/ModumateObjectStatics.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "Objects/ModumateObjectInstance.h"
#include "UnrealClasses/EditModelPlayerController.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "Quantities/QuantitiesManager.h"
#include "DrawingDesigner/DrawingDesignerLine.h"


void AMOIFinish::PreDestroy()
{
	MarkConnectedEdgeChildrenDirty(EObjectDirtyFlags::Structure);
	Super::PreDestroy();
}

FVector AMOIFinish::GetCorner(int32 index) const
{
	const AModumateObjectInstance *parentObj = GetParentObject();
	if (!ensure(parentObj) || (CachedPerimeter.Num() == 0))
	{
		return FVector::ZeroVector;
	}

	float thickness = CalculateThickness();
	int32 numPoints = CachedPerimeter.Num();
	FVector cornerOffset = (index < numPoints) ? FVector::ZeroVector : (GetNormal() * thickness);

	return CachedPerimeter[index % numPoints] + cornerOffset;
}

FVector AMOIFinish::GetNormal() const
{
	return CachedGraphOrigin.GetRotation().GetAxisZ();
}

bool AMOIFinish::CleanObject(EObjectDirtyFlags DirtyFlag, TArray<FDeltaPtr>* OutSideEffectDeltas)
{
	switch (DirtyFlag)
	{
	case EObjectDirtyFlags::Structure:
	{
		// The finish requires a surface polygon parent from which to extrude its layered assembly
		AModumateObjectInstance *surfacePolyParent = GetParentObject();
		if ((surfacePolyParent == nullptr) || !ensure(surfacePolyParent->GetObjectType() == EObjectType::OTSurfacePolygon))
		{
			return false;
		}

		bool bInteriorPolygon, bInnerBoundsPolygon;
		if (!UModumateObjectStatics::GetGeometryFromSurfacePoly(GetDocument(), surfacePolyParent->ID,
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
			GetAssembly(), 0.0f, FVector::ZeroVector, 0.0f, bToleratePlanarErrors);

		if (bLayerSetupSuccess)
		{
			DynamicMeshActor->UpdatePlaneHostedMesh(true, true, true, FVector::ZeroVector, InstanceData.FlipSigns);
		}

		MarkConnectedEdgeChildrenDirty(EObjectDirtyFlags::Structure);
	}
		break;
	case EObjectDirtyFlags::Visuals:
		return TryUpdateVisuals();
	default:
		break;
	}

	return true;
}

void AMOIFinish::GetStructuralPointsAndLines(TArray<FStructurePoint> &outPoints, TArray<FStructureLine> &outLines, bool bForSnapping, bool bForSelection) const
{
	int32 numPoints = CachedPerimeter.Num();
	FVector extrusionDelta = CalculateThickness() * GetNormal();

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

void AMOIFinish::ToggleAndUpdateCapGeometry(bool bEnableCap)
{
	if (DynamicMeshActor.IsValid())
	{
		bEnableCap ? DynamicMeshActor->SetupCapGeometry() : DynamicMeshActor->ClearCapGeometry();
	}
}

bool AMOIFinish::GetFlippedState(EAxis::Type FlipAxis, FMOIStateData& OutState) const
{
	if (FlipAxis == EAxis::Y)
	{
		return false;
	}

	OutState = GetStateData();

	FMOIFinishData modifiedFinishData = InstanceData;
	int32 flipAxisIdx = (FlipAxis == EAxis::X) ? 0 : 1;
	modifiedFinishData.FlipSigns[flipAxisIdx] *= -1.0f;

	return OutState.CustomData.SaveStructData(modifiedFinishData);
}

void AMOIFinish::GetDraftingLines(const TSharedPtr<FDraftingComposite>& ParentPage, const FPlane& Plane,
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

bool AMOIFinish::ProcessQuantities(FQuantitiesCollection& QuantitiesVisitor) const
{
	QuantitiesVisitor.Add(CachedQuantities);
	return true;
}

void AMOIFinish::UpdateQuantities()
{
	const FBIMAssemblySpec& assembly = GetAssembly();
	auto assemblyGuid = assembly.UniqueKey();
	const int32 hostingPolyId = GetParentID();
	TSharedPtr<FGraph2D> graph = Document->FindSurfaceGraphByObjID(hostingPolyId);
	if (!ensure(graph.IsValid()))
	{
		return;
	}
	const FGraph2DPolygon* hostingFace = graph->FindPolygon(hostingPolyId);

	if (!ensure(hostingFace))
	{
		return;
	}

	CachedQuantities.Empty();
	float assemblyArea = FQuantitiesCollection::AreaOfFace(*hostingFace);
	CachedQuantities.AddQuantity(assemblyGuid, 0.0f, 0.0f, assemblyArea);
	const ADynamicMeshActor* actor = CastChecked< ADynamicMeshActor>(GetActor());
	CachedQuantities.AddLayersQuantity(actor->LayerGeometries, assembly.Layers, assemblyGuid);
	GetWorld()->GetGameInstance<UModumateGameInstance>()->GetQuantitiesManager()->SetDirtyBit();
}

void AMOIFinish::UpdateConnectedEdges()
{
	CachedConnectedEdgeIDs.Reset();
	UModumateDocument* doc = GetDocument();

	auto grandParentGraph = doc->FindSurfaceGraphByObjID(ID);
	auto parentSurfacePoly = grandParentGraph ? grandParentGraph->FindPolygon(GetParentID()) : nullptr;

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
		AModumateObjectInstance* connectedEdgeMOI = doc->GetObjectById(connectedEdgeID);
		if (connectedEdgeMOI && (connectedEdgeMOI->GetObjectType() == EObjectType::OTSurfaceEdge))
		{
			CachedConnectedEdgeChildren.Append(connectedEdgeMOI->GetChildObjects());
		}
	}
}

void AMOIFinish::MarkConnectedEdgeChildrenDirty(EObjectDirtyFlags EdgeDirtyFlags)
{
	UpdateConnectedEdges();

	for (AModumateObjectInstance* connectedEdgeChild : CachedConnectedEdgeChildren)
	{
		connectedEdgeChild->MarkDirty(EdgeDirtyFlags);
	}
}

void AMOIFinish::GetInPlaneLines(const TSharedPtr<FDraftingComposite>& ParentPage, const FPlane& Plane,
	const FVector& AxisX, const FVector& AxisY, const FVector& Origin, const FBox2D& BoundingBox) const
{
	static const ModumateUnitParams::FThickness lineThickness = ModumateUnitParams::FThickness::Points(0.15f);
	static const FMColor lineColor = FMColor::Gray144;
	static const FModumateLayerType dwgLayerType = FModumateLayerType::kFinishCut;

	const ADynamicMeshActor* actor = CastChecked<ADynamicMeshActor>(GetActor());
	if (actor == nullptr)
	{
		return;
	}

	const TArray<FLayerGeomDef>& layers = actor->LayerGeometries;
	const int32 numLayers = layers.Num();

	if (numLayers == 0)
	{
		return;
	}

	TArray<FVector2D> previousLinePoints;
	float currentThickness = 0.0f;

	for (int32 layerIdx = 0; layerIdx <= numLayers; layerIdx++)
	{
		bool usePointsA = layerIdx < numLayers;
		auto& layer = usePointsA ? layers[layerIdx] : layers[layerIdx - 1];


		TArray<FVector> intersections;
		UModumateGeometryStatics::GetPlaneIntersections(intersections, usePointsA ? layer.OriginalPointsA : layer.OriginalPointsB, Plane);

		intersections.Sort(UModumateGeometryStatics::Points3dSorter);

		int32 linePoint = 0;

		for (int32 idx = 0; idx < intersections.Num() - 1; idx += 2)
		{

			TArray<TPair<float, float>> lineRanges;
			FVector intersectionStart = intersections[idx];
			FVector intersectionEnd = intersections[idx + 1];
			TPair<FVector, FVector> currentIntersection = TPair<FVector, FVector>(intersectionStart, intersectionEnd);
			// Hole coords are on the parent meta-plane so project.
			FVector samplePoint = usePointsA ? layer.OriginalPointsA[0] : layer.OriginalPointsB[0];
			FVector hole3DDisplacement = ((samplePoint - layers[0].Origin) | layer.Normal) * layer.Normal;
			layer.GetRangesForHolesOnPlane(lineRanges, currentIntersection,
				hole3DDisplacement, Plane, AxisX, AxisY, Origin);

			FVector2D start = UModumateGeometryStatics::ProjectPoint2D(intersectionStart, AxisX, AxisY, Origin);
			FVector2D end = UModumateGeometryStatics::ProjectPoint2D(intersectionEnd, AxisX, AxisY, Origin);
			FVector2D delta = end - start;

			ParentPage->inPlaneLines.Emplace(FVector(start, 0), FVector(end, 0));

			for (auto& range : lineRanges)
			{
				FVector2D clippedStart, clippedEnd;
				FVector2D rangeStart = start + delta * range.Key;
				FVector2D rangeEnd = start + delta * range.Value;

				if (UModumateFunctionLibrary::ClipLine2DToRectangle(rangeStart, rangeEnd, BoundingBox, clippedStart, clippedEnd))
				{
					TSharedPtr<FDraftingLine> line = MakeShared<FDraftingLine>(
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
						TSharedPtr<FDraftingLine> line = MakeShared<FDraftingLine>(
							FModumateUnitCoord2D::WorldCentimeters(clippedStart),
							FModumateUnitCoord2D::WorldCentimeters(clippedEnd),
							lineThickness, lineColor);
						ParentPage->Children.Add(line);
						line->SetLayerTypeRecursive(dwgLayerType);
					}
					if (UModumateFunctionLibrary::ClipLine2DToRectangle(previousLinePoints[linePoint + 1], rangeEnd, BoundingBox, clippedStart, clippedEnd))
					{
						TSharedPtr<FDraftingLine> line = MakeShared<FDraftingLine>(
							FModumateUnitCoord2D::WorldCentimeters(clippedStart),
							FModumateUnitCoord2D::WorldCentimeters(clippedEnd),
							lineThickness, lineColor);
						ParentPage->Children.Add(line);
						line->SetLayerTypeRecursive(dwgLayerType);
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

void AMOIFinish::GetBeyondLines(const TSharedPtr<FDraftingComposite>& ParentPage, const FPlane& Plane,
	const FVector& AxisX, const FVector& AxisY, const FVector& Origin, const FBox2D& BoundingBox) const
{
	static const ModumateUnitParams::FThickness lineThickness = ModumateUnitParams::FThickness::Points(0.15f);
	static const FMColor lineColor(0.439f, 0.439f, 0.439f);  // Gray112
	static const FModumateLayerType dwgOuterType = FModumateLayerType::kFinishBeyond;
	static const FModumateLayerType dwgHoleType = FModumateLayerType::kFinishBeyond;

	const ADynamicMeshActor* actor = CastChecked<ADynamicMeshActor>(GetActor());
	if (actor == nullptr)
	{
		return;
	}

	const TArray<FLayerGeomDef>& layers = actor->LayerGeometries;
	const int32 numLayers = layers.Num();
	const float totalThickness = Algo::Accumulate(layers, 0.0f, [](float s, const auto& layer) { return s + layer.Thickness; });

	TArray<TPair<FEdge, FModumateLayerType>> beyondLines;

	if (numLayers > 0)
	{
		for (int side = 0; side < 2; ++side)
		{
			const FLayerGeomDef& layer = side == 0 ? layers[0] : layers.Last();
			const TArray<FVector> FLayerGeomDef::* layerPoints = side == 0 ? &FLayerGeomDef::OriginalPointsA : &FLayerGeomDef::OriginalPointsB;
			const FVector finishOffset = totalThickness * layer.Normal;

			const int numPoints = layer.OriginalPointsA.Num();
			for (int i = 0; i < numPoints; ++i)
			{
				FVector point((layer.*layerPoints)[i]);
				beyondLines.Emplace(FEdge(point, (layer.*layerPoints)[(i + 1) % numPoints]), dwgOuterType);

				if (side == 1)
				{   // Lines along finish thickness.
					beyondLines.Emplace(FEdge(point, point - finishOffset), dwgOuterType);
				}
			}

			if (side == 1)
			{   // Hole outsides.
				for (const auto& hole: layer.Holes3D)
				{
					const int numHolePoints = hole.Points.Num();
					for (int i = 0; i < numHolePoints; ++i)
					{
						beyondLines.Emplace(FEdge(hole.Points[i] + finishOffset, hole.Points[(i + 1) % numHolePoints] + finishOffset),
							dwgHoleType);
						beyondLines.Emplace(FEdge(hole.Points[i], hole.Points[i] + finishOffset), dwgHoleType);
					}
				}
			}
		}
		
		FVector2D boxClipped0;
		FVector2D boxClipped1;
		for (const auto& line : beyondLines)
		{
			TArray<FEdge> clippedLineSections = ParentPage->lineClipping->ClipWorldLineToView(line.Key);

			for (const auto& lineSection : clippedLineSections)
			{
				FVector2D vert0(lineSection.Vertex[0]);
				FVector2D vert1(lineSection.Vertex[1]);

				if (UModumateFunctionLibrary::ClipLine2DToRectangle(vert0, vert1, BoundingBox, boxClipped0, boxClipped1))
				{
					TSharedPtr<FDraftingLine> draftingLine = MakeShared<FDraftingLine>(
						FModumateUnitCoord2D::WorldCentimeters(boxClipped0),
						FModumateUnitCoord2D::WorldCentimeters(boxClipped1),
						lineThickness, lineColor);
					ParentPage->Children.Add(draftingLine);
					draftingLine->SetLayerTypeRecursive(line.Value);
				}
			}
		}

	}
}

void AMOIFinish::GetDrawingDesignerItems(const FVector& ViewDirection, TArray<FDrawingDesignerLine>& OutDrawingLines, float MinLength /*= 0.0f*/) const
{
	const ADynamicMeshActor* actor = CastChecked<ADynamicMeshActor>(GetActor());
	if (actor == nullptr)
	{
		return;
	}

	const TArray<FLayerGeomDef>& layers = actor->LayerGeometries;
	const int32 numLayers = layers.Num();
	const float totalThickness = Algo::Accumulate(layers, 0.0f, [](float s, const auto& layer) { return s + layer.Thickness; });

	TArray<FEdge> finishEdges;

	if (numLayers > 0)
	{
		for (int side = 0; side < 2; ++side)
		{
			const FLayerGeomDef& layer = side == 0 ? layers[0] : layers.Last();
			const TArray<FVector> FLayerGeomDef::* layerPoints = side == 0 ? &FLayerGeomDef::OriginalPointsA : &FLayerGeomDef::OriginalPointsB;
			const FVector finishOffset = totalThickness * layer.Normal;

			const int numPoints = layer.OriginalPointsA.Num();
			for (int i = 0; i < numPoints; ++i)
			{
				FVector point((layer.*layerPoints)[i]);
				finishEdges.Emplace(point, (layer.*layerPoints)[(i + 1) % numPoints]);

				if (side == 1)
				{   // Lines along finish thickness.
					finishEdges.Emplace(point, point - finishOffset);
				}
			}

			if (side == 1)
			{   // Hole outsides.
				for (const auto& hole : layer.Holes3D)
				{
					const int numHolePoints = hole.Points.Num();
					for (int i = 0; i < numHolePoints; ++i)
					{
						finishEdges.Emplace(hole.Points[i], hole.Points[i] + finishOffset);
					}
				}
			}
		}

		float minLengthSquared = MinLength * MinLength;
		const int32 numInitialLines = OutDrawingLines.Num();
		for (const auto& edge : finishEdges)
		{
			if ((edge.Vertex[1] - edge.Vertex[0]).SizeSquared() >= minLengthSquared)
			{
				OutDrawingLines.Emplace(edge.Vertex[0], edge.Vertex[1]);
			}
		}

		for (int32 l = numInitialLines; l < OutDrawingLines.Num(); ++l)
		{
			auto& line = OutDrawingLines[l];
			line.Thickness = 0.053f;
		}
	}
}
