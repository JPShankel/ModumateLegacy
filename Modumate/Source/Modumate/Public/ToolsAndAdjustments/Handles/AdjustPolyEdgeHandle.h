// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "ToolsAndAdjustments/Common/AdjustmentHandleActor.h"

#include "AdjustPolyEdgeHandle.generated.h"

UCLASS()
class MODUMATE_API AAdjustPolyEdgeHandle : public AAdjustmentHandleActor
{
	GENERATED_BODY()

public:
	AAdjustPolyEdgeHandle();

	virtual bool BeginUse() override;
	virtual bool UpdateUse() override;
	virtual FVector GetHandlePosition() const override;
	virtual bool HandleInputNumber(float number) override;

protected:
	virtual bool GetHandleWidgetStyle(const USlateWidgetStyleAsset*& OutButtonStyle, FVector2D &OutWidgetSize, FVector2D &OutMainButtonOffset) const override;

	bool GetTransforms(const FVector Offset, TMap<int32, FTransform>& OutTransforms);

	FPlane PolyPlane;
	FVector CurrentDirection;
	TArray<FVector> OriginalPolyPoints;
	TArray<FVector> LastValidPolyPoints;
};

