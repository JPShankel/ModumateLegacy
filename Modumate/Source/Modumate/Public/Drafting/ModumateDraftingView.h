// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include <functional>
#include "CoreMinimal.h"
#include "ModumateUnits.h"
#include "ModumateDraftingPage.h"
#include "TitleBlock.h"
#include "ModumateDraftingTags.h"
#include "Runtime/Engine/Classes/Debug/ReporterGraph.h"

namespace Modumate
{
	class FModumateObjectInstance;
	class FModumateDocument;
	class FDraftingSchedule;

	using namespace Units;

	class IModumateDraftingDraw;

	class MODUMATE_API FModumateDraftingView
	{
	public:
		virtual ~FModumateDraftingView();
		FModumateDraftingView(UWorld *world, FModumateDocument *doc);

	public:
		 FString CurrentFilePath;
	private:
		bool ExportPDF(UWorld *world, const TCHAR *filepath);

	public:
		TArray<TSharedPtr<FDraftingPage>> DraftingPages;

		// List of all schedules; arranged on pages in PaginateScheduleViews
		TArray<TSharedPtr<FDraftingSchedule>> Schedules;

		//
		FModumatePDFDraw DrawingInterface;

		void PaginateScheduleViews(IModumateDraftingDraw *drawingInterface);

	public:
		void GeneratePagesFromCutPlanes(UWorld *world);

	// Generate schedules
	public:
		void GenerateScheduleViews();

		// name and number are defaulted - often name and number are fields that are dependent on pageNumber
		// the fields are public in FDraftingPage so they can be set after this function with the desired format
		TSharedPtr<FDraftingPage> CreateAndAddPage(FText name = FText(), FText number = FText());

	// Receive pages
	public:
		void FinishDraft();

	private:
		TWeakObjectPtr<UWorld> World;
		FModumateDocument * Document;

	public:
		// TODO: this stuff should probably be a setting, as opposed to hard-coded here
		// this can be removed once the remaining schedule code is moved somewhere else or deleted
		FCoordinates2D PageSize = FCoordinates2D(FXCoord::FloorplanInches(17.0f), FYCoord::FloorplanInches(11.0f));
		FCoordinates2D PageMargin = FCoordinates2D(FXCoord::FloorplanInches(0.5f), FYCoord::FloorplanInches(0.5f));

		const float TitleBarWidth = 3.0f;

		TArray<FPlane> SectionCuts;
	};
}
