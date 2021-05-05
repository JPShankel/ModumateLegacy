// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "ToolsAndAdjustments/Common/EditModelToolBase.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "UnrealClasses/DynamicMeshActor.h"

#include "EditModelBackgroundImageTool.generated.h"


UCLASS()
class MODUMATE_API UBackgroundImageTool: public UEditModelToolBase
{
	GENERATED_BODY()

public:
	explicit UBackgroundImageTool();

	virtual EToolMode GetToolMode() override { return EToolMode::VE_BACKGROUNDIMAGE; }

	virtual bool Activate() override;
	virtual bool Deactivate() override;
	virtual bool BeginUse() override;
	virtual bool EndUse() override;
	virtual bool AbortUse() override;
	virtual bool FrameUpdate() override;
	virtual bool EnterNextStage() override;

protected:
	UPROPERTY()
	AEditModelGameMode* GameMode = nullptr;
	FVector Origin;
	FVector Normal;
	EMouseMode OriginalMouseMode;
	TWeakObjectPtr<ADynamicMeshActor> PendingImageActor;
	FArchitecturalMaterial PendingPlaneMaterial;

	static constexpr float InitialSize = 100.0f;
};
