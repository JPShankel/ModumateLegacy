// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "Drafting/ModumateDimensions.h"

#include "Algo/AllOf.h"
#include "Algo/Replace.h"
#include "Algo/RemoveIf.h"
#include "Algo/ForEach.h"
#include "DocumentManagement/ModumateDocument.h"
#include "Graph/Graph2D.h"
#include "Objects/MetaGraph.h"
#include "Drafting/ModumateDraftingElements.h"
#include "Drafting/ModumateDraftingDraw.h"

namespace
{
	TSet<EObjectType> portalTypes = { EObjectType::OTWindow, EObjectType::OTDoor, EObjectType::OTSystemPanel };

	constexpr bool AreParallel(const FVec2d& a, const FVec2d& b)
	{
		return FMath::Abs(a.Normalized().Dot(b.Normalized())) > THRESH_NORMALS_ARE_PARALLEL;
	}
}

FModumateDimension::FModumateDimension(FVec2d StartVec, FVec2d EndVec)
{
	Points[0] = StartVec;
	Points[1] = EndVec;
	Length = (Points[1] - Points[0]).Length();
	Dir = (Points[1] - Points[0]) / Length;
	bHorizontal = AreParallel(Dir, FVec2d::UnitX());
	bVertical = AreParallel(Dir, FVec2d::UnitY());
}

//This is only called by the DD currently
bool FModumateDimensions::PopulateAndProcessDimensions(const UModumateDocument * Doc,
                                                       FPlane Plane, FVector Origin, FVector AxisX, const TSet<int32>* Groups)
{
	TSet<int32> graphIDs;

	if (Groups)
	{
		graphIDs = *Groups;
	}
	else
	{
		TArray<const AMOIMetaGraph*> groups;
		Doc->GetObjectsOfTypeCasted(EObjectType::OTMetaGraph, groups);
		Algo::Transform(groups, graphIDs, [](const AMOIMetaGraph* Group) { return Group->ID; });
	}

	const FVector axisY = Plane ^ AxisX;

	// Plane bounds for whole project:
	FBoxSphereBounds projectBounds = Doc->CalculateProjectBounds();
	FBox box = projectBounds.GetBox();
	box = box.ExpandBy(1.01);
	FVector extrema[] = { box.Min, {box.Min.X, box.Min.Y, box.Max.Z},
		{box.Min.X, box.Max.Y, box.Min.Z}, {box.Min.X, box.Max.Y, box.Max.Z},
		{box.Max.X, box.Min.Y, box.Min.Z}, {box.Max.X, box.Min.Y, box.Max.Z},
		{box.Max.X, box.Max.Y, box.Min.Z}, box.Max };

	FBox2D projectBoundingBox(ForceInitToZero);
	for (auto v: extrema)
	{
		projectBoundingBox += UModumateGeometryStatics::ProjectPoint2D(v, AxisX, axisY, Origin);
	}

	Reset();
	int32 nextID = 0;

	for (int32 graphID: graphIDs)
	{
		if (graphID == MOD_ID_NONE)
		{
			continue;
		}

		const FGraph3D& graph = *Doc->GetVolumeGraph(graphID);
		auto cutGraph = MakeShared<FGraph2D>();
		graph.Create2DGraph(Plane, AxisX, axisY, Origin, projectBoundingBox, cutGraph, GraphIDToObjID, nextID);
		cutGraph->CleanDirtyObjects(false);
		nextID = cutGraph->GetNextObjID();
	
		const auto& vertMap = cutGraph->GetVertices();
		Edges.Append(cutGraph->GetEdges());
		Vertices.Append(cutGraph->GetVertices());
	
		for (const auto& edgePair: cutGraph->GetEdges())
		{
			const FGraph2DEdge& edge = edgePair.Value;
			FVec2d a = vertMap.Find(edge.StartVertexID)->Position;
			FVec2d b = vertMap.Find(edge.EndVertexID)->Position;
	
			FModumateDimension dimension(a, b);
			dimension.Graph2DID[0] = edgePair.Key;
			dimension.Graph2DID[1] = edgePair.Key;
			GraphIDToDimID.FindOrAdd(edgePair.Key) = Dimensions.Num();
			dimension.MetaplaneID = GraphIDToObjID[edgePair.Key];
	
			const AModumateObjectInstance* planeMoi = Doc->GetObjectById(dimension.MetaplaneID);
			if (planeMoi)
			{
				const auto& children = planeMoi->GetChildObjects();
				dimension.bPortal = children.Num() != 0 && portalTypes.Contains(children[0]->GetObjectType());
			}
	
			if(dimension.Length > SMALL_NUMBER)
			{
				//All dimensions at this point are framing dimensions
				double offset = FramingDimOffset;
				FVec2d positionOffset = dimension.Dir * offset;
				if (dimension.LineSide == EDimensionSide::Left)
				{
					positionOffset = -positionOffset;
				}
	
				dimension.TextPosition = (dimension.Points[0] + dimension.Points[1]) / 2 + FVec2d(positionOffset.Y, -positionOffset.X);
				Dimensions.Add(dimension);	
			}
		}
	}

	ProcessDimensions();
	
	return true;
}

