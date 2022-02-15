// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Drafting/ModumateLineCorral.h"
#include "ModumateCore/ModumateGeometryStatics.h"
#include "ModumateCore/ModumateStats.h"
#include "SegmentTypes.h"

DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("Modumate Line Coalescing"), STAT_ModumateLineCoalescing, STATGROUP_Modumate);


// Map FModumateLayerType priorities in terms of obscuring in DWG.
const FModumateLayerType FModumateLineCorral::LayerTypeOrder[] =
{
	FModumateLayerType::kSeparatorCutOuterSurface,
	FModumateLayerType::kSeparatorCutMinorLayer,
	FModumateLayerType::kSeparatorCutStructuralLayer,
	FModumateLayerType::kBeamColumnCut,
	FModumateLayerType::kOpeningSystemCutLine,
	FModumateLayerType::kMullionCut,
	FModumateLayerType::kSystemPanelCut,
	FModumateLayerType::kCountertopCut,
	FModumateLayerType::kCabinetCutCarcass,
	FModumateLayerType::kCabinetCutAttachment,
	FModumateLayerType::kSeparatorCutTrim,
	FModumateLayerType::kFinishCut,
	FModumateLayerType::kTerrainCut,
	FModumateLayerType::kPartPointCut,
	FModumateLayerType::kPartEdgeCut,
	FModumateLayerType::kPartFaceCut,
	FModumateLayerType::kOpeningSystemOperatorLine,
	FModumateLayerType::kOpeningSystemBeyond,
	FModumateLayerType::kSeparatorBeyondModuleEdges,
	FModumateLayerType::kSeparatorBeyondSurfaceEdges,
	FModumateLayerType::kBeamColumnBeyond,
	FModumateLayerType::kMullionBeyond,
	FModumateLayerType::kSystemPanelBeyond,
	FModumateLayerType::kCountertopBeyond,
	FModumateLayerType::kCabinetBeyond,
	FModumateLayerType::kCabinetBeyondBlockedByCountertop,
	FModumateLayerType::kOpeningSystemBehind,
	FModumateLayerType::kCabinetBehind,
	FModumateLayerType::kFinishBeyond,
	FModumateLayerType::kFfeOutline,
	FModumateLayerType::kFfeInteriorEdges,
	FModumateLayerType::kTerrainBeyond,
	FModumateLayerType::kPartPointBeyond,
	FModumateLayerType::kPartEdgeBeyond,
	FModumateLayerType::kPartFaceBeyond,
	FModumateLayerType::kDebug1,
	FModumateLayerType::kDebug2,
	FModumateLayerType::kDefault
};

int FModumateLineCorral::LinePriorities[int32(FModumateLayerType::kFinalLayerType) + 1];
TAtomic<int32> FModumateLineCorral::InitPriorities{ 0 };

namespace
{
	// a < b: layer b obscures layer a.
	constexpr bool operator<(FModumateLayerType a, FModumateLayerType b)
	{
		return FModumateLineCorral::LinePriorities[int32(a)] > FModumateLineCorral::LinePriorities[int32(b)];
	}
}

FModumateLineCorral::FLineSegment::FLineSegment(float x1, float y1, float x2, float y2, int32 index, int32 lineData) :
	StartVert(x1, y1), EndVert(x2, y2),
	Index(index), LineData(lineData)
{
	BoundingBox += FVector2D(x1, y1);
	BoundingBox += FVector2D(x2, y2);
	BoundingBox = BoundingBox.ExpandBy(Epsilon);
}


FModumateLineCorral::FLineData::FLineData(ModumateUnitParams::FThickness thickness, FMColor color, LinePattern linePattern,
	ModumateUnitParams::FPhase phase, FModumateLayerType layerType) :
	Thickness(thickness), Color(color), Pattern(linePattern),
	Phase(phase), LayerType(layerType)
{ }

FModumateLineCorral::FModumateLineCorral(IModumateDraftingDraw * nextDrafting) :
	Next(nextDrafting)
{
	if (InitPriorities.OrExchange(1) == 0)
	{
		for (const auto& p: LayerTypeOrder)
		{
			ensureAlways(int32(p) < sizeof(LinePriorities) / sizeof(LinePriorities[0]));
			LinePriorities[int32(p)] = &p - LayerTypeOrder;
		}
	}
}

