// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once
#include "ModumateCore/ModumateUnits.h"
#include "Drafting/ModumateDraftingDraw.h"
#include "Drafting/ModumateDraftingElements.h"

namespace Modumate {

	class IModumateDraftingDraw;
	class FWallTag;

	class FDraftingSchedule : public FDraftingComposite
	{
	public:
		virtual ~FDraftingSchedule();
		virtual TSharedPtr<FDraftingText> MakeDraftingText(FText text);

	public:
		TSharedPtr<FDraftingText> Title;
		TArray<FText> ColumnHeaders;
		TArray<TArray<TSharedPtr<FDraftingComposite>>> Data;

	public:
		// Formatting
		float Margin =			2.0f / 64.0f;  // distance between content and cell dividers
		float TitleMargin =		16.0f / 64.0f;  // distance between title and column headers
		float LineWidth =		1.0f / 288.0f; // thickness of cell dividers

		float TitleHeight =		12.0f / 64.0f;	// title height
		float ContentHeight =	6.0f / 64.0f;	// size of cell content
		float RowHeight =		10.0f / 64.0f;	// total size of row

		float IconMargin =		8.0f / 64.0f;	// distance between icon
		float ColumnMargin =	16.0f / 64.0f;	// distance between columns in grids

		float ScheduleMargin =	16.0f / 64.0f; 	// distance between schedules

		float DefaultColumnWidth = 3.0f;		// default max column width, for wrapping and for comments space

	public:
		ModumateUnitParams::FXCoord maxDimensionX = ModumateUnitParams::FXCoord::FloorplanInches(0.0f);

	public:
		// Rendering helper variables
		float columnOffset = 0.0f;
		float scheduleY = 1.0f;

		// Drafting Variables
		ModumateUnitParams::FFontSize DefaultFontSize = ModumateUnitParams::FFontSize::FloorplanInches(ContentHeight);
		FMColor DefaultColor = FMColor::Black;
		FontType DefaultFont = FontType::Standard;
		DraftingAlignment DefaultAlignment = DraftingAlignment::Left;
	};

}