void FModumateDimensions::Reset()
{
	Dimensions.Empty();
	AngularDimensions.Empty();
	GraphIDToDimID.Empty();
	GraphIDToObjID.Empty();
	Edges.Empty();
	Vertices.Empty();
}

bool FModumateDimensions::AddDimensionsFromCutPlane(TSharedPtr<FDraftingComposite>& Page,
    const UModumateDocument * Doc, FPlane Plane, FVector Origin, FVector AxisX, const TSet<int32>* Groups /*= nullptr*/)
{
	PopulateAndProcessDimensions(Doc, Plane, Origin, AxisX, Groups);

	for (auto& dim: Dimensions)
	{
		if (dim && dim.Length > SMALL_NUMBER)
		{
			FModumateLayerType layerType = FModumateLayerType::kDimensionOpening;
			switch (dim.DimensionType)
			{
			case EDimensionType::Opening:
				layerType = FModumateLayerType::kDimensionOpening;
				break;

			case EDimensionType::Framing:
				layerType = FModumateLayerType::kDimensionFraming;
				break;

			case EDimensionType::Massing:
				layerType = FModumateLayerType::kDimensionMassing;
				break;

			case EDimensionType::Bridging:
				layerType = FModumateLayerType::kDimensionMassing;
				break;

			case EDimensionType::Reference:
				layerType = FModumateLayerType::KDimensionReference;
				break;

			default:
				ensure(false);
				break;
			}

			if (dim.DimensionType == EDimensionType::Reference)
			{
				TSharedPtr<FDraftingLine> refLine = MakeShared<FDraftingLine>(
					FModumateUnitCoord2D::WorldCentimeters(FVector2D(dim.Points[0])),
					FModumateUnitCoord2D::WorldCentimeters(FVector2D(dim.Points[1])),
					ModumateUnitParams::FThickness::Points(0.25f)
					);
				Page->Children.Add(refLine);
				refLine->SetLayerTypeRecursive(layerType);
			}
			else
			{
				TSharedPtr<FDimensionPrimitive> dimPrim = MakeShared<FDimensionPrimitive>(
					FModumateUnitCoord2D::WorldCentimeters(FVector2D(dim.Points[0])),
					FModumateUnitCoord2D::WorldCentimeters(FVector2D(dim.Points[1])),
					FModumateUnitCoord2D::WorldCentimeters(FVector2D(dim.TextPosition)),
					FMColor::Black);
				Page->Children.Add(dimPrim);
				dimPrim->SetLayerTypeRecursive(layerType);
			}
		}
	}

	for (const auto& angularDim : AngularDimensions)
	{
		TSharedPtr<FAngularDimensionPrimitive> angularDimPrim = MakeShared<FAngularDimensionPrimitive>(
			FModumateUnitCoord2D::WorldCentimeters(FVector2D(angularDim.Start)),
			FModumateUnitCoord2D::WorldCentimeters(FVector2D(angularDim.End)),
			FModumateUnitCoord2D::WorldCentimeters(FVector2D(angularDim.Center))
			);
		Page->Children.Add(angularDimPrim);
		angularDimPrim->SetLayerTypeRecursive(FModumateLayerType::kDimensionFraming);
	}

	return true;
}

TArray<FModumateDimension> FModumateDimensions::GetDimensions(const UModumateDocument* Doc, FPlane Plane,
	FVector Origin, FVector AxisX)
{
	PopulateAndProcessDimensions(Doc, Plane, Origin, AxisX, nullptr);
	return this->Dimensions; //Copy on purpose
}

