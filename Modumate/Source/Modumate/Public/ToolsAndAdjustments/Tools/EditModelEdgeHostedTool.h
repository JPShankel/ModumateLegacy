// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once
#include "ToolsAndAdjustments/Common/EditModelToolBase.h"

#include "EditModelEdgeHostedTool.generated.h"

UCLASS()
class MODUMATE_API UEdgeHostedTool : public UEditModelToolBase
{
	GENERATED_BODY()

public:
	UEdgeHostedTool();

	virtual EToolMode GetToolMode() override { return EToolMode::VE_EDGEHOSTED; }
};
