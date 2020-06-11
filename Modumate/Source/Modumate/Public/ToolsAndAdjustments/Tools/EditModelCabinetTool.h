// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/WeakObjectPtr.h"
#include "ToolsAndAdjustments/Common/EditModelToolBase.h"

#include "EditModelCabinetTool.generated.h"

class ALineActor;

UCLASS()
class MODUMATE_API UCabinetTool : public UEditModelToolBase
{
	GENERATED_BODY()

private:
	enum EState
	{
		Neutral = 0,
		NewSegmentPending,
		SetHeight
	};

	EState State;

	int32 PendingSegmentID;

	TArray<ALineActor*> BaseSegs, TopSegs, ConnectSegs;
	TArray<FVector> BasePoints;
	FPlane CabinetPlane = FPlane(ForceInitToZero);

	FVector LastPendingSegmentLoc = FVector::ZeroVector;
	bool LastPendingSegmentLocValid = false;

public:
	UCabinetTool(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual EToolMode GetToolMode() override { return EToolMode::VE_CABINET; }
	virtual bool Activate() override;
	virtual bool BeginUse() override;
	virtual bool FrameUpdate() override;
	virtual bool HandleInputNumber(double n) override;
	virtual bool AbortUse() override;
	virtual bool EndUse() override;
	virtual bool EnterNextStage() override;

	virtual void GetSnappingPointsAndLines(TArray<FVector> &OutPoints, TArray<TPair<FVector, FVector>> &OutLines) override;

	void MakeSegment(const FVector &hitLoc);
	void BeginSetHeightMode(const TArray<FVector> &basePoly);
};
