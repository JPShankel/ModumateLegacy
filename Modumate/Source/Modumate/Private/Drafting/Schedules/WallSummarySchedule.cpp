#include "Drafting/Schedules/WallSummarySchedule.h"

#include "ModumateCore/ModumateDimensionStatics.h"
#include "DocumentManagement/ModumateDocument.h"
#include "Drafting/ModumateDraftingElements.h"
#include "Drafting/ModumateDraftingTags.h"
#include "Drafting/Schedules/ScheduleGrid.h"

#define LOCTEXT_NAMESPACE "ModumateWallDetailsSchedule"

FWallSummarySchedule::FWallSummarySchedule(const UModumateDocument *doc, UWorld *World, IModumateDraftingDraw *drawingInterface)
{
	Title = MakeShareable(new FDraftingText(
		LOCTEXT("title", "Wall Schedule: Assembly Summaries"),
		ModumateUnitParams::FFontSize::FloorplanInches(TitleHeight),
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

	TArray<FBIMAssemblySpec> assemblies;
	doc->GetPresetCollection().GetProjectAssembliesForObjectType(EObjectType::OTWallSegment, assemblies);
	ensureAlways(assemblies.Num() > 0);

	static const FText InchesFormat = FText::FromString(TEXT("{0}\""));

	for (int32 i = 0; i < assemblies.Num(); i++)
	{
		TArray<TSharedPtr<FDraftingComposite>> row;

		// tag
		const FBIMAssemblySpec &assembly = assemblies[i];

		TSharedPtr<FMaterialTagSequence> tag = MakeShareable(new FMaterialTagSequence(assembly));
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
		row.Add(MakeDraftingText(FText::FromString(assembly.DisplayName)));

		// Thickness
		FText thickness = FText::FromString(UModumateDimensionStatics::DecimalToFractionString_DEPRECATED(tag->TotalThickness));

		thickness = FText::Format(InchesFormat, thickness);

		row.Add(MakeDraftingText(thickness));

		// Comments
		TSharedPtr<FDraftingComposite> whiteSpace = MakeShareable(new FDraftingComposite());
		whiteSpace->Dimensions.X = ModumateUnitParams::FXCoord::FloorplanInches(CommentsWidth);
		whiteSpace->Dimensions.Y = ModumateUnitParams::FYCoord::FloorplanInches(External->RowHeight);

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
	Dimensions.Y += ModumateUnitParams::FYCoord::FloorplanInches(TitleMargin);

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
		Dimensions.Y += ModumateUnitParams::FYCoord::FloorplanInches(ScheduleMargin);
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

#undef LOCTEXT_NAMESPACE
