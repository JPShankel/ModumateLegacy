// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "Drafting/ModumateDimensions.h"

#include "Algo/AllOf.h"
#include "Algo/Replace.h"
#include "Algo/RemoveIf.h"
#include "DocumentManagement/ModumateDocument.h"
#include "Graph/Graph2D.h"
#include "Drafting/ModumateDraftingElements.h"
#include "Drafting/ModumateDraftingDraw.h"

namespace
{
	TSet<EObjectType> portalTypes = { EObjectType::OTWindow, EObjectType::OTDoor, EObjectType::OTSystemPanel };
	constexpr bool AreParallel(const FVec2d& a, const FVec2d& b)
	{
		return FMath::Abs(a.Normalized().Dot(b.Normalized()) ) > 1.0 - KINDA_SMALL_NUMBER;
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

bool FModumateDimensions::AddDimensionsFromCutPlane(TSharedPtr<Modumate::FDraftingComposite>& Page,
	const UModumateDocument * Doc, FPlane Plane, FVector Origin, FVector AxisX)
{
	const Modumate::FGraph3D& graph = Doc->GetVolumeGraph();

	const FVector axisY = Plane ^ AxisX;

	// Plane bounds for whole project:
	FBoxSphereBounds projectBounds = Doc->CalculateProjectBounds();
	FBox box = projectBounds.GetBox();
	box.ExpandBy(1.01);
	FVector extrema[] = { box.Min, {box.Min.X, box.Min.Y, box.Max.Z},
		{box.Min.X, box.Max.Y, box.Min.Z}, {box.Min.X, box.Max.Y, box.Max.Z},
		{box.Max.X, box.Min.Y, box.Min.Z}, {box.Max.X, box.Min.Y, box.Max.Z},
		{box.Max.X, box.Max.Y, box.Min.Z}, box.Max };

	FBox2D projectBoundingBox(ForceInitToZero);
	for (auto v: extrema)
	{
		projectBoundingBox += UModumateGeometryStatics::ProjectPoint2D(v, AxisX, axisY, Origin);
	}

	CutGraph = MakeShared<Modumate::FGraph2D>();
	GraphIDToObjID.Empty();
	graph.Create2DGraph(Plane, AxisX, axisY, Origin, projectBoundingBox, CutGraph, GraphIDToObjID);

	Dimensions.Empty();

	const auto& vertMap = CutGraph->GetVertices();

	for (const auto& edgePair: CutGraph->GetEdges())
	{
		const Modumate::FGraph2DEdge& edge = edgePair.Value;
		FVec2d a = vertMap.Find(edge.StartVertexID)->Position;
		FVec2d b = vertMap.Find(edge.EndVertexID)->Position;

		FModumateDimension dimension(a, b);
		dimension.Graph2DID = edgePair.Key;
		dimension.MetaplaneID = GraphIDToObjID[edgePair.Key];

		const AModumateObjectInstance* planeMoi = Doc->GetObjectById(dimension.MetaplaneID);
		if (planeMoi)
		{
			const auto& children = planeMoi->GetChildObjects();
			dimension.bPortal = children.Num() != 0 && portalTypes.Contains(children[0]->GetObjectType());
		}

		Dimensions.Add(dimension);
	}

	ProcessDimensions();

	for (auto& dim: Dimensions)
	{
		if (dim)
		{
			double offset = 0.0;
			Modumate::FModumateLayerType layerType = Modumate::FModumateLayerType::kDimensionOpening;
			switch (dim.DimensionType)
			{
			case FModumateDimension::EType::Opening:
				offset = OpeningDimOffset;
				layerType = Modumate::FModumateLayerType::kDimensionOpening;
				break;

			case FModumateDimension::EType::Framing:
				layerType = Modumate::FModumateLayerType::kDimensionFraming;
				offset = FramingDimOffset;
				break;

			case FModumateDimension::EType::Massing:
				offset = MassingDimOffset;
				layerType = Modumate::FModumateLayerType::kDimensionMassing;
				break;

			default:
				ensure(false);
				break;
			}

			FVec2d positionOffset = dim.Dir * offset;
			if (dim.LineSide == FModumateDimension::Right)
			{
				positionOffset = -positionOffset;
			}
			dim.TextPosition = (dim.Points[0] + dim.Points[1]) / 2 + FVec2d(positionOffset.Y, -positionOffset.X);

			TSharedPtr<Modumate::FDimensionPrimitive> dimPrim = MakeShared<Modumate::FDimensionPrimitive>(
				FModumateUnitCoord2D::WorldCentimeters(FVector2D(dim.Points[0])),
				FModumateUnitCoord2D::WorldCentimeters(FVector2D(dim.Points[1])),
				FModumateUnitCoord2D::WorldCentimeters(FVector2D(dim.TextPosition)),
				Modumate::FMColor::Black);
			Page->Children.Add(dimPrim);
			dimPrim->SetLayerTypeRecursive(layerType);
		}
	}
	return true;
}

void FModumateDimensions::ProcessDimensions()
{
	const auto& edges = CutGraph->GetEdges();
	const auto& verts = CutGraph->GetVertices();

	auto findByEdge = [&](int32 id)
		{ return Dimensions.FindByPredicate([id](FModumateDimension& d) { return d.Graph2DID == id; }); };

	// Fill in connectivity from Graph2D.
	for (auto& d: Dimensions)
	{
		const Modumate::FGraph2DEdge& edge = edges[d.Graph2DID];
		const Modumate::FGraph2DVertex* vert[2] = { &verts[edge.StartVertexID], &verts[edge.EndVertexID] };
		for (int32 i = 0; i < 2; ++i)
		{
			for (FGraphSignedID edgeIDSigned: vert[i]->Edges)
			{
				int32 edgeID = FMath::Abs(edgeIDSigned);
				if (edgeID != d.Graph2DID)
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
		dim.LineSide = dim.Dir.Cross(centrePoint - dim.Points[0]) > 0 ? FModumateDimension::Left : FModumateDimension::Right;
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
						removeIds[v].Add(current);
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
					Dimensions[d].DimensionType = FModumateDimension::Opening;
					d =INDEX_NONE;
					for (int v = 0; v < 2; ++v)
					{
						for (int32 droppedId: removeIds[v])
						{
							Dimensions[droppedId].DimensionType = FModumateDimension::Opening;
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
				dim.LineSide = i == 0 ? FModumateDimension::Left : FModumateDimension::Right;  // Text always outside perim
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
	
	// 5. Combine linear perimeter dimensions into massing dimensions.
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
		massingDim.DimensionType = FModumateDimension::Massing;
		massingDim.bActive = true;
		massingDim.LineSide = Dimensions[perimeterArray[startP]].LineSide;
		Dimensions.Add(massingDim);
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
	for (int32 n: Dimensions[d].Connections[vert])
	{
		if (Dimensions[n].bHorizontal)
		{
			PropagateHorizontalStatus(n, Dimensions[n].Points[0] == point ? 1 : 0);
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
	for (int32 n: Dimensions[d].Connections[vert])
	{
		if (Dimensions[n].bVertical)
		{
			PropagateVerticalStatus(n, Dimensions[n].Points[0] == point ? 1 : 0);
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
