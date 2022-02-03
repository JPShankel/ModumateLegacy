// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include "ToolsAndAdjustments/Interface/EditModelToolInterface.h"

#include "BIMKernel/Core/BIMKey.h"
#include "Database/ModumateObjectEnums.h"
#include "DocumentManagement/DocumentDelta.h"
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

	UEditModelToolBase();

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
	virtual bool HandleOffset(const FVector2D& ViewSpaceDirection) override { return false; }
	virtual bool HandleControlKey(bool pressed) override { return true; }
	virtual bool HandleMouseUp() override { return true; }
	virtual bool ShowSnapCursorAffordances() override { return true; }
	virtual bool GetRequiredViewModes(TArray<EEditViewModes>& OutViewModes) const override { return false; }

	virtual void SetCreateObjectMode(EToolCreateObjectMode InCreateObjectMode) override;
	virtual EToolCreateObjectMode GetCreateObjectMode() const override { return CreateObjectMode; }
	virtual void SetAssemblyGUID(const FGuid& InAssemblyKey) override;
	virtual FGuid GetAssemblyGUID() const override { return AssemblyGUID; }

	// TODO: potentially duplicate with handles
	virtual bool HasDimensionActor() { return false; }

	virtual bool CycleMode() { return false; }

	virtual void RegisterToolDataUI(class UToolTrayBlockProperties* PropertiesUI, int32& OutMaxNumRegistrations) {}

protected:
	UFUNCTION()
	void OnTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	virtual void InitializeDimension();
	virtual void OnCreateObjectModeChanged();
	virtual void OnAssemblyChanged();
	virtual bool IsObjectInActiveGroup(const AModumateObjectInstance* MOI) const;

	bool InUse;
	bool Active;

	UPROPERTY()
	class AEditModelPlayerController* Controller;

	UPROPERTY()
	class UModumateGameInstance* GameInstance;

	UPROPERTY()
	class AEditModelGameState* GameState;

	UPROPERTY()
	const class AEditModelGameMode* GameMode;

	UPROPERTY()
	class UDimensionManager* DimensionManager;

	EToolCreateObjectMode CreateObjectMode;

	FGuid AssemblyGUID;

	int32 PendingSegmentID;

	// The original template state data for the MOI(s) that this tool creates, either preview for for real.
	FMOIStateData NewMOIStateData;

	// The IDs of objects that this tool references with its creation deltas, preview or not.
	TArray<int32> NewObjectIDs;

	// The current deltas generated by this tool, either for previewing the results, or for immediately applying to the document.
	TArray<FDeltaPtr> CurDeltas;

	UPROPERTY()
	class ADimensionActor* PendingDimensionActor;

	UPROPERTY()
	class ALineActor* PendingSegment;

	// ModumateGreen
	FColor AffordanceLineColor = FColor(3, 196, 168);
	float AffordanceLineThickness = 3.0f;
	float AffordanceLineInterval = 6.0f;
};
