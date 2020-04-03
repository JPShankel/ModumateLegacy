// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "DraftingSchedule.h"

namespace Modumate {

	class FDraftingComposite;

	// A common element of several types of schedules is a grid-like structure
	class FScheduleGrid : public FDraftingSchedule
	{
	public:
		virtual EDrawError InitializeColumns(TArray<FText> ColumnData, bool bAddHeaders = true);
		virtual EDrawError InitializeBounds(IModumateDraftingDraw *drawingInterface) override;
		EDrawError MakeRow(TArray<TSharedPtr<FDraftingComposite>> row);
		bool IsEmpty();

	public:
		// Column objects
		TSharedPtr<FDraftingComposite> Columns;
		TArray<Units::FYCoord> RowHeights;

		bool bShowLines = true;
	};
}
