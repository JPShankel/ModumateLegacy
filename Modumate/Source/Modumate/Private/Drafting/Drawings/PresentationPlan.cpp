#include "Drafting/Drawings/PresentationPlan.h"
#include "DocumentManagement/ModumateDocument.h"
#include "Drafting/ModumateDraftingTags.h"
#include "Objects/ModumateObjectInstance.h"
#include "Objects/CutPlane.h"
#include "Graph/Graph2D.h"
#include "Drafting/Drawings/DrawingArea.h"
#include "Drafting/DraftingManager.h"
#include "Drafting/Schedules/ScheduleGrid.h"
#include "Drafting/Schedules/RoomLegend.h"
#include "ModumateCore/ModumateDimensionStatics.h"


FPresentationPlan::FPresentationPlan(const UModumateDocument *doc, UWorld *world, SceneCaptureID captureObjID)
	: FDraftingDrawing(doc,world,captureObjID)
{
	bScaleToFit = true;
}

bool FPresentationPlan::MakeScaleTag(IModumateDraftingDraw *drawingInterface)
{
	drawingInterface->DrawingScale = DrawingScale;

	// TODO: unclear how the scale tag is supposed to relate to each drawing exactly (and what drawing types would want this
	// it is here right now because of how presentation plans are scale-to-fit

	// lengths are the sizes of rectangle to be put in the scale tag for measurements
	// text is placed above the rectangles representing the length traveled so far (0, 1, 2, 4, 8, 16)
	TArray<ModumateUnitParams::FLength> lengths = {
		ModumateUnitParams::FLength::FloorplanInches(1 * UModumateDimensionStatics::InchesPerFoot),
		ModumateUnitParams::FLength::FloorplanInches(1 * UModumateDimensionStatics::InchesPerFoot),
		ModumateUnitParams::FLength::FloorplanInches(2 * UModumateDimensionStatics::InchesPerFoot),
		ModumateUnitParams::FLength::FloorplanInches(4 * UModumateDimensionStatics::InchesPerFoot),
		ModumateUnitParams::FLength::FloorplanInches(8 * UModumateDimensionStatics::InchesPerFoot)
	};

	TSharedPtr<FScaleTag> scaleTag = MakeShareable(new FScaleTag(lengths));
	scaleTag->InitializeBounds(drawingInterface);
	scaleTag->SetLocalPosition(FModumateUnitCoord2D::FloorplanInches(FVector2D(ForceInitToZero)));

	// move to bottom right corner of the page
	scaleTag->TranslateBy(Dimensions.X); 
	scaleTag->TranslateBy(scaleTag->Dimensions.X * -1.0f);

	// offset from the bottom right corner
	scaleTag->TranslateBy(ModumateUnitParams::FXCoord::FloorplanInches(-16.0f / 64.0f));
	scaleTag->TranslateBy(ModumateUnitParams::FYCoord::FloorplanInches(2.0f / 64.0f));

	scaleTag->SetLayerTypeRecursive(FModumateLayerType::kDefault);
	Children.Add(scaleTag);

	return true;
}

