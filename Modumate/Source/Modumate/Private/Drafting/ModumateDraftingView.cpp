// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "Drafting/ModumateDraftingView.h"
#include <functional>
#include "ModumateCore/PlatformFunctions.h"
#include "UnrealClasses/Modumate.h"
#include "DocumentManagement/ModumateDocument.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "Database/ModumateObjectDatabase.h"
#include "Objects/CutPlane.h"
#include "Objects/ModumateRoomStatics.h"
#include "DocumentManagement/ModumateSceneCaptureObjectInterface.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "Drafting/ModumateDDDraw.h"
#include "Drafting/ModumateLineCorral.h"
#include "Drafting/ModumateDraftingDraw.h"
#include "UnrealClasses/ThumbnailCacheManager.h"
#include "ModumateCore/ModumateDimensionStatics.h"

// schedules
#include "Drafting/Schedules/DoorSummarySchedule.h"
#include "Drafting/Schedules/FFESchedule.h"
#include "Drafting/Schedules/DoorSchedule.h"
#include "Drafting/Schedules/WallSummarySchedule.h"
#include "Drafting/Schedules/WallDetailsSchedule.h"
#include "Drafting/Schedules/ScheduleGrid.h"
#include "Drafting/Schedules/IconElement.h"
#include "Drafting/Schedules/RoomLegend.h"

// drawings
#include "Drafting/Drawings/DrawingArea.h"
#include "Drafting/Drawings/PresentationPlan.h"
#include "Drafting/Drawings/Floorplan.h"

#define LOCTEXT_NAMESPACE "ModumateDraftingView"


FMColor FMColor::Black = FMColor(0, 0, 0);
FMColor FMColor::White = FMColor(1, 1, 1);
FMColor FMColor::Gray32 = FMColor(32.0f / 255.0f, 32.0f / 255.0f, 32.0f / 255.0f);
FMColor FMColor::Gray64 = FMColor(64.0f / 255.0f, 64.0f / 255.0f, 64.0f / 255.0f);
FMColor FMColor::Gray96 = FMColor(96.0f / 255.0f, 96.0f / 255.0f, 96.0f / 255.0f);
FMColor FMColor::Gray128 = FMColor(128.0f / 255.0f, 128.0f / 255.0f, 128.0f / 255.0f);
FMColor FMColor::Gray144 = FMColor(144.0f / 255.0f, 144.0f / 255.0f, 144.0f / 255.0f);
FMColor FMColor::Gray160 = FMColor(160.0f / 255.0f, 160.0f / 255.0f, 160.0f / 255.0f);
FMColor FMColor::Gray208 = FMColor(208.0f / 255.0f, 208.0f / 255.0f, 208.0f / 255.0f);

FModumateDraftingView::~FModumateDraftingView()
{
	DraftingPages.Empty();
}


FModumateDraftingView::FModumateDraftingView(UWorld *world, UModumateDocument *doc, UDraftingManager::EDraftType draftType) :
	World(world),
	Document(doc),
	ExportType(draftType)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDraftingView::ModumateDraftingView"));
}

bool FModumateDraftingView::ExportDraft(const TCHAR *filepath)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDraftingView::ExportPDF"));

	UE_LOG(LogAutoDrafting, Display, TEXT("STARTED AUTODRAFTING"));
	UE_LOG(LogAutoDrafting, Display, TEXT("Filename: %s"), filepath);

	for (auto &page : DraftingPages)
	{

		if (!DrawingInterface->StartPage(page->PageNumber,
			page->Dimensions.X.AsFloorplanInches(), page->Dimensions.Y.AsFloorplanInches(), page->Name) )
		{
			return false;
		}
		page->Draw(DrawingInterface.Get());
	}

	bool result = DrawingInterface->SaveDocument(filepath);
	DrawingInterface.Reset();
	return result;
}

TSharedPtr<FDraftingPage> FModumateDraftingView::CreateAndAddPage(FString name, FString number)
{
	TSharedPtr<FDraftingPage> newPage = MakeShareable(new FDraftingPage());

#if 0 //TODO: get from project metadata when implemented
	newPage->StampPath = Document->LeadArchitect.LogoPath;
#endif
	newPage->PageNumber = DraftingPages.Num() > 0 ? DraftingPages.Last()->PageNumber + 1 : 1;
	newPage->Date = FText::AsDate(FDateTime::Now().GetDate(), EDateTimeStyle::Long);

	// name and number are fields defined by the type of page displayed
	// often these are set after this function because they are dependent on PageNumber
	newPage->Name = name;
	newPage->Number = number;

	newPage->Dimensions = PageSize;

	DraftingPages.Add(newPage);

	return newPage;
}

