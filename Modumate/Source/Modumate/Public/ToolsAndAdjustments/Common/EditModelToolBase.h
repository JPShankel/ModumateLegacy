// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include "ToolsAndAdjustments/Interface/EditModelToolInterface.h"
#include "CoreMinimal.h"
#include "BIMKernel/BIMKey.h"
#include "Engine/Engine.h"

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

	UPROPERTY()
	class AEditModelPlayerController_CPP* Controller;

	UPROPERTY()
	class UModumateGameInstance* GameInstance;

	UPROPERTY()
	class AEditModelGameState_CPP* GameState;

	UPROPERTY()
	class UDimensionManager* DimensionManager;

	EAxisConstraint AxisConstraint;
	EToolCreateObjectMode CreateObjectMode;

	FBIMKey AssemblyKey;

	int32 PendingSegmentID;

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
	virtual bool PostEndOrAbort() override;
	virtual bool HandleInvert() override { return true; }
	virtual bool HandleControlKey(bool pressed) override { return true; }
	virtual bool HandleMouseUp() override { return true; }
	virtual bool ShowSnapCursorAffordances() { return true; }

	virtual void SetAxisConstraint(EAxisConstraint InAxisConstraint) override { AxisConstraint = InAxisConstraint; }
	virtual void SetCreateObjectMode(EToolCreateObjectMode InCreateObjectMode) override { CreateObjectMode = InCreateObjectMode; }
	virtual void SetAssemblyKey(const FBIMKey &InAssemblyKey) override { AssemblyKey = InAssemblyKey; }
	virtual FBIMKey GetAssemblyKey() const override { return AssemblyKey; }

	UFUNCTION()
	void OnTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);
};