void FPresentationPlan::OnPageCompleted()
{
	MakeWorldObjects();

	TSharedPtr<FDraftingComposite> worldObjects = WorldObjects.Pin();
	ensureAlways(worldObjects.IsValid());

	auto drawingBoundingBox = worldObjects->BoundingBox;

	auto viewArea = DrawingContent.Pin();
	ensureAlways(viewArea.IsValid());

	TSharedPtr<FDraftingComposite> roomAnnotations = MakeShareable(new FDraftingComposite());
	bool bValidRooms = FindRooms(roomAnnotations);
	worldObjects->Children.Add(roomAnnotations);

	TSharedPtr<FRoomLegend> roomLegend = RoomLegend.Pin();
	ensureAlways(roomLegend.IsValid());

	FModumateUnitCoord2D roomLegendDimensions = bValidRooms ? roomLegend->Dimensions : FModumateUnitCoord2D::FloorplanInches(FVector2D::ZeroVector);

	FModumateUnitCoord2D roomLegendArea;
	roomLegendArea.X = roomLegendDimensions.X < viewArea->Dimensions.X ? roomLegendDimensions.X : (viewArea->Dimensions.X - ModumateUnitParams::FXCoord::FloorplanInches(1.0f));
	roomLegendArea.Y = roomLegendDimensions.Y < viewArea->Dimensions.Y ? roomLegendDimensions.Y : (viewArea->Dimensions.Y - ModumateUnitParams::FYCoord::FloorplanInches(1.0f));

	float scaleToLegendX = (drawingBoundingBox.GetSize().X / UModumateDimensionStatics::InchesToCentimeters) / (viewArea->Dimensions.X - roomLegendArea.X).AsFloorplanInches();
	float scaleToLegendY = (drawingBoundingBox.GetSize().Y / UModumateDimensionStatics::InchesToCentimeters) / (viewArea->Dimensions.Y - roomLegendArea.Y).AsFloorplanInches();

	float scaleToFit = 1.0f;


	if (bValidRooms)
	{
		// given that there is a room legend, scale to fit on the axis shrunk by the legend or the axis without the legend
		float scaleToViewX = (drawingBoundingBox.GetSize().X / UModumateDimensionStatics::InchesToCentimeters) / (viewArea->Dimensions.X).AsFloorplanInches();
		float scaleToViewY = (drawingBoundingBox.GetSize().Y / UModumateDimensionStatics::InchesToCentimeters) / (viewArea->Dimensions.Y).AsFloorplanInches();
		if (scaleToLegendX < scaleToLegendY)
		{
			scaleToFit = scaleToLegendX < scaleToViewY ? scaleToViewY : scaleToLegendX;
			roomLegend->SetLocalPosition(roomLegend->Dimensions.X * -1.0f, roomLegend->Dimensions.Y);
		}
		else
		{
			scaleToFit = scaleToLegendY < scaleToViewX ? scaleToViewX : scaleToLegendY;
			roomLegend->SetLocalPosition(ModumateUnitParams::FXCoord::FloorplanInches(0.0f), viewArea->Dimensions.Y);
		}
	}
	else
	{
		if (scaleToLegendX < scaleToLegendY)
		{
			scaleToFit = scaleToLegendY;
		}
		else
		{
			scaleToFit = scaleToLegendX;
		}
	}
	DrawingScale = scaleToFit;
			
	viewArea->Dimensions = FModumateUnitCoord2D::WorldCentimeters(viewArea->Dimensions.AsWorldCentimeters(DrawingScale));
	worldObjects->TranslateBy(viewArea->Dimensions.X);

	viewArea->Children.Add(worldObjects);
	auto viewSize = drawingBoundingBox.GetSize();
	ModumateUnitParams::FXCoord viewWidth = ModumateUnitParams::FXCoord::WorldCentimeters(viewSize.X);
	ModumateUnitParams::FYCoord viewHeight = ModumateUnitParams::FYCoord::WorldCentimeters(viewSize.Y);
	worldObjects->Dimensions = FModumateUnitCoord2D(viewWidth, viewHeight);
	worldObjects->TranslateBy(worldObjects->Dimensions.X * -1.0f);

	UModumateGameInstance *modGameInst = World ? World->GetGameInstance<UModumateGameInstance>() : nullptr;
	UDraftingManager *draftMan = modGameInst ? modGameInst->DraftingManager : nullptr;
	auto drawingInterface = draftMan->CurrentDrawingInterface;
	MakeScaleTag(drawingInterface);

	UDraftingManager::OnPageCompleted(CaptureObjID, World);
}

