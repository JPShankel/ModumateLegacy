// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "ToolsAndAdjustments/Common/AdjustmentHandleActor.h"

#include "AdjustPortalOrientHandle.generated.h"

UCLASS()
class MODUMATE_API AAdjustPortalOrientHandle : public AAdjustmentHandleActor
{
	GENERATED_BODY()

public:
	virtual bool BeginUse() override;

	virtual FVector GetHandleDirection() const override;
	virtual FVector GetHandlePosition() const override;
	virtual bool HasDimensionActor() override
		{ return false; }

	bool CounterClockwise = false;

protected:
	virtual bool GetHandleWidgetStyle(const USlateWidgetStyleAsset*& OutButtonStyle, FVector2D &OutWidgetSize, FVector2D &OutMainButtonOffset) const override;
};
