// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "Drafting/ModumateDraftingView.h"
#include <functional>
#include "Drafting/DraftingManager.h"
#include "UnrealClasses/EditModelGameMode_CPP.h"
#include "ModumateCore/PlatformFunctions.h"
#include "UnrealClasses/Modumate.h"
#include "DocumentManagement/ModumateDocument.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "UnrealClasses/ModumateGameInstance.h"
#include "Database/ModumateObjectDatabase.h"
#include "DocumentManagement/ModumateObjectInstance.h"
#include "ModumateCore/ModumateRoomStatics.h"
#include "DocumentManagement/ModumateSceneCaptureObjectInterface.h"
#include "ModumateCore/ModumateFunctionLibrary.h"
#include "Drafting/ModumateDwgDraw.h"
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

using namespace Modumate::PDF;
using namespace Modumate::Units;
using namespace Modumate;


namespace Modumate { 


FModumateDraftingView::~FModumateDraftingView()
{
	DraftingPages.Empty();
}


FModumateDraftingView::FModumateDraftingView(UWorld *world, Modumate::FModumateDocument *doc, DraftType draftType) :
	World(world),
	Document(doc),
	ExportType(draftType)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDraftingView::ModumateDraftingView"));

	// TODO: this should be temporary - potentially the cut planes will have some data that populates the
	// type of drawing they represent (floorplane, elevation, section), the name of the drawing,
	// and some desired pagination information
	GeneratePagesFromCutPlanes(world);
}

bool FModumateDraftingView::ExportDraft(UWorld *world,const TCHAR *filepath)
{
	UE_LOG(LogCallTrace, Display, TEXT("ModumateDraftingView::ExportPDF"));

	UE_LOG(LogAutoDrafting, Display, TEXT("STARTED AUTODRAFTING"));
	UE_LOG(LogAutoDrafting, Display, TEXT("Filename: %s"), filepath);

	for (auto &page : DraftingPages)
	{

		if (!DrawingInterface->StartPage(page->PageNumber,
			page->Dimensions.X.AsFloorplanInches(), page->Dimensions.Y.AsFloorplanInches()) )
		{
			return false;
		}
		page->Draw(DrawingInterface.Get());
	}

	bool result = DrawingInterface->SaveDocument(filepath);
	DrawingInterface.Reset();
	return result;
}

