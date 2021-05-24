// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "Drafting/Schedules/DraftingSchedule.h"

class MODUMATE_API UModumateDocument;

class MODUMATE_API IModumateDraftingDraw;
class MODUMATE_API FScheduleGrid;

class MODUMATE_API FWallSummarySchedule : public FDraftingSchedule
{
public:
	FWallSummarySchedule(const UModumateDocument *doc, UWorld *World, IModumateDraftingDraw *drawingInterface);
	virtual EDrawError InitializeBounds(IModumateDraftingDraw *drawingInterface) override;

public:
	TSharedPtr<FScheduleGrid> Internal;
	TSharedPtr<FScheduleGrid> External;

	float CommentsWidth = 3.0f;			// whitespace width of comments sections
};

