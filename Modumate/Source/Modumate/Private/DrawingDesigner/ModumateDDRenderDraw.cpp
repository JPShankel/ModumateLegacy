// Copyright 2022 Modumate, Inc. All Rights Reserved.

#include "DrawingDesigner/ModumateDDRenderDraw.h"


FModumateDDRenderDraw::FModumateDDRenderDraw(const FDraftingHUDDraw& Base, DrawCallback& CallbackOperator)
	: FDraftingHUDDraw(Base)
{
	LineCallback = CallbackOperator;
}

EDrawError FModumateDDRenderDraw::DrawLine(const ModumateUnitParams::FXCoord& x1, const ModumateUnitParams::FYCoord& y1,
	const ModumateUnitParams::FXCoord& x2, const ModumateUnitParams::FYCoord& y2,
	const ModumateUnitParams::FThickness& thickness, const FMColor& color, const LinePattern& linePattern,
	const ModumateUnitParams::FPhase& phase, FModumateLayerType layerType /*= FModumateLayerType::kDefault*/)
{
	FVector p1 = CurrentAxisX * x1.AsWorldCentimeters() + CurrentAxisY * y1.AsWorldCentimeters() + CurrentOrigin;
	FVector p2 = CurrentAxisX * x2.AsWorldCentimeters() + CurrentAxisY * y2.AsWorldCentimeters() + CurrentOrigin;
	LineCallback(p1, p2, layerType);

	return EDrawError::ErrorNone;
}
