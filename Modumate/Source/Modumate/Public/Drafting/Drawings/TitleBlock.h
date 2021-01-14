// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "DocumentManagement/ModumateDocument.h"
#include "ModumateCore/ModumateUnits.h"
#include "Drafting/Drawings/DraftingDrawing.h"

namespace Modumate
{
	class IModumateDraftingDraw;

	class MODUMATE_API FTitleBlock : public FDraftingDrawing
	{
	public:
		FTitleBlock(const UModumateDocument *doc, UWorld *world, SceneCaptureID captureObjID);

		// TODO: most drafting objects do not have a document as an argument to their initialize function
		EDrawError InitializeWithData(IModumateDraftingDraw *drawingInterface, const UModumateDocument *Document);

		EDrawError MakeContent(IModumateDraftingDraw *drawingInterface, FText headerText, TSharedPtr<FDraftingComposite> content);
		EDrawError MakeSeparatorLine();
		EDrawError MakePersonalInformation(IModumateDraftingDraw *drawingInterface, UModumateDocument::FPartyProfile profile);

	private:
		ModumateUnitParams::FYCoord ContentHeight = ModumateUnitParams::FYCoord::FloorplanInches(0.0f);

	// formatting
	public:
		// Title block
		FModumateUnitCoord2D ContentMargin = FModumateUnitCoord2D(ModumateUnitParams::FXCoord::FloorplanInches(4.0f / 64.0f), ModumateUnitParams::FYCoord::FloorplanInches(8.0f / 64.0f));

		ModumateUnitParams::FFontSize SmallFontSize = ModumateUnitParams::FFontSize::FloorplanInches(4.0f / 64.0f);
		// Header Format
		FMColor HeaderColor = FMColor::Gray144;
		ModumateUnitParams::FFontSize HeaderFontSize = ModumateUnitParams::FFontSize::FloorplanInches(6.0f / 64.0f);
		FontType HeaderType = FontType::Italic;

		// Content Format
		FMColor ContentColor = FMColor::Black;
		FontType ContentType = FontType::Bold;

		// Sheet Info Format
		ModumateUnitParams::FFontSize SheetDateFontSize = ModumateUnitParams::FFontSize::FloorplanInches(8.0f / 64.0f);
		ModumateUnitParams::FFontSize SheetNameFontSize = ModumateUnitParams::FFontSize::FloorplanInches(16.0f / 64.0f);
		ModumateUnitParams::FFontSize SheetNumberFontSize = ModumateUnitParams::FFontSize::FloorplanInches(24.0f / 64.0f);

		// Line separator Format
		FMColor LineColor = FMColor::Black;
		ModumateUnitParams::FThickness LineThickness = ModumateUnitParams::FThickness::Points(0.15f);

		// Stamp Format
		ModumateUnitParams::FYCoord StampHeight = ModumateUnitParams::FYCoord::FloorplanInches(3.0f);

	public:
		int32 TotalPages;
		int32 SheetIndex;
		FText SheetNumber;
		FText SheetDate;
		FText SheetName;
	};
}

