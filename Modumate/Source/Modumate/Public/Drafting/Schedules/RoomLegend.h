// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "DraftingSchedule.h"

namespace Modumate {

	class FScheduleGrid;

	class FRoomLegend : public FDraftingSchedule
	{
	public:
		FRoomLegend();
		virtual EDrawError InitializeBounds(IModumateDraftingDraw *drawingInterface) override;

	public:
		TSharedPtr<FScheduleGrid> Data;
	};

}
