// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/WeakObjectPtr.h"
#include "Tools/EditModelPlaneHostedObjTool.h"

#include "EditModelRoofFaceTool.generated.h"

class ALineActor;

UCLASS()
class MODUMATE_API URoofFaceTool : public UPlaneHostedObjTool
{
	GENERATED_BODY()

#if 0
private:
	enum EState
	{
		Neutral = 0,
		NewSegmentPending,
	};

	EState State;
	TWeakObjectPtr<ALineActor> PendingSegment;
	FVector LastPendingSegmentLoc;
	bool bLastPendingSegmentLocValid;
	float DefaultEdgeSlope;
#endif

public:
	URoofFaceTool(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual EToolMode GetToolMode() override { return EToolMode::VE_ROOF_FACE; }

#if 0
	virtual bool Activate() override;
	virtual bool BeginUse() override;
	virtual bool FrameUpdate() override;
	void HandleClick(const FVector &p);
	virtual bool HandleInputNumber(double n) override;
	virtual bool AbortUse() override;
	virtual bool EndUse() override;
	virtual bool EnterNextStage() override;
#endif
};
