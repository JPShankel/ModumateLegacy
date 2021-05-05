// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "ToolsAndAdjustments/Common/EditModelToolBase.h"
#include "ToolsAndAdjustments/Common/SelectedObjectToolMixin.h"

#include "EditModelRotateTool.generated.h"

class ALineActor;
class AEditModelPlayerController;

UCLASS()
class URotateObjectTool : public UEditModelToolBase, public FSelectedObjectToolMixin
{
	GENERATED_BODY()

private:
	FVector AnchorPoint, AngleAnchor;
	enum eStages
	{
		Neutral = 0,
		AnchorPlaced,
		SelectingAngle
	};

	eStages Stage;

	TWeakObjectPtr<ALineActor> PendingSegmentStart;
	TWeakObjectPtr<ALineActor> PendingSegmentEnd;
	bool bOverrideAngleWithInput = false;
	float InputAngle = 0.f;

	bool bOriginalVerticalAffordanceSnap = false;
	bool bClockwise = true;

public:
	URotateObjectTool();

	virtual EToolMode GetToolMode() override { return EToolMode::VE_ROTATE; }
	virtual bool Activate() override;
	virtual bool Deactivate() override;
	virtual bool BeginUse() override;
	virtual bool EnterNextStage() override;
	virtual bool EndUse() override;
	virtual bool AbortUse() override;
	virtual bool PostEndOrAbort() override;
	void ApplyRotation();
	FQuat CalcToolAngle();
	virtual bool FrameUpdate() override;
	virtual bool HandleInputNumber(double n) override;
};
