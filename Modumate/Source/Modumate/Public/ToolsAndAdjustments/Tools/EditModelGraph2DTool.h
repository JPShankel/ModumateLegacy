// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "ToolsAndAdjustments/Common/EditModelToolBase.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "UnrealClasses/DynamicMeshActor.h"
#include "Graph/Graph3D.h"
#include "Graph/ModumateGraph.h"

#include "EditModelGraph2DTool.generated.h"

class AEditModelGameMode_CPP;
class AEditModelGameState_CPP;
class ALineActor;

namespace Modumate
{
	class FGraph;
	class FGraph3D;
	class FModumateObjectInstance;
}

UCLASS()
class MODUMATE_API UGraph2DTool : public UEditModelToolBase
{
	GENERATED_BODY()

public:
	UGraph2DTool(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual EToolMode GetToolMode() override { return EToolMode::VE_GRAPH2D; }
	virtual bool Activate() override;
	virtual bool Deactivate() override;
};