EDrawError FModumateLineCorral::DrawLine(
	const ModumateUnitParams::FXCoord &x1,
	const ModumateUnitParams::FYCoord &y1,
	const ModumateUnitParams::FXCoord &x2,
	const ModumateUnitParams::FYCoord &y2,
	const ModumateUnitParams::FThickness &thickness,
	const FMColor &color,
	const LinePattern &linePattern,
	const ModumateUnitParams::FPhase &phase,
	FModumateLayerType layerType /*= FModumateLayerType::kDefault */)
{
	static const TSet<FModumateLayerType>  localPassedThroughTypes = {
	FModumateLayerType::kFfeInteriorEdges, FModumateLayerType::kFfeOutline, FModumateLayerType::kDebug1, FModumateLayerType::kDebug2,
	FModumateLayerType::kDimensionMassing, FModumateLayerType::kDimensionFraming,  FModumateLayerType::kDimensionOpening, FModumateLayerType::KDimensionReference
	};

	if (x1.GetUnitType() != EModumateUnitType::WorldCentimeters
		|| localPassedThroughTypes.Contains(layerType))
	{
		return Next->DrawLine(x1, y1, x2, y2, thickness, color, linePattern, phase, layerType);
	}
	else
	{
		InLines.Emplace(x1.AsWorldCentimeters(), y1.AsWorldCentimeters(), x2.AsWorldCentimeters(), y2.AsWorldCentimeters(),
			 CurrentIndex++, LineDataItems.Num());
		LineDataItems.Emplace(thickness, color, linePattern, phase, layerType);
		return EDrawError::ErrorNone;
	}
}

EDrawError FModumateLineCorral::AddText(
	const TCHAR *text,
	const ModumateUnitParams::FFontSize &fontSize,
	const ModumateUnitParams::FXCoord &xpos,
	const ModumateUnitParams::FYCoord &ypos,
	const ModumateUnitParams::FAngle &rotateByRadians,
	const FMColor &color,
	DraftingAlignment textJustify,
	const ModumateUnitParams::FWidth &containingRectWidth,
	FontType type,
	FModumateLayerType layerType /*= FModumateLayerType::kDefault */)
{
	return Next->AddText(text, fontSize, xpos, ypos, rotateByRadians, color, textJustify, containingRectWidth,
		type, layerType);
}

EDrawError FModumateLineCorral::GetTextLength(
	const TCHAR *text,
	const ModumateUnitParams::FFontSize &fontSize,
	FModumateUnitValue &textLength,
	FontType type /*= FontType::Standard*/)
{
	return Next->GetTextLength(text, fontSize, textLength, type);
}

EDrawError FModumateLineCorral::DrawArc(
	const ModumateUnitParams::FXCoord &x,
	const ModumateUnitParams::FYCoord &y,
	const ModumateUnitParams::FAngle &a1,
	const ModumateUnitParams::FAngle &a2,
	const ModumateUnitParams::FRadius &radius,
	const ModumateUnitParams::FThickness &lineWidth,
	const FMColor &color,
	const LinePattern &linePattern,
	int slices,
	FModumateLayerType layerType /*= FModumateLayerType::kDefault */)
{
	return Next->DrawArc(x, y, a1, a2, radius, lineWidth, color, linePattern, slices, layerType);
}

EDrawError FModumateLineCorral::AddImage(
	const TCHAR *imageFileFullPath,
	const ModumateUnitParams::FXCoord &x,
	const ModumateUnitParams::FYCoord &y,
	const ModumateUnitParams::FWidth &width,
	const ModumateUnitParams::FHeight &height,
	FModumateLayerType layerType /*= FModumateLayerType::kDefault */)
{
	return Next->AddImage(imageFileFullPath, x, y, width, height, layerType);
}

EDrawError FModumateLineCorral::FillPoly(
	const float *points,
	int numPoints,
	const FMColor &color,
	FModumateLayerType layerType /*= FModumateLayerType::kDefault */)
{
	return Next->FillPoly(points, numPoints, color, layerType);
}

