// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "Drafting/Schedules/DraftingSchedule.h"

namespace Modumate {

	class FModumateDocument;
	class IModumateDraftingDraw;
	class FScheduleGrid;
	class FImagePrimitive;

	class FWallDetailsSchedule : public FDraftingSchedule
	{
	public:
		FWallDetailsSchedule(const FModumateDocument *doc, UWorld *World, IModumateDraftingDraw *drawingInterface);
		virtual EDrawError InitializeBounds(IModumateDraftingDraw *drawingInterface) override;

	public:
		TArray<TSharedPtr<FScheduleGrid>> assemblyData;
		TArray<TSharedPtr<FImagePrimitive>> iconData;

		float CommentsWidth =	3.0f;			// whitespace width of comments sections
	};

}
