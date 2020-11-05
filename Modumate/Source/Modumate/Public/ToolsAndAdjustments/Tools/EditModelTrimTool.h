// Copyright 2019 Modumate, Inc. All Rights Reserved.
#pragma once

#include "ToolsAndAdjustments/Common/EditModelToolBase.h"

#include "EditModelTrimTool.generated.h"

UCLASS()
class MODUMATE_API UTrimTool : public UEditModelToolBase
{
	GENERATED_BODY()

public:
	UTrimTool(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual EToolMode GetToolMode() override { return EToolMode::VE_TRIM; }
	virtual bool Activate() override;
	virtual bool HandleInputNumber(double n) override;
	virtual bool Deactivate() override;
	virtual bool BeginUse() override;
	virtual bool FrameUpdate() override;

protected:
	int32 TargetEdgeID;
	FVector TargetEdgeStartPos, TargetEdgeEndPos;
};
