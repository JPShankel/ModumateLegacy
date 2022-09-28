// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include <functional>
#include "CoreMinimal.h"
#include "ModumateDimensions.h"
#include "ModumateCore/ModumateUnits.h"
#include "Drafting/ModumateDraftingPage.h"
#include "Drafting/ModumateDraftingTags.h"
#include "Drafting/DraftingManager.h"
#include "Runtime/Engine/Classes/Debug/ReporterGraph.h"
#include "DrawingDesigner/DrawingDesignerRequests.h"

class UModumateDocument;
class AModumateObjectInstance;

class FDraftingSchedule;

class IModumateDraftingDraw;

class MODUMATE_API FModumateDraftingView
{
public:
	FModumateDraftingView(UWorld *world, UModumateDocument *doc, UDraftingManager::EDraftType draftType);
	virtual ~FModumateDraftingView();

public:
		FString CurrentFilePath;
private:
	bool ExportDraft(const TCHAR *filepath);

public:
	TArray<TSharedPtr<FDraftingPage>> DraftingPages;

	// List of all schedules; arranged on pages in PaginateScheduleViews
	TArray<TSharedPtr<FDraftingSchedule>> Schedules;

	TSharedPtr<IModumateDraftingDraw> DrawingInterface;

	void PaginateScheduleViews(IModumateDraftingDraw *drawingInterface);

public:
	void GeneratePagesFromCutPlanes(TArray<int32> InCutPlaneIDs);
	void GeneratePageForDD(int32 CutPlaneID, const FDrawingDesignerGenericRequest& Request);

// Generate schedules
public:
	void GenerateScheduleViews();

	// name and number are defaulted - often name and number are fields that are dependent on pageNumber
	// the fields are public in FDraftingPage so they can be set after this function with the desired format
	TSharedPtr<FDraftingPage> CreateAndAddPage(FString name = FString(), FString number = FString());

// Receive pages
public:
	void FinishDraft();

private:
	TWeakObjectPtr<UWorld> World;
	UModumateDocument * Document;
	const UDraftingManager::EDraftType ExportType;

public:
	// TODO: this stuff should probably be a setting, as opposed to hard-coded here
	// this can be removed once the remaining schedule code is moved somewhere else or deleted
	FModumateUnitCoord2D PageSize = FModumateUnitCoord2D(ModumateUnitParams::FXCoord::FloorplanInches(17.0f), ModumateUnitParams::FYCoord::FloorplanInches(11.0f));
	FModumateUnitCoord2D PageMargin = FModumateUnitCoord2D(ModumateUnitParams::FXCoord::FloorplanInches(0.5f), ModumateUnitParams::FYCoord::FloorplanInches(0.5f));

	const float TitleBarWidth = 3.0f;

	TArray<FPlane> SectionCuts;
};