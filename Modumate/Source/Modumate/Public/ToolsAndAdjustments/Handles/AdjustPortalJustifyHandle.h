// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "ToolsAndAdjustments/Common/AdjustmentHandleActor.h"

#include "AdjustPortalJustifyHandle.generated.h"

UCLASS()
class MODUMATE_API AAdjustPortalJustifyHandle : public AAdjustmentHandleActor
{
	GENERATED_BODY()

public:
	virtual bool BeginUse() override;
	virtual bool UpdateUse() override;
	virtual void EndUse();
	virtual void AbortUse();


	virtual FVector GetHandleDirection() const override;
	virtual FVector GetHandlePosition() const override;
	virtual bool HandleInputNumber(float number) override;

	virtual bool HasDimensionActor() override
		{ return false; }


protected:
	virtual bool GetHandleWidgetStyle(const USlateWidgetStyleAsset*& OutButtonStyle, FVector2D &OutWidgetSize, FVector2D &OutMainButtonOffset) const override;

	void ApplyJustification(bool preview);

	FVector2D InitialLocation;
	FMOIStateData InitialState;
	float CurrentDistance = 0.0f;
	float InitialJustifyValue = 0.0f;
};
