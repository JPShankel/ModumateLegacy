// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine.h"
#include "ModumateObjectEnums.h"

#include "EditModelToolInterface.generated.h"

struct FShoppingItem;
class AEditModelPlayerController_CPP;

namespace Modumate {
	class FModumateObjectInstance;
}


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
	virtual bool Activate() = 0;
	virtual bool Deactivate() = 0;
	virtual bool IsInUse() const = 0;
	virtual bool IsActive() const = 0;
	virtual bool BeginUse() = 0;
	virtual bool EnterNextStage() = 0;
	virtual bool FrameUpdate() = 0;
	virtual bool EndUse() = 0;
	virtual bool AbortUse() = 0;
	virtual bool ScrollToolOption(int32 dir) = 0;
	virtual bool HandleInputNumber(double n) = 0;
	virtual bool HandleSpacebar() = 0;
	virtual bool HandleControlKey(bool pressed) = 0;
	virtual bool HandleMouseUp() = 0;
	virtual bool ShowSnapCursorAffordances() = 0;
	virtual const FShoppingItem &GetAssembly() const = 0;
	virtual void SetAssembly(const FShoppingItem &str) = 0;
	virtual void SetAxisConstraint(EAxisConstraint AxisConstraint) = 0;
	virtual void SetCreateObjectMode(EToolCreateObjectMode InCreateObjectMode) = 0;
};
