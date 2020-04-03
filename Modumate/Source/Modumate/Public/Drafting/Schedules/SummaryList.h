// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "DraftingSchedule.h"

namespace Modumate {

	class IModumateDraftingDraw;
	class FIconElement;

	class FSummaryList : public FDraftingSchedule
	{
	public:
		FSummaryList() {};
		virtual EDrawError InitializeBounds(IModumateDraftingDraw *drawingInterface) override;

		TArray<TSharedPtr<FIconElement>> ffeIconElements;
	};
}