void FModumateDraftingView::GenerateScheduleViews()
{
	// TODO: preferably, unify how all schedules are made and use a factory pattern powered by user settings

	// Door Schedule
	auto doors = Document->GetObjectsOfType(EObjectType::OTDoor);
	if (doors.Num() > 0)
	{
		TSharedPtr<FDoorSummarySchedule> doorSummarySchedule = MakeShareable(new FDoorSummarySchedule(Document, DrawingInterface.Get()));
		Schedules.Add(doorSummarySchedule);

		TSharedPtr<FDoorSchedule> doorSchedule = MakeShareable(new FDoorSchedule(Document));
		Schedules.Add(doorSchedule);
	}

	// Wall Schedules
	TArray<FBIMAssemblySpec> assemblies;

	Document->GetPresetCollection().GetProjectAssembliesForObjectType(EObjectType::OTWallSegment, assemblies);

	if (assemblies.Num() > 0)
	{
		// Wall Assembly Summaries Schedule
		TSharedPtr<FWallSummarySchedule> summarySchedule = MakeShareable(new FWallSummarySchedule(Document, World.Get(), DrawingInterface.Get()));
		Schedules.Add(summarySchedule);

		// Wall Assembly Details Schedule
		TSharedPtr<FWallDetailsSchedule> detailsSchedule = MakeShareable(new FWallDetailsSchedule(Document, World.Get(), DrawingInterface.Get()));
		Schedules.Add(detailsSchedule);
	}

	// FFE Schedule
	auto ffes = Document->GetObjectsOfType(EObjectType::OTFurniture);
	if (ffes.Num() > 0)
	{
		TSharedPtr<FFFESchedule> ffeschedule = MakeShareable(new FFFESchedule(Document, DrawingInterface.Get()));
		Schedules.Add(ffeschedule);
	}

	PaginateScheduleViews(DrawingInterface.Get());
}

void FModumateDraftingView::PaginateScheduleViews(IModumateDraftingDraw *drawingInterface)
{
	if (Schedules.Num() == 0)
	{
		return;
	}

	int32 scheduleIndex = 1;
	FText schedulePageName = LOCTEXT("schedulepage_name", "SCHEDULES");
	FText sheetNumberFormat = LOCTEXT("schedulepage_number", "A7.{0}");
	TSharedPtr<FDraftingPage> currentPage = CreateAndAddPage(schedulePageName.ToString(), FText::Format(sheetNumberFormat, FText::AsNumber(scheduleIndex)).ToString());
	scheduleIndex++;

	// fill column with schedules until the available area is consumed
	TSharedPtr<FDraftingComposite> currentColumn = MakeShareable(new FDraftingComposite());

	// TODO: there is some duplication associated with the population of this value
	TSharedPtr<FDraftingComposite> currentScheduleArea = MakeShareable(new FDraftingComposite());

	FModumateUnitCoord2D drawableDimensions = PageSize - PageMargin * 2.0f;
	drawableDimensions.X -= ModumateUnitParams::FXCoord::FloorplanInches(TitleBarWidth);

	// TODO: value is the same as FDraftingSchedule::ScheduleMargin
	FModumateUnitCoord2D scheduleMargin = FModumateUnitCoord2D(ModumateUnitParams::FXCoord::FloorplanInches(16.0f / 64.0f), ModumateUnitParams::FYCoord::FloorplanInches(16.0f / 64.0f));

	// TODO: margin won't be applied on the left with this implementation - could create another child view that holds the schedules
	// without a margin and have the margin applied at a higher level
	float maxAvailableX = (drawableDimensions.X - scheduleMargin.X).AsFloorplanInches();
	float maxAvailableY = (drawableDimensions.Y - scheduleMargin.Y).AsFloorplanInches();

	float availableX = maxAvailableX;
	float availableY = maxAvailableY;

	// create a column aligned to the widest schedule, with all schedules in the column left-aligned
	ModumateUnitParams::FXCoord maxScheduleX = ModumateUnitParams::FXCoord::FloorplanInches(0.0f);

	for (auto& schedule : Schedules)
	{
		schedule->maxDimensionX = drawableDimensions.X / 2.0f;
		schedule->InitializeBounds(drawingInterface);
		float height = schedule->Dimensions.Y.AsFloorplanInches();

		// TODO: schedules that are taller than the height of a schedule page need to be able to split themselves
		// and have new title (wall schedule, cont., for example)

		if(height > availableY) // page height is exceeded, create a new column or create a new page
		{
			if (maxScheduleX.AsFloorplanInches() <= availableX)
			{
				availableX -= maxScheduleX.AsFloorplanInches();

				currentColumn->MoveXTo(ModumateUnitParams::FXCoord::FloorplanInches(availableX));

				availableX -= schedule->ScheduleMargin;
			}
			else
			{
				currentScheduleArea->SetLocalPosition(PageMargin);
				currentScheduleArea->Children.Add(MakeShareable(new FDraftingRectangle(drawableDimensions)));
				currentPage->Children.Add(currentScheduleArea);
				currentPage = CreateAndAddPage(schedulePageName.ToString(), FText::Format(sheetNumberFormat, FText::AsNumber(scheduleIndex)).ToString());
				scheduleIndex++;
				availableX = maxAvailableX;
			}

			currentScheduleArea->Children.Add(currentColumn);
			currentColumn = MakeShareable(new FDraftingComposite());

			maxScheduleX = ModumateUnitParams::FXCoord::FloorplanInches(0.0f);
			availableY = maxAvailableY;
		}

		if (schedule->Dimensions.X > maxScheduleX)
		{
			maxScheduleX = schedule->Dimensions.X;
		}

		schedule->HorizontalAlignment = DraftingAlignment::Left;

		// schedules are drawn with the top-left corner as the origin so the height of the schedule is
		// added to the position to match
		schedule->MoveYTo(drawableDimensions.Y + ModumateUnitParams::FYCoord::FloorplanInches(-availableY + height));

		currentColumn->Children.Add(schedule);

		availableY -= height;
		availableY -= schedule->ScheduleMargin;
	}

	if (maxScheduleX.AsFloorplanInches() > availableX)
	{
		currentScheduleArea->SetLocalPosition(PageMargin);
		currentScheduleArea->Children.Add(MakeShareable(new FDraftingRectangle(drawableDimensions)));
		currentPage->Children.Add(currentScheduleArea);
		currentPage = CreateAndAddPage(schedulePageName.ToString(), FText::Format(sheetNumberFormat, FText::AsNumber(scheduleIndex)).ToString());
		scheduleIndex++;
		availableX = maxAvailableX;
	}

	// TODO: the value subtracted here is shared with FDraftingSchedule::ScheduleMargin.  Currently, it could be a constant, but it should probably
	// be a part of a future class that helps group details schedules with summary schedules
	currentColumn->MoveXTo(ModumateUnitParams::FXCoord::FloorplanInches(availableX) - maxScheduleX);
	currentScheduleArea->Children.Add(currentColumn);
	currentScheduleArea->SetLocalPosition(PageMargin);
	currentScheduleArea->Children.Add(MakeShareable(new FDraftingRectangle(drawableDimensions)));
	currentPage->Children.Add(currentScheduleArea);
}

