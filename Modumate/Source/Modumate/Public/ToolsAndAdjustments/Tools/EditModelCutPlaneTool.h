// Copyright 2019 Modumate, Inc. All Rights Reserved.
#pragma once

#include "ToolsAndAdjustments/Common/EditModelToolBase.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "UnrealClasses/DynamicMeshActor.h"

#include "EditModelCutPlaneTool.generated.h"

class AEditModelGameMode;
class AEditModelGameState;
class ALineActor;

UCLASS()
class MODUMATE_API UCutPlaneTool : public UEditModelToolBase
{
	GENERATED_BODY()
public:
	UCutPlaneTool();

	virtual EToolMode GetToolMode() override { return EToolMode::VE_CUTPLANE; }
	virtual bool Activate() override;
	virtual bool Deactivate() override;
	virtual bool BeginUse() override;
	virtual bool FrameUpdate() override;

	virtual bool EnterNextStage() override;

	virtual bool EndUse() override;
	virtual bool AbortUse() override;

protected:
	TWeakObjectPtr<AEditModelGameMode> GameMode;

	TWeakObjectPtr<ALineActor> PendingSegment;
	TWeakObjectPtr<ADynamicMeshActor> PendingPlane;
	TArray<FVector> PendingPlanePoints;
	FArchitecturalMaterial PendingPlaneMaterial;

	FVector Origin;
	FVector Normal;
	EMouseMode OriginalMouseMode;

	FString GetNextName() const;

	float DefaultPlaneDimension = 100.0f;
	int32 RecentCreatedCutPlaneID = MOD_ID_NONE;
};
