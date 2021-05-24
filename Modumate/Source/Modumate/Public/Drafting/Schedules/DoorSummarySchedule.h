// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "Drafting/Schedules/SummaryList.h"

class MODUMATE_API UModumateDocument;

class MODUMATE_API IModumateDraftingDraw;

class MODUMATE_API FDoorSummarySchedule : public FSummaryList
{
public:
	FDoorSummarySchedule(const UModumateDocument *doc, IModumateDraftingDraw *drawingInterface);
};
