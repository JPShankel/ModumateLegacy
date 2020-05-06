// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "EditModelToolBase.h"
#include "EditModelPlayerState_CPP.h"
#include "DynamicMeshActor.h"

#include "EditModelScopeBoxTool.generated.h"

class AEditModelGameMode_CPP;
class AEditModelGameState_CPP;
class ALineActor;

UCLASS()
class MODUMATE_API UScopeBoxTool : public UEditModelToolBase
{
	GENERATED_BODY()

public:
	UScopeBoxTool(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

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

	TWeakObjectPtr<AEditModelGameMode_CPP> GameMode;
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
