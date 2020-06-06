// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Drafting/ModumateDraftingDraw.h"
#include "UI/HUDDrawWidget.h"

namespace Modumate
{
	class FModumateHUDDraw : public IModumateDraftingDraw
	{
	public:
		UHUDDrawWidget* HUDDrawWidget;
		FVector CurrentOrigin;
		FVector CurrentAxisX, CurrentAxisY;

		// IModumateDraftingDraw
		virtual EDrawError DrawLine(
			const Units::FXCoord &x1,
			const Units::FYCoord &y1,
			const Units::FXCoord &x2,
			const Units::FYCoord &y2,
			const Units::FThickness &thickness,
			const FMColor &color,
			const LinePattern &linePattern,
			const Units::FPhase &phase,
			FModumateLayerType layerType = FModumateLayerType::kDefault
		) override;

		virtual EDrawError AddText(
			const TCHAR *text,
			const Units::FFontSize &fontSize,
			const Units::FXCoord &xpos,
			const Units::FYCoord &ypos,
			const Units::FAngle &rotateByRadians,
			const FMColor &color,
			DraftingAlignment textJustify,
			const Units::FWidth &containingRectWidth,
			FontType type,
			FModumateLayerType layerType = FModumateLayerType::kDefault
		) override;

		virtual EDrawError GetTextLength(
			const TCHAR *text,
			const Units::FFontSize &fontSize,
			Units::FUnitValue &textLength,
			FontType type) override;

		virtual EDrawError DrawArc(
			const Units::FXCoord &x,
			const Units::FYCoord &y,
			const Units::FAngle &a1,
			const Units::FAngle &a2,
			const Units::FRadius &radius,
			const Units::FThickness &lineWidth,
			const FMColor &color,
			const LinePattern &linePattern,
			int slices,
			FModumateLayerType layerType = FModumateLayerType::kDefault
		) override;

		virtual EDrawError AddImage(
			const TCHAR *imageFileFullPath,
			const Units::FXCoord &x,
			const Units::FYCoord &y,
			const Units::FWidth &width,
			const Units::FHeight &height,
			FModumateLayerType layerType = FModumateLayerType::kDefault
		) override;

		virtual EDrawError FillPoly(
			const float *points,
			int numPoints,
			const FMColor &color,
			FModumateLayerType layerType = FModumateLayerType::kDefault
		) override;

		virtual EDrawError DrawCircle(
			const Units::FXCoord &cx,
			const Units::FYCoord &cy,
			const Units::FRadius &radius,
			const Units::FThickness &lineWidth,
			const LinePattern &linePattern,
			const FMColor &color,
			FModumateLayerType layerType = FModumateLayerType::kDefault
		) override;

		virtual EDrawError FillCircle(
			const Units::FXCoord &cx,
			const Units::FYCoord &cy,
			const Units::FRadius &radius,
			const FMColor &color,
			FModumateLayerType layerType = FModumateLayerType::kDefault) override;
	};
}
