// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "ToolsAndAdjustments/Common/AdjustmentHandleActor.h"

#include "AdjustPolyExtrusionHandle.generated.h"
UCLASS()
class MODUMATE_API AAdjustPolyExtrusionHandle : public AAdjustmentHandleActor
{
	GENERATED_BODY()

public:
	virtual bool BeginUse() override;
	virtual bool UpdateUse() override;
	virtual FVector GetHandlePosition() const override;
	virtual FVector GetHandleDirection() const override;
	virtual bool HandleInputNumber(float number) override;

protected:
	virtual bool GetHandleWidgetStyle(const USlateWidgetStyleAsset*& OutButtonStyle, FVector2D &OutWidgetSize, FVector2D &OutMainButtonOffset) const override;

	TArray<FVector> OriginalControlPoints;
	FPlane OriginalPlane;
	float OriginalExtrusion;
	float LastValidExtrusion;
	FVector AnchorLoc;
};