void FModumateDimensions::ProcessDimensions()
{
	auto findByEdge = [&](int32 id)
		{ return Dimensions.FindByPredicate([id](FModumateDimension& d) { return d.Graph2DID[0] == id; }); };

	// Fill in connectivity from Graph2D.
	for (auto& d: Dimensions)
	{
		const FGraph2DEdge& edge = Edges[d.Graph2DID[0]];
		const FGraph2DVertex* vert[2] = { &Vertices[edge.StartVertexID], &Vertices[edge.EndVertexID] };
		for (int32 i = 0; i < 2; ++i)
		{
			for (FGraphSignedID edgeIDSigned: vert[i]->Edges)
			{
				int32 edgeID = FMath::Abs(edgeIDSigned);
				if (edgeID != d.Graph2DID[0])
				{
					d.Connections[i].Add(findByEdge(edgeID) - &Dimensions[0]);
				}
			}
		}
	}

	TArray<TSet<int32>> planIslands;
	for (int32 d = 0; d < Dimensions.Num(); ++d)
	{
		if (Algo::AllOf(planIslands, [d](const TSet<int32>& e) {return !e.Contains(d); }))
		{
			AddEdgeAndConnected(d, planIslands.AddDefaulted_GetRef());
		}
	}

	for (const auto& group: planIslands)
	{
		ProcessConnectedGroup(group);
	}

	ConnectIslands(planIslands);

}

void FModumateDimensions::AddEdgeAndConnected(int32 Edge, TSet<int32>& OutEdges) const
{
	OutEdges.Add(Edge);
	for (int vertex = 0; vertex < 2; ++vertex)
	{
		for (int32 edgeID: Dimensions[Edge].Connections[vertex])
		{
			if (!OutEdges.Contains(edgeID))
			{
				AddEdgeAndConnected(edgeID, OutEdges);
			}
		}
	}
}

