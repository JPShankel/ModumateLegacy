// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "Drafting/APDFLLib.h"

#pragma warning (disable:4456)	// declaration of 'identifier' hides previous local declaration
#pragma warning (disable:4458)	// declaration of 'identifier' hides class member
#pragma warning (disable:4668)	// 'symbol' is not defined as a preprocessor macro, replacing with '0' for 'directives'

#include <PDF/PDFNet.h>
#include <PDF/PDFDoc.h>
#include <PDF/PDFDraw.h>
#include <Filters/MappedFile.h>
#include <Filters/FilterWriter.h>
#include <SDF/ObjSet.h>
#include <SDF/Obj.h>
#include <PDF/Annot.h>
#include <PDF/ElementBuilder.h>
#include <PDF/ElementWriter.h>
#include <Common/Matrix2D.h>
#include <Filters/FlateEncode.h>
#include <PDF/Convert.h>
#include <PDF/CADModule.h>
#include <PDF/CADConvertOptions.h>

#include "ModumateCore/PlatformFunctions.h"

#include "HAL/FileManager.h"

#define DEF_OUTPUT "CreateDocument-out.pdf"
#define PAGES_TO_INSERT 5

using namespace pdftron::PDF;
using namespace pdftron::SDF;
using namespace pdftron::Filters;
using namespace pdftron::FDF;

const char *pdfTronKey = "Modumate, Inc(modumate.com):OEM:Modumate::W:AMS(20200331):B77756501F1774D0E37352386F617F9B9740B523EB1CB48A8D6408F062CABEF5C7";

DEFINE_LOG_CATEGORY(LogPDFTron);

namespace Modumate
{
	namespace PDF {

		const float DotsPerInch = 72.0f;

		// Local helper function for handling caught PDFTron exceptions
		void HandleException(PDFResult &result, pdftron::Common::Exception &exc)
		{
			std::string excStdStr = exc.ToString();
			const char *excCStr = excStdStr.c_str();
			UE_LOG(LogPDFTron, Log, TEXT("PDFTron %s"), StringCast<TCHAR>(excCStr).Get());
			result.ErrorCode = EDrawError::ErrorException;
		}

		/*
		Library Init
		*/

		PDFResult InitLibrary()
		{
			PDFResult res;
			res.Object = nullptr;
			res.ErrorCode = EDrawError::ErrorNone;

			try
			{
				pdftron::PDFNet::Initialize(pdfTronKey);
				pdftron::PDFNet::AddResourceSearchPath("../../../../Modumate/PDFTron/Lib/");
			}
			catch (pdftron::Common::Exception &exc)
			{
				HandleException(res, exc);
			}

			return res;
		}

		PDFResult ShutdownLibrary(PDFOBJECT ob)
		{
			PDFResult res;
			res.ErrorCode = EDrawError::ErrorNone;
			res.Object = NULL;

			try
			{
				pdftron::PDFNet::Terminate();
			}
			catch (pdftron::Common::Exception &exc)
			{
				HandleException(res, exc);
			}

			return res;
		}

		PDFResult CreatePDF()
		{
			PDFResult res;
			res.ErrorCode = EDrawError::ErrorNone;
			res.Object = NULL;

			try
			{
				PDFDoc *newDoc = new PDFDoc();
				newDoc->InitSecurityHandler();
				res.Object = newDoc;
			}
			catch (pdftron::Common::Exception &exc)
			{
				HandleException(res, exc);
			}

			return res;
		}

		PDFResult AddPage(PDFOBJECT ob, float widthInches, float heightInches)
		{
			PDFResult res;
			res.Object = ob;
			res.ErrorCode = EDrawError::ErrorNone;

			try {
				PDFDoc *doc = reinterpret_cast<PDFDoc*>(ob);

				Page pg = doc->PageCreate(pdftron::PDF::Rect(0, 0, widthInches * DotsPerInch, heightInches * DotsPerInch));
				doc->PagePushBack(pg);

				ElementWriter ew;
				ew.Begin(pg, ElementWriter::e_overlay, false);
				ew.End();
			}
			catch (pdftron::Common::Exception &exc)
			{
				HandleException(res, exc);
			}

			return res;
		}

