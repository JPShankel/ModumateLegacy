// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Drafting/ModumateLineCorral.h"
#include "ModumateCore/ModumateGeometryStatics.h"

// Map FModumateLayerType priorities in terms of obscuring in DWG.
const FModumateLayerType FModumateLineCorral::LayerTypeOrder[] =
{
	FModumateLayerType::kSeparatorCutOuterSurface,
	FModumateLayerType::kSeparatorCutMinorLayer,
	FModumateLayerType::kSeparatorCutStructuralLayer,
	FModumateLayerType::kOpeningSystemCutLine,
	FModumateLayerType::kCountertopCut,
	FModumateLayerType::kCabinetCutCarcass,
	FModumateLayerType::kCabinetCutAttachment,
	FModumateLayerType::kSeparatorCutTrim,
	FModumateLayerType::kOpeningSystemOperatorLine,
	FModumateLayerType::kSeparatorBeyondModuleEdges,
	FModumateLayerType::kSeparatorBeyondSurfaceEdges,
	FModumateLayerType::kOpeningSystemBeyond,
	FModumateLayerType::kCountertopBeyond,
	FModumateLayerType::kCabinetBeyond,
	FModumateLayerType::kCabinetBeyondBlockedByCountertop,
	FModumateLayerType::kOpeningSystemBehind,
	FModumateLayerType::kCabinetBehind,
	FModumateLayerType::kFfeOutline,
	FModumateLayerType::kFfeInteriorEdges,
	FModumateLayerType::kDefault
};
int FModumateLineCorral::LinePriorities[sizeof(LayerTypeOrder) / sizeof(FModumateLayerType)];
TAtomic<int32> FModumateLineCorral::InitPriorities{ 0 };

const TSet<FModumateLayerType>  FModumateLineCorral::PassedThroughTypes = 
    { FModumateLayerType::kFfeInteriorEdges, FModumateLayerType::kFfeOutline };


namespace
{
	// a < b: layer b obscures layer a.
	constexpr bool operator<(FModumateLayerType a, FModumateLayerType b)
	{
		return FModumateLineCorral::LinePriorities[int(a)] > FModumateLineCorral::LinePriorities[int(b)];
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


FModumateLineCorral::FLineData::FLineData(Units::FThickness thickness, FMColor color, LinePattern linePattern,
	Units::FPhase phase, FModumateLayerType layerType) :
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
			LinePriorities[int(p)] = &p - LayerTypeOrder;
		}
	}
}

