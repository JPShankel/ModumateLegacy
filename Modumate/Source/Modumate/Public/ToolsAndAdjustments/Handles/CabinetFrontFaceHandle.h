// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "ToolsAndAdjustments/Common/AdjustmentHandleActor.h"

#include "CabinetFrontFaceHandle.generated.h"

UCLASS()
class MODUMATE_API ACabinetFrontFaceHandle : public AAdjustmentHandleActor
{
	GENERATED_BODY()

public:
	static const float BaseCenterOffset;

	virtual bool BeginUse() override;
	virtual FVector GetHandlePosition() const override;
	virtual FVector GetHandleDirection() const;

protected:
	virtual bool GetHandleWidgetStyle(const USlateWidgetStyleAsset*& OutButtonStyle, FVector2D& OutWidgetSize, FVector2D& OutMainButtonOffset) const override;
};
