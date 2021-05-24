// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "Drafting/Schedules/DraftingSchedule.h"

class MODUMATE_API UModumateDocument;

class MODUMATE_API FScheduleGrid;

class FDoorSchedule : public FDraftingSchedule
{
public:
	FDoorSchedule(const UModumateDocument *doc);
	virtual EDrawError InitializeBounds(IModumateDraftingDraw *drawingInterface) override;

public:
	TSharedPtr<FScheduleGrid> Data;

public:
	TArray<FName> FinishNames;

	float CommentsWidth = 3.0f;			// whitespace width of comments sections
};
