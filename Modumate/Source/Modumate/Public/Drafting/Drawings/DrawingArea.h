// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "ModumateDraftingElements.h"

namespace Modumate {

	class FDrawingArea : public FDraftingComposite
	{
	public:
		virtual EDrawError Draw(IModumateDraftingDraw *drawingInterface,
			Units::FCoordinates2D position = Units::FCoordinates2D(),
			Units::FAngle orientation = Units::FAngle::Radians(0),
			float scale = 1.0f) override;

	public:
		bool bClipped = true;
	};

}
