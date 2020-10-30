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
			const Units::FFontSize &fontSize,
			const Units::FXCoord &xpos,
			const Units::FYCoord &ypos,
			const Units::FAngle &rotateByRadians,
			const FMColor &color,
			const float scale,
			DraftingAlignment textJustify = DraftingAlignment::Left,
			const Units::FWidth &containingRectWidth = Units::FWidth::Points(0.0f),
			FontType type = FontType::Standard);

		PDFResult GetTextLength(PDFOBJECT ob,
			const TCHAR *text,
			const Units::FFontSize &fontSize,
			Units::FUnitValue &textLength,
			const float scale,
			FontType type = FontType::Standard);

		PDFResult AddImage(PDFOBJECT ob,
			int pageNumber,
			const TCHAR *imageFileDir,
			const Units::FXCoord &x,
			const Units::FYCoord &y,
			const Units::FWidth &width,
			const Units::FHeight &height,
			const float scale);

		PDFResult DrawLine(PDFOBJECT ob,
			int pageNumber,
			const Units::FXCoord &x1,
			const Units::FYCoord &y1,
			const Units::FXCoord &x2,
			const Units::FYCoord &y2,
			const Units::FThickness &thickness,
			const FMColor &color,
			const LinePattern &linePattern,
			const Units::FPhase &patternPhase,
			const float scale);

		PDFResult DrawArc(PDFOBJECT ob,
			int pageNumber,
			const Units::FXCoord &x,
			const Units::FYCoord &y,
			const Units::FAngle &a1,
			const Units::FAngle &a2,
			const Units::FRadius &radius,
			const Units::FThickness &lineWidth,
			const FMColor &color,
			const LinePattern &linePattern,
			int slices,
			const float scale);

		PDFResult DrawCircle(PDFOBJECT ob,
			int pageNumber,
			const Units::FXCoord &cx,
			const Units::FYCoord &cy,
			const Units::FRadius &radius,
			const Units::FThickness &lineWidth,
			const LinePattern &linePattern,
			const FMColor &color,
			const float scale);

		PDFResult FillCircle(PDFOBJECT ob,
			int pageNumber,
			const Units::FXCoord &cx,
			const Units::FYCoord &cy,
			const Units::FRadius &radius,
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
			FModumateLayerType layerType = FModumateLayerType::kDefault) override;

		virtual EDrawError AddImage(
			const TCHAR *imageFileFullPath,
			const Units::FXCoord &x,
			const Units::FYCoord &y,
			const Units::FWidth &width,
			const Units::FHeight &height,
			FModumateLayerType layerType = FModumateLayerType::kDefault) override;

		virtual EDrawError FillPoly(
			const float *points,
			int numPoints,
			const FMColor &color,
			FModumateLayerType layerType = FModumateLayerType::kDefault) override;

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
			FModumateLayerType layerType = FModumateLayerType::kDefault
		) override;

		virtual bool StartPage(int32 pageNumber, float widthInches, float heightInches, FString pageName = FString()) override;
		virtual bool SaveDocument(const FString& filename) override;
	};
}