void FModumateDimensions::ProcessConnectedGroup(const TSet<int32>& Group)
{
	TArray<int32> framingDimensions(Group.Array());
	framingDimensions.Reserve(Dimensions.Num());
	TArray<int32> replacementIds;

	FBox2D boundingBox(ForceInit);
	for (int32 d: Group)
	{
		boundingBox += FVector2D(Dimensions[d].Points[0]);
		boundingBox += FVector2D(Dimensions[d].Points[1]);
	}

	// 1. Set dimension text/dimension line side away from centre.
	const FVec2d centrePoint = boundingBox.GetCenter();
	for (int32 d: Group)
	{
		auto& dim = Dimensions[d];
		dim.LineSide = dim.Dir.Cross(centrePoint - dim.Points[0]) > 0 ? EDimensionSide::Left : EDimensionSide::Right;
	}

	// 2. Replace portals in a single wall section, plus wall section, with new framing dimension.
	for (int32& d: framingDimensions)
	{
		if (d >= 0 && Dimensions[d].bPortal)
		{
			int32 metaplaneID = INDEX_NONE;  // Meta-plane ID.
			if (Dimensions[d].Connections[0].Num() == 1)
			{
				metaplaneID = Dimensions[Dimensions[d].Connections[0][0]].MetaplaneID;
			}
			else if (Dimensions[d].Connections[1].Num() == 1)
			{
				metaplaneID = Dimensions[Dimensions[d].Connections[1][0]].MetaplaneID;
			}
			if (metaplaneID != INDEX_NONE)
			{
				// For portal dims move left and right, coalescing simple runs of portals & unique wall/floor.
				TArray<int32> removeIds[2];
				int32 outerVerts[2];
				for (int32 v = 0; v < 2; ++v)
				{
					int32 current = d;
					int32 nextVert = v;
					FVec2d direction(Dimensions[d].Dir);
					while (true)
					{
						FModumateDimension& dim = Dimensions[current];
						const FVec2d& point = dim.Points[nextVert];
						if (dim.Connections[nextVert].Num() != 1)
						{
							break;
						}
						current = dim.Connections[nextVert][0];
						if (!Dimensions[current].bPortal && Dimensions[current].MetaplaneID != metaplaneID)
						{
							break;
						}
						if (!AreParallel(direction, Dimensions[current].Dir))
						{
							break;
						}
						const int32 vectorSize = removeIds[v].Num();
						// Check for dimension cycles along wall.
						if (!ensure(removeIds[v].AddUnique(current) == vectorSize))
						{   // Don't remove any such cycles and make original dimension active.
							removeIds[v].Empty();
							Dimensions[d].bActive = true;
							break;
						}
						nextVert = Dimensions[current].Points[0] == point ? 1 : 0;
						outerVerts[v] = nextVert;
					}
				}

				if (removeIds[0].Num() + removeIds[1].Num() > 0)
				{ // Replace portal and contiguous runs of same wall & portals with one new dimension
					if (removeIds[0].Num() == 0)
					{
						removeIds[0].Add(d);
						outerVerts[0] = 0;
					}
					else if (removeIds[1].Num() == 0)
					{
						removeIds[1].Add(d);
						outerVerts[1] = 1;
					}

					int32 firstRemovedId = removeIds[0].Last();
					int32 lastRemovedId = removeIds[1].Last();
					FModumateDimension& firstDim = Dimensions[firstRemovedId];
					FModumateDimension& lastDim = Dimensions[lastRemovedId];

					FModumateDimension newDim(firstDim.Points[outerVerts[0]], lastDim.Points[outerVerts[1]]);
					newDim.Connections[0] = firstDim.Connections[outerVerts[0]];
					newDim.Connections[1] = lastDim.Connections[outerVerts[1]];
					newDim.LineSide = firstDim.LineSide;
					newDim.Graph2DID[0] = firstDim.Graph2DID[outerVerts[0]];
					newDim.Graph2DID[1] = lastDim.Graph2DID[outerVerts[1]];

					int32 newDimIndex = Dimensions.Num();
					Dimensions.Add(newDim);
					replacementIds.Add(newDimIndex);

					// Hookup
					for (int32 connectedDim: newDim.Connections[0])
					{
						Algo::Replace(Dimensions[connectedDim].Connections[0], firstRemovedId, newDimIndex);
						Algo::Replace(Dimensions[connectedDim].Connections[1], firstRemovedId, newDimIndex);
					}
					for (int32 connectedDim: newDim.Connections[1])
					{
						Algo::Replace(Dimensions[connectedDim].Connections[0], lastRemovedId, newDimIndex);
						Algo::Replace(Dimensions[connectedDim].Connections[1], lastRemovedId, newDimIndex);
					}

					// Mark dropped dimensions as unused for main dimensions:
					DropLongestOpeningDimension(removeIds);
					Dimensions[d].bActive = true;
					Dimensions[d].DimensionType = EDimensionType::Opening;
					d =INDEX_NONE;
					for (int v = 0; v < 2; ++v)
					{
						for (int32 droppedId: removeIds[v])
						{
							Dimensions[droppedId].DimensionType = EDimensionType::Opening;
							int32 droppedIndex = framingDimensions.Find(droppedId);
							if (droppedIndex != INDEX_NONE)
							{
								framingDimensions[droppedIndex] = INDEX_NONE;
							}
						}
					}
				}
			}
		}
	}

	// Remove references to no-longer-used dimensions (erased as INDEX_NONE) and add the new references:
	framingDimensions.SetNum(Algo::RemoveIf(framingDimensions, [](int32 a) { return a == INDEX_NONE; }));
	framingDimensions.Append(replacementIds);

	// 3. Identify perimeter.
	// First find lowest point:
	double minY = BIG_NUMBER;
	int32 lowestDim = INDEX_NONE;
	int32 lowIndex = 0;

	for (int32 d: framingDimensions)
	{
		const FModumateDimension& dim = Dimensions[d];
		for (int32 i = 0; i < 2; ++i)
		{
			if (dim.Points[i].Y < minY)
			{
				lowestDim = d;
				minY = dim.Points[i].Y;
				lowIndex = i;
			}
		}
	}

	if (lowestDim == INDEX_NONE)
	{
		ensure(false);
		return;
	}

	FVec2d startPoint = Dimensions[lowestDim].Points[lowIndex];
	TArray<int32> lowestEdges(Dimensions[lowestDim].Connections[lowIndex]);
	lowestEdges.Add(lowestDim);

	// Find outside edge incident on lowest point:
	lowestDim = INDEX_NONE;
	double maxX = -BIG_NUMBER;
	for (int32 d: lowestEdges)
	{
		double x = Dimensions[d].Dir.X;
		if (Dimensions[d].Points[1] == startPoint)
		{
			x = -x;
		}
		if (x > maxX)
		{
			maxX = x;
			lowestDim = d;
		}
	}

	if (!ensure(lowestDim != INDEX_NONE))
	{
		return;
	}

	// startPoint/lowestDim is start point for perimeter.

	TSet<int32> perimeter;
	TArray<int32> perimeterArray;

	int32 current = lowestDim;
	FVec2d currentPoint = startPoint;

	// Walk around perimeter anticlockwise:
	while (!perimeter.Contains(current))
	{
		perimeter.Add(current);
		perimeterArray.Add(current);
		auto& dim = Dimensions[current];

		for (int32 i = 0; i < 2; ++i)
		{
			if (dim.Points[i] == currentPoint)
			{
				currentPoint = dim.Points[1 - i];
				current = SmallestConnectedEdgeAngle(currentPoint, i == 0 ? -dim.Dir : dim.Dir, dim.Connections[1 - i]);
				dim.LineSide = i == 0 ? EDimensionSide::Right : EDimensionSide::Left;  // Text always outside perim
				break;
			}

		}
		if (current == INDEX_NONE)
		{
			break;
		}

	}

	// Perimeter dimensions are shown and considered fixed.
	for (int32 d: perimeter)
	{
		Dimensions[d].bActive = true;
		Dimensions[d].Depth = 0;
		Dimensions[d].StartFixed.X = true;
		Dimensions[d].StartFixed.Y = true;
		Dimensions[d].EndFixed.X = true;
		Dimensions[d].EndFixed.Y = true;
		PropagateVerticalStatus(d, 0);
		PropagateVerticalStatus(d, 1);
		PropagateHorizontalStatus(d, 0);
		PropagateHorizontalStatus(d, 1);
	}

	// 4. Drop dimensions for which ends are derived via other orthogonal dimensions.
	int depth = 1;
	bool bContinue = false;
	do
	{
		bContinue = false;
		for (int32 d: framingDimensions)
		{
			auto& dim = Dimensions[d];

			for (int i = 0; i < 2; ++i)
			{
				FVec2d point = dim.Points[i];
				for (int32 n: dim.Connections[i])
				{
					const auto& nb = Dimensions[n];

					if (dim.Depth > depth && nb.Depth == depth - 1)
					{
						dim.Depth = depth;
						bContinue = true;
					}
				}
			}
		}

		++depth;
	} while (bContinue);

	const int32 maxDepth = depth - 1;
	for (depth = 1; depth <= maxDepth; ++depth)
	{
		for (int32 d: framingDimensions)
		{
			auto& dim = Dimensions[d];
			if (dim.Depth == depth)
			{
				if (dim.bHorizontal)
				{
					// If either X position is unknown then draw dimension, then
					// if either is known both will be.
					if (!dim.StartFixed.X || !dim.EndFixed.X)
					{
						dim.bActive = true;
						dim.StartFixed.X = dim.EndFixed.X = dim.StartFixed.X || dim.EndFixed.X;
						if (dim.StartFixed.X)
						{
							PropagateVerticalStatus(d, 0);
							PropagateVerticalStatus(d, 1);
						}
					}
				}
				else if (dim.bVertical)
				{
					if (!dim.StartFixed.Y || !dim.EndFixed.Y)
					{
						dim.bActive = true;
						dim.StartFixed.Y = dim.EndFixed.Y = dim.StartFixed.Y || dim.EndFixed.Y;
						if (dim.StartFixed.Y)
						{
							PropagateHorizontalStatus(d, 0);
							PropagateHorizontalStatus(d, 1);
						}

					}
				}
				else
				{
					dim.bActive = true;
				}

				PropagateStatus(d, 0);
				PropagateStatus(d, 1);
			}

		}
	}

	// 5. Add any angular dimensions.
	AddAngularDimensions(framingDimensions);
	
	// 6. Combine linear perimeter dimensions into massing dimensions.
	for (int32 startP  = 0; startP < perimeterArray.Num();)
	{
		int32 dimIndex = perimeterArray[startP];
		FVec2d perimDir = Dimensions[dimIndex].Dir;
		int32 endP = startP + 1;
		FVec2d nextPoint = startPoint == Dimensions[dimIndex].Points[0] ? Dimensions[dimIndex].Points[1] : Dimensions[dimIndex].Points[0];
		while (endP < perimeterArray.Num() && AreParallel(perimDir, Dimensions[perimeterArray[endP]].Dir))
		{
			int32 d = perimeterArray[endP];
			nextPoint = nextPoint == Dimensions[d].Points[0] ? Dimensions[d].Points[1] : Dimensions[d].Points[0];
			++endP;
		}

		FModumateDimension massingDim(startPoint, nextPoint);
		massingDim.DimensionType = EDimensionType::Massing;
		massingDim.bActive = true;
		massingDim.LineSide = EDimensionSide::Right;
		if (massingDim.Length > SMALL_NUMBER)
		{
			Dimensions.Add(massingDim);
		}

		if (endP - startP == 1)
		{   // If containing single framing dimension then drop that dimension.
			Dimensions[perimeterArray[startP]].bActive = false;
		}

		startP = endP;
		startPoint = nextPoint;
	}
}

