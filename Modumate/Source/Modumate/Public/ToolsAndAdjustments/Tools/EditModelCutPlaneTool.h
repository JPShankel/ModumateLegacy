// Copyright 2019 Modumate, Inc. All Rights Reserved.
#pragma once

#include "ToolsAndAdjustments/Common/EditModelToolBase.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "UnrealClasses/DynamicMeshActor.h"

#include "EditModelCutPlaneTool.generated.h"

class AEditModelGameMode_CPP;
class AEditModelGameState_CPP;
class ALineActor;

UCLASS()
class MODUMATE_API UCutPlaneTool : public UEditModelToolBase
{
	GENERATED_BODY()
public:
	UCutPlaneTool(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual EToolMode GetToolMode() override { return EToolMode::VE_CUTPLANE; }
	virtual bool Activate() override;
	virtual bool Deactivate() override;
	virtual bool BeginUse() override;
	virtual bool FrameUpdate() override;

	virtual bool EnterNextStage() override;

	virtual bool EndUse() override;
	virtual bool AbortUse() override;

protected:
	TWeakObjectPtr<AEditModelGameMode_CPP> GameMode;

	TWeakObjectPtr<ALineActor> PendingSegment;
	TWeakObjectPtr<ADynamicMeshActor> PendingPlane;
	TArray<FVector> PendingPlanePoints;
	FArchitecturalMaterial PendingPlaneMaterial;

	FVector Origin;
	FVector Normal;
	EMouseMode OriginalMouseMode;

	float DefaultPlaneDimension = 100.0f;
};
