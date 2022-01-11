// Copyright 2022 Modumate, Inc. All Rights Reserved.
#pragma once

#include "ToolsAndAdjustments/Common/EditModelToolBase.h"
#include "UnrealClasses/EditModelPlayerState.h"

#include "EditModelUngroupTool.generated.h"

UCLASS()
class MODUMATE_API UUngroupTool : public UEditModelToolBase
{
	GENERATED_BODY()

public:
	UUngroupTool();
	virtual EToolMode GetToolMode() override { return EToolMode::VE_UNGROUP; }

	virtual bool Activate() override;
private:
	bool ExplodeGroup(UModumateDocument* Doc, AModumateObjectInstance* GroupObject, int32& NextID, TArray<FDeltaPtr>& OutDeltas);

	TSet<int32> ReselectionItems;
};