void FModumateDraftingView::GeneratePagesFromCutPlanes(TArray<int32> InCutPlaneIDs)
{
	UModumateGameInstance *modGameInst = World.IsValid() ? World->GetGameInstance<UModumateGameInstance>() : nullptr;
	UDraftingManager *draftMan = modGameInst ? modGameInst->DraftingManager : nullptr;

	// TODO: most likely this information should be controlled by UI and requested here
	FModumateUnitCoord2D presentationSeriesSize = FModumateUnitCoord2D(ModumateUnitParams::FXCoord::FloorplanInches(36.0f), ModumateUnitParams::FYCoord::FloorplanInches(24.0f));
	FModumateUnitCoord2D floorplanSeriesSize = FModumateUnitCoord2D(ModumateUnitParams::FXCoord::FloorplanInches(36.0f), ModumateUnitParams::FYCoord::FloorplanInches(24.0f));
	FModumateUnitCoord2D titleBlockSize = FModumateUnitCoord2D(ModumateUnitParams::FXCoord::FloorplanInches(3.0f), ModumateUnitParams::FYCoord::FloorplanInches(24.0f));
	FModumateUnitCoord2D drawingMargin = FModumateUnitCoord2D(ModumateUnitParams::FXCoord::FloorplanInches(0.5f), ModumateUnitParams::FYCoord::FloorplanInches(0.5f));
	FModumateUnitCoord2D pageMargin = FModumateUnitCoord2D(ModumateUnitParams::FXCoord::FloorplanInches(0.5f), ModumateUnitParams::FYCoord::FloorplanInches(0.5f));

	TArray<AMOICutPlane*> cutPlanes;
	Document->GetObjectsOfTypeCasted(EObjectType::OTCutPlane, cutPlanes);
	TArray<AMOICutPlane*> exportableCutPlanes;

	for (const auto curCutPlane : cutPlanes)
	{
		if (InCutPlaneIDs.Contains(curCutPlane->ID))
		{
			exportableCutPlanes.Add(curCutPlane);
		}
	}
	TArray<AModumateObjectInstance*> scopeBoxes = Document->GetObjectsOfType(EObjectType::OTScopeBox);

	if (ExportType == UDraftingManager::kDWG)
	{
		DrawingInterface = MakeShared<FModumateLineCorral>(new FModumateDwgDraw(World.Get()) );
	}
	else
	{
		//TODO: error code
		return;
	}

	draftMan->CurrentDraftingView = this;
	draftMan->CurrentDrawingInterface = DrawingInterface.Get();

	for (AModumateObjectInstance* cutPlane : exportableCutPlanes)
	{
		draftMan->RequestRender(TPair<int32, int32>(cutPlane->ID, MOD_ID_NONE));
	}

	for (AModumateObjectInstance* cutPlane : exportableCutPlanes)
	{
		ISceneCaptureObject* sceneCaptureInterface = cutPlane->GetSceneCaptureInterface();
		if (sceneCaptureInterface == nullptr)
		{
			continue;
		}

		sceneCaptureInterface->SetupPendingRenders();

		FMOICutPlaneData cutPlaneData;
		cutPlane->GetStateData().CustomData.LoadStructData(cutPlaneData);
		FString cutPlaneName = cutPlaneData.Name;
		cutPlaneName.ReplaceInline(TEXT("/"), TEXT("_"));
		auto page = CreateAndAddPage(cutPlaneName);
		TSharedPtr<FFloorplan> floorplan = MakeShareable(new FFloorplan(Document, World.Get(), TPair<int32, int32>(cutPlane->ID, MOD_ID_NONE)));
		floorplan->InitializeDimensions(presentationSeriesSize - (drawingMargin*2.0f), drawingMargin);
		floorplan->SetLocalPosition(pageMargin);
		floorplan->SetDraftingType(ExportType);
		if (ExportType == UDraftingManager::kDWG)
		{   // For DWGs don't clip to the paper dimensions MOD-779.
			auto viewArea = floorplan->DrawingContent.Pin();
			if (viewArea)
			{
				viewArea->bClipped = false;
			}
		}

		page->Children.Add(floorplan);
		page->Dimensions = presentationSeriesSize;
		sceneCaptureInterface->CaptureDelegate.AddSP(floorplan.Get(), &FFloorplan::OnPageCompleted);
		if (!sceneCaptureInterface->StartRender(Document))
		{
			sceneCaptureInterface->CaptureDelegate.Broadcast();
		}
	}

	// TODO: potentially, FDraftingView doesn't exist at all anymore, if
	// the types of drawings that appear is entirely managed by the app
	// in the short term, the implementations of each drawing type should gradually move code out of here
	if (UDraftingManager::IsRenderPending(World.Get()))
	{
		draftMan->CurrentDraftingView = this;
		draftMan->CurrentDrawingInterface = DrawingInterface.Get();
	}
}

