// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once
#include "ToolsAndAdjustments/Common/EditModelToolBase.h"
#include "Graph/Graph2D.h"

#include "EditModelRoofPerimeterTool.generated.h"

UCLASS()
class MODUMATE_API URoofPerimeterTool : public UEditModelToolBase
{
	GENERATED_BODY()

public:
	URoofPerimeterTool();

	virtual EToolMode GetToolMode() override { return EToolMode::VE_ROOF_PERIMETER; }
	virtual bool Activate() override;
	virtual bool Deactivate() override;

protected:
	TSharedPtr<FGraph2D> SelectedGraph;
};
