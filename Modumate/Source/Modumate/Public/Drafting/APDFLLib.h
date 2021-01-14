// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include <vector>
#include "CoreMinimal.h"
#include "ModumateCore/ModumateUnits.h"
#include "Drafting/ModumateDraftingDraw.h"

DECLARE_LOG_CATEGORY_EXTERN(LogPDFTron, Log, All);

namespace Modumate
{
	namespace PDF {
		typedef void *PDFOBJECT;

		struct PDFResult
		{
			PDFOBJECT Object;
			EDrawError ErrorCode;
		};

		/*
		PDF Draw Functions
		*/

		// float rotateByRadians - rotate by this many radians clockwise; to rotate counterclockwise, provide a negative angle.
		PDFResult AddText(PDFOBJECT ob, int page,
			const TCHAR *text,
			const ModumateUnitParams::FFontSize &fontSize,
			const ModumateUnitParams::FXCoord &xpos,
			const ModumateUnitParams::FYCoord &ypos,
			const ModumateUnitParams::FAngle &rotateByRadians,
			const FMColor &color,
			const float scale,
			DraftingAlignment textJustify = DraftingAlignment::Left,
			const ModumateUnitParams::FWidth &containingRectWidth = ModumateUnitParams::FWidth::Points(0.0f),
			FontType type = FontType::Standard);

		PDFResult GetTextLength(PDFOBJECT ob,
			const TCHAR *text,
			const ModumateUnitParams::FFontSize &fontSize,
			FModumateUnitValue &textLength,
			const float scale,
			FontType type = FontType::Standard);

		PDFResult AddImage(PDFOBJECT ob,
			int pageNumber,
			const TCHAR *imageFileDir,
			const ModumateUnitParams::FXCoord &x,
			const ModumateUnitParams::FYCoord &y,
			const ModumateUnitParams::FWidth &width,
			const ModumateUnitParams::FHeight &height,
			const float scale);

		PDFResult DrawLine(PDFOBJECT ob,
			int pageNumber,
			const ModumateUnitParams::FXCoord &x1,
			const ModumateUnitParams::FYCoord &y1,
			const ModumateUnitParams::FXCoord &x2,
			const ModumateUnitParams::FYCoord &y2,
			const ModumateUnitParams::FThickness &thickness,
			const FMColor &color,
			const LinePattern &linePattern,
			const ModumateUnitParams::FPhase &patternPhase,
			const float scale);

		PDFResult DrawArc(PDFOBJECT ob,
			int pageNumber,
			const ModumateUnitParams::FXCoord &x,
			const ModumateUnitParams::FYCoord &y,
			const ModumateUnitParams::FAngle &a1,
			const ModumateUnitParams::FAngle &a2,
			const ModumateUnitParams::FRadius &radius,
			const ModumateUnitParams::FThickness &lineWidth,
			const FMColor &color,
			const LinePattern &linePattern,
			int slices,
			const float scale);

		PDFResult DrawCircle(PDFOBJECT ob,
			int pageNumber,
			const ModumateUnitParams::FXCoord &cx,
			const ModumateUnitParams::FYCoord &cy,
			const ModumateUnitParams::FRadius &radius,
			const ModumateUnitParams::FThickness &lineWidth,
			const LinePattern &linePattern,
			const FMColor &color,
			const float scale);

		PDFResult FillCircle(PDFOBJECT ob,
			int pageNumber,
			const ModumateUnitParams::FXCoord &cx,
			const ModumateUnitParams::FYCoord &cy,
			const ModumateUnitParams::FRadius &radius,
			const FMColor &color,
			const float scale);

		PDFResult FillPoly(PDFOBJECT ob,
			int pageNumber,
			const float *points,
			int numPoints,
			const FMColor &color,
			const float scale);
	}

	namespace PDF
	{
		PDFResult InitLibrary();
		PDFResult ShutdownLibrary(PDFOBJECT ob);

		PDFResult CreatePDF();
		PDFResult ClosePDF(PDFOBJECT ob);
		PDFResult AddPage(PDFOBJECT ob, float widthInches, float heightInches);
		PDFResult SavePDF(PDFOBJECT ob, const TCHAR *path);
		void Test3D();
	}

	class FModumatePDFDraw : public IModumateDraftingDraw
	{
	public:
		Modumate::PDF::PDFResult Doc;
		int32 PageNum { 0 };

		FModumatePDFDraw() : Doc(PDF::CreatePDF()) { }

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
			FModumateLayerType layerType = FModumateLayerType::kDefault) override;

		virtual EDrawError AddImage(
			const TCHAR *imageFileFullPath,
			const ModumateUnitParams::FXCoord &x,
			const ModumateUnitParams::FYCoord &y,
			const ModumateUnitParams::FWidth &width,
			const ModumateUnitParams::FHeight &height,
			FModumateLayerType layerType = FModumateLayerType::kDefault) override;

		virtual EDrawError FillPoly(
			const float *points,
			int numPoints,
			const FMColor &color,
			FModumateLayerType layerType = FModumateLayerType::kDefault) override;

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
			FModumateLayerType layerType = FModumateLayerType::kDefault
		) override;

		virtual bool StartPage(int32 pageNumber, float widthInches, float heightInches, FString pageName = FString()) override;
		virtual bool SaveDocument(const FString& filename) override;
	};
}
