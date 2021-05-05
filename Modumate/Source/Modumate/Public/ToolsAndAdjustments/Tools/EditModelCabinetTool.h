// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once
#include "ToolsAndAdjustments/Common/EditModelToolBase.h"
#include "ToolsAndAdjustments/Common/ModumateSnappedCursor.h"

#include "EditModelCabinetTool.generated.h"

class ALineActor;

UCLASS()
class MODUMATE_API UCabinetTool : public UEditModelToolBase
{
	GENERATED_BODY()

public:
	UCabinetTool();

	virtual EToolMode GetToolMode() override { return EToolMode::VE_CABINET; }
	virtual bool Activate() override;
	virtual bool HandleInputNumber(double n) override;
	virtual bool Deactivate() override;
	virtual bool BeginUse() override;
	virtual bool FrameUpdate() override;
	virtual bool AbortUse() override;
	virtual bool EndUse() override;
	virtual bool EnterNextStage() override;

	virtual bool HasDimensionActor() { return true; }

protected:
	int32 TargetPolygonID;
	TArray<FVector> BasePoints;
	FVector BaseOrigin, BaseNormal;
	float ExtrusionDist;
	EMouseMode PrevMouseMode;

	FColor ExtrusionLineColor = FColor::Black;
	float ExtrusionLineThickness = 3.0f;
	
	float MinimumExtrusionDist = 1.0f;
};
