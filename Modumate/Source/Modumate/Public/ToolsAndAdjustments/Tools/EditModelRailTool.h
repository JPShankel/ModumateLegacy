// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once
#include "EditModelToolBase.h"

#include "EditModelRailTool.generated.h"

class ALineActor3D_CPP;

UCLASS()
class MODUMATE_API URailTool : public UEditModelToolBase
{
	GENERATED_BODY()

private:
	ALineActor3D_CPP * PendingSegment;

public:
	URailTool(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual EToolMode GetToolMode() override { return EToolMode::VE_RAIL; }
	virtual bool Activate() override;
	virtual bool BeginUse() override;
	virtual bool EnterNextStage() override;
	virtual bool FrameUpdate() override;
	virtual bool EndUse() override;
	virtual bool AbortUse() override;
	virtual bool HandleSpacebar() override;
	virtual bool HandleControlKey(bool pressed) override;
	bool HandleInputNumber(double n) override;
};
