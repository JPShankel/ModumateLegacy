// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolsAndAdjustments/Common/EditModelToolBase.h"

#include "EditModelWandTool.generated.h"

UCLASS()
class MODUMATE_API UWandTool : public UEditModelToolBase
{
	GENERATED_BODY()

public:
	UWandTool(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual EToolMode GetToolMode() override { return EToolMode::VE_WAND; }
	virtual bool Activate() override;
	virtual bool Deactivate() override;
	virtual bool BeginUse() override;
	virtual bool HandleMouseUp() override;
	virtual bool FrameUpdate() override;
	virtual bool EndUse() override;
	virtual bool ShowSnapCursorAffordances() override { return false; }
};
