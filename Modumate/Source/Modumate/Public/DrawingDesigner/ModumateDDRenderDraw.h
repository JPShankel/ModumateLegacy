// Copyright 2022 Modumate, Inc. All Rights Reserved.

#pragma once

#include "Drafting/DraftingHUDDraw.h"

class FModumateDDRenderDraw : public FDraftingHUDDraw
{
public:
	using DrawCallback = TFunction<void(FVector, FVector, FModumateLayerType)>;
	FModumateDDRenderDraw(const FDraftingHUDDraw& Base, DrawCallback& CallbackOperator);

	virtual EDrawError DrawLine(const ModumateUnitParams::FXCoord& x1, const ModumateUnitParams::FYCoord& y1,
		const ModumateUnitParams::FXCoord& x2, const ModumateUnitParams::FYCoord& y2,
		const ModumateUnitParams::FThickness& thickness, const FMColor& color,
		const LinePattern& linePattern, const ModumateUnitParams::FPhase& phase,
		FModumateLayerType layerType = FModumateLayerType::kDefault) override;

protected:
	DrawCallback LineCallback;

};
