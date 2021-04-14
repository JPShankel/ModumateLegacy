#pragma once
// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include <vector>
#include "ModumateCore/ModumateUnits.h"
#include "Drafting/ModumateLayerType.h"

namespace Modumate
{
	enum class DraftingAlignment
	{
		Left = 1,
		Center = 2,
		Right = 3,
		Bottom = 1,
		Top = 3,
	};

	enum class DraftingLineStyle
	{
		Solid,
		Dashed,
	};

	struct LinePattern
	{
		DraftingLineStyle LineStyle;
		// TODO: convert to TArray from std::vector
		std::vector<double> DashPattern;
	};

	struct FMColor
	{
		float R, G, B;
		FMColor(float r, float g, float b) : R(r), G(g), B(b) {}
		static FMColor Black;
		static FMColor White;
		static FMColor Gray32;
		static FMColor Gray64;
		static FMColor Gray96;
		static FMColor Gray128;
		static FMColor Gray144;
		static FMColor Gray160;
		static FMColor Gray208;
	};

	enum class EDrawError
	{
		ErrorNone = 0,
		ErrorBadParam,
		ErrorException,
		ErrorUnimplemented
	};

	// TODO: add FontName enum when fonts besides Arial are used
	// TODO: currently, if a type is added here, its representation in APDFLLIB::PDF::ToFontString is also necessary
	// for it to be considered by the pdf AddText and GetTextLength functions
	enum class FontType
	{
		Standard = 0,
		Bold,
		Italic
	};

	class MODUMATE_API IModumateDraftingDraw
	{

	public:
		// View rectangle used for automatic object clipping
		// If values are less than zero, no clipping will be done
		FBox2D Viewport = FBox2D(EForceInit::ForceInitToZero);

		// Scale to convert between the world units and the drawings
		// a value of 48 is the same as 1/4" = 1' scale
		float DrawingScale = 48.0f;

	public:
		virtual ~IModumateDraftingDraw() { };
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
		) = 0;

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
		) = 0;

		virtual EDrawError GetTextLength(
			const TCHAR *text,
			const ModumateUnitParams::FFontSize &fontSize,
			FModumateUnitValue &textLength,
			FontType type = FontType::Standard) = 0;

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
		) = 0;

		virtual EDrawError AddImage(
			const TCHAR *imageFileFullPath,
			const ModumateUnitParams::FXCoord &x,
			const ModumateUnitParams::FYCoord &y,
			const ModumateUnitParams::FWidth &width,
			const ModumateUnitParams::FHeight &height,
			FModumateLayerType layerType = FModumateLayerType::kDefault
		) = 0;

		virtual EDrawError FillPoly(
			const float *points,
			int numPoints,
			const FMColor &color,
			FModumateLayerType layerType = FModumateLayerType::kDefault
		) = 0;

		virtual EDrawError DrawCircle(
			const ModumateUnitParams::FXCoord &cx,
			const ModumateUnitParams::FYCoord &cy,
			const ModumateUnitParams::FRadius &radius,
			const ModumateUnitParams::FThickness &lineWidth,
			const LinePattern &linePattern,
			const FMColor &color,
			FModumateLayerType layerType = FModumateLayerType::kDefault
		) = 0;

		virtual EDrawError FillCircle(
			const ModumateUnitParams::FXCoord &cx,
			const ModumateUnitParams::FYCoord &cy,
			const ModumateUnitParams::FRadius &radius,
			const FMColor &color,
			FModumateLayerType layerType = FModumateLayerType::kDefault
		) = 0;

		virtual EDrawError AddDimension(
			const ModumateUnitParams::FXCoord& startx,
			const ModumateUnitParams::FXCoord& starty,
			const ModumateUnitParams::FXCoord& endx,
			const ModumateUnitParams::FXCoord& endy,
			const ModumateUnitParams::FXCoord& positionx,
			const ModumateUnitParams::FXCoord& positiony,
			const FMColor &color,
			FModumateLayerType layerType = FModumateLayerType::kDefault
		) { return EDrawError::ErrorNone; }

		virtual bool StartPage(int32 pageNumber, float widthInches, float heightInches, FString pageName = FString())
		{ return true; }

		virtual bool SaveDocument(const FString& filename)
		{ return true; }
	};
}