		PDFResult SavePDF(PDFOBJECT ob, const TCHAR *path)
		{
			PDFResult res;
			res.Object = ob;
			res.ErrorCode = EDrawError::ErrorNone;

			try
			{
				PDFDoc *doc = reinterpret_cast<PDFDoc*>(ob);

				if (doc == nullptr)
				{
					res.ErrorCode = EDrawError::ErrorBadParam;
					return res;
				}

				doc->Save(pdftron::UString(path), SDFDoc::e_compatibility);
			}
			catch (pdftron::Common::Exception &exc)
			{
				HandleException(res, exc);
			}

			return res;
		}

		PDFResult ClosePDF(PDFOBJECT ob)
		{
			PDFResult res;
			res.Object = ob;
			res.ErrorCode = EDrawError::ErrorNone;

			PDFDoc *doc = reinterpret_cast<PDFDoc*>(ob);

			if (doc == nullptr)
			{
				res.ErrorCode = EDrawError::ErrorBadParam;
				return res;
			}

			res.Object = nullptr;
			doc->Close();

			return res;
		}


		PDFResult FillPoly(PDFOBJECT ob,
			int pageNumber,
			const float *points,
			int numPoints,
			const FMColor &color,
			const float scale)
		{
			PDFResult res;
			res.ErrorCode = EDrawError::ErrorNone;
			res.Object = ob;
			try
			{
				PDFDoc *doc = reinterpret_cast<PDFDoc*>(ob);
				if (doc == nullptr)
				{
					res.ErrorCode = EDrawError::ErrorBadParam;
					return res;
				}

				Page page = doc->GetPage(pageNumber);
				if (!page.IsValid())
				{
					res.ErrorCode = EDrawError::ErrorBadParam;
					return res;
				}

				if (numPoints < 3)
				{
					res.ErrorCode = EDrawError::ErrorBadParam;
					return res;
				}

				ElementBuilder elementBuilder;

				elementBuilder.Reset();

				elementBuilder.PathBegin();

				elementBuilder.MoveTo(points[0], points[1]);
				for (int i = 1; i < numPoints; ++i)
				{
					elementBuilder.LineTo(points[i * 2], points[i * 2 + 1]);
				}

				elementBuilder.ClosePath();
				Element element = elementBuilder.PathEnd();

				GState gstate = element.GetGState();

				element.SetPathStroke(true);
				element.SetPathFill(true);

				gstate.SetStrokeColorSpace(ColorSpace::CreateDeviceRGB());
				gstate.SetStrokeColor(ColorPt(color.R, color.G, color.B));
				gstate.SetFillColorSpace(ColorSpace::CreateDeviceRGB());
				gstate.SetFillColor(ColorPt(color.R, color.G, color.B));

				gstate.SetTransform(1, 0, 0, 1, 0, 0);
				gstate.SetLineWidth(0);

				ElementWriter elementWriter;
				elementWriter.Begin(page);
				elementWriter.WriteElement(element);
				elementWriter.End();
			}
			catch (pdftron::Common::Exception &exc)
			{
				HandleException(res, exc);
			}
			return res;
		}

		PDFResult DrawLine(PDFOBJECT ob,
			int pageNumber,
			const ModumateUnitParams::FXCoord &x1,
			const ModumateUnitParams::FYCoord &y1,
			const ModumateUnitParams::FXCoord &x2,
			const ModumateUnitParams::FYCoord &y2,
			const ModumateUnitParams::FThickness &thickness,
			const FMColor &lineColor,
			const LinePattern &linePattern,
			const ModumateUnitParams::FPhase &patternPhase,
			const float scale)
		{
			PDFResult res;
			res.ErrorCode = EDrawError::ErrorNone;
			res.Object = ob;

			try
			{
				PDFDoc *doc = reinterpret_cast<PDFDoc*>(ob);

				if (doc == nullptr)
				{
					res.ErrorCode = EDrawError::ErrorBadParam;
					return res;
				}

				Page page = doc->GetPage(pageNumber);

				if (!page.IsValid())
				{
					res.ErrorCode = EDrawError::ErrorBadParam;
					return res;
				}

				ElementBuilder elementBuilder;
				ElementWriter elementWriter;
				Element element;

				elementBuilder.Reset();
				elementBuilder.PathBegin();
				elementBuilder.MoveTo(x1.AsPoints(scale), y1.AsPoints(scale));
				elementBuilder.LineTo(x2.AsPoints(scale), y2.AsPoints(scale));
				element = elementBuilder.PathEnd();

				element.SetPathStroke(true);
				element.SetPathFill(false);

				GState gstate = element.GetGState();
				if (linePattern.LineStyle == DraftingLineStyle::Dashed)
				{
					gstate.SetDashPattern(linePattern.DashPattern, patternPhase.AsPoints(scale));
				}

				float r = lineColor.R;
				float g = lineColor.G;
				float b = lineColor.B;

				gstate.SetStrokeColorSpace(ColorSpace::CreateDeviceRGB());
				gstate.SetStrokeColor(ColorPt(r,g,b));
				gstate.SetFillColorSpace(ColorSpace::CreateDeviceRGB());
				gstate.SetFillColor(ColorPt(r, g, b));
				gstate.SetTransform(1, 0, 0, 1, 0, 0);
				gstate.SetLineWidth(thickness.AsPoints(scale));

				elementWriter.Begin(page);
				elementWriter.WriteElement(element);
				elementWriter.End();
			}
			catch (pdftron::Common::Exception &exc)
			{
				HandleException(res, exc);
			}

			return res;
		}