// Find connected dimension with smallest signed angle.
int32 FModumateDimensions::SmallestConnectedEdgeAngle(const FVec2d& Point, const FVec2d& Dir,
	const TArray<int32>& Connected) const
{
	double minAngle = 5.0;
	int32 minEdge = INDEX_NONE;
	for (int32 edgeID: Connected)
	{
		const auto& edge = Dimensions[edgeID];
		FVec2d edgeDir(edge.Points[0] == Point ? edge.Dir : -edge.Dir);
		double dotprod = Dir.Dot(edgeDir);
		// Angle is ordered, [0.0 .. 4.0)
		double angle = Dir.Cross(edgeDir) >= 0 ? 1.0 - dotprod : 3.0 + dotprod;
		if (angle < minAngle)
		{
			minEdge = edgeID;
			minAngle = angle;
		}
	}

	return minEdge;
}

void FModumateDimensions::PropagateStatus(int32 d, int32 vert)
{
	const auto& dim = Dimensions[d];
	FVec2d point = dim.Points[vert];
	for (int32 n: dim.Connections[vert])
	{
		if (Dimensions[n].Points[0] == point)
		{
			Dimensions[n].StartFixed.X |= (vert == 0 ? dim.StartFixed.X : dim.EndFixed.X);
			Dimensions[n].StartFixed.Y |= (vert == 0 ? dim.StartFixed.Y : dim.EndFixed.Y);
		}
		else
		{
			Dimensions[n].EndFixed.X |= (vert == 0 ? dim.StartFixed.X : dim.EndFixed.X);
			Dimensions[n].EndFixed.Y |= (vert == 0 ? dim.StartFixed.Y : dim.EndFixed.Y);
		}
	}
}

