// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "ToolsAndAdjustments/Handles/AdjustInvertHandle.h"

#include "AdjustPortalInvertHandle.generated.h"

UCLASS()
class MODUMATE_API AAdjustPortalInvertHandle : public AAdjustInvertHandle
{
	GENERATED_BODY()

public:
	virtual bool BeginUse() override;
	virtual FVector GetHandlePosition() const override;
};
