// Copyright 2019 Modumate, Inc. All Rights Reserved.
#pragma once

#include "ToolsAndAdjustments/Common/EditModelToolBase.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "UnrealClasses/DynamicMeshActor.h"

#include "EditModelCreateSimilarTool.generated.h"

class AEditModelGameMode;
class AEditModelGameState;
class ALineActor;

UCLASS()
class MODUMATE_API UCreateSimilarTool : public UEditModelToolBase
{
	GENERATED_BODY()

public:
	UCreateSimilarTool(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

public:
	virtual EToolMode GetToolMode() override { return EToolMode::VE_CREATESIMILAR; }
	virtual bool Activate() override;
	virtual bool Deactivate() override;

	virtual bool BeginUse() override;
	virtual bool HandleMouseUp() override;
	virtual bool EnterNextStage() override;
	virtual bool FrameUpdate() override;
	virtual bool EndUse() override;
	virtual bool AbortUse() override;

	bool MatchTargetObject(bool bUseMouseHoverObject);
};