void FModumateDimensions::PropagateHorizontalStatus(int32 d, int32 vert)
{
	auto& dim = Dimensions[d];

	dim.StartFixed.Y = true;
	dim.EndFixed.Y = true;
	PropagateStatus(d, 0);
	PropagateStatus(d, 1);

	const FVec2d& point = dim.Points[vert];
	for (int32 connectIdx = 0; connectIdx < Dimensions[d].Connections[vert].Num(); ++connectIdx)
	{
		int32 n = Dimensions[d].Connections[vert][connectIdx];
		if (Dimensions[n].bHorizontal)
		{
			FVec2d farPoint = FarPoint(d, vert, connectIdx);
			// Check that we keep going horizontally in the same direction.
			if (!dim.bHorizontal || ((farPoint.X > point.X) ^ (point.X < dim.Points[1 - vert].X)) )
			{
				PropagateHorizontalStatus(n, Dimensions[n].Points[0] == point ? 1 : 0);
			}
		}
	}
}

void FModumateDimensions::PropagateVerticalStatus(int32 d, int32 vert)
{
	auto& dim = Dimensions[d];

	dim.StartFixed.X = true;
	dim.EndFixed.X = true;
	PropagateStatus(d, 0);
	PropagateStatus(d, 1);

	const FVec2d& point = dim.Points[vert];
	for (int32 connectIdx = 0; connectIdx < Dimensions[d].Connections[vert].Num(); ++connectIdx)
	{
		int32 n = Dimensions[d].Connections[vert][connectIdx];
		if (Dimensions[n].bVertical)
		{
			FVec2d farPoint = FarPoint(d, vert, connectIdx);
			// Check that we keep going vertically in the same direction.
			if (!dim.bVertical || ((farPoint.Y > point.Y) ^ (point.Y < dim.Points[1 - vert].Y)) )
			{
				PropagateVerticalStatus(n, Dimensions[n].Points[0] == point ? 1 : 0);
			}
		}
	}
}

void FModumateDimensions::DropLongestOpeningDimension(const TArray<int32>* OpeningIds)
{
	int32 longestIndex = INDEX_NONE;
	double dimensionLength = 0.0;
	for (int32 v = 0; v < 2; ++v)
	{
		for (int32 d: OpeningIds[v])
		{
			if (!Dimensions[d].bPortal && Dimensions[d].Length > dimensionLength)
			{
				dimensionLength = Dimensions[d].Length;
				longestIndex = d;
			}
		}
	}

	for (int32 v = 0; v < 2; ++v)
	{
		for (int32 d: OpeningIds[v])
		{
			Dimensions[d].bActive = d != longestIndex;
		}
	}
}

// Add a new angular dimension from Vertex1 of Edge1 to its incident edge Edge2.
void FModumateDimensions::CreateAngularDimension(int32 Edge1, int32 Vertex1, int32 Edge2)
{
	static constexpr double pointOffset = 20.0;  // World units
	FVec2d center = Dimensions[Edge1].Points[Vertex1];
	FVec2d startPos = Vertex1 == 0 ? center + pointOffset * Dimensions[Edge1].Dir
		: center - pointOffset * Dimensions[Edge1].Dir;

	const auto& dim2 = Dimensions[Edge2];
	FVec2d endPos = dim2.Points[0] == center ? center + pointOffset * dim2.Dir
		: center - pointOffset * dim2.Dir;
	AngularDimensions.Emplace(startPos, endPos, center);
}

namespace
{
	static constexpr float thresholdDegrees = 1.0f;

