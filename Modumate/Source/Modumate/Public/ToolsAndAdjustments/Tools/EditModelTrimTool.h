// Copyright 2019 Modumate, Inc. All Rights Reserved.
#pragma once

#include "ToolsAndAdjustments/Common/EditModelToolBase.h"
#include "Objects/Trim.h"

#include "EditModelTrimTool.generated.h"

UCLASS()
class MODUMATE_API UTrimTool : public UEditModelToolBase
{
	GENERATED_BODY()

public:
	UTrimTool();

	virtual EToolMode GetToolMode() override { return EToolMode::VE_TRIM; }
	virtual bool Activate() override;
	virtual bool HandleInputNumber(double n) override;
	virtual bool Deactivate() override;
	virtual bool BeginUse() override;
	virtual bool FrameUpdate() override;

protected:
	bool GetTrimCreationDeltas(int32 InTargetEdgeID, int32& NextID, TArray<FDeltaPtr>& OutDeltas) const;

	int32 TargetEdgeID;
	FVector TargetEdgeStartPos, TargetEdgeEndPos;

	FMOITrimData PendingTrimData;
};
