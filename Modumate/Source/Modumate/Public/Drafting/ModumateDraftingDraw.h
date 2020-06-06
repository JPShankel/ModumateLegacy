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
			const Units::FXCoord &x1,
			const Units::FYCoord &y1,
			const Units::FXCoord &x2,
			const Units::FYCoord &y2,
			const Units::FThickness &thickness,
			const FMColor &color,
			const LinePattern &linePattern,
			const Units::FPhase &phase,
			FModumateLayerType layerType = FModumateLayerType::kDefault
		) = 0;

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
		) = 0;

		virtual EDrawError GetTextLength(
			const TCHAR *text,
			const Units::FFontSize &fontSize,
			Units::FUnitValue &textLength,
			FontType type = FontType::Standard) = 0;

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
		) = 0;

		virtual EDrawError AddImage(
			const TCHAR *imageFileFullPath,
			const Units::FXCoord &x,
			const Units::FYCoord &y,
			const Units::FWidth &width,
			const Units::FHeight &height,
			FModumateLayerType layerType = FModumateLayerType::kDefault
		) = 0;

		virtual EDrawError FillPoly(
			const float *points,
			int numPoints,
			const FMColor &color,
			FModumateLayerType layerType = FModumateLayerType::kDefault
		) = 0;

		virtual EDrawError DrawCircle(
			const Units::FXCoord &cx,
			const Units::FYCoord &cy,
			const Units::FRadius &radius,
			const Units::FThickness &lineWidth,
			const LinePattern &linePattern,
			const FMColor &color,
			FModumateLayerType layerType = FModumateLayerType::kDefault
		) = 0;

		virtual EDrawError FillCircle(
			const Units::FXCoord &cx,
			const Units::FYCoord &cy,
			const Units::FRadius &radius,
			const FMColor &color,
			FModumateLayerType layerType = FModumateLayerType::kDefault
		) = 0;

		virtual bool StartPage(int32 pageNumber, float widthInches, float heightInches)
		{ return true; }

		virtual bool SaveDocument(const FString& filename)
		{ return true; }
	};
}