EDrawError FModumateLineCorral::DrawCircle(
	const ModumateUnitParams::FXCoord &cx,
	const ModumateUnitParams::FYCoord &cy,
	const ModumateUnitParams::FRadius &radius,
	const ModumateUnitParams::FThickness &lineWidth,
	const LinePattern &linePattern,
	const FMColor &color,
	FModumateLayerType layerType /*= FModumateLayerType::kDefault */)
{
	return Next->DrawCircle(cx, cy, radius, lineWidth, linePattern, color, layerType);
}

EDrawError FModumateLineCorral::FillCircle(
	const ModumateUnitParams::FXCoord &cx,
	const ModumateUnitParams::FYCoord &cy,
	const ModumateUnitParams::FRadius &radius,
	const FMColor &color,
	FModumateLayerType layerType /*= FModumateLayerType::kDefault */)
{
	return Next->FillCircle(cx, cy, radius, color, layerType);
}

EDrawError FModumateLineCorral::AddDimension(
	const ModumateUnitParams::FXCoord& startx,
	const ModumateUnitParams::FXCoord& starty,
	const ModumateUnitParams::FXCoord& endx,
	const ModumateUnitParams::FXCoord& endy,
	const ModumateUnitParams::FXCoord& positionx,
	const ModumateUnitParams::FXCoord& positiony,
	const FMColor& color,FModumateLayerType layerType /*= FModumateLayerType::kDefault*/)
{
	return Next->AddDimension(startx, starty, endx, endy, positionx, positiony, color, layerType);
}

EDrawError FModumateLineCorral::AddAngularDimension(
	const ModumateUnitParams::FXCoord& startx,
	const ModumateUnitParams::FXCoord& starty,
	const ModumateUnitParams::FXCoord& endx,
	const ModumateUnitParams::FXCoord& endy,
	const ModumateUnitParams::FXCoord& centerx,
	const ModumateUnitParams::FXCoord& centery,
	const FMColor& color,
	FModumateLayerType layerType /*= FModumateLayerType::kDefault*/)
{
	return Next->AddAngularDimension(startx, starty, endx, endy, centerx, centery, color, layerType);
}

bool FModumateLineCorral::StartPage(int32 pageNumber, float widthInches, float heightInches, FString pageName)
{
	ProcessLines();
	return Next->StartPage(pageNumber, widthInches, heightInches, pageName);
}

bool FModumateLineCorral::SaveDocument(const FString& filename)
{
	ProcessLines();
	return Next->SaveDocument(filename);
}