TSharedPtr<FDraftingPage> FModumateDraftingView::CreateAndAddPage(FText name, FText number)
{
	TSharedPtr<FDraftingPage> newPage = MakeShareable(new FDraftingPage());

	newPage->StampPath = Document->LeadArchitect.LogoPath;
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
	// TODO: should use GetPresentAssembliesForToolMode
	TArray<FModumateObjectAssembly> assemblies = Document->GetAssembliesForToolMode_DEPRECATED(World.Get(), EToolMode::VE_WALL);

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
	TSharedPtr<FDraftingPage> currentPage = CreateAndAddPage(schedulePageName,FText::Format(sheetNumberFormat, FText::AsNumber(scheduleIndex)));
	scheduleIndex++;

	// fill column with schedules until the available area is consumed
	TSharedPtr<FDraftingComposite> currentColumn = MakeShareable(new FDraftingComposite());

	// TODO: there is some duplication associated with the population of this value
	TSharedPtr<FDraftingComposite> currentScheduleArea = MakeShareable(new FDraftingComposite());

	FCoordinates2D drawableDimensions = PageSize - PageMargin * 2.0f;
	drawableDimensions.X -= FXCoord::FloorplanInches(TitleBarWidth);

	// TODO: value is the same as FDraftingSchedule::ScheduleMargin
	FCoordinates2D scheduleMargin = FCoordinates2D(FXCoord::FloorplanInches(16.0f / 64.0f), FYCoord::FloorplanInches(16.0f / 64.0f));

	// TODO: margin won't be applied on the left with this implementation - could create another child view that holds the schedules
	// without a margin and have the margin applied at a higher level
	float maxAvailableX = (drawableDimensions.X - scheduleMargin.X).AsFloorplanInches();
	float maxAvailableY = (drawableDimensions.Y - scheduleMargin.Y).AsFloorplanInches();

	float availableX = maxAvailableX;
	float availableY = maxAvailableY;

	// create a column aligned to the widest schedule, with all schedules in the column left-aligned
	FXCoord maxScheduleX = FXCoord::FloorplanInches(0.0f);

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

				currentColumn->MoveXTo(FXCoord::FloorplanInches(availableX));

				availableX -= schedule->ScheduleMargin;
			}
			else
			{
				currentScheduleArea->SetLocalPosition(PageMargin);
				currentScheduleArea->Children.Add(MakeShareable(new FDraftingRectangle(drawableDimensions)));
				currentPage->Children.Add(currentScheduleArea);
				currentPage = CreateAndAddPage(schedulePageName,FText::Format(sheetNumberFormat, FText::AsNumber(scheduleIndex)));
				scheduleIndex++;
				availableX = maxAvailableX;
			}

			currentScheduleArea->Children.Add(currentColumn);
			currentColumn = MakeShareable(new FDraftingComposite());

			maxScheduleX = FXCoord::FloorplanInches(0.0f);
			availableY = maxAvailableY;
		}

		if (schedule->Dimensions.X > maxScheduleX)
		{
			maxScheduleX = schedule->Dimensions.X;
		}

		schedule->HorizontalAlignment = DraftingAlignment::Left;

		// schedules are drawn with the top-left corner as the origin so the height of the schedule is
		// added to the position to match
		schedule->MoveYTo(drawableDimensions.Y + FYCoord::FloorplanInches(-availableY + height));

		currentColumn->Children.Add(schedule);

		availableY -= height;
		availableY -= schedule->ScheduleMargin;
	}

	if (maxScheduleX.AsFloorplanInches() > availableX)
	{
		currentScheduleArea->SetLocalPosition(PageMargin);
		currentScheduleArea->Children.Add(MakeShareable(new FDraftingRectangle(drawableDimensions)));
		currentPage->Children.Add(currentScheduleArea);
		currentPage = CreateAndAddPage(schedulePageName,FText::Format(sheetNumberFormat, FText::AsNumber(scheduleIndex)));
		scheduleIndex++;
		availableX = maxAvailableX;
	}

	// TODO: the value subtracted here is shared with FDraftingSchedule::ScheduleMargin.  Currently, it could be a constant, but it should probably
	// be a part of a future class that helps group details schedules with summary schedules
	currentColumn->MoveXTo(FXCoord::FloorplanInches(availableX) - maxScheduleX);
	currentScheduleArea->Children.Add(currentColumn);
	currentScheduleArea->SetLocalPosition(PageMargin);
	currentScheduleArea->Children.Add(MakeShareable(new FDraftingRectangle(drawableDimensions)));
	currentPage->Children.Add(currentScheduleArea);
}

