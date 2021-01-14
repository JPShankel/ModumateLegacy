#include "Drafting/ModumateHUDDraw.h"
#include "ModumateCore/ModumateGeometryStatics.h"

namespace Modumate
{
	EDrawError FModumateHUDDraw::DrawLine(
		const ModumateUnitParams::FXCoord &x1,
		const ModumateUnitParams::FYCoord &y1,
		const ModumateUnitParams::FXCoord &x2,
		const ModumateUnitParams::FYCoord &y2,
		const ModumateUnitParams::FThickness &thickness,
		const FMColor &color,
		const LinePattern &linePattern,
		const ModumateUnitParams::FPhase &phase,
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
		const ModumateUnitParams::FFontSize &fontSize,
		const ModumateUnitParams::FXCoord &xpos,
		const ModumateUnitParams::FYCoord &ypos,
		const ModumateUnitParams::FAngle &rotateByRadians,
		const FMColor &color,
		DraftingAlignment textJustify,
		const ModumateUnitParams::FWidth &containingRectWidth,
		FontType type,
		FModumateLayerType layerType)
	{
		return EDrawError::ErrorUnimplemented;
	}

	EDrawError FModumateHUDDraw::GetTextLength(
		const TCHAR *text,
		const ModumateUnitParams::FFontSize &fontSize,
		FModumateUnitValue &textLength,
		FontType type)
	{
		return EDrawError::ErrorUnimplemented;
	}

	EDrawError FModumateHUDDraw::DrawArc(
		const ModumateUnitParams::FXCoord &x,
		const ModumateUnitParams::FYCoord &y,
		const ModumateUnitParams::FAngle &a1,
		const ModumateUnitParams::FAngle &a2,
		const ModumateUnitParams::FRadius &radius,
		const ModumateUnitParams::FThickness &lineWidth,
		const FMColor &color,
		const LinePattern &linePattern,
		int slices,
		FModumateLayerType layerType)
	{
		return EDrawError::ErrorUnimplemented;
	}

	EDrawError FModumateHUDDraw::AddImage(
		const TCHAR *imageFileFullPath,
		const ModumateUnitParams::FXCoord &x,
		const ModumateUnitParams::FYCoord &y,
		const ModumateUnitParams::FWidth &width,
		const ModumateUnitParams::FHeight &height,
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
		const ModumateUnitParams::FXCoord &cx,
		const ModumateUnitParams::FYCoord &cy,
		const ModumateUnitParams::FRadius &radius,
		const ModumateUnitParams::FThickness &lineWidth,
		const LinePattern &linePattern,
		const FMColor &color,
		FModumateLayerType layerType)
	{
		return EDrawError::ErrorUnimplemented;
	}

	EDrawError FModumateHUDDraw::FillCircle(
		const ModumateUnitParams::FXCoord &cx,
		const ModumateUnitParams::FYCoord &cy,
		const ModumateUnitParams::FRadius &radius,
		const FMColor &color,
		FModumateLayerType layerType)
	{
		return EDrawError::ErrorUnimplemented;
	}
}