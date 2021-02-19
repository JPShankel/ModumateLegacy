// Copyright 2021 Modumate, Inc. All Rights Reserved.
#pragma once

#include "ToolsAndAdjustments/Common/EditModelToolBase.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "UnrealClasses/DynamicMeshActor.h"

#include "EditModelCopyTool.generated.h"

class AEditModelGameMode;
class AEditModelGameState;
class ALineActor;

UCLASS()
class MODUMATE_API UCopyTool : public UEditModelToolBase
{
	GENERATED_BODY()

public:
	UCopyTool(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

public:
	virtual EToolMode GetToolMode() override { return EToolMode::VE_COPY; }

	virtual bool Activate() override;

};
