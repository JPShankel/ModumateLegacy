// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "Drafting/Schedules/DraftingSchedule.h"

class FScheduleGrid;

class FRoomLegend : public FDraftingSchedule
{
public:
	FRoomLegend();
	virtual EDrawError InitializeBounds(IModumateDraftingDraw *drawingInterface) override;

public:
	TSharedPtr<FScheduleGrid> Data;
};