		const wchar_t* GetFontPath(FontType type)
		{
			switch (type)
			{
			case (FontType::Bold):
					return L"c:\\windows\\fonts\\arialbd.ttf";

			case (FontType::Italic):
					return L"c:\\windows\\fonts\\ariali.ttf";

			case (FontType::Standard):
					return L"c:\\windows\\fonts\\arial.ttf";
			}

			return L"";
		}

		PDFResult AddText(PDFOBJECT ob,
			int page,
			const TCHAR *text,
			const ModumateUnitParams::FFontSize &fontSize,
			const ModumateUnitParams::FXCoord &xpos,
			const ModumateUnitParams::FYCoord &ypos,
			const ModumateUnitParams::FAngle &rotateByRadians,
			const FMColor &color,
			const float scale,
			DraftingAlignment textJustify,
			const ModumateUnitParams::FWidth &containingRectWidth,
			FontType type)
		{
			PDFResult res;
			res.ErrorCode = EDrawError::ErrorNone;
			res.Object = ob;

			try
			{
				PDFDoc *doc = reinterpret_cast<PDFDoc*>(ob);
				double h, v;

				if (doc == nullptr)
				{
					res.ErrorCode = EDrawError::ErrorBadParam;
					return res;
				}

				Page pg = doc->GetPage(page);

				if (!pg.IsValid())
				{
					res.ErrorCode = EDrawError::ErrorBadParam;
					return res;
				}

				ElementBuilder elementBuilder;
				ElementWriter elementWriter;
				Element element;

				elementWriter.Begin(pg);

				Font font = Font::CreateCIDTrueTypeFont(*doc, pdftron::UString(GetFontPath(type)));

				element = elementBuilder.CreateTextBegin(font, fontSize.AsFontHeight(scale));
				element.SetPathFill(true);
				elementWriter.WriteElement(element);

				pdftron::UString str(text);
				element = elementBuilder.CreateUnicodeTextRun(str.CStr(), str.GetLength());

				float textLength = (float)element.GetTextLength();

				// rotateByRadians has to be passed in to pdftron::Common::Matrix2D::RotationMatrix with a negative sign.
				// Please see documentation for pdftron::Common::Matrix2D::RotationMatrix.
				pdftron::Common::Matrix2D textMatrix = pdftron::Common::Matrix2D::RotationMatrix(-rotateByRadians.AsRadians());

				switch (textJustify) {
				case DraftingAlignment::Left:
					h = xpos.AsPoints(scale);
					v = ypos.AsPoints(scale);
					break;
				case DraftingAlignment::Center:
					h = xpos.AsPoints(scale) - (textLength/2) * cosf(rotateByRadians.AsRadians());
					v = ypos.AsPoints(scale) - (textLength/2) * sinf(rotateByRadians.AsRadians());
					break;
				case DraftingAlignment::Right:
					h = xpos.AsPoints(scale) + (containingRectWidth.AsPoints(scale) - textLength) * cosf(rotateByRadians.AsRadians());
					v = ypos.AsPoints(scale) - textLength * sinf(rotateByRadians.AsRadians());
					break;
				default:
					// Left Justify by default
					h = xpos.AsPoints(scale);
					v = ypos.AsPoints(scale);
					break;
				}

				textMatrix.Translate(h, v);
				element.SetTextMatrix(textMatrix);

				// Set the font color (using CMYK color system)
				// Save off the previous fill color space
				ColorSpace prevColorSpace = element.GetGState().GetFillColorSpace();
				GState gState = element.GetGState();
				gState.SetTextRenderMode(pdftron::PDF::GState::TextRenderingMode::e_fill_text);
				ColorSpace colorSpace = ColorSpace::CreateDeviceRGB();
				gState.SetFillColorSpace(colorSpace);
				gState.SetFillColor(ColorPt(color.R,color.G,color.B));

				elementWriter.WriteElement(element);

				// Restore the fill color to black
				gState.SetFillColor(ColorPt(0.0f, 0.0f, 0.0f, 1.0f));
				gState.SetFillColorSpace(prevColorSpace);

				elementWriter.WriteElement(elementBuilder.CreateTextEnd());
				elementWriter.End();
			}
			catch (pdftron::Common::Exception &exc)
			{
				HandleException(res, exc);
			}

			return res;
		}

