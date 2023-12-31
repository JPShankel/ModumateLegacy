// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolsAndAdjustments/Common/EditModelToolBase.h"
#include "DocumentManagement/DocumentDelta.h"
#include "DocumentManagement/ModumateSerialization.h"

#include "EditModelSelectTool.generated.h"

UCLASS()
class MODUMATE_API USelectTool : public UEditModelToolBase
{
	GENERATED_BODY()

public:
	USelectTool();

	virtual EToolMode GetToolMode() override { return EToolMode::VE_SELECT; }
	virtual bool Activate() override;
	virtual bool Deactivate() override;
	virtual bool BeginUse() override;
	virtual bool HandleMouseUp() override;
	virtual bool FrameUpdate() override;
	virtual bool EndUse() override;
	virtual bool ShowSnapCursorAffordances() override { return false; }
	virtual bool HandleInvert() override;
	virtual bool HandleFlip(EAxis::Type FlipAxis) override;
	virtual bool HandleOffset(const FVector2D& ViewSpaceDirection) override;

	bool ProcessDragSelect();

	virtual void RegisterToolDataUI(class UToolTrayBlockProperties* PropertiesUI, int32& OutMaxNumRegistrations) override;

protected:
	TMap<const AModumateObjectInstance*, float> LastObjectSelectionAttemptTimes;
	AModumateObjectInstance *InitialClickedObject;
	FVector2D InitialClickLocation;
	bool Dragging;

	float DoubleClickTime = 0.2f;
	float MinDragDist = 5.0f;
};
