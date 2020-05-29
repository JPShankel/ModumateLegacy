// Copyright 2020 Modumate, Inc. All Rights Reserved.

#include "Drafting/ModumateDwgDraw.h"
#include "Drafting/ModumateDwgConnect.h"

#include "Core/Public/Misc/FileHelper.h"

namespace Modumate
{
	FModumateDwgDraw::FJsonValuePtr FModumateDwgDraw::ColorToJson(const FMColor& color)
	{
		FValueArray colorComponents;
		colorComponents.Add(MakeShared<FJsonValueNumber>(color.R));
		colorComponents.Add(MakeShared<FJsonValueNumber>(color.G));
		colorComponents.Add(MakeShared<FJsonValueNumber>(color.B));
		return MakeShared<FJsonValueArray>(colorComponents);
	}

	FModumateDwgDraw::FJsonValuePtr FModumateDwgDraw::LinePatternToJson(const LinePattern& linePattern)
	{
		FValueArray lineComponents;
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
		FValueArray lineParams;
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
		JsonDocument.Last().Add(MakeShared<FJsonValueObject>(lineJsonObject));
		
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
		
		FValueArray textParams;
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
		JsonDocument.Last().Add(MakeShared<FJsonValueObject>(textJsonObject));

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
		FValueArray arcParams;
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
		JsonDocument.Last().Add(MakeShared<FJsonValueObject>(arcJsonObject));

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
		ImageFilepaths.Add(filePath);
		filePath = FPaths::GetCleanFilename(filePath);

		FValueArray imageParams;
		imageParams.Add(MakeShared<FJsonValueString>(filePath.ReplaceCharWithEscapedChar()) );
		imageParams.Add(MakeShared<FJsonValueNumber>(x.AsWorldInches()) );
		imageParams.Add(MakeShared<FJsonValueNumber>(y.AsWorldInches()) );
		imageParams.Add(MakeShared<FJsonValueNumber>(width.AsWorldInches()) );
		imageParams.Add(MakeShared<FJsonValueNumber>(height.AsWorldInches()) );

		auto imageJsonObject = MakeShared<FJsonObject>();
		imageJsonObject->SetArrayField("image", imageParams);
		JsonDocument.Last().Add(MakeShared<FJsonValueObject>(imageJsonObject));

		return EDrawError::ErrorNone;
	}

	EDrawError FModumateDwgDraw::FillPoly(
		const float * points,
		int numPoints,
		const FMColor& color)
	{
		const double scaleFactor = DrawingScale / defaultScaleFactor;

		FValueArray fillPolyParams;
		fillPolyParams.Add(ColorToJson(color));

		for (int p = 0; p < numPoints * 2; ++p)
		{
			fillPolyParams.Add(MakeShared<FJsonValueNumber>
				(Units::FUnitValue(points[p], Units::EUnitType::Points).AsWorldInches() * scaleFactor) );
		}

		auto filledPolyJsonObject = MakeShared<FJsonObject>();
		filledPolyJsonObject->SetArrayField("fillpoly", fillPolyParams);
		JsonDocument.Last().Add(MakeShared<FJsonValueObject>(filledPolyJsonObject));

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
		FValueArray circleParams;
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
		JsonDocument.Last().Add(MakeShared<FJsonValueObject>(circleJsonObject));

		return EDrawError::ErrorNone;
	}

	EDrawError FModumateDwgDraw::FillCircle(
		const Units::FXCoord& cx,
		const Units::FYCoord& cy,
		const Units::FRadius& radius,
		const FMColor& color)
	{
		FValueArray fillCircleParams;
		fillCircleParams.Add(MakeShared<FJsonValueNumber>(cx.AsWorldInches()) );
		fillCircleParams.Add(MakeShared<FJsonValueNumber>(cy.AsWorldInches()) );
		fillCircleParams.Add(MakeShared<FJsonValueNumber>(radius.AsWorldInches()) );
		fillCircleParams.Add(ColorToJson(color));

		auto fillCircleJsonObject = MakeShared<FJsonObject>();
		fillCircleJsonObject->SetArrayField("fillcircle", fillCircleParams);
		JsonDocument.Last().Add(MakeShared<FJsonValueObject>(fillCircleJsonObject));

		return EDrawError::ErrorNone;
	}

	bool FModumateDwgDraw::StartPage(int32 pageNumber, float widthInches, float heightInches)
	{
		JsonDocument.Push(FValueArray());
		return true;
	}

	FString Modumate::FModumateDwgDraw::GetJsonAsString(int index) const
	{
		if (index >= JsonDocument.Num())
		{
			return FString();
		}

		FString serializedJson;
		auto serializer = TJsonWriterFactory<>::Create(&serializedJson);
		FJsonSerializer::Serialize(JsonDocument[index], serializer);

		return serializedJson;
	}

	bool FModumateDwgDraw::SaveDocument(const FString& filename)
	{
#if 0
		// Save out JSON for debugging ...
		static int fileNumber = 0;

		for (auto& page: JsonDocument)
		{
			FString filenameJson = FString::Printf(TEXT("%s%d.json"), *filename, ++fileNumber);
			FString serializedJson;
			auto serializer = TJsonWriterFactory<>::Create(&serializedJson);
			FJsonSerializer::Serialize(page, serializer);

			FFileHelper::SaveStringToFile(serializedJson, *filenameJson, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		}
#endif

		FModumateDwgConnect dwgConnect(*this);
		dwgConnect.ConvertJsonToDwg(filename);

		return true;
	}
}