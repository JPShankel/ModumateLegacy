// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once
#include "BIMKernel/AssemblySpec/BIMAssemblySpec.h"
#include "ToolsAndAdjustments/Common/EditModelToolBase.h"
#include "ToolsAndAdjustments/Common/ModumateSnappedCursor.h"

#include "EditModelPointHostedTool.generated.h"

UCLASS()
class MODUMATE_API UPointHostedTool : public UEditModelToolBase
{
	GENERATED_BODY()

public:
	UPointHostedTool();

	virtual EToolMode GetToolMode() override { return EToolMode::VE_POINTHOSTED; }
	virtual bool Activate() override;
	virtual bool Deactivate() override;

	virtual bool FrameUpdate() override;
	virtual bool BeginUse() override;

protected:

	void ResetState();
	void SetTargetID(int32 NewTargetID);
	bool GetObjectCreationDeltas(const int32 InTargetVertexID, TArray<FDeltaPtr>& OutDeltaPtrs);

	bool bWasShowingSnapCursor;
	EMouseMode OriginalMouseMode;
	bool bWantedVerticalSnap;
	int32 LastValidTargetID;
	FVector PointHostedPos;
};
