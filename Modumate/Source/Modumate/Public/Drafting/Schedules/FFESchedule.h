// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "SummaryList.h"

namespace Modumate {

	class FModumateDocument;
	class IModumateDraftingDraw;

	class FFFESchedule : public FSummaryList
	{
	public:
		FFFESchedule(const FModumateDocument *doc, IModumateDraftingDraw *drawingInterface);
	};

}
