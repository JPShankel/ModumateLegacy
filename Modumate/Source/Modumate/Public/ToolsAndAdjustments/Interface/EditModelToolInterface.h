// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine.h"
#include "BIMKernel/Core/BIMKey.h"
#include "Database/ModumateObjectEnums.h"

#include "EditModelToolInterface.generated.h"

class AEditModelPlayerController_CPP;
class AModumateObjectInstance;

UINTERFACE(meta = (CannotImplementInterfaceInBlueprint))
class UEditModelToolInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class IEditModelToolInterface
{
	GENERATED_IINTERFACE_BODY()

public:
	virtual EToolMode GetToolMode() = 0;
	virtual void Initialize() = 0;
	virtual bool Activate() = 0;
	virtual bool Deactivate() = 0;
	virtual bool IsInUse() const = 0;
	virtual bool IsActive() const = 0;
	virtual bool BeginUse() = 0;
	virtual bool EnterNextStage() = 0;
	virtual bool FrameUpdate() = 0;
	virtual bool EndUse() = 0;
	virtual bool AbortUse() = 0;
	virtual bool PostEndOrAbort() = 0;
	virtual bool ScrollToolOption(int32 dir) = 0;
	virtual bool HandleInputNumber(double n) = 0;
	virtual bool HandleInvert() = 0;
	virtual bool HandleFlip(EAxis::Type FlipAxis) = 0;
	virtual bool HandleAdjustJustification(const FVector2D& ViewSpaceDirection) = 0;
	virtual bool HandleControlKey(bool pressed) = 0;
	virtual bool HandleMouseUp() = 0;
	virtual bool ShowSnapCursorAffordances() = 0;
	virtual bool GetRequiredViewModes(TArray<EEditViewModes>& OutViewModes) const = 0;
	virtual void SetCreateObjectMode(EToolCreateObjectMode InCreateObjectMode) = 0;
	virtual EToolCreateObjectMode GetCreateObjectMode() const = 0;
	virtual void SetAssemblyGUID(const FGuid &InAssemblyKey) = 0;
	virtual FGuid GetAssemblyGUID() const = 0;
};
