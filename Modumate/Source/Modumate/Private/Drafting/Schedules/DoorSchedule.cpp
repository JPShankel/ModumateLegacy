#include "Drafting/Schedules/DoorSchedule.h"

#include "ModumateCore/ModumateDimensionStatics.h"
#include "DocumentManagement/ModumateDocument.h"
#include "Objects/ModumateObjectInstance.h"
#include "Drafting/Schedules/ScheduleGrid.h"


#define LOCTEXT_NAMESPACE "ModumateDoorSchedule"

FDoorSchedule::FDoorSchedule(const UModumateDocument *doc)
{
	Title = MakeShareable(new FDraftingText(
		LOCTEXT("title", "Door Schedule: Instances"),
		ModumateUnitParams::FFontSize::FloorplanInches(TitleHeight),
		DefaultColor,
		FontType::Bold));

	Children.Add(Title);

	ColumnHeaders = {
		LOCTEXT("mark", "Mark"),
		LOCTEXT("otherroom", "(other room)"),
		LOCTEXT("assembly", "Assembly"),
		LOCTEXT("width", "Width"),
		LOCTEXT("height", "Height"),
		LOCTEXT("hardware", "Hardware"),
		LOCTEXT("finishoverrides", "Finish Overrides"),
		LOCTEXT("comments", "Comments")
	};

	FinishNames = {
		FName(TEXT("Interior_Finish")),
		FName(TEXT("Exterior_Finish")),
		FName(TEXT("Glass_Finish")),
		FName(TEXT("Hardware_Finish"))
	};

	Data = MakeShareable(new FScheduleGrid());
	Children.Add(Data);


	auto doors = doc->GetObjectsOfType(EObjectType::OTDoor);
	ensureAlways(doors.Num() > 0);

	Data->InitializeColumns(ColumnHeaders, false);
	TArray<TSharedPtr<FDraftingComposite>> headerRow;

	int32 columnIndex = 0;
	int32 otherRoomIndex = 1;
	for (auto header : ColumnHeaders)
	{
		if (columnIndex != otherRoomIndex)
		{
			headerRow.Add(MakeShareable(new FDraftingText(header, DefaultFontSize, DefaultColor, FontType::Bold, DefaultAlignment)));
		}
		else
		{
			headerRow.Add(MakeShareable(new FDraftingText(header, DefaultFontSize, FMColor::Gray144, FontType::Italic, DefaultAlignment)));
		}
		columnIndex++;
	}
	Data->MakeRow(headerRow);

	for (int32 i = 0; i < doors.Num(); i++)
	{
		TArray<TSharedPtr<FDraftingComposite>> row;
		// Create Row
		auto door = doors[i];

		// TODO: Use new object properties
		/*
		// Mark
		row.Add(doorSchedule->MakeDraftingText(FText::FromString(door->GetMetaValue(AModumateObjectInstance::Param_InstanceMarkRoomID))));

		// Other room - this column has a specific font configuration
		row.Add(new FDraftingText(
			FText::FromString(door->GetMetaValue(AModumateObjectInstance::Param_InstanceMarkOtherRoomID)),
			doorSchedule->DefaultFontSize,
			FMColor::Gray144,
			FontType::Italic));

		// Assembly
		row.Add(doorSchedule->MakeDraftingText(
			FText::FromString(
				door->ObjectAssembly.GetProperty(BIM::Parameters::Code).AsString() +
				door->ObjectAssembly.GetProperty(BIM::Parameters::Tag).AsString())
			)
		);
		*/

		// TODO: use meta plane parent for portal dimensions
		/*
		// Dimensions
		FVector doorDiagonal = door->GetControlPoint(2) - door->GetControlPoint(0);
		FWidth w = FWidth::WorldCentimeters(doorDiagonal.X);
		FHeight h = FHeight::WorldCentimeters(doorDiagonal.Z);

		FText widthText = FText::FromString(UModumateDimensionStatics::DecimalToFractionString(w.AsWorldInches(), true, true));
		FText heightText = FText::FromString(UModumateDimensionStatics::DecimalToFractionString(h.AsWorldInches(), true, true));
		row.Add(MakeDraftingText(widthText));
		row.Add(MakeDraftingText(heightText));
		*/

		// Hardware
		FString partsList;
		bool bAddSeparator = false;
		FString separator = ", ";
		// Create comma-separated list from the list of FPortalParts in the assembly
		// TODO: DDL 2 portal part information (DDL 1 left commented out for reference)
#if 0
		for (auto& partPair : door->GetAssembly().CachedAssembly.PortalParts)
		{
			if (!bAddSeparator)
			{
				bAddSeparator = true;
			}
			else
			{
				partsList += separator;
			}

			partsList += partPair.Value.DisplayName.ToString();
		}
		row.Add(MakeDraftingText(FText::FromString(partsList)));
#endif
		// Finish overrides
		// TODO: this should be replaced with the correct contents of the Finish overrides column
		// and ultimately remove the FinishNames variable from schedules
		/*
		auto doorMaterials = door->ObjectAssembly.PortalConfiguration.MaterialsPerChannel;
		for (FName finish : doorSchedule->FinishNames)
		{
			row.Add(doorSchedule->MakeDraftingText(doorMaterials[finish].DisplayName));
		}
		//*/
		row.Add(MakeShareable(new FDraftingComposite()));

		// Comments
		TSharedPtr<FDraftingComposite> whiteSpace = MakeShareable(new FDraftingComposite());
		whiteSpace->Dimensions.X = ModumateUnitParams::FXCoord::FloorplanInches(CommentsWidth);
		whiteSpace->Dimensions.Y = ModumateUnitParams::FYCoord::FloorplanInches(RowHeight);

		row.Add(whiteSpace);

		Data->MakeRow(row);
	}

	HorizontalAlignment = DraftingAlignment::Right;
	VerticalAlignment = DraftingAlignment::Bottom;
}

EDrawError FDoorSchedule::InitializeBounds(IModumateDraftingDraw *drawingInterface)
{
	EDrawError error;

	Title->InitializeBounds(drawingInterface);
	Title->MoveYTo(Title->Dimensions.Y * -1.0f);

	Dimensions.Y = Title->Dimensions.Y + ModumateUnitParams::FYCoord::FloorplanInches(TitleMargin);


	Data->MoveYTo(Dimensions.Y * -1.0f);

	error = Data->InitializeBounds(drawingInterface);

	Dimensions.X = Data->Dimensions.X;
	Dimensions.Y += Data->Dimensions.Y;

	return error;
}


#undef LOCTEXT_NAMESPACE
