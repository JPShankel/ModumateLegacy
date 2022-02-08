// Copyright 2022 Modumate, Inc. All Rights Reserved.
#pragma once

#include "ToolsAndAdjustments/Common/EditModelToolBase.h"

#include "EditModelPattern2DTool.generated.h"


UCLASS()
class MODUMATE_API UEditModelPattern2DTool : public UEditModelToolBase
{
	GENERATED_BODY()

public:
	UEditModelPattern2DTool();

	virtual EToolMode GetToolMode() override { return EToolMode::VE_PATTERN2D; }

	virtual bool Activate() override;
};