Modumate::EDrawError FModumateLineCorral::DrawLine(
	const Units::FXCoord &x1,
	const Units::FYCoord &y1,
	const Units::FXCoord &x2,
	const Units::FYCoord &y2,
	const Units::FThickness &thickness,
	const FMColor &color,
	const LinePattern &linePattern,
	const Units::FPhase &phase,
	FModumateLayerType layerType /*= FModumateLayerType::kDefault */)
{
	if (x1.GetUnitType() != Units::EUnitType::WorldCentimeters
		|| PassedThroughTypes.Contains(layerType))
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

Modumate::EDrawError FModumateLineCorral::AddText(
	const TCHAR *text,
	const Units::FFontSize &fontSize,
	const Units::FXCoord &xpos,
	const Units::FYCoord &ypos,
	const Units::FAngle &rotateByRadians,
	const FMColor &color,
	DraftingAlignment textJustify,
	const Units::FWidth &containingRectWidth,
	FontType type,
	FModumateLayerType layerType /*= FModumateLayerType::kDefault */)
{
	return Next->AddText(text, fontSize, xpos, ypos, rotateByRadians, color, textJustify, containingRectWidth,
		type, layerType);
}

Modumate::EDrawError FModumateLineCorral::GetTextLength(
	const TCHAR *text,
	const Units::FFontSize &fontSize,
	Units::FUnitValue &textLength,
	FontType type /*= FontType::Standard*/)
{
	return Next->GetTextLength(text, fontSize, textLength, type);
}

Modumate::EDrawError FModumateLineCorral::DrawArc(
	const Units::FXCoord &x,
	const Units::FYCoord &y,
	const Units::FAngle &a1,
	const Units::FAngle &a2,
	const Units::FRadius &radius,
	const Units::FThickness &lineWidth,
	const FMColor &color,
	const LinePattern &linePattern,
	int slices,
	FModumateLayerType layerType /*= FModumateLayerType::kDefault */)
{
	return Next->DrawArc(x, y, a1, a2, radius, lineWidth, color, linePattern, slices, layerType);
}

Modumate::EDrawError FModumateLineCorral::AddImage(
	const TCHAR *imageFileFullPath,
	const Units::FXCoord &x,
	const Units::FYCoord &y,
	const Units::FWidth &width,
	const Units::FHeight &height,
	FModumateLayerType layerType /*= FModumateLayerType::kDefault */)
{
	return Next->AddImage(imageFileFullPath, x, y, width, height, layerType);
}

Modumate::EDrawError FModumateLineCorral::FillPoly(
	const float *points,
	int numPoints,
	const FMColor &color,
	FModumateLayerType layerType /*= FModumateLayerType::kDefault */)
{
	return Next->FillPoly(points, numPoints, color, layerType);
}

Modumate::EDrawError FModumateLineCorral::DrawCircle(
	const Units::FXCoord &cx,
	const Units::FYCoord &cy,
	const Units::FRadius &radius,
	const Units::FThickness &lineWidth,
	const LinePattern &linePattern,
	const FMColor &color,
	FModumateLayerType layerType /*= FModumateLayerType::kDefault */)
{
	return Next->DrawCircle(cx, cy, radius, lineWidth, linePattern, color, layerType);
}

Modumate::EDrawError FModumateLineCorral::FillCircle(
	const Units::FXCoord &cx,
	const Units::FYCoord &cy,
	const Units::FRadius &radius,
	const FMColor &color,
	FModumateLayerType layerType /*= FModumateLayerType::kDefault */)
{
	return Next->FillCircle(cx, cy, radius, color, layerType);
}

bool FModumateLineCorral::StartPage(int32 pageNumber, float widthInches, float heightInches)
{
	ProcessLines();
	return Next->StartPage(pageNumber, widthInches, heightInches);
}

bool FModumateLineCorral::SaveDocument(const FString& filename)
{
	ProcessLines();
	return Next->SaveDocument(filename);
}

void FModumateLineCorral::ProcessLines()
{
	FLineSegments originalLines(InLines);
	while (InLines.Num() != 0)
	{
		FLineSegment lineA = InLines.Pop();
		FVector2D startA(lineA.StartVert);
		FVector2D endA(lineA.EndVert);
		FVector2D deltaA = endA - startA;
		if (deltaA.IsNearlyZero(Epsilon))
		{
			continue;
		}
		FVector2D dirA = deltaA.GetSafeNormal();

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
				FVector2D startB(lineB.StartVert);
				FVector2D endB(lineB.EndVert);
				FVector2D deltaB = endB - startB;
				FVector2D dirB = deltaB.GetSafeNormal();

				if (UModumateGeometryStatics::IsLineSegmentWithin2D(lineB.StartVert, lineB.EndVert,
					lineA.StartVert, lineA.EndVert, Epsilon))
				{
					deltaA = deltaA.ZeroVector;
					break;
				}

				float aDotB = dirA | dirB;
				if (FMath::Abs(aDotB) > THRESH_NORMALS_ARE_PARALLEL)
				{	// Lines parallel and possibly overlap.
					FVector2D intersectOnA = FMath::ClosestPointOnSegment2D(startB, startA, endA);
					if (FVector2D::DistSquared(intersectOnA, startB) < Epsilon)
					{   // Lines overlap
						if (aDotB > 0.0f)
						{
							endA = intersectOnA;
							deltaA = endA - startA;
							FLineSegment clippedLine = lineA;
							clippedLine.StartVert = endB;
							clippedLine.EndVert = startB;
							clippedSegments.Push(clippedLine);
							lineA.EndVert = endA;
							if (deltaA.IsNearlyZero(Epsilon))
							{
								break;
							}
						}
						else
						{
							startA = intersectOnA;
							deltaA = endA - startA;
							FLineSegment clippedLine = lineA;
							clippedLine.StartVert = startB;
							clippedLine.EndVert = endB;
							clippedSegments.Push(clippedLine);
							lineA.StartVert = startA;
							if (deltaA.IsNearlyZero(Epsilon))
							{
								break;
							}
						}
					}
					intersectOnA = FMath::ClosestPointOnSegment2D(endB, startA, endA);
					if (FVector2D::DistSquared(intersectOnA, endB) < Epsilon)
					{   // Lines overlap
						if (aDotB > 0.0f)
						{
							startA = intersectOnA;
							deltaA = endA - startA;
							FLineSegment clippedLine = lineA;
							clippedLine.StartVert = endB;
							clippedLine.EndVert = startB;
							clippedSegments.Push(clippedLine);
							lineA.StartVert = startA;
							if (deltaA.IsNearlyZero(Epsilon))
							{
								break;
							}
						}
						else
						{
							endA = intersectOnA;
							deltaA = endA - startA;
							FLineSegment clippedLine = lineA;
							clippedLine.StartVert = startB;
							clippedLine.EndVert = endB;
							clippedSegments.Push(clippedLine);
							lineA.EndVert = endA;
							if (deltaA.IsNearlyZero(Epsilon))
							{
								break;
							}
						}
					}

				}


			}
		}

		if (!deltaA.IsNearlyZero(Epsilon))
		{
			OutLines.Push(lineA);
		}
		InLines.Append(clippedSegments);

	}

	for (const auto& l: OutLines)
	{
		const auto& lineData = LineDataItems[l.LineData];
		Next->DrawLine(Units::FUnitValue::WorldCentimeters(l.StartVert.X),
			Units::FUnitValue::WorldCentimeters(l.StartVert.Y),
			Units::FUnitValue::WorldCentimeters(l.EndVert.X),
			Units::FUnitValue::WorldCentimeters(l.EndVert.Y),
			lineData.Thickness, lineData.Color, lineData.Pattern, lineData.Phase, lineData.LayerType);
	}

	OutLines.Empty();
	InLines.Empty();
	LineDataItems.Empty();
}
