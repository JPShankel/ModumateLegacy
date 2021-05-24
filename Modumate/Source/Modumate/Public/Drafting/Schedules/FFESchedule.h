// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "Drafting/Schedules/SummaryList.h"

class MODUMATE_API UModumateDocument;

class MODUMATE_API IModumateDraftingDraw;

class MODUMATE_API FFFESchedule : public FSummaryList
{
public:
	FFFESchedule(const UModumateDocument *doc, IModumateDraftingDraw *drawingInterface);
};
