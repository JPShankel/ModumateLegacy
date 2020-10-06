// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once
#include "ToolsAndAdjustments/Tools/EditModelPlaneHostedObjTool.h"

#include "EditModelRailTool.generated.h"

UCLASS()
class MODUMATE_API URailTool : public UPlaneHostedObjTool
{
	GENERATED_BODY()

public:
	URailTool(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual EToolMode GetToolMode() override { return EToolMode::VE_RAIL; }

protected:
	virtual float GetDefaultPlaneHeight() const override;
};
