// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "ToolsAndAdjustments/Common/AdjustmentHandleActor.h"

#include "JustificationHandle.generated.h"
UCLASS()
class MODUMATE_API AJustificationHandle : public AAdjustmentHandleActor
{
	GENERATED_BODY()

public:
	virtual bool BeginUse() override;
	virtual void Tick(float DeltaTime) override;
	virtual FVector GetHandlePosition() const override;
	virtual FVector GetHandleDirection() const override;

	void SetJustification(float InJustificationValue);

protected:
	virtual void SetEnabled(bool bNewEnabled) override;
	virtual bool GetHandleWidgetStyle(const USlateWidgetStyleAsset*& OutButtonStyle, FVector2D &OutWidgetSize, FVector2D &OutMainButtonOffset) const override;

	const static float DesiredWorldDist;
	const static float MaxScreenDist;

	float JustificationValue = 0.f;
	bool bRootExpanded = false;
};
