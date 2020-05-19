// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "ToolsAndAdjustments/Common/EditModelToolBase.h"
#include "ToolsAndAdjustments/Tools/EditModelSelectTool.h"

#include "EditModelRotateTool.generated.h"

class ALineActor;
class AEditModelPlayerController_CPP;

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

public:
	URotateObjectTool(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual EToolMode GetToolMode() override { return EToolMode::VE_ROTATE; }
	virtual bool Activate() override;
	virtual bool Deactivate() override;
	virtual bool BeginUse() override;
	virtual bool EnterNextStage() override;
	virtual bool EndUse() override;
	virtual bool AbortUse() override;
	void ApplyRotation();
	float CalcToolAngle();
	virtual bool FrameUpdate() override;
	virtual bool HandleInputNumber(double n) override;
};
