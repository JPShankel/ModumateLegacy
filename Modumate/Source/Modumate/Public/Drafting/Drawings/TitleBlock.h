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
		Units::FYCoord ContentHeight = Units::FYCoord::FloorplanInches(0.0f);

	// formatting
	public:
		// Title block
		Units::FCoordinates2D ContentMargin = Units::FCoordinates2D(Units::FXCoord::FloorplanInches(4.0f / 64.0f), Units::FYCoord::FloorplanInches(8.0f / 64.0f));

		Units::FFontSize SmallFontSize = Units::FFontSize::FloorplanInches(4.0f / 64.0f);
		// Header Format
		FMColor HeaderColor = FMColor::Gray144;
		Units::FFontSize HeaderFontSize = Units::FFontSize::FloorplanInches(6.0f / 64.0f);
		FontType HeaderType = FontType::Italic;

		// Content Format
		FMColor ContentColor = FMColor::Black;
		FontType ContentType = FontType::Bold;

		// Sheet Info Format
		Units::FFontSize SheetDateFontSize = Units::FFontSize::FloorplanInches(8.0f / 64.0f);
		Units::FFontSize SheetNameFontSize = Units::FFontSize::FloorplanInches(16.0f / 64.0f);
		Units::FFontSize SheetNumberFontSize = Units::FFontSize::FloorplanInches(24.0f / 64.0f);

		// Line separator Format
		FMColor LineColor = FMColor::Black;
		Units::FThickness LineThickness = Units::FThickness::Points(0.15f);

		// Stamp Format
		Units::FYCoord StampHeight = Units::FYCoord::FloorplanInches(3.0f);

	public:
		int32 TotalPages;
		int32 SheetIndex;
		FText SheetNumber;
		FText SheetDate;
		FText SheetName;
	};
}

