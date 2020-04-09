// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/WeakObjectPtr.h"
#include "EditModelToolBase.h"

#include "EditModelRoofTool.generated.h"

class ALineActor3D_CPP;

UCLASS()
class MODUMATE_API URoofTool : public UEditModelToolBase
{
	GENERATED_BODY()

private:
	enum EState
	{
		Neutral = 0,
		NewSegmentPending,
	};

	EState State;
	TWeakObjectPtr<ALineActor3D_CPP> PendingSegment;
	FVector LastPendingSegmentLoc;
	bool bLastPendingSegmentLocValid;
	float DefaultEdgeSlope;

public:
	URoofTool(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual EToolMode GetToolMode() override { return EToolMode::VE_ROOF; }
	virtual bool Activate() override;
	virtual bool BeginUse() override;
	virtual bool FrameUpdate() override;
	void HandleClick(const FVector &p);
	virtual bool HandleInputNumber(double n) override;
	virtual bool AbortUse() override;
	virtual bool EndUse() override;
	virtual bool EnterNextStage() override;
};
