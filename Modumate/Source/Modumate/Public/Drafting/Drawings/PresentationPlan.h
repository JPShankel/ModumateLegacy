// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "Drafting/Drawings/DraftingDrawing.h"

namespace Modumate {

	class FDrawingArea;
	class FRoomLegend;

	class FPresentationPlan : public FDraftingDrawing
	{
	public:
		FPresentationPlan(const FModumateDocument *doc, UWorld *world, SceneCaptureID captureObjID);

		virtual bool MakeScaleTag(IModumateDraftingDraw *drawingInterface) override;

		virtual void OnPageCompleted() override;

		TWeakPtr<FRoomLegend> RoomLegend = nullptr;

	private:
		bool FindRooms(TSharedPtr<FDraftingComposite> Parent);
	};

}
