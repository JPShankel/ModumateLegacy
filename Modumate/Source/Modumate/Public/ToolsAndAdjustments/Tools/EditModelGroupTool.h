// Copyright 2021 Modumate, Inc. All Rights Reserved.
#pragma once

#include "ToolsAndAdjustments/Common/EditModelToolBase.h"
#include "UnrealClasses/EditModelPlayerState.h"

#include "EditModelGroupTool.generated.h"

UCLASS()
class MODUMATE_API UGroupTool : public UEditModelToolBase
{
	GENERATED_BODY()

public:
	UGroupTool();

public:
	virtual EToolMode GetToolMode() override { return EToolMode::VE_GROUP; }

	virtual bool Activate() override;
};