void FModumateLineCorral::ProcessLines()
{
	SCOPE_MS_ACCUMULATOR(STAT_ModumateLineCoalescing);

	using FVector2Double = FVector2<double>;
	static constexpr double Epsilon2 = Epsilon * Epsilon;

	FLineSegments originalLines(InLines);
	while (InLines.Num() != 0)
	{
		FLineSegment lineA = InLines.Pop();
		FVector2Double startA(lineA.StartVert);
		FVector2Double endA(lineA.EndVert);
		FVector2Double deltaA = endA - startA;
		if (deltaA.SquaredLength() < Epsilon2)
		{
			continue;
		}
		FVector2Double dirA = deltaA.Normalized();

		FModumateLayerType lineAType = LineDataItems[lineA.LineData].LayerType;
		FLineSegments clippedSegments;  // To be reprocessed.

		// Idea: cycle lineA through all other lines, clipping it when overlapping another line
		// of same or higher priority. LineA remains a single segment (unless it disappears), but
		// clipped-off segments are put back in the list for processing.
		for (const auto& lineB: originalLines)
		{
			if (lineB.Index == lineA.Index)
			{
				continue;
			}
			FModumateLayerType lineBType = LineDataItems[lineB.LineData].LayerType;

			bool bLayerEqual = lineAType == lineBType && lineA.Index < lineB.Index;
			bool bLayerGreater = lineAType < lineBType;
			if ((bLayerEqual || bLayerGreater) && lineA.BoundingBox.Intersect(lineB.BoundingBox))
			{
				FVector2Double startB(lineB.StartVert);
				FVector2Double endB(lineB.EndVert);
				FVector2Double deltaB = endB - startB;
				FVector2Double dirB = deltaB.Normalized();

				if (UModumateGeometryStatics::IsLineSegmentWithin2D(FVector2Double(lineB.StartVert), FVector2Double(lineB.EndVert),
					FVector2Double(lineA.StartVert), FVector2Double(lineA.EndVert), Epsilon))
				{
					deltaA = FVector2Double::Zero();
					break;
				}

				bool bNotClipped = false;
				double aDotB = dirA.Dot(dirB);
				if (FMath::Abs(aDotB) > THRESH_NORMALS_ARE_PARALLEL)
				{	// Lines parallel and possibly overlap.
					FSegment2d lineSegmentA(startA, endA);
					FVector2Double intersectOnA = lineSegmentA.NearestPoint(startB);
					if (intersectOnA.DistanceSquared(startB) < Epsilon2)
					{   // Lines overlap
						if (aDotB > 0.0f)
						{
							if (startA.DistanceSquared(intersectOnA) > Epsilon2)
							{
								endA = intersectOnA;
								lineSegmentA.SetEndPoint(endA);
								deltaA = endA - startA;
								FLineSegment clippedLine = lineA;
								clippedLine.StartVert = lineA.EndVert;
								clippedLine.EndVert = FVector2D(intersectOnA);
								lineA.EndVert = FVector2D(endA);
								clippedSegments.Push(clippedLine);
								if (deltaA.SquaredLength() < Epsilon2)
								{
									break;
								}
							}
							else
							{
								bNotClipped = true;
							}
						}
						else
						{
							if (endA.DistanceSquared(intersectOnA) > Epsilon2)
							{
								startA = intersectOnA;
								lineSegmentA.SetStartPoint(startA);
								deltaA = endA - startA;
								FLineSegment clippedLine = lineA;
								clippedLine.StartVert = FVector2D(intersectOnA);
								clippedLine.EndVert = lineA.StartVert;
								clippedSegments.Push(clippedLine);
								lineA.StartVert = FVector2D(startA);
								clippedSegments.Push(clippedLine);
								if (deltaA.SquaredLength() < Epsilon2)
								{
									break;
								}
							}
							else
							{
								bNotClipped = true;
							}

						}
					}
					intersectOnA = lineSegmentA.NearestPoint(endB);
					if (intersectOnA.DistanceSquared(endB) < Epsilon2)
					{   // Lines overlap
						if (aDotB > 0.0f)
						{
							if (endA.DistanceSquared(intersectOnA) > Epsilon2)
							{
								startA = intersectOnA;
								deltaA = endA - startA;
								FLineSegment clippedLine = lineA;
								clippedLine.StartVert = FVector2D(intersectOnA);
								clippedLine.EndVert = lineA.StartVert;
								clippedSegments.Push(clippedLine);
								lineA.StartVert = FVector2D(startA);
								if (deltaA.SquaredLength() < Epsilon2)
								{
									break;
								}
							}
						}
						else
						{
							if (startA.DistanceSquared(intersectOnA) > Epsilon2)
							{
								endA = intersectOnA;
								deltaA = endA - startA;
								FLineSegment clippedLine = lineA;
								clippedLine.StartVert = lineA.EndVert;
								clippedLine.EndVert = FVector2D(intersectOnA);
								clippedSegments.Push(clippedLine);
								lineA.EndVert = FVector2D(endA);
								if (deltaA.SquaredLength() < Epsilon2)
								{
									break;
								}
							}
						}
					}
					else if (bNotClipped)
					{
						deltaA = FVector2Double::Zero();
						break;
					}

				}


			}
		}

		if (deltaA.SquaredLength() >= Epsilon2)
		{
			OutLines.Push(lineA);
		}
		InLines.Append(clippedSegments);

	}

	for (const auto& l: OutLines)
	{
		const auto& lineData = LineDataItems[l.LineData];
		Next->DrawLine(FModumateUnitValue::WorldCentimeters(l.StartVert.X),
			FModumateUnitValue::WorldCentimeters(l.StartVert.Y),
			FModumateUnitValue::WorldCentimeters(l.EndVert.X),
			FModumateUnitValue::WorldCentimeters(l.EndVert.Y),
			lineData.Thickness, lineData.Color, lineData.Pattern, lineData.Phase, lineData.LayerType);
	}

	OutLines.Empty();
	InLines.Empty();
	LineDataItems.Empty();
}
