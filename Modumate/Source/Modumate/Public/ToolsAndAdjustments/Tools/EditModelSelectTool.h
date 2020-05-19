// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ToolsAndAdjustments/Common/EditModelToolBase.h"
#include "DocumentManagement/ModumateSerialization.h"

#include "EditModelSelectTool.generated.h"

class MODUMATE_API FSelectedObjectToolMixin
{
protected:

	TMap<Modumate::FModumateObjectInstance*, FMOIDataRecordV1> OriginalObjectData;
	TWeakObjectPtr<class AEditModelPlayerController_CPP> ControllerPtr;

	FSelectedObjectToolMixin(class AEditModelPlayerController_CPP *InController = nullptr);

	void AcquireSelectedObjects();
	void RestoreSelectedObjects();
	void ReleaseSelectedObjects();

	static const FName StateRequestTag;
};

UCLASS()
class MODUMATE_API USelectTool : public UEditModelToolBase
{
	GENERATED_BODY()

public:
	USelectTool(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual EToolMode GetToolMode() override { return EToolMode::VE_SELECT; }
	virtual bool Activate() override;
	virtual bool Deactivate() override;
	virtual bool BeginUse() override;
	virtual bool HandleMouseUp() override;
	virtual bool FrameUpdate() override;
	virtual bool EndUse() override;
	virtual bool ShowSnapCursorAffordances() override { return false; }
	virtual bool HandleSpacebar() override;

	bool ProcessDragSelect();

	float DoubleClickTime = 0.2f;
	float MinDragDist = 1.0f;

protected:
	TMap<const Modumate::FModumateObjectInstance*, float> LastObjectSelectionAttemptTimes;
	Modumate::FModumateObjectInstance *InitialClickedObject;
	FVector2D InitialClickLocation;
	bool Dragging;
};
