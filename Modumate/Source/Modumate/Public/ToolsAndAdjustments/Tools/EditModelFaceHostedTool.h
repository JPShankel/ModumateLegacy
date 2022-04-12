// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once
#include "ToolsAndAdjustments/Common/EditModelToolBase.h"
#include "ToolsAndAdjustments/Common/ModumateSnappedCursor.h"

#include "EditModelFaceHostedTool.generated.h"

UCLASS()
class MODUMATE_API UFaceHostedTool : public UEditModelToolBase
{
	GENERATED_BODY()

public:
	UFaceHostedTool();

	virtual EToolMode GetToolMode() override { return EToolMode::VE_FACEHOSTED; }
	virtual bool Activate() override;
	virtual bool Deactivate() override;

	virtual bool FrameUpdate() override;
	virtual bool BeginUse() override;

protected:

	void ResetState();
	void SetTargetID(int32 NewTargetID);
	bool GetObjectCreationDeltas(const TArray<int32>& InTargetEdgeIDs, TArray<FDeltaPtr>& OutDeltaPtrs, int32 HitFaceHostedID = MOD_ID_NONE);

	bool bWasShowingSnapCursor = false;
	EMouseMode OriginalMouseMode = EMouseMode::Location;
	bool bWantedVerticalSnap = false;
	int32 LastValidTargetID = MOD_ID_NONE;
	int32 LastTargetStructureLineID = MOD_ID_NONE;

};
