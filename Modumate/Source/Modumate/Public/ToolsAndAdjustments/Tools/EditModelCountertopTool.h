// Copyright 2019 Modumate, Inc. All Rights Reserved.
#pragma once
#include "ToolsAndAdjustments/Common/EditModelToolBase.h"

#include "EditModelCountertopTool.generated.h"

class ALineActor;

UCLASS()
class MODUMATE_API UCountertopTool : public UEditModelToolBase
{
	GENERATED_BODY()

private:
	enum EState
	{
		Neutral = 0,
		NewSegmentPending,
	};
	EState State;

	int32 PendingSegmentID;
	bool Inverted = true;
	FVector AnchorPointDegree;

	TArray<ALineActor*> BaseSegs;
	TArray<FVector> BasePoints;

public:
	UCountertopTool(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual EToolMode GetToolMode() override { return EToolMode::VE_COUNTERTOP; }
	virtual bool Activate() override;
	virtual bool HandleInputNumber(double n) override;
	virtual bool Deactivate() override;
	virtual bool BeginUse() override;
	virtual bool EnterNextStage() override;
	bool MakeSegment(const FVector &hitLoc);
	virtual bool FrameUpdate() override;
	virtual bool EndUse() override;
	virtual bool AbortUse() override;
	virtual void GetSnappingPointsAndLines(TArray<FVector> &OutPoints, TArray<TPair<FVector, FVector>> &OutLines) override;
	virtual bool HandleInvert() override;
};
