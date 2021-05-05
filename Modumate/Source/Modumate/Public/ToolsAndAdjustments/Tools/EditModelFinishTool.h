// Copyright 2019 Modumate, Inc. All Rights Reserved.
#pragma once

#include "ToolsAndAdjustments/Tools/EditModelSurfaceGraphTool.h"
#include "UnrealClasses/EditModelGameState.h"
#include "UnrealClasses/EditModelPlayerController.h"

#include "EditModelFinishTool.generated.h"

UCLASS()
class MODUMATE_API UFinishTool : public USurfaceGraphTool
{
	GENERATED_BODY()

public:
	UFinishTool();

	virtual EToolMode GetToolMode() override { return EToolMode::VE_FINISH; }
	virtual bool Activate() override;
	virtual bool Deactivate() override;
	virtual bool BeginUse() override;
	virtual bool FrameUpdate() override;

protected:
	bool GetFinishCreationDeltas(TArray<FDeltaPtr>& OutDeltas);
};
