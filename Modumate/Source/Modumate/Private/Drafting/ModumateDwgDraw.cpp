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
		const ModumateUnitParams::FXCoord& x1,
		const ModumateUnitParams::FYCoord& y1,
		const ModumateUnitParams::FXCoord& x2,
		const ModumateUnitParams::FYCoord& y2,
		const ModumateUnitParams::FThickness& thickness,
		const FMColor& color,
		const LinePattern& linePattern,
		const ModumateUnitParams::FPhase& phase,
		FModumateLayerType layerType)
	{
		FValueArray lineParams;
		lineParams.Add(MakeShared<FJsonValueNumber>(x1.AsWorldInches()) );
		lineParams.Add(MakeShared<FJsonValueNumber>(y1.AsWorldInches()) );
		lineParams.Add(MakeShared<FJsonValueNumber>(x2.AsWorldInches()) );
		lineParams.Add(MakeShared<FJsonValueNumber>(y2.AsWorldInches()) );
		lineParams.Add(MakeShared<FJsonValueNumber>(thickness.AsFloorplanInches()) );

		lineParams.Add(ColorToJson(color));
		lineParams.Add(MakeShared<FJsonValueNumber>(int(layerType)) );
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
		const ModumateUnitParams::FFontSize& fontSize,
		const ModumateUnitParams::FXCoord& xpos,
		const ModumateUnitParams::FYCoord& ypos,
		const ModumateUnitParams::FAngle& rotateByRadians,
		const FMColor& color,
		DraftingAlignment textJustify,
		const ModumateUnitParams::FWidth& containingRectWidth,
		FontType type,
		FModumateLayerType layerType)
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
		textParams.Add(MakeShared<FJsonValueNumber>(int(layerType)));

		auto textJsonObject = MakeShared<FJsonObject>();
		textJsonObject->SetArrayField("text", textParams);
		JsonDocument.Last().Add(MakeShared<FJsonValueObject>(textJsonObject));

		return EDrawError::ErrorNone;
	}

	EDrawError FModumateDwgDraw::GetTextLength(
		const TCHAR * text,
		const ModumateUnitParams::FFontSize& fontSize,
		FModumateUnitValue& textLength,
		FontType type)
	{
		textLength = FModumateUnitValue::FloorplanInches(FString(text).Len() * fontSize.AsFloorplanInches() * 0.5);
		return EDrawError();
	}

	EDrawError FModumateDwgDraw::DrawArc(
		const ModumateUnitParams::FXCoord& x,
		const ModumateUnitParams::FYCoord& y,
		const ModumateUnitParams::FAngle& a1,
		const ModumateUnitParams::FAngle& a2,
		const ModumateUnitParams::FRadius& radius,
		const ModumateUnitParams::FThickness& lineWidth,
		const FMColor& color,
		const LinePattern& linePattern,
		int slices,
		FModumateLayerType layerType)
	{
		FValueArray arcParams;
		arcParams.Add(MakeShared<FJsonValueNumber>(x.AsWorldInches()));
		arcParams.Add(MakeShared<FJsonValueNumber>(y.AsWorldInches()));
		arcParams.Add(MakeShared<FJsonValueNumber>(a1.AsRadians()));
		arcParams.Add(MakeShared<FJsonValueNumber>(a2.AsRadians()));
		arcParams.Add(MakeShared<FJsonValueNumber>(radius.AsWorldInches()));
		arcParams.Add(MakeShared<FJsonValueNumber>(lineWidth.AsFloorplanInches()));
		arcParams.Add(ColorToJson(color));
		arcParams.Add(MakeShared<FJsonValueNumber>(int(layerType)));

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
		const ModumateUnitParams::FXCoord& x,
		const ModumateUnitParams::FYCoord& y,
		const ModumateUnitParams::FWidth& width,
		const ModumateUnitParams::FHeight& height,
		FModumateLayerType layerType)
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
		imageParams.Add(MakeShared<FJsonValueNumber>(int(layerType)));

		auto imageJsonObject = MakeShared<FJsonObject>();
		imageJsonObject->SetArrayField("image", imageParams);
		JsonDocument.Last().Add(MakeShared<FJsonValueObject>(imageJsonObject));

		return EDrawError::ErrorNone;
	}

	EDrawError FModumateDwgDraw::FillPoly(
		const float * points,
		int numPoints,
		const FMColor& color,
		FModumateLayerType layerType)
	{
		const double scaleFactor = DrawingScale / defaultScaleFactor;

		FValueArray fillPolyParams;
		fillPolyParams.Add(ColorToJson(color));
		fillPolyParams.Add(MakeShared<FJsonValueNumber>(int(layerType)));

		for (int p = 0; p < numPoints * 2; ++p)
		{
			fillPolyParams.Add(MakeShared<FJsonValueNumber>
				(FModumateUnitValue(points[p], EModumateUnitType::Points).AsWorldInches() * scaleFactor) );
		}

		auto filledPolyJsonObject = MakeShared<FJsonObject>();
		filledPolyJsonObject->SetArrayField("fillpoly", fillPolyParams);
		JsonDocument.Last().Add(MakeShared<FJsonValueObject>(filledPolyJsonObject));

		return EDrawError::ErrorNone;
	}

	EDrawError FModumateDwgDraw::DrawCircle(
		const ModumateUnitParams::FXCoord& cx,
		const ModumateUnitParams::FYCoord& cy,
		const ModumateUnitParams::FRadius& radius,
		const ModumateUnitParams::FThickness& lineWidth,
		const LinePattern& linePattern,
		const FMColor& color,
		FModumateLayerType layerType)
	{
		FValueArray circleParams;
		circleParams.Add(MakeShared<FJsonValueNumber>(cx.AsWorldInches()) );
		circleParams.Add(MakeShared<FJsonValueNumber>(cy.AsWorldInches()) );
		circleParams.Add(MakeShared<FJsonValueNumber>(radius.AsWorldInches()) );
		circleParams.Add(MakeShared<FJsonValueNumber>(lineWidth.AsFloorplanInches()) );
		circleParams.Add(ColorToJson(color));
		circleParams.Add(MakeShared<FJsonValueNumber>(int(layerType)));

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
		const ModumateUnitParams::FXCoord& cx,
		const ModumateUnitParams::FYCoord& cy,
		const ModumateUnitParams::FRadius& radius,
		const FMColor& color,
		FModumateLayerType layerType)
	{
		FValueArray fillCircleParams;
		fillCircleParams.Add(MakeShared<FJsonValueNumber>(cx.AsWorldInches()) );
		fillCircleParams.Add(MakeShared<FJsonValueNumber>(cy.AsWorldInches()) );
		fillCircleParams.Add(MakeShared<FJsonValueNumber>(radius.AsWorldInches()) );
		fillCircleParams.Add(ColorToJson(color));
		fillCircleParams.Add(MakeShared<FJsonValueNumber>(int(layerType)));

		auto fillCircleJsonObject = MakeShared<FJsonObject>();
		fillCircleJsonObject->SetArrayField("fillcircle", fillCircleParams);
		JsonDocument.Last().Add(MakeShared<FJsonValueObject>(fillCircleJsonObject));

		return EDrawError::ErrorNone;
	}

	bool FModumateDwgDraw::StartPage(int32 pageNumber, float widthInches, float heightInches, FString pageName /*= FString()*/)
	{
		JsonDocument.Push(FValueArray());
		PageNames.Push(MoveTemp(pageName));
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

	FString FModumateDwgDraw::GetPageName(int index) const
	{
		if (PageNames.IsValidIndex(index))
		{
			return PageNames[index];
		}
		else
		{
			return FString();
		}
	}

	Modumate::EDrawError FModumateDwgDraw::AddDimension(
		const ModumateUnitParams::FXCoord& startx,
		const ModumateUnitParams::FXCoord& starty,
		const ModumateUnitParams::FXCoord& endx,
		const ModumateUnitParams::FXCoord& endy,
		const ModumateUnitParams::FXCoord& positionx,
		const ModumateUnitParams::FXCoord& positiony,
		const FMColor& color,
		FModumateLayerType layerType /*= FModumateLayerType::kDefault*/)
	{
		FValueArray dimParams;
		dimParams.Add(MakeShared<FJsonValueNumber>(startx.AsWorldInches()));
		dimParams.Add(MakeShared<FJsonValueNumber>(starty.AsWorldInches()));
		dimParams.Add(MakeShared<FJsonValueNumber>(endx.AsWorldInches()));
		dimParams.Add(MakeShared<FJsonValueNumber>(endy.AsWorldInches()));
		dimParams.Add(MakeShared<FJsonValueNumber>(positionx.AsWorldInches()));
		dimParams.Add(MakeShared<FJsonValueNumber>(positiony.AsWorldInches()));

		dimParams.Add(ColorToJson(color));
		dimParams.Add(MakeShared<FJsonValueNumber>(int(layerType)));

		auto dimensionJsonObject = MakeShared<FJsonObject>();
		dimensionJsonObject->SetArrayField("dimension", dimParams);
		JsonDocument.Last().Add(MakeShared<FJsonValueObject>(dimensionJsonObject));

		return EDrawError::ErrorNone;
	}

}
