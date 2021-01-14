// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "Drafting/ModumateDraftingElements.h"

namespace Modumate {

	class FDrawingArea : public FDraftingComposite
	{
	public:
		virtual EDrawError Draw(IModumateDraftingDraw *drawingInterface,
			FModumateUnitCoord2D position = FModumateUnitCoord2D(),
			ModumateUnitParams::FAngle orientation = ModumateUnitParams::FAngle::Radians(0),
			float scale = 1.0f) override;

	public:
		bool bClipped = true;
	};

}