		PDFResult GetTextLength(PDFOBJECT ob,
			const TCHAR *text,
			const ModumateUnitParams::FFontSize &fontSize,
			FModumateUnitValue &textLength,
			const float scale,
			FontType type)
		{
			PDFResult res;
			res.ErrorCode = EDrawError::ErrorNone;
			res.Object = ob;

			try
			{
				PDFDoc *doc = reinterpret_cast<PDFDoc*>(ob);

				if (doc == nullptr)
				{
					res.ErrorCode = EDrawError::ErrorBadParam;
					return res;
				}

				ElementBuilder elementBuilder;
				Element element;

				Font font = Font::CreateCIDTrueTypeFont(*doc, pdftron::UString(GetFontPath(type)));

				element = elementBuilder.CreateTextBegin(font, fontSize.AsFontHeight(scale));
				pdftron::UString str(text);
				element = elementBuilder.CreateUnicodeTextRun(str.CStr(), str.GetLength());
				textLength = FModumateUnitValue::Points((float)element.GetTextLength());
			}
			catch (pdftron::Common::Exception &exc)
			{
				HandleException(res, exc);
			}

			return res;
		}

		PDFResult DrawCircle(PDFOBJECT ob,
			int pageNumber,
			const ModumateUnitParams::FXCoord &cx,
			const ModumateUnitParams::FYCoord &cy,
			const ModumateUnitParams::FRadius &radius,
			const ModumateUnitParams::FThickness &lineWidth,
			const LinePattern &linePattern,
			const FMColor &color,
			const float scale)
		{
			PDFResult res;
			res.ErrorCode = EDrawError::ErrorNone;
			res.Object = ob;
			try
			{
				PDFDoc *doc = reinterpret_cast<PDFDoc*>(ob);

				if (doc == nullptr)
				{
					res.ErrorCode = EDrawError::ErrorBadParam;
					return res;
				}

				Page page = doc->GetPage(pageNumber);

				if (!page.IsValid())
				{
					res.ErrorCode = EDrawError::ErrorBadParam;
					return res;
				}

				ElementBuilder elementBuilder;
				ElementWriter elementWriter;
				Element element;

				elementWriter.Begin(page);
				elementBuilder.Reset();			// Reset the GState to default
				element = elementBuilder.CreateEllipse(cx.AsPoints(scale), cy.AsPoints(scale), radius.AsPoints(scale), radius.AsPoints(scale));

				element.SetPathStroke(true); // this path should be stroked

				GState gstate = element.GetGState();
				element.SetPathFill(false);	 // this path is should not be filled

				if (linePattern.LineStyle == DraftingLineStyle::Dashed)
				{
					gstate.SetDashPattern(linePattern.DashPattern, 0);
				}

				gstate.SetStrokeColorSpace(ColorSpace::CreateDeviceRGB());
				gstate.SetStrokeColor(ColorPt(color.R,color.G,color.B));
				gstate.SetFillColorSpace(ColorSpace::CreateDeviceRGB());
				gstate.SetFillColor(ColorPt(color.R, color.G, color.B));

				gstate.SetTransform(1, 0, 0, 1, 0, 0);
				gstate.SetLineWidth(lineWidth.AsPoints(scale));

				elementWriter.WriteElement(element);
				elementWriter.End();

			}
			catch (pdftron::Common::Exception &exc)
			{
				HandleException(res, exc);
			}
			return res;
		}

