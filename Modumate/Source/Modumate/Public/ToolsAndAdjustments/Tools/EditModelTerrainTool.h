// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "ToolsAndAdjustments/Common/EditModelToolBase.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "UnrealClasses/DynamicMeshActor.h"

#include "EditModelTerrainTool.generated.h"

UCLASS()
class MODUMATE_API UTerrainTool : public UEditModelToolBase
{
	GENERATED_BODY()

public:
	UTerrainTool();

	virtual EToolMode GetToolMode() override { return EToolMode::VE_TERRAIN; }

	virtual bool Activate() override;
	virtual bool Deactivate() override;
 	virtual bool BeginUse() override;
 	virtual bool EndUse() override;
 	virtual bool AbortUse() override;
 	virtual bool FrameUpdate() override;
 	virtual bool EnterNextStage() override;
	virtual bool PostEndOrAbort() override;

	virtual bool HasDimensionActor() override { return true; }

protected:
	bool GetDeltas(const FVector& CurrentPoint, bool bClosed, TArray<FDeltaPtr>& OutDeltas);
	bool MakeObject();

	enum EState { Idle, FirstPoint, MultiplePoints };
	EState State = Idle;

	EMouseMode OriginalMouseMode;
	TArray<FVector> Points;
	float ZHeight = 0.0f;

	static constexpr float CloseLoopEpsilon = 15.0f;
};