	constexpr bool IsCalledoutAngle(float angleDegrees)
	{
		return (angleDegrees >= thresholdDegrees && angleDegrees <= (90.0f - thresholdDegrees))
			|| (angleDegrees >= (90.0f + thresholdDegrees) && angleDegrees <= (180.0f - thresholdDegrees));
	}
}

void FModumateDimensions::AddAngularDimensions(const TArray<int32>& Group)
{
	TSet<int32> processedEdges;

	for (int32 edge : Group)
	{
		const auto& dim = Dimensions[edge];
		for (int vertex = 0; vertex < 2; ++vertex)
		{
			int32 edgeID = dim.Graph2DID[vertex];
			TArray<int32> graph2dVertexIDs;
			const FGraph2DEdge* graphEdge = &Edges[edgeID];
			float angle = GetEdgeAngle(graphEdge);
			if (vertex == 1)
			{
				angle += 180.0f;
			}

			graphEdge->GetVertexIDs(graph2dVertexIDs);
			TArray<int32> adjacentEdges = Vertices[graph2dVertexIDs[vertex]].Edges;
			Algo::ForEach(adjacentEdges, [](int32& a) { a = FMath::Abs(a); });
			const int32 numEdges = adjacentEdges.Num();

			if (numEdges > 1)
			{
				const int32 e = adjacentEdges.Find(edgeID);
				ensureAlways(e != INDEX_NONE);
				const FGraph2DEdge* graphEdge0 = &Edges[adjacentEdges[(e + 1) % numEdges]];
				if (!processedEdges.Contains(graphEdge0->ID))
				{
					float angle0 = GetEdgeAngle(graphEdge0);
					if (graph2dVertexIDs[vertex] != graphEdge0->StartVertexID)
					{
						angle0 += 180.0f;
					}
					float diff = FRotator::ClampAxis(angle0 - angle);
					if (IsCalledoutAngle(diff))
					{
						CreateAngularDimension(edge, vertex, GraphIDToDimID[graphEdge0->ID]);
					}
				}

				const FGraph2DEdge* graphEdge1 = &Edges[adjacentEdges[(e + numEdges - 1) % numEdges]];
				if (!processedEdges.Contains(graphEdge1->ID))
				{
					float angle1 = GetEdgeAngle(graphEdge1);
					if (graph2dVertexIDs[vertex] != graphEdge1->StartVertexID)
					{
						angle1 += 180.0f;
					}
					float diff = FRotator::ClampAxis(angle - angle1);
					if (IsCalledoutAngle(diff))
					{
						int32 otherVertex = graph2dVertexIDs[vertex] == graphEdge1->StartVertexID ? 0 : 1;
						CreateAngularDimension(GraphIDToDimID[graphEdge1->ID], otherVertex, edge);
					}
				}

				processedEdges.Add(graphEdge->ID);
			}
		}
	}
}

namespace
{
	struct BridgeSpec
	{
		int32 DimA;
		int32 VertA;
		int32 DimB;
		int32 VertB;
		bool operator==(const BridgeSpec& rhs) const
		{
			return (DimA == rhs.DimA && VertA == rhs.VertA && DimB == rhs.DimB && VertB == rhs.VertB)
				|| (DimA == rhs.DimB && VertA == rhs.VertB && DimB == rhs.DimA && VertB == rhs.VertA);
		}
	};

}

