// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModumateCore/ModumateUnits.h"
#include "Drafting/ModumateDraftingDraw.h"
#include "Json/Public/Json.h"

namespace Modumate
{

	class FModumateDwgDraw: public IModumateDraftingDraw
	{
	public:
		virtual EDrawError DrawLine(
			const Units::FXCoord &x1,
			const Units::FYCoord &y1,
			const Units::FXCoord &x2,
			const Units::FYCoord &y2,
			const Units::FThickness &thickness,
			const FMColor &color,
			const LinePattern &linePattern,
			const Units::FPhase &phase
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
			FontType type = FontType::Standard
		) override;

		virtual EDrawError GetTextLength(
			const TCHAR *text,
			const Units::FFontSize &fontSize,
			Units::FUnitValue &textLength,
			FontType type = FontType::Standard) override;

		virtual EDrawError DrawArc(
			const Units::FXCoord &x,
			const Units::FYCoord &y,
			const Units::FAngle &a1,
			const Units::FAngle &a2,
			const Units::FRadius &radius,
			const Units::FThickness &lineWidth,
			const FMColor &color,
			const LinePattern &linePattern,
			int slices) override;

		virtual EDrawError AddImage(
			const TCHAR *imageFileFullPath,
			const Units::FXCoord &x,
			const Units::FYCoord &y,
			const Units::FWidth &width,
			const Units::FHeight &height) override;

		virtual EDrawError FillPoly(
			const float *points,
			int numPoints,
			const FMColor &color) override;

		virtual EDrawError DrawCircle(
			const Units::FXCoord &cx,
			const Units::FYCoord &cy,
			const Units::FRadius &radius,
			const Units::FThickness &lineWidth,
			const LinePattern &linePattern,
			const FMColor &color) override;

		virtual EDrawError FillCircle(
			const Units::FXCoord &cx,
			const Units::FYCoord &cy,
			const Units::FRadius &radius,
			const FMColor &color) override;

		virtual bool StartPage(int32 pageNumber, float widthInches, float heightInches) override;
		virtual bool SaveDocument(const FString& filename) override;

		int GetNumPages() const { return JsonDocument.Num(); }
		FString GetJsonAsString(int index) const;

		const TArray<FString>& GetImages() const { return ImageFilepaths; }

	private:
		using FJsonValuePtr = TSharedPtr<FJsonValue>;
		using FValueArray = TArray<FJsonValuePtr>;

		static FJsonValuePtr ColorToJson(const FMColor& color);
		static FJsonValuePtr LinePatternToJson(const LinePattern& linePattern);

		TArray<FValueArray> JsonDocument;

		TArray<FString> ImageFilepaths;

		static constexpr double defaultScaleFactor = 48.0;
	};

}