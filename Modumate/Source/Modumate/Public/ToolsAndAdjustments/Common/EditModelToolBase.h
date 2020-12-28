// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include "ToolsAndAdjustments/Interface/EditModelToolInterface.h"

#include "BIMKernel/Core/BIMKey.h"
#include "Database/ModumateObjectEnums.h"
#include "Objects/MOIState.h"

#include "EditModelToolBase.generated.h"

/**
 *
 */

UCLASS()
class MODUMATE_API UEditModelToolBase : public UObject, public IEditModelToolInterface
{
	GENERATED_BODY()

public:

	UEditModelToolBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual EToolMode GetToolMode() override { return EToolMode::VE_NONE; }
	virtual void Initialize() override {};
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
	virtual bool HandleFlip(EAxis::Type FlipAxis) override { return false; }
	virtual bool HandleAdjustJustification(const FVector2D& ViewSpaceDirection) override { return false; }
	virtual bool HandleControlKey(bool pressed) override { return true; }
	virtual bool HandleMouseUp() override { return true; }
	virtual bool ShowSnapCursorAffordances() override { return true; }
	virtual bool GetRequiredViewModes(TArray<EEditViewModes>& OutViewModes) const override { return false; }

	virtual void SetAxisConstraint(EAxisConstraint InAxisConstraint) override;
	virtual EAxisConstraint GetAxisConstraint() const { return AxisConstraint; }
	virtual void SetCreateObjectMode(EToolCreateObjectMode InCreateObjectMode) override;
	virtual EToolCreateObjectMode GetCreateObjectMode() const override { return CreateObjectMode; }
	virtual void SetAssemblyGUID(const FGuid& InAssemblyKey) override;
	virtual FGuid GetAssemblyGUID() const override { return AssemblyGUID; }

	// TODO: potentially duplicate with handles
	virtual bool HasDimensionActor() { return false; }

protected:
	UFUNCTION()
	void OnTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	virtual void InitializeDimension();
	virtual void OnAxisConstraintChanged();
	virtual void OnCreateObjectModeChanged();
	virtual void OnAssemblyChanged();

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

	FGuid AssemblyGUID;

	int32 PendingSegmentID;

	UPROPERTY()
	class ADimensionActor* PendingDimensionActor;

	UPROPERTY()
	class ALineActor* PendingSegment;

	// ModumateBlue
	FColor AffordanceLineColor = FColor(28, 159, 255);
	float AffordanceLineThickness = 3.0f;
	float AffordanceLineInterval = 6.0f;
};
