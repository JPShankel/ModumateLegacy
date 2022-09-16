// Copyright 2022 Modumate, Inc. All Rights Reserved.

#pragma once

#include "ToolsAndAdjustments/Common/EditModelToolBase.h"

#include "EditModelSymbolTool.generated.h"

struct FBIMPresetInstance;

UCLASS()
class MODUMATE_API USymbolTool : public UEditModelToolBase
{
	GENERATED_BODY()

public:
	USymbolTool();

	virtual EToolMode GetToolMode() override { return EToolMode::VE_SYMBOL; }
	virtual bool Activate() override;
	virtual bool Deactivate() override;

	virtual bool FrameUpdate() override;
	virtual bool BeginUse() override;

protected:
	virtual void OnAssemblyChanged() override;

	bool GetObjectCreationDeltas(const FVector& Location, bool bPresetDelta, TArray<FDeltaPtr>& OutDeltas);

	FVector SymbolAnchor = FVector::ZeroVector;
	FGuid SymbolGuid;
};
