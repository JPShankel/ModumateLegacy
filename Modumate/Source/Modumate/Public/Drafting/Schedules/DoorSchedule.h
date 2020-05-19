// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "Drafting/Schedules/DraftingSchedule.h"

namespace Modumate {

	class FModumateDocument;
	class FScheduleGrid;

	class FDoorSchedule : public FDraftingSchedule
	{
	public:
		FDoorSchedule(const FModumateDocument *doc);
		virtual EDrawError InitializeBounds(IModumateDraftingDraw *drawingInterface) override;

	public:
		TSharedPtr<FScheduleGrid> Data;

	public:
		TArray<FName> FinishNames;

		float CommentsWidth = 3.0f;			// whitespace width of comments sections
	};
}
