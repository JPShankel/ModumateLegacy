// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "ToolsAndAdjustments/Common/EditModelToolBase.h"

#include "EditModelDrawingTool.generated.h"

UCLASS()
class MODUMATE_API UDrawingTool : public UEditModelToolBase
{
	GENERATED_BODY()

public:
	UDrawingTool();

	virtual EToolMode GetToolMode() override { return EToolMode::VE_DRAWING; }
	virtual bool Activate() override;
	virtual bool Deactivate() override;
	virtual bool BeginUse() override;
	virtual bool FrameUpdate() override;

	virtual bool EnterNextStage() override;

	virtual bool EndUse() override;
	virtual bool AbortUse() override;
};
