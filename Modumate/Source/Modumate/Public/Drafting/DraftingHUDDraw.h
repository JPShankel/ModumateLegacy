// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Drafting/ModumateDraftingDraw.h"
#include "UI/HUDDrawWidget.h"

namespace Modumate
{
	class FDraftingHUDDraw : public IModumateDraftingDraw
	{
	public:
		UHUDDrawWidget* HUDDrawWidget;
		FVector CurrentOrigin;
		FVector CurrentAxisX, CurrentAxisY;

		// IModumateDraftingDraw
		virtual EDrawError DrawLine(
			const ModumateUnitParams::FXCoord &x1,
			const ModumateUnitParams::FYCoord &y1,
			const ModumateUnitParams::FXCoord &x2,
			const ModumateUnitParams::FYCoord &y2,
			const ModumateUnitParams::FThickness &thickness,
			const FMColor &color,
			const LinePattern &linePattern,
			const ModumateUnitParams::FPhase &phase,
			FModumateLayerType layerType = FModumateLayerType::kDefault
		) override;

		virtual EDrawError AddText(
			const TCHAR *text,
			const ModumateUnitParams::FFontSize &fontSize,
			const ModumateUnitParams::FXCoord &xpos,
			const ModumateUnitParams::FYCoord &ypos,
			const ModumateUnitParams::FAngle &rotateByRadians,
			const FMColor &color,
			DraftingAlignment textJustify,
			const ModumateUnitParams::FWidth &containingRectWidth,
			FontType type,
			FModumateLayerType layerType = FModumateLayerType::kDefault
		) override;

		virtual EDrawError GetTextLength(
			const TCHAR *text,
			const ModumateUnitParams::FFontSize &fontSize,
			FModumateUnitValue &textLength,
			FontType type) override;

		virtual EDrawError DrawArc(
			const ModumateUnitParams::FXCoord &x,
			const ModumateUnitParams::FYCoord &y,
			const ModumateUnitParams::FAngle &a1,
			const ModumateUnitParams::FAngle &a2,
			const ModumateUnitParams::FRadius &radius,
			const ModumateUnitParams::FThickness &lineWidth,
			const FMColor &color,
			const LinePattern &linePattern,
			int slices,
			FModumateLayerType layerType = FModumateLayerType::kDefault
		) override;

		virtual EDrawError AddImage(
			const TCHAR *imageFileFullPath,
			const ModumateUnitParams::FXCoord &x,
			const ModumateUnitParams::FYCoord &y,
			const ModumateUnitParams::FWidth &width,
			const ModumateUnitParams::FHeight &height,
			FModumateLayerType layerType = FModumateLayerType::kDefault
		) override;

		virtual EDrawError FillPoly(
			const float *points,
			int numPoints,
			const FMColor &color,
			FModumateLayerType layerType = FModumateLayerType::kDefault
		) override;

		virtual EDrawError DrawCircle(
			const ModumateUnitParams::FXCoord &cx,
			const ModumateUnitParams::FYCoord &cy,
			const ModumateUnitParams::FRadius &radius,
			const ModumateUnitParams::FThickness &lineWidth,
			const LinePattern &linePattern,
			const FMColor &color,
			FModumateLayerType layerType = FModumateLayerType::kDefault
		) override;

		virtual EDrawError FillCircle(
			const ModumateUnitParams::FXCoord &cx,
			const ModumateUnitParams::FYCoord &cy,
			const ModumateUnitParams::FRadius &radius,
			const FMColor &color,
			FModumateLayerType layerType = FModumateLayerType::kDefault) override;

		virtual EDrawError AddAngularDimension(
			const ModumateUnitParams::FXCoord& startx,
			const ModumateUnitParams::FXCoord& starty,
			const ModumateUnitParams::FXCoord& endx,
			const ModumateUnitParams::FXCoord& endy,
			const ModumateUnitParams::FXCoord& centerx,
			const ModumateUnitParams::FXCoord& centery,
			const FMColor& color,
			FModumateLayerType layerType = FModumateLayerType::kDefault) override;

		bool bUseDwgMode = true;
	};
}
