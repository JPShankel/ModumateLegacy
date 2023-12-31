// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "ToolsAndAdjustments/Common/EditModelToolBase.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "UnrealClasses/DynamicMeshActor.h"
#include "Graph/Graph2D.h"

#include "EditModelGraph2DTool.generated.h"

class AEditModelGameMode;
class AEditModelGameState;
class ALineActor;

UCLASS()
class MODUMATE_API UGraph2DTool : public UEditModelToolBase
{
	GENERATED_BODY()

public:
	UGraph2DTool();

	virtual EToolMode GetToolMode() override { return EToolMode::VE_GRAPH2D; }
	virtual bool Activate() override;
	virtual bool Deactivate() override;

protected:
	TSharedPtr<FGraph2D> SelectedGraph;
};
