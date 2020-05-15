// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "ModumateDwgDraw.h"

#include "Core/Public/Misc/FileHelper.h"

namespace Modumate
{
	FModumateDwgDraw::FModumateDwgDraw()
	{
	}


	FModumateDwgDraw::~FModumateDwgDraw()
	{
		static const TCHAR filenamePart[] { TEXT("DwgDraw") };
		static int fileNumber = 0;

		FString filename = FString::Printf(TEXT("%s%d.json"), filenamePart, ++fileNumber);
		FString serializedJson;
		auto serializer = TJsonWriterFactory<>::Create(&serializedJson);
		FJsonSerializer::Serialize(JsonDocument, serializer);

		FFileHelper::SaveStringToFile(serializedJson, *filename, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}

	FModumateDwgDraw::JsonValuePtr FModumateDwgDraw::ColorToJson(const FMColor& color)
	{
		ValueArray colorComponents;
		colorComponents.Add(MakeShared<FJsonValueNumber>(color.R));
		colorComponents.Add(MakeShared<FJsonValueNumber>(color.G));
		colorComponents.Add(MakeShared<FJsonValueNumber>(color.B));
		return MakeShared<FJsonValueArray>(colorComponents);
	}

	FModumateDwgDraw::JsonValuePtr FModumateDwgDraw::LinePatternToJson(const LinePattern& linePattern)
	{
		ValueArray lineComponents;
		lineComponents.Add(MakeShared<FJsonValueNumber>(int(linePattern.LineStyle)) );
		for (double d: linePattern.DashPattern)
		{
			lineComponents.Add(MakeShared<FJsonValueNumber>(d));
		}
		return MakeShared<FJsonValueArray>(lineComponents);
	}

	EDrawError FModumateDwgDraw::DrawLine(
		const Units::FXCoord& x1,
		const Units::FYCoord& y1,
		const Units::FXCoord& x2,
		const Units::FYCoord& y2,
		const Units::FThickness& thickness,
		const FMColor& color,
		const LinePattern& linePattern,
		const Units::FPhase& phase)
	{
		ValueArray lineParams;
		lineParams.Add(MakeShared<FJsonValueNumber>(x1.AsWorldInches()) );
		lineParams.Add(MakeShared<FJsonValueNumber>(y1.AsWorldInches()) );
		lineParams.Add(MakeShared<FJsonValueNumber>(x2.AsWorldInches()) );
		lineParams.Add(MakeShared<FJsonValueNumber>(y2.AsWorldInches()) );
		lineParams.Add(MakeShared<FJsonValueNumber>(thickness.AsFloorplanInches()) );

		lineParams.Add(ColorToJson(color));
		if (linePattern.LineStyle != DraftingLineStyle::Solid)
		{
			lineParams.Add(LinePatternToJson(linePattern));
		}

		auto lineJsonObject = MakeShared<FJsonObject>();
		lineJsonObject->SetArrayField("line", lineParams);
		JsonDocument.Add(MakeShared<FJsonValueObject>(lineJsonObject));
		
		return EDrawError::ErrorNone;
	}

	EDrawError FModumateDwgDraw::AddText(
		const TCHAR * text,
		const Units::FFontSize& fontSize,
		const Units::FXCoord& xpos,
		const Units::FYCoord& ypos,
		const Units::FAngle& rotateByRadians,
		const FMColor& color,
		DraftingAlignment textJustify,
		const Units::FWidth& containingRectWidth,
		FontType type)
	{
		FString textString(text);
		
		ValueArray textParams;
		textParams.Add(MakeShared<FJsonValueString>(textString.ReplaceCharWithEscapedChar()) );
		textParams.Add(MakeShared<FJsonValueNumber>(fontSize.AsWorldInches()) );
		textParams.Add(MakeShared<FJsonValueNumber>(xpos.AsWorldInches()) );
		textParams.Add(MakeShared<FJsonValueNumber>(ypos.AsWorldInches()) );
		textParams.Add(MakeShared<FJsonValueNumber>(rotateByRadians.AsRadians()) );
		textParams.Add(ColorToJson(color));
		textParams.Add(MakeShared<FJsonValueNumber>(int(textJustify)) );
		textParams.Add(MakeShared<FJsonValueNumber>(int(type)) );

		auto textJsonObject = MakeShared<FJsonObject>();
		textJsonObject->SetArrayField("text", textParams);
		JsonDocument.Add(MakeShared<FJsonValueObject>(textJsonObject));

		return EDrawError::ErrorNone;
	}

	EDrawError FModumateDwgDraw::GetTextLength(
		const TCHAR * text,
		const Units::FFontSize& fontSize,
		Units::FUnitValue& textLength,
		FontType type)
	{
		textLength = Units::FUnitValue::FloorplanInches(FString(text).Len() * fontSize.AsFloorplanInches() * 0.5);
		return EDrawError();
	}

	EDrawError FModumateDwgDraw::DrawArc(
		const Units::FXCoord& x,
		const Units::FYCoord& y,
		const Units::FAngle& a1,
		const Units::FAngle& a2,
		const Units::FRadius& radius,
		const Units::FThickness& lineWidth,
		const FMColor& color,
		const LinePattern& linePattern,
		int slices)
	{
		ValueArray arcParams;
		arcParams.Add(MakeShared<FJsonValueNumber>(x.AsWorldInches()));
		arcParams.Add(MakeShared<FJsonValueNumber>(y.AsWorldInches()));
		arcParams.Add(MakeShared<FJsonValueNumber>(a1.AsRadians()));
		arcParams.Add(MakeShared<FJsonValueNumber>(a2.AsRadians()));
		arcParams.Add(MakeShared<FJsonValueNumber>(radius.AsWorldInches()));
		arcParams.Add(MakeShared<FJsonValueNumber>(lineWidth.AsFloorplanInches()));
		arcParams.Add(ColorToJson(color));

		if (linePattern.LineStyle != DraftingLineStyle::Solid)
		{
			arcParams.Add(LinePatternToJson(linePattern));
		}

		auto arcJsonObject = MakeShared<FJsonObject>();
		arcJsonObject->SetArrayField("arc", arcParams);
		JsonDocument.Add(MakeShared<FJsonValueObject>(arcJsonObject));

		return EDrawError::ErrorNone;
	}

	EDrawError FModumateDwgDraw::AddImage(
		const TCHAR * imageFileFullPath,
		const Units::FXCoord& x,
		const Units::FYCoord& y,
		const Units::FWidth& width,
		const Units::FHeight& height)
	{
		FString filePath(imageFileFullPath);

		ValueArray imageParams;
		imageParams.Add(MakeShared<FJsonValueString>(filePath.ReplaceCharWithEscapedChar()) );
		imageParams.Add(MakeShared<FJsonValueNumber>(x.AsWorldInches()) );
		imageParams.Add(MakeShared<FJsonValueNumber>(y.AsWorldInches()) );
		imageParams.Add(MakeShared<FJsonValueNumber>(width.AsWorldInches()) );
		imageParams.Add(MakeShared<FJsonValueNumber>(height.AsWorldInches()) );

		auto imageJsonObject = MakeShared<FJsonObject>();
		imageJsonObject->SetArrayField("image", imageParams);
		JsonDocument.Add(MakeShared<FJsonValueObject>(imageJsonObject));

		return EDrawError::ErrorNone;
	}

	EDrawError FModumateDwgDraw::FillPoly(
		const float * points,
		int numPoints,
		const FMColor& color)
	{
		const double scaleFactor = DrawingScale / defaultScaleFactor;

		ValueArray fillPolyParams;
		fillPolyParams.Add(ColorToJson(color));

		for (int p = 0; p < numPoints * 2; ++p)
		{
			fillPolyParams.Add(MakeShared<FJsonValueNumber>
				(Units::FUnitValue(points[p], Units::EUnitType::Points).AsWorldInches() * scaleFactor) );
		}

		auto filledPolyJsonObject = MakeShared<FJsonObject>();
		filledPolyJsonObject->SetArrayField("fillpoly", fillPolyParams);
		JsonDocument.Add(MakeShared<FJsonValueObject>(filledPolyJsonObject));

		return EDrawError::ErrorNone;
	}

	EDrawError FModumateDwgDraw::DrawCircle(
		const Units::FXCoord& cx,
		const Units::FYCoord& cy,
		const Units::FRadius& radius,
		const Units::FThickness& lineWidth,
		const LinePattern& linePattern,
		const FMColor& color)
	{
		ValueArray circleParams;
		circleParams.Add(MakeShared<FJsonValueNumber>(cx.AsWorldInches()) );
		circleParams.Add(MakeShared<FJsonValueNumber>(cy.AsWorldInches()) );
		circleParams.Add(MakeShared<FJsonValueNumber>(radius.AsWorldInches()) );
		circleParams.Add(MakeShared<FJsonValueNumber>(lineWidth.AsFloorplanInches()) );
		circleParams.Add(ColorToJson(color));

		if (linePattern.LineStyle != DraftingLineStyle::Solid)
		{
			circleParams.Add(LinePatternToJson(linePattern));
		}

		auto circleJsonObject = MakeShared<FJsonObject>();
		circleJsonObject->SetArrayField("circle", circleParams);
		JsonDocument.Add(MakeShared<FJsonValueObject>(circleJsonObject));

		return EDrawError::ErrorNone;
	}

	EDrawError FModumateDwgDraw::FillCircle(
		const Units::FXCoord& cx,
		const Units::FYCoord& cy,
		const Units::FRadius& radius,
		const FMColor& color)
	{
		ValueArray fillCircleParams;
		fillCircleParams.Add(MakeShared<FJsonValueNumber>(cx.AsWorldInches()) );
		fillCircleParams.Add(MakeShared<FJsonValueNumber>(cy.AsWorldInches()) );
		fillCircleParams.Add(MakeShared<FJsonValueNumber>(radius.AsWorldInches()) );
		fillCircleParams.Add(ColorToJson(color));

		auto fillCircleJsonObject = MakeShared<FJsonObject>();
		fillCircleJsonObject->SetArrayField("fillcircle", fillCircleParams);
		JsonDocument.Add(MakeShared<FJsonValueObject>(fillCircleJsonObject));

		return EDrawError::ErrorNone;
	}

}