// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "ToolsAndAdjustments/Common/EditModelToolBase.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "UnrealClasses/DynamicMeshActor.h"

#include "EditModelScopeBoxTool.generated.h"

class ALineActor;

UCLASS()
class MODUMATE_API UScopeBoxTool : public UEditModelToolBase
{
	GENERATED_BODY()

public:
	UScopeBoxTool();

	virtual EToolMode GetToolMode() override { return EToolMode::VE_SCOPEBOX; }
	virtual bool Activate() override;
	virtual bool Deactivate() override;
	virtual bool BeginUse() override;
	virtual bool FrameUpdate() override;

	virtual bool EnterNextStage() override;

	virtual bool EndUse() override;
	virtual bool AbortUse() override;

protected:
	enum EState
	{
		SeekingCutPlane,
		BasePending,
		NormalPending
	};

	bool ResetState();

	TWeakObjectPtr<ADynamicMeshActor> PendingBox;
	TWeakObjectPtr<ALineActor> PendingSegment;
	FArchitecturalMaterial PendingBoxMaterial;

	TArray<FVector> PendingBoxBasePoints;

	FVector Origin;
	FVector Normal;
	float Extrusion;
	EMouseMode OriginalMouseMode;

	EState CurrentState;
};
