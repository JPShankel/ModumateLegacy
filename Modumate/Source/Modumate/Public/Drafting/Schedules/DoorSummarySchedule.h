// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "Drafting/Schedules/SummaryList.h"

namespace Modumate {

	class FModumateDocument;
	class IModumateDraftingDraw;

	class FDoorSummarySchedule : public FSummaryList
	{
	public:
		FDoorSummarySchedule(const FModumateDocument *doc, IModumateDraftingDraw *drawingInterface);
	};
}
