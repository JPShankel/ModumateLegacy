// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "Drafting/Schedules/DraftingSchedule.h"

class MODUMATE_API UModumateDocument;

namespace Modumate {

	class IModumateDraftingDraw;
	class FScheduleGrid;
	class FImagePrimitive;

	class FWallDetailsSchedule : public FDraftingSchedule
	{
	public:
		FWallDetailsSchedule(const UModumateDocument *doc, UWorld *World, IModumateDraftingDraw *drawingInterface);
		virtual EDrawError InitializeBounds(IModumateDraftingDraw *drawingInterface) override;

	public:
		TArray<TSharedPtr<FScheduleGrid>> assemblyData;
		TArray<TSharedPtr<FImagePrimitive>> iconData;

		float CommentsWidth =	3.0f;			// whitespace width of comments sections
	};

}
