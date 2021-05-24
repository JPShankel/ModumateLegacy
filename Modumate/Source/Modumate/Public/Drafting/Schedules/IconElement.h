// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "Drafting/Schedules/DraftingSchedule.h"


class FDraftingComposite;
// An icon and its basic information supporting an icon: IDTag, name, count, finish, etc.
class FIconElement : public FDraftingSchedule
{
public:
	FIconElement();
	virtual EDrawError InitializeBounds(IModumateDraftingDraw *drawingInterface) override;

public:
	TSharedPtr<FDraftingComposite> Information;
	TSharedPtr<FDraftingComposite> Icon;

public:
	// by default, the icon is on the left of the supporting information
	DraftingAlignment iconLocation = DraftingAlignment::Left;
};