		PDFResult FillCircle(PDFOBJECT ob,
			int pageNumber,
			const ModumateUnitParams::FXCoord &cx,
			const ModumateUnitParams::FYCoord &cy,
			const ModumateUnitParams::FRadius &radius,
			const FMColor &color,
			const float scale)
		{
			PDFResult res;
			res.ErrorCode = EDrawError::ErrorNone;
			res.Object = ob;
			try
			{
				PDFDoc *doc = reinterpret_cast<PDFDoc*>(ob);

				if (doc == nullptr)
				{
					res.ErrorCode = EDrawError::ErrorBadParam;
					return res;
				}

				Page page = doc->GetPage(pageNumber);

				if (!page.IsValid())
				{
					res.ErrorCode = EDrawError::ErrorBadParam;
					return res;
				}

				ElementBuilder elementBuilder;
				ElementWriter elementWriter;
				Element element;

				elementWriter.Begin(page);
				elementBuilder.Reset();			// Reset the GState to default
				element = elementBuilder.CreateEllipse(cx.AsPoints(scale), cy.AsPoints(scale), radius.AsPoints(scale), radius.AsPoints(scale));

				element.SetPathStroke(false); // this path should be stroked

				GState gstate = element.GetGState();
				element.SetPathFill(true);	 // this path is should not be filled


				gstate.SetStrokeColorSpace(ColorSpace::CreateDeviceRGB());
				gstate.SetStrokeColor(ColorPt(color.R, color.G, color.B));
				gstate.SetFillColorSpace(ColorSpace::CreateDeviceRGB());
				gstate.SetFillColor(ColorPt(color.R,color.G,color.B));

				gstate.SetTransform(1, 0, 0, 1, 0, 0);
				gstate.SetLineWidth(1.0f);

				elementWriter.WriteElement(element);
				elementWriter.End();

			}
			catch (pdftron::Common::Exception &exc)
			{
				HandleException(res, exc);
			}
			return res;
		}

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
			const float scale)
		{

			// TODO: ElementBuilder has ArcTo functions that may be simpler to use than this implementation
			PDFResult res;
			res.ErrorCode = EDrawError::ErrorNone;
			res.Object = ob;
			try
			{
				float cosA1 = cosf(a1.AsRadians());
				float sinA1 = sinf(a1.AsRadians());
				float cosA2 = cosf(a2.AsRadians());
				float sinA2 = sinf(a2.AsRadians());

				ModumateUnitParams::FXCoord x1 = x + radius * cosA1;
				ModumateUnitParams::FYCoord y1 = y + radius * sinA1;
				ModumateUnitParams::FXCoord x2 = x + radius * cosA2;
				ModumateUnitParams::FYCoord y2 = y + radius * sinA2;

				// the midpoint
				float mx = (x1.AsFloorplanInches(scale) + x2.AsFloorplanInches(scale))*0.5f;
				float my = (y1.AsFloorplanInches(scale) + y2.AsFloorplanInches(scale))*0.5f;

				// the direction
				float dx = (x2.AsFloorplanInches(scale) - x1.AsFloorplanInches(scale));
				float dy = (y2.AsFloorplanInches(scale) - y1.AsFloorplanInches(scale));

				// the direction rotated 90 degrees
				float dx90 = -dy;
				float dy90 = dx;

				// the magnitude of the direction
				float m = sqrtf(dx*dx + dy*dy);
				if (fabsf(m) < 0.01f)
				{
					res.ErrorCode = EDrawError::ErrorException;
					return res;
				}

				dx90 /= m;
				dy90 /= m;

				float adjacent = sqrtf((radius.AsFloorplanInches(scale) * radius.AsFloorplanInches(scale)) - (m*m*0.25f));
				float cx = mx + dx90 * adjacent;
				float cy = my + dy90 * adjacent;


				for (int i = 0; i < slices; ++i)
				{
					float f1 = static_cast<float>(i) / slices;
					float f2 = static_cast<float>(i + 1) / slices;

					float sx1 = x1.AsFloorplanInches(scale) + f1 * (x2.AsFloorplanInches(scale) - x1.AsFloorplanInches(scale));
					float sy1 = y1.AsFloorplanInches(scale) + f1 * (y2.AsFloorplanInches(scale) - y1.AsFloorplanInches(scale));
					float m1 = sqrtf((sx1 - cx) * (sx1 - cx) + (sy1 - cy) * (sy1 - cy));
					sx1 = cx + (sx1-cx) * radius.AsFloorplanInches(scale) / m1;
					sy1 = cy + (sy1-cy) * radius.AsFloorplanInches(scale) / m1;

					float sx2 = x1.AsFloorplanInches(scale) + f2 * (x2.AsFloorplanInches(scale) - x1.AsFloorplanInches(scale));
					float sy2 = y1.AsFloorplanInches(scale) + f2 * (y2.AsFloorplanInches(scale) - y1.AsFloorplanInches(scale));
					float m2 = sqrtf((sx2 - cx) * (sx2 - cx) + (sy2 - cy) * (sy2 - cy));
					sx2 = cx + (sx2-cx) * radius.AsFloorplanInches(scale) / m2;
					sy2 = cy + (sy2-cy) * radius.AsFloorplanInches(scale) / m2;


					DrawLine(ob,
						pageNumber,
						ModumateUnitParams::FXCoord::FloorplanInches(sx1),
						ModumateUnitParams::FYCoord::FloorplanInches(sy1),
						ModumateUnitParams::FXCoord::FloorplanInches(sx2),
						ModumateUnitParams::FYCoord::FloorplanInches(sy2),
						lineWidth,
						color,
						linePattern,
						ModumateUnitParams::FPhase::Points(0),
						scale);
				}
			}
			catch (pdftron::Common::Exception &exc)
			{
				HandleException(res, exc);
			}
			return res;
		}

