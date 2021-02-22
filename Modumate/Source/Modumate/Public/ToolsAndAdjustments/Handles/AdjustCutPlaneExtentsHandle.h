// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "ToolsAndAdjustments/Common/AdjustmentHandleActor.h"

#include "AdjustCutPlaneExtentsHandle.generated.h"

UCLASS()
class MODUMATE_API AAdjustCutPlaneExtentsHandle : public AAdjustmentHandleActor
{
	GENERATED_BODY()

public:
	AAdjustCutPlaneExtentsHandle(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual bool BeginUse() override;
	virtual bool UpdateUse() override;
	virtual void EndUse() override;
	virtual bool HandleInputNumber(float number) override;

	virtual FVector GetHandlePosition() const override;

protected:
	virtual bool GetHandleWidgetStyle(const USlateWidgetStyleAsset*& OutButtonStyle, FVector2D &OutWidgetSize, FVector2D &OutMainButtonOffset) const override;

protected:
	virtual void UpdateTransform(float Offset);
	virtual void ApplyExtents(bool bIsPreview);

	FVector2D LastValidExtents;
	FVector2D OriginalExtents;
	FVector LastValidLocation;
	FVector OriginalLocation;
	float LastSign;
};