bool FPresentationPlan::FindRooms(TSharedPtr<FDraftingComposite> Parent)
{
#if 0
	UModumateGameInstance *modGameInst = World ? World->GetGameInstance<UModumateGameInstance>() : nullptr;
	UDraftingManager *draftMan = modGameInst ? modGameInst->DraftingManager : nullptr;
	auto drawingInterface = draftMan->CurrentDrawingInterface;

	TMap<int32, FVector2D> roomsAndLocations;
	GetVisibleRoomsAndLocations(roomsAndLocations);

	TArray<FRoomConfigurationBlueprint> roomConfigs;
	for (auto& kvp : roomsAndLocations)
	{
		auto roomObj = Doc->GetObjectById(kvp.Key);

		// Get room information for legend
		FRoomConfigurationBlueprint roomConfigData;
		if (UModumateRoomStatics::GetRoomConfig(roomObj, roomConfigData))
		{
			TSharedPtr<FFFETag> roomTag = MakeShareable(new FFFETag(roomConfigData.RoomNumber));
			roomTag->SetLocalPosition(FModumateUnitCoord2D::WorldCentimeters(kvp.Value));
			roomTag->InitializeBounds(drawingInterface);
			Parent->Children.Add(roomTag);

			roomConfigs.Add(MoveTemp(roomConfigData));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Could not get room config for room ID: %d!"), roomObj->ID);
			continue;
		}

	}

	roomConfigs.Sort([](const FRoomConfigurationBlueprint &RoomConfigA, const FRoomConfigurationBlueprint &RoomConfigB) {
		return RoomConfigA.RoomNumber.Compare(RoomConfigB.RoomNumber) < 0;
	});

	TSharedPtr<FRoomLegend> roomLegend = MakeShareable(new FRoomLegend());
	roomLegend->ColumnMargin = 8.0f / 64.0f;
	roomLegend->Data->Margin = 3.0f / 64.0f;
	roomLegend->Data->bShowLines = false;

	for (const FRoomConfigurationBlueprint &roomConfigData : roomConfigs)
	{
		TArray<TSharedPtr<FDraftingComposite>> row;

		// Number
		row.Add(roomLegend->MakeDraftingText(FText::FromString(roomConfigData.RoomNumber)));

		// Color
		TSharedPtr<FDraftingComposite> colorAreaCenterSize = MakeShareable(new FDraftingComposite());
		colorAreaCenterSize->Dimensions = FModumateUnitCoord2D::FloorplanInches(FVector2D(4.0f / 64.0f, 4.0f / 64.0f));
		TSharedPtr<FFilledRoundedRectTag> ColorArea = MakeShareable(new FFilledRoundedRectTag(colorAreaCenterSize));
		FLinearColor colorValue = roomConfigData.RoomColor.ReinterpretAsLinear();
		ColorArea->FillColor = FMColor(colorValue.R, colorValue.G, colorValue.B);
		ColorArea->CornerRadius = ModumateUnitParams::FRadius::FloorplanInches(1.0f / 64.0f);
		ColorArea->OuterMargins = FModumateUnitCoord2D(ModumateUnitParams::FXCoord(ColorArea->CornerRadius), ModumateUnitParams::FYCoord(ColorArea->CornerRadius));
		ColorArea->InitializeBounds(drawingInterface);
		row.Add(ColorArea);

		// Name
		row.Add(MakeShareable(new FDraftingText(
			roomConfigData.DisplayName,
			roomLegend->DefaultFontSize,
			FMColor::Black,
			FontType::Bold)));

		// Area
		row.Add(MakeShareable(new FDraftingText(
			FText::AsNumber(roomConfigData.Area),
			roomLegend->DefaultFontSize,
			FMColor::Gray64,
			FontType::Standard)));

		roomLegend->Data->MakeRow(row);
	}

	roomLegend->InitializeBounds(drawingInterface);
	Parent->Children.Add(roomLegend);

	RoomLegend = TWeakPtr<FRoomLegend>(roomLegend);

	return roomConfigs.Num() > 0;
#endif
	return true;
}
