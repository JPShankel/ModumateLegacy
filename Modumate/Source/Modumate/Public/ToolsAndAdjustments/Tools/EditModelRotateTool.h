// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "EditModelToolbase.h"
#include "EditModelSelectTool.h"

#include "EditModelRotateTool.generated.h"

class ALineActor3D_CPP;
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

	TWeakObjectPtr<ALineActor3D_CPP> PendingSegmentStart;
	TWeakObjectPtr<ALineActor3D_CPP> PendingSegmentEnd;
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
