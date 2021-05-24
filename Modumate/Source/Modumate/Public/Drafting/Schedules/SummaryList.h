// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "Drafting/Schedules/DraftingSchedule.h"

class IModumateDraftingDraw;
class FIconElement;

class FSummaryList : public FDraftingSchedule
{
public:
	FSummaryList() {};
	virtual EDrawError InitializeBounds(IModumateDraftingDraw *drawingInterface) override;

	TArray<TSharedPtr<FIconElement>> ffeIconElements;
};
