#include "Drafting/Schedules/ScheduleGrid.h"
#include "Drafting/ModumateDraftingElements.h"

EDrawError FScheduleGrid::MakeRow(TArray<TSharedPtr<FDraftingComposite>> row)
{
	if (row.Num() != Columns->Children.Num())
	{
		return EDrawError::ErrorBadParam;
	}

	for (int i = 0; i < row.Num(); i++)
	{
		auto column = Columns->Children[i];

		column->Children.Add(row[i]);
		row[i]->MoveYTo(ModumateUnitParams::FYCoord::FloorplanInches(-1.0f * column->Children.Num() * RowHeight));
	}

	Dimensions.Y = Dimensions.Y + ModumateUnitParams::FYCoord::FloorplanInches(RowHeight);

	return EDrawError::ErrorNone;
}

bool FScheduleGrid::IsEmpty()
{
	// returns true if there are columns and the columns have at least one row (besides the headers)
	return !(Columns->Children.Num() > 0 && Columns->Children[0]->Children.Num() > 1);
}

EDrawError FScheduleGrid::InitializeColumns(TArray<FText> ColumnData, bool bAddHeaders)
{
	Columns.Reset();

	Dimensions = FModumateUnitCoord2D::FloorplanInches(FVector2D::ZeroVector);

	Columns = MakeShareable(new FDraftingComposite());
	for (auto header : ColumnData)
	{
		TSharedPtr<FDraftingComposite> column = MakeShareable(new FDraftingComposite());
		if (bAddHeaders)
		{
			TSharedPtr<FDraftingText> text = MakeShareable(new FDraftingText(
				header,
				DefaultFontSize,
				DefaultColor,
				FontType::Bold,
				DefaultAlignment));

			column->Children.Add(text);
			text->MoveYTo(ModumateUnitParams::FYCoord::FloorplanInches(-1.0f * column->Children.Num() * RowHeight));
		}

		Columns->Children.Add(column);
	}
	Children.Add(Columns);

	return EDrawError::ErrorNone;
}

EDrawError FScheduleGrid::InitializeBounds(IModumateDraftingDraw *drawingInterface)
{
	EDrawError error = EDrawError::ErrorNone;

	TArray<ModumateUnitParams::FYCoord> ySum;
	for (int32 i = 0; i < Columns->Children[0]->Children.Num(); i++)
	{
		ModumateUnitParams::FYCoord maxY = ModumateUnitParams::FYCoord::FloorplanInches(0.0f);
		for (int32 j = 0; j < Columns->Children.Num(); j++)
		{
			ModumateUnitParams::FYCoord currentY = Columns->Children[j]->Children[i]->GetDimensions(drawingInterface).Y;
			maxY = currentY > maxY ? currentY : maxY;
		}

		maxY += ModumateUnitParams::FYCoord::FloorplanInches(2.0f * Margin);
		ySum.Add(maxY);
		if (i != 0)
		{
			ySum[i] += ySum[i - 1];
		}
	}

	// calculate width based on minimum width of all columns
	float xOffset = 0.0f;
	for (int32 i = 0; i < Columns->Children.Num(); i++)
	{
		xOffset += 2.0f * Margin;

		TSharedPtr<FDraftingElement> column = Columns->Children[i];
		column->MoveXTo(ModumateUnitParams::FXCoord::FloorplanInches(xOffset));

		float maxWidth = 0.0f;
		for (int32 j = 0; j < column->Children.Num(); j++)
		{
			auto child = column->Children[j];

			float currentWidth = child->GetDimensions(drawingInterface).X.AsFloorplanInches();
			maxWidth = currentWidth > maxWidth ? currentWidth : maxWidth;

			ModumateUnitParams::FYCoord currentRowHeight = j == 0 ? ySum[j] : ySum[j] - ySum[j - 1];

			child->MoveYTo(ySum[j] * -1.0f + currentRowHeight / 2.0f);
			child->VerticalAlignment = DraftingAlignment::Center;
		}

		xOffset += maxWidth;
		xOffset += ColumnMargin;
	}

	Dimensions.X = ModumateUnitParams::FXCoord::FloorplanInches(xOffset);
	Dimensions.Y = ySum[Columns->Children[0]->Children.Num() - 1];

	if (bShowLines)
	{
		for (int32 i = 0; i <= Columns->Children[0]->Children.Num(); i++)
		{
			TSharedPtr<FDraftingLine> line = MakeShareable(new FDraftingLine(
				ModumateUnitParams::FLength::FloorplanInches(xOffset),
				ModumateUnitParams::FThickness::FloorplanInches(LineWidth)));
			if (i == 0)
			{
				line->MoveYTo(ModumateUnitParams::FYCoord::FloorplanInches(0.0f));
			}
			else
			{
				line->MoveYTo(ySum[i - 1] * -1.0f);
			}

			Children.Add(line);
		}

		ModumateUnitParams::FYCoord vertLineY = ModumateUnitParams::FYCoord::FloorplanInches(-Dimensions.Y.AsFloorplanInches());

		TSharedPtr<FDraftingLine> line = MakeShareable(new FDraftingLine(
			ModumateUnitParams::FLength::FloorplanInches(Dimensions.Y.AsFloorplanInches()),
			ModumateUnitParams::FThickness::FloorplanInches(LineWidth)));
		line->SetLocalOrientation(ModumateUnitParams::FAngle::Degrees(90.0f));
		line->MoveYTo(vertLineY);
		Children.Add(line);

		line = MakeShareable(new FDraftingLine(
			ModumateUnitParams::FLength::FloorplanInches(Dimensions.Y.AsFloorplanInches()),
			ModumateUnitParams::FThickness::FloorplanInches(LineWidth)));
		line->SetLocalOrientation(ModumateUnitParams::FAngle::Degrees(90.0f));
		line->SetLocalPosition(ModumateUnitParams::FXCoord::FloorplanInches(xOffset), vertLineY);
		Children.Add(line);
	}

	return error;
}