void FModumateDraftingView::GeneratePageForDD(int32 CutPlaneID, const FDrawingDesignerGenericRequest& Request)
{
	UModumateGameInstance* gameInstance = World.IsValid() ? World->GetGameInstance<UModumateGameInstance>() : nullptr;
	UDraftingManager* draftMan = gameInstance ? gameInstance->DraftingManager : nullptr;

	AModumateObjectInstance* cutPlane = Document->GetObjectById(CutPlaneID);
	if (!ensure(cutPlane))
	{
		return;
	}

	DrawingInterface = MakeShared<FModumateDDDraw>(Document, World.Get(), Request);

	draftMan->CurrentDraftingView = this;
	draftMan->CurrentDrawingInterface = DrawingInterface.Get();

	draftMan->RequestRender(TPair<int32, int32>(CutPlaneID, MOD_ID_NONE));

	ISceneCaptureObject* sceneCaptureInterface = cutPlane->GetSceneCaptureInterface();
	sceneCaptureInterface->SetupPendingRenders();

	if (sceneCaptureInterface)
	{
		auto page = CreateAndAddPage();
		TSharedPtr<FFloorplan> floorplan = MakeShareable(new FFloorplan(Document, World.Get(), TPair<int32, int32>(CutPlaneID, MOD_ID_NONE)));
		floorplan->SetDraftingType(ExportType);

		auto viewArea = floorplan->DrawingContent.Pin();
		if (viewArea)
		{
			viewArea->bClipped = false;
		}

		page->Children.Add(floorplan);

		sceneCaptureInterface->CaptureDelegate.AddSP(floorplan.Get(), &FFloorplan::OnPageCompleted);
		if (!sceneCaptureInterface->StartRender(Document))
		{
			sceneCaptureInterface->CaptureDelegate.Broadcast();
		}
	}

}

void FModumateDraftingView::FinishDraft()
{
	// TODO: presentation plans do not have schedules or title blocks right now, 
	// re-enable these conditionally based on what the drawing type
//	GenerateScheduleViews();

	ExportDraft(*CurrentFilePath);
}

#undef LOCTEXT_NAMESPACE
