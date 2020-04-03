// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "ModumateSlateHelper.h"
#include "Rendering/DrawElements.h"
#include "Framework/Application/SlateApplication.h"

void UModumateSlateHelper::DrawSplineBP(const FPaintContext& context, const TArray<FVector2D>& points, const FLinearColor& color, float thickness)
{
	if (points.Num() != 4)
	{
		return;
	}
	FSlateDrawElement::MakeSpline(
		context.OutDrawElements,
		context.MaxLayer,
		context.AllottedGeometry.ToPaintGeometry(),
		points[0], //start
		points[1], //startDir
		points[2], //end
		points[3], //endDir
		thickness, ESlateDrawEffect::None, color);
}

void UModumateSlateHelper::DrawCubicBezierSplineBP(const FPaintContext& context, const TArray<FVector2D>& points, const FLinearColor& color, float thickness)
{
	if (points.Num() != 4)
	{
		return;
	}
	FSlateDrawElement::MakeCubicBezierSpline(
		context.OutDrawElements,
		context.MaxLayer,
		context.AllottedGeometry.ToPaintGeometry(),
		points[0],
		points[1],
		points[2],
		points[3],
		thickness, ESlateDrawEffect::None, color);
}