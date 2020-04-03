// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "DraftingSchedule.h"

namespace Modumate {

	class FModumateDocument;
	class IModumateDraftingDraw;
	class FScheduleGrid;

	class FWallSummarySchedule : public FDraftingSchedule
	{
	public:
		FWallSummarySchedule(const FModumateDocument *doc, UWorld *World, IModumateDraftingDraw *drawingInterface);
		virtual EDrawError InitializeBounds(IModumateDraftingDraw *drawingInterface) override;

	public:
		TSharedPtr<FScheduleGrid> Internal;
		TSharedPtr<FScheduleGrid> External;

		float CommentsWidth = 3.0f;			// whitespace width of comments sections
	};
}
