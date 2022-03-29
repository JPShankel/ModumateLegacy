// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once
#include "ToolsAndAdjustments/Common/EditModelToolBase.h"
#include "ToolsAndAdjustments/Common/ModumateSnappedCursor.h"

#include "EditModelEdgeHostedTool.generated.h"

UCLASS()
class MODUMATE_API UEdgeHostedTool : public UEditModelToolBase
{
	GENERATED_BODY()

public:
	UEdgeHostedTool();

	virtual EToolMode GetToolMode() override { return EToolMode::VE_EDGEHOSTED; }
	virtual bool Activate() override;
	virtual bool Deactivate() override;

	virtual bool FrameUpdate() override;
	virtual bool BeginUse() override;
	virtual void CommitSpanEdit() override;
	virtual void CancelSpanEdit() override;

protected:
	virtual void OnCreateObjectModeChanged() override;

	void ResetState();
	void SetTargetID(int32 NewTargetID);
	bool GetObjectCreationDeltas(const int32 InTargetEdgeID, TArray<FDeltaPtr>& OutDeltaPtrs);
	bool GetSpanCreationDelta(TArray<FDeltaPtr>& OutDeltaPtrs);
	void ResetSpanIDs();

	bool bWasShowingSnapCursor = false;
	EMouseMode OriginalMouseMode = EMouseMode::Location;
	bool bWantedVerticalSnap = false;
	int32 LastValidTargetID = MOD_ID_NONE;
	TArray<int32> PreviewSpanGraphMemberIDs;

	FVector LineStartPos, LineEndPos, LineDir, ObjNormal, ObjUp;
};
