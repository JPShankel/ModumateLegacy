// Copyright 2021 Modumate, Inc. All Rights Reserved.
#pragma once

#include "ToolsAndAdjustments/Common/EditModelToolBase.h"

#include "DocumentManagement/DocumentDelta.h"
#include "ToolsAndAdjustments/Common/ModumateSnappedCursor.h"

#include "EditModelLineTool.generated.h"

class AEditModelGameMode;
class AEditModelGameState;

UCLASS()
class MODUMATE_API ULineTool : public UEditModelToolBase
{
	GENERATED_BODY()

protected:
	enum EState
	{
		Neutral = 0,
		NewSegmentPending,
	};
	EState State;

	TWeakObjectPtr<AEditModelGameMode> GameMode;
	EMouseMode OriginalMouseMode;

public:
	ULineTool();

	virtual EToolMode GetToolMode() override { return EToolMode::VE_LINE; }
	virtual void Initialize() override;
	virtual bool Activate() override;
	virtual bool HandleInputNumber(double n) override;
	virtual bool Deactivate() override;
	virtual bool BeginUse() override;
	virtual bool EnterNextStage() override;
	virtual bool FrameUpdate() override;
	virtual bool EndUse() override;
	virtual bool AbortUse() override;
	virtual bool PostEndOrAbort() override;

	virtual bool HasDimensionActor() { return true; }

protected:
	bool MakeObject(const FVector& Location);
	bool GetEdgeDeltas(const FVector& StartPosition, const FVector &EndPosition, bool bIsPreview);
};
