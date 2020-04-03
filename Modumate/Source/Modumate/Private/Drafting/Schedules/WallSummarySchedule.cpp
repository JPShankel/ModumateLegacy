#include "WallSummarySchedule.h"

#include "ModumateDimensionStatics.h"
#include "ModumateDocument.h"
#include "ModumateDraftingElements.h"
#include "ModumateDraftingTags.h"
#include "ScheduleGrid.h"

using namespace Modumate::Units;

#define LOCTEXT_NAMESPACE "ModumateWallDetailsSchedule"

namespace Modumate {

	FWallSummarySchedule::FWallSummarySchedule(const FModumateDocument *doc, UWorld *World, IModumateDraftingDraw *drawingInterface)
	{
		Title = MakeShareable(new FDraftingText(
			LOCTEXT("title", "Wall Schedule: Assembly Summaries"),
			FFontSize::FloorplanInches(TitleHeight),
			DefaultColor,
			FontType::Bold));

		Children.Add(Title);

		ColumnHeaders = {
			LOCTEXT("id", "ID"),
			LOCTEXT("customname", "Custom Name"),
			LOCTEXT("thickness", "Thickness"),
			LOCTEXT("comments", "Comments")
		};

		/*
		SectionHeaders = {
			LOCTEXT("interior", "Interior"),
			LOCTEXT("exterior", "Exterior")
		};
		//*/
		Internal = MakeShareable(new FScheduleGrid());
		Internal->InitializeColumns(ColumnHeaders);

		External = MakeShareable(new FScheduleGrid());
		External->InitializeColumns(ColumnHeaders);

		TArray<FModumateObjectAssembly> assemblies = doc->GetAssembliesForToolMode_DEPRECATED(World, EToolMode::VE_WALL);
		ensureAlways(assemblies.Num() > 0);

		static const FText InchesFormat = FText::FromString(TEXT("{0}\""));

		for (int32 i = 0; i < assemblies.Num(); i++)
		{
			TArray<TSharedPtr<FDraftingComposite>> row;

			// tag
			FModumateObjectAssembly assembly = assemblies[i];

			TSharedPtr<FMaterialTagSequence> tag = MakeShareable(new FMaterialTagSequence(assembly.Layers));
			tag->InitializeBounds(drawingInterface);
			TSharedPtr<FFilledRoundedRectTag> idTag = MakeShareable(new FFilledRoundedRectTag(tag));
			idTag->InitializeBounds(drawingInterface);

			if (i == 0)
			{
				float rowHeight = idTag->Dimensions.Y.AsFloorplanInches() + 2.0f * Margin;
				Internal->RowHeight = rowHeight;
				External->RowHeight = rowHeight;
			}

			row.Add(idTag);

			// Custom Name
			row.Add(MakeDraftingText(FText::FromString(assembly.GetProperty(BIM::Parameters::Name))));

			// Thickness
			FText thickness = FText::FromString(UModumateDimensionStatics::DecimalToFractionString(tag->TotalThickness));

			thickness = FText::Format(InchesFormat, thickness);

			row.Add(MakeDraftingText(thickness));

			// Comments
			TSharedPtr<FDraftingComposite> whiteSpace = MakeShareable(new FDraftingComposite());
			whiteSpace->Dimensions.X = FXCoord::FloorplanInches(CommentsWidth);
			whiteSpace->Dimensions.Y = FYCoord::FloorplanInches(External->RowHeight);

			row.Add(whiteSpace);

			External->MakeRow(row);
		}

		HorizontalAlignment = DraftingAlignment::Right;
		VerticalAlignment = DraftingAlignment::Bottom;

	}

	EDrawError FWallSummarySchedule::InitializeBounds(IModumateDraftingDraw *drawingInterface)
	{
		EDrawError error = EDrawError::ErrorNone;

		Title->InitializeBounds(drawingInterface);
		Title->MoveYTo(Title->Dimensions.Y * -1.0f);

		Dimensions = Title->Dimensions;
		Dimensions.Y += FYCoord::FloorplanInches(TitleMargin);

		if (!Internal->IsEmpty())
		{
			error = Internal->InitializeBounds(drawingInterface);
			Internal->MoveYTo(Dimensions.Y * -1.0f);
			Dimensions.X = Internal->Dimensions.X > Dimensions.X ? Internal->Dimensions.X : Dimensions.X;
			Dimensions.Y += Internal->Dimensions.Y;

			Children.Add(Internal);
		}

		// if there are both internal and external walls, add margin between the schedule grids
		if (!Internal->IsEmpty() && !External->IsEmpty())
		{
			Dimensions.Y += FYCoord::FloorplanInches(ScheduleMargin);
		}

		if (!External->IsEmpty())
		{
			error = External->InitializeBounds(drawingInterface);
			External->MoveYTo(Dimensions.Y * -1.0f);
			Dimensions.X = External->Dimensions.X > Dimensions.X ? External->Dimensions.X : Dimensions.X;
			Dimensions.Y += External->Dimensions.Y;

			Children.Add(External);
		}

		return error;
	}
}

#undef LOCTEXT_NAMESPACE