void FModumateDimensions::ConnectIslands(const TArray<TSet<int32>>& plans)
{
	const int32 numPlans = plans.Num();
	if (numPlans < 2)
	{
		return;
	}

	TArray<BridgeSpec> bridges;

	for (int32 plan = 0; plan < numPlans; ++plan)
	{
		TArray<int32> otherDimensions;
		for (int32 p = 1; p < numPlans; ++p)
		{
			otherDimensions.Append(plans[(plan + p) % numPlans].Array());
		}

		double minDist2 = TMathUtilConstants<double>::MaxReal;
		double minDist = TMathUtilConstants<double>::MaxReal;
		int32 minPlan1Dim = INDEX_NONE;
		int32 minPlan2Dim = INDEX_NONE;
		int32 minVert1 = INDEX_NONE;
		int32 minVert2 = INDEX_NONE;

		// Brute-force search for closest pair -
		for (int32 plan1Dimension : plans[plan])
		{
			for (int planVertex = 0; planVertex < 2; ++planVertex)
			{
				FVec2d plan1Position = Dimensions[plan1Dimension].Points[planVertex];
				for (int32 plan2Dimension : otherDimensions)
				{
					for (int32 plan2Vertex = 0; plan2Vertex < 2; ++plan2Vertex)
					{
						if (FMath::Abs(Dimensions[plan2Dimension].Points[plan2Vertex].X - plan1Position.X) < minDist)
						{
							FVec2d delta = Dimensions[plan2Dimension].Points[plan2Vertex] - plan1Position;
							double dist2 = plan1Position.DistanceSquared(Dimensions[plan2Dimension].Points[plan2Vertex]);
							if (dist2 < minDist2)
							{
								minDist2 = dist2;
								minDist = FMathd::Sqrt(dist2);

								minPlan1Dim = plan1Dimension;
								minPlan2Dim = plan2Dimension;
								minVert1 = planVertex;
								minVert2 = plan2Vertex;
							}
						}
					}
				}
			}
		}

		if (minPlan1Dim != INDEX_NONE)
		{
			BridgeSpec newBridge = { minPlan1Dim, minVert1, minPlan2Dim, minVert2 };
			if (!bridges.Contains(newBridge))
			{
				FModumateDimension bridgingDim(Dimensions[minPlan1Dim].Points[minVert1], Dimensions[minPlan2Dim].Points[minVert2]);
				bridgingDim.DimensionType = EDimensionType::Bridging;
				bridgingDim.bActive = true;
				Dimensions.Add(bridgingDim);

				bridges.Add(newBridge);
			}
		}
	}

	for (const auto& bridge : bridges)
	{
		FVec2d v1(Dimensions[bridge.DimA].Points[bridge.VertA]);
		FVec2d v2(Dimensions[bridge.DimB].Points[bridge.VertB]);

		FModumateDimension bridgingDim(v1, v2);
		bridgingDim.DimensionType = EDimensionType::Bridging;
		bridgingDim.bActive = true;
		Dimensions.Add(bridgingDim);

		FModumateDimension referenceDim(v1, v2);
		referenceDim.DimensionType = EDimensionType::Reference;
		referenceDim.bActive = true;
		int32 refDimIndex = Dimensions.Add(referenceDim);

		FVec2d dir(referenceDim.Dir);
		float angle = FRotator::ClampAxis(FMath::RadiansToDegrees(FMathd::Atan2(dir.Y, dir.X)) );

		for (int32 bridgeVertex = 0; bridgeVertex < 2; ++bridgeVertex)
		{
			int32 currentVert = bridgeVertex == 0 ? bridge.VertA : bridge.VertB;
			int32 edgeID = bridgeVertex == 0 ? Dimensions[bridge.DimA].Graph2DID[currentVert]
				: Dimensions[bridge.DimB].Graph2DID[currentVert];

			FGraph2DEdge& graphEdge = Edges[edgeID];
			TArray<int32> graphVerts;
			graphEdge.GetVertexIDs(graphVerts);
			TArray<int32> adjacentEdges = Vertices[graphVerts[currentVert]].Edges;
			float minAngle = 400.0f, maxAngle = -1.0f;
			int32 minEdge = INDEX_NONE, maxEdge = INDEX_NONE;
			for (int32 edge : adjacentEdges)
			{
				float adjacentAngle = GetEdgeAngle(&Edges[FMath::Abs(edge)], edge < 0);
				float angleDiff = FRotator::ClampAxis(adjacentAngle - angle);
				if (angleDiff < minAngle)
				{
					minAngle = angleDiff;
					minEdge = edge;
				}
				if (angleDiff > maxAngle)
				{
					maxAngle = angleDiff;
					maxEdge = edge;
				}

			}

			if (IsCalledoutAngle(minAngle))
			{
				CreateAngularDimension(refDimIndex, bridgeVertex, GraphIDToDimID[FMath::Abs(minEdge)]);
			}

			maxAngle = 360.0f - maxAngle;
			if (IsCalledoutAngle(maxAngle))
			{
				CreateAngularDimension(GraphIDToDimID[FMath::Abs(maxEdge)], maxEdge > 0 ? 0 : 1, refDimIndex);
			}

			// Reference line angle from 2nd point:
			angle = FRotator::ClampAxis(angle + 180.0f);
		}

	}
}

FVec2d FModumateDimensions::FarPoint(int32 DimensionIndex, int32 VertexIndex, int32 ConnectionIndex) const
{
	int32 connectedEdge = Dimensions[DimensionIndex].Connections[VertexIndex][ConnectionIndex];
	if (Dimensions[connectedEdge].Points[0] == Dimensions[DimensionIndex].Points[VertexIndex])
	{
		return Dimensions[connectedEdge].Points[1];
	}
	else
	{
		return Dimensions[connectedEdge].Points[0];
	}
}

float FModumateDimensions::GetEdgeAngle(const FGraph2DEdge* Edge, bool bFlippedEdge /*= false*/)
{
	return bFlippedEdge ? FRotator::ClampAxis(Edge->CachedAngle + 180.0f) : Edge->CachedAngle;
}
