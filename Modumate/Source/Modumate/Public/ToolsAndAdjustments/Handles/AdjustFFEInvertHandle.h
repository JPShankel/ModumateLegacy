// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "ToolsAndAdjustments/Common/AdjustmentHandleActor.h"

#include "AdjustFFEInvertHandle.generated.h"

UCLASS()
class MODUMATE_API AAdjustFFEInvertHandle : public AAdjustmentHandleActor
{
	GENERATED_BODY()

public:
	virtual bool BeginUse() override;
	virtual FVector GetHandlePosition() const override;
	virtual FVector GetHandleDirection() const override;

protected:
	virtual bool GetHandleWidgetStyle(const USlateWidgetStyleAsset*& OutButtonStyle, FVector2D &OutWidgetSize, FVector2D &OutMainButtonOffset) const override;
};
