// Copyright 2019 Modumate, Inc. All Rights Reserved.
#pragma once

#include "ToolsAndAdjustments/Common/EditModelToolBase.h"
#include "ToolsAndAdjustments/Common/SelectedObjectToolMixin.h"

#include "EditModelMoveTool.generated.h"

class APendingSegmentActor;

UCLASS()
class MODUMATE_API UMoveObjectTool : public UEditModelToolBase, public FSelectedObjectToolMixin
{
	GENERATED_BODY()

private:
	void GetDeltasForGraphMoves(UModumateDocument* Doc, const TMap<int32, FTransform>& Transforms, TArray<FDeltaPtr>& OutDeltas);
	FVector AnchorPoint;
	bool bCtrlIsPressed = false;

public:
	UMoveObjectTool();

	virtual EToolMode GetToolMode() override { return EToolMode::VE_MOVEOBJECT; }
	virtual bool Activate() override;
	virtual bool Deactivate() override;
	virtual bool BeginUse() override;
	virtual bool FrameUpdate() override;
	virtual bool HandleInputNumber(double n) override;
	virtual bool EndUse() override;
	virtual bool AbortUse() override;
	virtual bool PostEndOrAbort() override;
	virtual bool HasDimensionActor() { return true; };
};
