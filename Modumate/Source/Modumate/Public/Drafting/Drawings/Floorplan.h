// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "Drafting/Drawings/DraftingDrawing.h"

namespace Modumate {

	class FDrawingArea;

	class FFloorplan : public FDraftingDrawing
	{
	public:
		FFloorplan(const UModumateDocument *doc, UWorld *world, SceneCaptureID captureObjID);

		virtual void OnPageCompleted() override;
	};
}
