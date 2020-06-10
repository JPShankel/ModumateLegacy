// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include "ToolsAndAdjustments/Interface/EditModelToolInterface.h"
#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "Database/ModumateShoppingItem.h"

#include "EditModelToolBase.generated.h"

/**
 *
 */

UCLASS()
class MODUMATE_API UEditModelToolBase : public UObject, public IEditModelToolInterface
{
	GENERATED_BODY()

protected:
	bool InUse;
	bool Active;
	FShoppingItem Assembly;

	UPROPERTY()
	class AEditModelPlayerController_CPP* Controller;

	UPROPERTY()
	class UModumateGameInstance *GameInstance;

	EAxisConstraint AxisConstraint;
	EToolCreateObjectMode CreateObjectMode;

public:

	UEditModelToolBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual EToolMode GetToolMode() override { return EToolMode::VE_NONE; }
	virtual bool Activate() override;
	virtual bool HandleInputNumber(double n) override;
	virtual bool Deactivate() override;
	virtual bool IsInUse() const override { return InUse; }
	virtual bool IsActive() const override { return Active; }
	virtual bool BeginUse() override;
	virtual bool EnterNextStage() override;
	virtual bool ScrollToolOption(int32 dir) override;
	virtual bool FrameUpdate() override;
	virtual bool EndUse() override;
	virtual bool AbortUse() override;
	virtual bool HandleInvert() override { return true; }
	virtual bool HandleControlKey(bool pressed) override { return true; }
	virtual bool HandleMouseUp() override { return true; }
	virtual bool ShowSnapCursorAffordances() { return true; }

	virtual const FShoppingItem &GetAssembly() const override { return Assembly; }
	virtual void SetAssembly(const FShoppingItem &key) override { Assembly = key; }
	virtual void SetAxisConstraint(EAxisConstraint InAxisConstraint) override { AxisConstraint = InAxisConstraint; }
	virtual void SetCreateObjectMode(EToolCreateObjectMode InCreateObjectMode) override { CreateObjectMode = InCreateObjectMode; }
};
