// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "ToolsAndAdjustments/Handles/AdjustCutPlaneExtentsHandle.h"

#include "AdjustBackgroundImageExtentsHandle.generated.h"

UCLASS()
class MODUMATE_API AAdjustBackgroundImageExtentsHandle : public AAdjustCutPlaneExtentsHandle
{
	GENERATED_BODY()

public:
	explicit AAdjustBackgroundImageExtentsHandle();

	virtual bool BeginUse() override;

protected:
	virtual void ApplyExtents(bool bIsPreview) override;
	virtual void UpdateTransform(float Offset) override;

	float AspectRatio = 0.0f;
	FVector2D OriginalImageSize;
};
