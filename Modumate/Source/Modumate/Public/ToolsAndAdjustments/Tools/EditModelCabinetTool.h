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

	TWeakObjectPtr<ALineActor> PendingSegment;

	TArray<ALineActor*> BaseSegs, TopSegs, ConnectSegs;
	FPlane CabinetPlane = FPlane(ForceInitToZero);

	FVector LastPendingSegmentLoc = FVector::ZeroVector;
	bool LastPendingSegmentLocValid = false;

	void DoMakeLineSegmentCommand(const FVector &P1, const FVector &P2);

public:
	UCabinetTool(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual EToolMode GetToolMode() override { return EToolMode::VE_CABINET; }
	virtual bool Activate() override;
	virtual bool BeginUse() override;
	virtual bool FrameUpdate() override;
	void HandleClick(const FVector &p);
	virtual bool HandleInputNumber(double n) override;
	virtual bool AbortUse() override;
	virtual bool EndUse() override;
	virtual bool EnterNextStage() override;

	void BeginSetHeightMode(const TArray<FVector> &basePoly);
};
