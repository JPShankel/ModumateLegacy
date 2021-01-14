#include "CoreMinimal.h"

#include "Drafting/APDFLLib.h"
#include "Drafting/ModumateDraftingElements.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "ModumateCore/ModumateUnits.h"

namespace Modumate
{
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateDraftingClipRectangle, "Modumate.Drafting.Drawing.ClipRectangle", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
		bool FModumateDraftingClipRectangle::RunTest(const FString& Parameters)
	{
		bool ret = true;

		// Test 1 - Big numbers
		FVector2D minBorder = FVector2D(0.f, 0.f);
		FVector2D maxBorder = FVector2D(1741.f, 796.f);

		FVector2D lineStart = FVector2D(1110.f, 276.f);
		FVector2D lineEnd = FVector2D(7287565824.f, -2740469504.f);

		FVector2D returnLineStart, returnLineEnd;
		float tolerance = 0.02f;
		bool clipTest = UModumateFunctionLibrary::ClipLine2DToRectangle(
			lineStart, lineEnd,
			FBox2D(minBorder, maxBorder),
			returnLineStart, returnLineEnd, tolerance);

		FVector2D originalDir = (lineEnd - lineStart).GetSafeNormal();
		FVector2D newDir = (returnLineEnd - returnLineStart).GetSafeNormal();
		ret = originalDir.Equals(newDir);

		// Test 2 - Simple
		minBorder = FVector2D(0.f, 0.f);
		maxBorder = FVector2D(1741.f, 796.f);

		lineStart = FVector2D(1110.f, 276.f);
		lineEnd = FVector2D(174.f, 79.f);

		tolerance = 0.02f;
		clipTest = UModumateFunctionLibrary::ClipLine2DToRectangle(
			lineStart, lineEnd,
			FBox2D(minBorder, maxBorder),
			returnLineStart, returnLineEnd, tolerance);

		originalDir = (lineEnd - lineStart).GetSafeNormal();
		newDir = (returnLineEnd - returnLineStart).GetSafeNormal();
		ret = ret && originalDir.Equals(newDir);

		return ret;
	}

	namespace PDF
	{
		IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateDraftingPDF3D, "Modumate.Drafting.PDF.Test3D", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
			bool FModumateDraftingPDF3D::RunTest(const FString& Parameters)
		{
			// TODO: this test looks like it is dead because it returns at the beginning of the function
			Test3D();

			return true;
		}
	}
	namespace Units
	{
		IMPLEMENT_SIMPLE_AUTOMATION_TEST(FModumateDraftingPDF3D, "Modumate.Drafting.PDF.DrawPrimitives", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter | EAutomationTestFlags::HighPriority)
			bool FModumateDraftingPDF3D::RunTest(const FString& Parameters)

		{
			UE_LOG(LogCallTrace, Display, TEXT("ModumateDraftingView::TestPrimitivesPDF"));

			FString filename = FPaths::ProjectDir() + "composites.pdf";
			int pageNumber = 1;
			const char * emptyCaption = "";

			PDF::PDFResult doc = PDF::CreatePDF();
			if (doc.ErrorCode != EDrawError::ErrorNone)
			{
				return false;
			}

			doc = PDF::AddPage(doc.Object, 8.5, 11);

			if (doc.ErrorCode != EDrawError::ErrorNone)
			{
				return false;
			}

			FModumatePDFDraw drawingInterface;
			drawingInterface.PageNum = 1;
			drawingInterface.Doc.Object = doc.Object;

			ModumateUnitParams::FXCoord xpos = ModumateUnitParams::FXCoord::FloorplanInches(4.0f);
			ModumateUnitParams::FYCoord ypos = ModumateUnitParams::FYCoord::FloorplanInches(8.3f);
			ModumateUnitParams::FAngle angle = ModumateUnitParams::FAngle::Degrees(0);


			TSharedPtr<FDraftingComposite> testTag = MakeShareable(new FDraftingComposite());

			for (int x = 0; x < 10; x++)
			{
				TSharedPtr<FDraftingComposite> tag = MakeShareable(new FDraftingComposite());
				tag->SetLocalPosition(ModumateUnitParams::FXCoord::FloorplanInches(0.15f * x), ModumateUnitParams::FYCoord::FloorplanInches(0.0f));

				TSharedPtr<FDraftingRectangle> rect = MakeShareable(new FDraftingRectangle(
					FModumateUnitCoord2D(ModumateUnitParams::FXCoord::FloorplanInches(0.1f), ModumateUnitParams::FYCoord::FloorplanInches(0.2f)),
					ModumateUnitParams::FThickness::FloorplanInches(0.01f),
					FMColor::Black));
				rect->HorizontalAlignment = DraftingAlignment::Center;
				rect->VerticalAlignment = DraftingAlignment::Center;

				TSharedPtr<FDraftingText> text = MakeShareable(new FDraftingText(FText::AsNumber(x)));
				text->HorizontalAlignment = DraftingAlignment::Center;
				text->VerticalAlignment = DraftingAlignment::Center;

				tag->Children.Add(rect);
				tag->Children.Add(text);

				testTag->Children.Add(tag);
			}

			testTag->Draw(&drawingInterface);

			testTag->SetLocalPosition(xpos, ypos);
			testTag->Draw(&drawingInterface);

			xpos = ModumateUnitParams::FXCoord::FloorplanInches(4.5f);
			ypos = ModumateUnitParams::FYCoord::FloorplanInches(6.0f);
			testTag->SetLocalPosition(xpos, ypos);
			testTag->SetLocalOrientation(ModumateUnitParams::FAngle::Degrees(45));
			testTag->Draw(&drawingInterface);

			xpos = ModumateUnitParams::FXCoord::FloorplanInches(4.0f);
			ypos = ModumateUnitParams::FYCoord::FloorplanInches(7.0f);
			testTag->SetLocalPosition(xpos, ypos);
			testTag->SetLocalOrientation(ModumateUnitParams::FAngle::Degrees(90));
			testTag->Draw(&drawingInterface);

			TSharedPtr<FDraftingComposite> testAngles = MakeShareable(new FDraftingComposite());
			xpos = ModumateUnitParams::FXCoord::FloorplanInches(2.5f);

			testAngles->SetLocalPosition(xpos, ypos);

			ModumateUnitParams::FRadius radius = ModumateUnitParams::FRadius::FloorplanInches(0.1f);
			ModumateUnitParams::FAngle degs = ModumateUnitParams::FAngle::Degrees(45.0f);
			ModumateUnitParams::FThickness lineWidth = ModumateUnitParams::FThickness::FloorplanInches(0.01f);

			TSharedPtr<FDraftingArc> arc1 = MakeShareable(new FDraftingArc(radius, degs, lineWidth));
			testAngles->Children.Add(arc1);

			degs = ModumateUnitParams::FAngle::Degrees(150.0f);
			TSharedPtr<FDraftingArc> arc2 = MakeShareable(new FDraftingArc(radius, degs, lineWidth));
			arc2->MoveXTo(ModumateUnitParams::FXCoord::FloorplanInches(0.25f));
			arc2->SetLocalOrientation(ModumateUnitParams::FAngle::Degrees(270.0f));
			testAngles->Children.Add(arc2);

			testAngles->SetLocalOrientation(ModumateUnitParams::FAngle::Degrees(45.0f));

			testAngles->Draw(&drawingInterface);

			doc = PDF::SavePDF(doc.Object, *filename);
			PDF::ClosePDF(doc.Object);

			return true;
		}
	}
}
