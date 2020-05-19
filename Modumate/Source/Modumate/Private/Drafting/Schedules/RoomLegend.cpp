#include "Drafting/Schedules/RoomLegend.h"

#include "Drafting/ModumateDraftingElements.h"
#include "Drafting/Schedules/ScheduleGrid.h"

using namespace Modumate::Units;

#define LOCTEXT_NAMESPACE "ModumateRoomLegend"

namespace Modumate {
	FRoomLegend::FRoomLegend()
	{
		Title = MakeShareable(new FDraftingText(
			LOCTEXT("roomlegend_title", "Room Legend"),
			FFontSize::FloorplanInches(TitleHeight),
			DefaultColor,
			FontType::Bold));

		Children.Add(Title);

		// TODO: the commented out columns are not a part of the legend used for presentation plans,
		// but there will be more detail when this is used for other plans
		ColumnHeaders = {
			LOCTEXT("number", "#"),
			LOCTEXT("color", "   "),
			//LOCTEXT("group", "Group"),
			LOCTEXT("name", "Name"),
			LOCTEXT("area", "Area (sq. ft.)")
			//LOCTEXT("load", "Load Factor"),
			//LOCTEXT("occ", "Occupants"),
			//LOCTEXT("netgross", "Net/Gross")
			// potentially add comments column here
		};

		Data = MakeShareable(new FScheduleGrid());
		Data->InitializeColumns(ColumnHeaders);

		Children.Add(Data);
	}

	EDrawError FRoomLegend::InitializeBounds(IModumateDraftingDraw *drawingInterface)
	{
		EDrawError error;

		Title->InitializeBounds(drawingInterface);
		Title->MoveYTo(Title->Dimensions.Y * -1.0f);

		Dimensions.Y = Title->Dimensions.Y + FYCoord::FloorplanInches(TitleMargin);

		Data->MoveYTo(Dimensions.Y * -1.0f);

		error = Data->InitializeBounds(drawingInterface);
		Dimensions.X = Data->Dimensions.X;
		Dimensions.Y += Data->Dimensions.Y;

		return error;
	}
}

#undef LOCTEXT_NAMESPACE
