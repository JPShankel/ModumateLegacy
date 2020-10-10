// Copyright 2019 Modumate, Inc. All Rights Reserved.
#pragma once

#include "ToolsAndAdjustments/Tools/EditModelSurfaceGraphTool.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"

#include "EditModelFinishTool.generated.h"

UCLASS()
class MODUMATE_API UFinishTool : public USurfaceGraphTool
{
	GENERATED_BODY()

public:
	UFinishTool(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual EToolMode GetToolMode() override { return EToolMode::VE_FINISH; }
	virtual bool Activate() override;
	virtual bool Deactivate() override;
	virtual bool BeginUse() override;
	virtual bool FrameUpdate() override;
	virtual TArray<EEditViewModes> GetRequiredEditModes() const override;
};