		PDFResult AddImage(PDFOBJECT ob,
			int pageNumber,
			const TCHAR *imageFileDir,
			const ModumateUnitParams::FXCoord &x,
			const ModumateUnitParams::FYCoord &y,
			const ModumateUnitParams::FWidth &w,
			const ModumateUnitParams::FHeight &h,
			const float scale)
		{
			PDFResult res;
			res.ErrorCode = EDrawError::ErrorNone;
			res.Object = ob;

			try
			{
				PDFDoc *doc = reinterpret_cast<PDFDoc*>(ob);

				if (doc == nullptr)
				{
					res.ErrorCode = EDrawError::ErrorBadParam;
					return res;
				}

				Page pg = doc->GetPage(pageNumber);

				if (!pg.IsValid())
				{
					res.ErrorCode = EDrawError::ErrorBadParam;
					return res;
				}

				ElementBuilder elementBuilder;
				ElementWriter elementWriter;
				Element element;

				elementWriter.Begin(pg);

				if (!IFileManager::Get().FileExists(imageFileDir))
				{
					res.ErrorCode = EDrawError::ErrorBadParam;
					return res;
				}

				// ----------------------------------------------------------
				// Add JPEG image to the output PDF file. The image size will be scaled
				// to fit the target area. The aspect ratio of the target area should
				// match the aspect ratio of the source image.
				pdftron::PDF::Image img = pdftron::PDF::Image::Create(*doc, pdftron::UString(imageFileDir));

				float width = w.AsFloorplanInches(scale);
				float height = h.AsFloorplanInches(scale);

				// Re-calculate the width and height of the image element, if necessary
				float sourceImageWidth = img.GetImageWidth(), sourceImageHeight = img.GetImageHeight();
				float sourceImageAspectRatio = sourceImageWidth / sourceImageHeight;
				float targetAreaAspectRatio = width / height;

				float xpos = x.AsFloorplanInches(scale);
				float ypos = y.AsFloorplanInches(scale);

				if (targetAreaAspectRatio > sourceImageAspectRatio)
				{
					// Target area width is too large; shrink it
					float desiredTargetAreaWidth = (height * sourceImageWidth) / sourceImageHeight;
					xpos = xpos + (width - desiredTargetAreaWidth) / 2.0;
					width = desiredTargetAreaWidth;
				}
				else if (targetAreaAspectRatio < sourceImageAspectRatio)
				{
					// Target area height is too large; shrink it
					float desiredTargetHeight = (width * sourceImageHeight) / sourceImageWidth;
					ypos = ypos + (height - desiredTargetHeight) / 2.0;
					height = desiredTargetHeight;
				}

				element = elementBuilder.CreateImage(img, xpos * DotsPerInch, ypos * DotsPerInch, width * DotsPerInch, height * DotsPerInch);
				elementWriter.WritePlacedElement(element);
				elementWriter.End();
			}
			catch (pdftron::Common::Exception &exc)
			{
				HandleException(res, exc);
			}

			return res;
		}

