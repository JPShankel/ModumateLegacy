// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "DraftingDrawing.h"

namespace Modumate {

	class FDrawingArea;

	class FFloorplan : public FDraftingDrawing
	{
	public:
		FFloorplan(const FModumateDocument *doc, UWorld *world, SceneCaptureID captureObjID);

		virtual void OnPageCompleted() override;
	};
}
