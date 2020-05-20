// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/WeakObjectPtr.h"
#include "ToolsAndAdjustments/Tools/EditModelPlaneHostedObjTool.h"

#include "EditModelRoofFaceTool.generated.h"

class ALineActor;

UCLASS()
class MODUMATE_API URoofFaceTool : public UPlaneHostedObjTool
{
	GENERATED_BODY()

public:
	URoofFaceTool(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual EToolMode GetToolMode() override { return EToolMode::VE_ROOF_FACE; }

};
