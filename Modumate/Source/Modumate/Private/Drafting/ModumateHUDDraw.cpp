#include "Drafting/ModumateHUDDraw.h"
#include "ModumateCore/ModumateGeometryStatics.h"

namespace Modumate
{
	EDrawError FModumateHUDDraw::DrawLine(
		const Units::FXCoord &x1,
		const Units::FYCoord &y1,
		const Units::FXCoord &x2,
		const Units::FYCoord &y2,
		const Units::FThickness &thickness,
		const FMColor &color,
		const LinePattern &linePattern,
		const Units::FPhase &phase,
		FModumateLayerType layerType)
	{
		FModumateLines line;
		line.Thickness = thickness.AsWorldCentimeters();

		// currently everything in drafting is opaque
		line.Color = FLinearColor(color.R, color.G, color.B, 1.0f);

		// TODO: dash pattern is unimplemented here (would use linePattern and phase)

		line.Point1 = FVector(CurrentOrigin) + CurrentAxisX * x1.AsWorldCentimeters() + CurrentAxisY * y1.AsWorldCentimeters();
		line.Point2 = FVector(CurrentOrigin) + CurrentAxisX * x2.AsWorldCentimeters() + CurrentAxisY * y2.AsWorldCentimeters();

		HUDDrawWidget->LinesToDraw.Add(MoveTemp(line));

		return EDrawError::ErrorNone;
	}

	EDrawError FModumateHUDDraw::AddText(
		const TCHAR *text,
		const Units::FFontSize &fontSize,
		const Units::FXCoord &xpos,
		const Units::FYCoord &ypos,
		const Units::FAngle &rotateByRadians,
		const FMColor &color,
		DraftingAlignment textJustify,
		const Units::FWidth &containingRectWidth,
		FontType type,
		FModumateLayerType layerType)
	{
		return EDrawError::ErrorUnimplemented;
	}

	EDrawError FModumateHUDDraw::GetTextLength(
		const TCHAR *text,
		const Units::FFontSize &fontSize,
		Units::FUnitValue &textLength,
		FontType type)
	{
		return EDrawError::ErrorUnimplemented;
	}

	EDrawError FModumateHUDDraw::DrawArc(
		const Units::FXCoord &x,
		const Units::FYCoord &y,
		const Units::FAngle &a1,
		const Units::FAngle &a2,
		const Units::FRadius &radius,
		const Units::FThickness &lineWidth,
		const FMColor &color,
		const LinePattern &linePattern,
		int slices,
		FModumateLayerType layerType)
	{
		return EDrawError::ErrorUnimplemented;
	}

	EDrawError FModumateHUDDraw::AddImage(
		const TCHAR *imageFileFullPath,
		const Units::FXCoord &x,
		const Units::FYCoord &y,
		const Units::FWidth &width,
		const Units::FHeight &height,
		FModumateLayerType layerType)
	{
		return EDrawError::ErrorUnimplemented;
	}

	EDrawError FModumateHUDDraw::FillPoly(
		const float *points,
		int numPoints,
		const FMColor &color,
		FModumateLayerType layerType)
	{
		return EDrawError::ErrorUnimplemented;
	}

	EDrawError FModumateHUDDraw::DrawCircle(
		const Units::FXCoord &cx,
		const Units::FYCoord &cy,
		const Units::FRadius &radius,
		const Units::FThickness &lineWidth,
		const LinePattern &linePattern,
		const FMColor &color,
		FModumateLayerType layerType)
	{
		return EDrawError::ErrorUnimplemented;
	}

	EDrawError FModumateHUDDraw::FillCircle(
		const Units::FXCoord &cx,
		const Units::FYCoord &cy,
		const Units::FRadius &radius,
		const FMColor &color,
		FModumateLayerType layerType)
	{
		return EDrawError::ErrorUnimplemented;
	}
}