		//TODO: remove once the dwg pipeline is in
		void Test3D()
		{
			return;
			PDFResult doc = CreatePDF();
			PDFDoc *pdoc = reinterpret_cast<PDFDoc*>(doc.Object);
//			PDFResult page = AddPage(doc.Object, 8.5, 11);
			PDFResult res;

#if 0
			PDFResult res = AddText(doc.Object, 1, TEXT("THIS IS A TEST"),
				FFontSize::FloorplanInches(0.5f),
				FXCoord::FloorplanInches(3),
				FYCoord::FloorplanInches(3),
				FAngle::Degrees(0),
				FMColor::Black);
#endif

			try
			{
				if (CADModule::IsModuleAvailable())
				{
					pdftron::PDF::Convert::FromCAD(*pdoc, "c:/test/test3d.dwg", NULL);
					res = SavePDF(doc.Object, TEXT("c:/test/test3d.dwg.pdf"));
					if (res.ErrorCode != EDrawError::ErrorNone)
					{
						Modumate::PlatformFunctions::ShowMessageBox(TEXT("Could Not Save"), TEXT("ERROR"), Modumate::PlatformFunctions::Okay);
					}
					res = ClosePDF(doc.Object);
					if (res.ErrorCode != EDrawError::ErrorNone)
					{
						Modumate::PlatformFunctions::ShowMessageBox(TEXT("Could Not Close"), TEXT("ERROR"), Modumate::PlatformFunctions::Okay);
					}
				}
				else
				{
					Modumate::PlatformFunctions::ShowMessageBox(TEXT("CAD Module Not Available"), TEXT("ERROR"), Modumate::PlatformFunctions::Okay);
				}

			}
			catch (pdftron::Common::Exception& e)
			{
				HandleException(res, e);

			}
		}
	}



	/*
	PDF Draw Interface
	*/

	FMColor FMColor::Black = FMColor(0, 0, 0);
	FMColor FMColor::White = FMColor(1, 1, 1);
	FMColor FMColor::Gray32 = FMColor(32.0f / 255.0f, 32.0f / 255.0f, 32.0f / 255.0f);
	FMColor FMColor::Gray64 = FMColor(64.0f / 255.0f, 64.0f / 255.0f, 64.0f / 255.0f);
	FMColor FMColor::Gray96 = FMColor(96.0f / 255.0f, 96.0f / 255.0f, 96.0f / 255.0f);
	FMColor FMColor::Gray128 = FMColor(128.0f / 255.0f, 128.0f / 255.0f, 128.0f / 255.0f);
	FMColor FMColor::Gray144 = FMColor(144.0f / 255.0f, 144.0f / 255.0f, 144.0f / 255.0f);
	FMColor FMColor::Gray160 = FMColor(160.0f / 255.0f, 160.0f / 255.0f, 160.0f / 255.0f);
	FMColor FMColor::Gray208 = FMColor(208.0f / 255.0f, 208.0f / 255.0f, 208.0f / 255.0f);


	EDrawError FModumatePDFDraw::DrawLine(
		const ModumateUnitParams::FXCoord &x1,
		const ModumateUnitParams::FYCoord &y1,
		const ModumateUnitParams::FXCoord &x2,
		const ModumateUnitParams::FYCoord &y2,
		const ModumateUnitParams::FThickness &thickness,
		const FMColor &color,
		const LinePattern &linePattern,
		const ModumateUnitParams::FPhase &phase,
		FModumateLayerType layerType
	)
	{
		return PDF::DrawLine(
			Doc.Object,
			PageNum,
			x1, y1, x2, y2,
			thickness,
			color,
			linePattern,
			phase,
			DrawingScale).ErrorCode;
	}

	EDrawError FModumatePDFDraw::AddText(
		const TCHAR *text,
		const ModumateUnitParams::FFontSize &fontSize,
		const ModumateUnitParams::FXCoord &xpos,
		const ModumateUnitParams::FYCoord &ypos,
		const ModumateUnitParams::FAngle &rotateByRadians,
		const FMColor &color,
		DraftingAlignment textJustify,
		const ModumateUnitParams::FWidth &containingRectWidth,
		FontType type = FontType::Standard,
		FModumateLayerType layerType
	)
	{
		return PDF::AddText(
			Doc.Object,
			PageNum,
			text,
			fontSize,
			xpos,
			ypos,
			rotateByRadians,
			color,
			DrawingScale,
			textJustify,
			containingRectWidth,
			type).ErrorCode;
	};

	EDrawError FModumatePDFDraw::GetTextLength(
		const TCHAR *text,
		const ModumateUnitParams::FFontSize &fontSize,
		FModumateUnitValue &textLength,
		FontType type = FontType::Standard)
	{
		return PDF::GetTextLength(Doc.Object, text, fontSize, textLength, DrawingScale, type).ErrorCode;
	}

	EDrawError FModumatePDFDraw::DrawArc(
		const ModumateUnitParams::FXCoord &x,
		const ModumateUnitParams::FYCoord &y,
		const ModumateUnitParams::FAngle &a1,
		const ModumateUnitParams::FAngle &a2,
		const ModumateUnitParams::FRadius &radius,
		const ModumateUnitParams::FThickness &lineWidth,
		const FMColor &color,
		const LinePattern &linePattern,
		int slices,
		FModumateLayerType layerType)
	{
		return PDF::DrawArc(
			Doc.Object,
			PageNum,
			x, y, a1, a2,
			radius,
			lineWidth,
			color,
			linePattern,
			slices,
			DrawingScale).ErrorCode;
	}

	EDrawError FModumatePDFDraw::AddImage(
		const TCHAR *imageFileFullPath,
		const ModumateUnitParams::FXCoord &x,
		const ModumateUnitParams::FYCoord &y,
		const ModumateUnitParams::FWidth &width,
		const ModumateUnitParams::FHeight &height,
		FModumateLayerType layerType)
	{
		return PDF::AddImage(
			Doc.Object,
			PageNum,
			imageFileFullPath,
			x, y,
			width, height,
			DrawingScale).ErrorCode;
	}

	EDrawError FModumatePDFDraw::FillPoly(
		const float *points,
		int numPoints,
		const FMColor &color,
		FModumateLayerType layerType)
	{
		return PDF::FillPoly(
			Doc.Object,
			PageNum,
			points,
			numPoints,
			color,
			DrawingScale).ErrorCode;
	}

	EDrawError FModumatePDFDraw::DrawCircle(
		const ModumateUnitParams::FXCoord &cx,
		const ModumateUnitParams::FYCoord &cy,
		const ModumateUnitParams::FRadius &radius,
		const ModumateUnitParams::FThickness &lineWidth,
		const LinePattern &linePattern,
		const FMColor &color,
		FModumateLayerType layerType)
	{
		return PDF::DrawCircle(
			Doc.Object,
			PageNum,
			cx, cy,
			radius, lineWidth,
			linePattern,
			color,
			DrawingScale).ErrorCode;
	}

	EDrawError FModumatePDFDraw::FillCircle(
		const ModumateUnitParams::FXCoord &cx,
		const ModumateUnitParams::FYCoord &cy,
		const ModumateUnitParams::FRadius &radius,
		const FMColor &color,
		FModumateLayerType layerType)
	{
		return PDF::FillCircle(Doc.Object, PageNum, cx, cy, radius, color, DrawingScale).ErrorCode;
	}

	using namespace PDF;

	bool Modumate::FModumatePDFDraw::StartPage(int32 pageNumber, float widthInches, float heightInches, FString pageName)
	{
		PageNum = pageNumber;
		AddPage(Doc.Object, widthInches, heightInches);
		return true;
	}

	bool Modumate::FModumatePDFDraw::SaveDocument(const FString& filename)
	{
		auto doc = SavePDF(Doc.Object, *filename);
		ClosePDF(doc.Object);

		return doc.ErrorCode == EDrawError::ErrorNone;
	}
}

#pragma warning (default:4456)
#pragma warning (default:4458)
#pragma warning (default:4668)

