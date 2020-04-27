// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "EditModelToolBase.h"
#include "EditModelPlayerState_CPP.h"
#include "DynamicMeshActor.h"
#include "Graph/Graph3D.h"
#include "Graph/ModumateGraph.h"

#include "EditModelGraph2DTool.generated.h"

class AEditModelGameMode_CPP;
class AEditModelGameState_CPP;
class ALineActor3D_CPP;

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

	static bool GetGraph2DFromObjs(const Modumate::FGraph3D &VolumeGraph, const TArray<Modumate::FModumateObjectInstance*> Objects,
		FPlane &OutPlane, Modumate::FGraph &OutGraph, bool bRequireConnected = true);
};