void FModumateDraftingView::GeneratePagesFromCutPlanes(UWorld *world)
{
	UModumateGameInstance *modGameInst = world ? world->GetGameInstance<UModumateGameInstance>() : nullptr;
	UDraftingManager *draftMan = modGameInst ? modGameInst->DraftingManager : nullptr;

	// TODO: most likely this information should be controlled by UI and requested here
	FCoordinates2D presentationSeriesSize = FCoordinates2D(FXCoord::FloorplanInches(36.0f), FYCoord::FloorplanInches(24.0f));
	FCoordinates2D floorplanSeriesSize = FCoordinates2D(FXCoord::FloorplanInches(36.0f), FYCoord::FloorplanInches(24.0f));
	FCoordinates2D titleBlockSize = FCoordinates2D(FXCoord::FloorplanInches(3.0f), FYCoord::FloorplanInches(24.0f));
	FCoordinates2D drawingMargin = FCoordinates2D(FXCoord::FloorplanInches(0.5f), FYCoord::FloorplanInches(0.5f));
	FCoordinates2D pageMargin = FCoordinates2D(FXCoord::FloorplanInches(0.5f), FYCoord::FloorplanInches(0.5f));

	TArray<FModumateObjectInstance*> cutPlanes = Document->GetObjectsOfType(EObjectType::OTCutPlane);
	TArray<FModumateObjectInstance*> scopeBoxes = Document->GetObjectsOfType(EObjectType::OTScopeBox);

	if (ExportType == kPDF)
	{
		DrawingInterface = MakeShared<FModumatePDFDraw>();
	}
	else
	{
		DrawingInterface = MakeShared<FModumateDwgDraw>();
	}

	for (FModumateObjectInstance* cutPlane : cutPlanes)
	{
		if (cutPlane == nullptr)
		{
			continue;
		}

		ISceneCaptureObject* sceneCaptureInterface = cutPlane->GetSceneCaptureInterface();
		if (sceneCaptureInterface == nullptr)
		{
			continue;
		}

		FPlane plane = FPlane(cutPlane->GetControlPoint(0), cutPlane->GetNormal());


// disabled implementation of scope boxes determining drawing transforms, extents
#if 0
		for (FModumateObjectInstance* scopeBox : scopeBoxes)
		{
			if (scopeBox == nullptr)
			{
				continue;
			}

			FVector scopeBoxNormal = scopeBox->GetNormal();
			int32 numPoints = scopeBox->GetControlPoints().Num();
			// cut plane and scope box must be facing the same direction to produce a drawing
			if (!FVector::Coincident(scopeBoxNormal, cutPlane->GetNormal()))
			{
				continue;
			}

			// drawings will only be created when all four extruded segments of the scope box 
			// intersect with the cut plane
			bool bValidIntersection = true;

			TArray<FVector> intersection;
			intersection.SetNumZeroed(numPoints);

			for (int32 cornerIdx = 0; cornerIdx < numPoints; cornerIdx++)
			{
				FVector corner = scopeBox->GetCorner(cornerIdx);
				FVector extrudedCorner = corner + (scopeBoxNormal * scopeBox->GetExtents().Y);

				bool bIntersects = FMath::SegmentPlaneIntersection(corner, extrudedCorner, plane, intersection[cornerIdx]);
				bValidIntersection = bValidIntersection && bIntersects;
			}

			if (!bValidIntersection)
			{
				continue;
			}

			FVector origin = intersection[0];
			FVector width = intersection[1] - origin;
			FVector height = intersection[3] - origin;

			sceneCaptureInterface->AddCaptureArea(scopeBox->ID, intersection);

			// TODO: pages need to be associated with drawings, preferably in the app
			// in theory, make all of the drawings that are associated to the cut plane here
			auto page = CreateAndAddPage();
			TSharedPtr<FPresentationPlan> presentationPlan = MakeShareable(new FPresentationPlan(Document, world, TPair<int32, int32>(cutPlane->ID, scopeBox->ID)));
			presentationPlan->InitializeDimensions(presentationSeriesSize - (drawingMargin*2.0f), drawingMargin);
			presentationPlan->SetLocalPosition(pageMargin);

			page->Children.Add(presentationPlan);
			page->Dimensions = presentationSeriesSize;
			draftMan->RequestRender(TPair<int32, int32>(cutPlane->ID, scopeBox->ID));
			sceneCaptureInterface->CaptureDelegate.AddSP(presentationPlan.Get(), &FPresentationPlan::OnPageCompleted);
			

		}
#else
		// TODO: normally would be the intersection between the scope box and this cut plane. 
		// currently the entire cut plane area is used
		sceneCaptureInterface->AddCaptureArea(MOD_ID_NONE, {});

		auto page = CreateAndAddPage();
		TSharedPtr<FFloorplan> floorplan = MakeShareable(new FFloorplan(Document, world, TPair<int32, int32>(cutPlane->ID, MOD_ID_NONE)));
		floorplan->InitializeDimensions(presentationSeriesSize - (drawingMargin*2.0f), drawingMargin);
		floorplan->SetLocalPosition(pageMargin);

		page->Children.Add(floorplan);
		page->Dimensions = presentationSeriesSize;
		draftMan->RequestRender(TPair<int32, int32>(cutPlane->ID, MOD_ID_NONE));
		sceneCaptureInterface->CaptureDelegate.AddSP(floorplan.Get(), &FFloorplan::OnPageCompleted);
#endif
		sceneCaptureInterface->StartRender();
	}

	// TODO: potentially, FDraftingView doesn't exist at all anymore, if
	// the types of drawings that appear is entirely managed by the app
	// in the short term, the implementations of each drawing type should gradually move code out of here
	if (UDraftingManager::IsRenderPending(world))
	{
		draftMan->CurrentDraftingView = this;
		draftMan->CurrentDrawingInterface = DrawingInterface.Get();
	}
}

void FModumateDraftingView::FinishDraft()
{
	// TODO: presentation plans do not have schedules or title blocks right now, 
	// re-enable these conditionally based on what the drawing type
//	GenerateScheduleViews();

	ExportDraft(World.Get(), *CurrentFilePath);
}

}

#undef LOCTEXT_NAMESPACE