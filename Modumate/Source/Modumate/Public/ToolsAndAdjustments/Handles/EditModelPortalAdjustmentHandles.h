// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include "ToolsAndAdjustments/Common/AdjustmentHandleActor.h"

#include "EditModelPortalAdjustmentHandles.generated.h"


UCLASS()
class MODUMATE_API AAdjustPortalInvertHandle : public AAdjustmentHandleActor
{
	GENERATED_BODY()

public:
	virtual bool BeginUse() override;
	virtual FVector GetHandlePosition() const override;
	virtual FVector GetHandleDirection() const override;
	void SetTransvert(bool bInShouldTransvert);

protected:
	virtual bool GetHandleWidgetStyle(const USlateWidgetStyleAsset*& OutButtonStyle, FVector2D &OutWidgetSize, FVector2D &OutMainButtonOffset) const override;
	bool GetBottomCorners(FVector &OutCorner0, FVector &OutCorner1) const;

	bool bShouldTransvert = false;
};
