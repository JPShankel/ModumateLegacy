// Copyright 2019 Modumate, Inc. All Rights Reserved.
#pragma once

#include "ToolsAndAdjustments/Tools/EditModelSelectTool.h"

#include "EditModelMoveTool.generated.h"

class ALineActor;

UCLASS()
class MODUMATE_API UMoveObjectTool : public UEditModelToolBase, public FSelectedObjectToolMixin
{
	GENERATED_BODY()

private:
	FVector AnchorPoint;
	TMap<int32, FTransform> StartTransforms;
	TWeakObjectPtr<ALineActor> PendingMoveLine;

public:
	UMoveObjectTool(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual EToolMode GetToolMode() override { return EToolMode::VE_MOVEOBJECT; }
	virtual bool Activate() override;
	virtual bool Deactivate() override;
	virtual bool BeginUse() override;
	virtual bool FrameUpdate() override;
	virtual bool HandleInputNumber(double n) override;
	virtual bool EndUse() override;
	virtual bool AbortUse() override;
	virtual bool HandleControlKey(bool pressed) override;
};
