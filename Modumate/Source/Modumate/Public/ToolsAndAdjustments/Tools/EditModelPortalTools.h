// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once

#include "DocumentManagement/DocumentDelta.h"
#include "ToolsAndAdjustments/Common/EditModelToolBase.h"

#include "EditModelPortalTools.generated.h"

class APortalFrameActor;
class AEditModelPlayerController;
class ACompoundMeshActor;
class UModumateDocument;
struct FBIMAssemblySpec;

UCLASS()
class MODUMATE_API UPortalToolBase : public UEditModelToolBase
{
	GENERATED_BODY()

public:
	UPortalToolBase();

	virtual bool Activate() override;
	virtual bool Deactivate() override;
	virtual bool IsInUse() const override { return false; }
	virtual bool IsActive() const override { return Active; }
	virtual bool BeginUse() override;
	virtual bool FrameUpdate() override;
	virtual bool EnterNextStage() override;
	virtual bool EndUse() override;
	virtual bool ScrollToolOption(int32 dir) override;
	virtual bool HandleInputNumber(double n) override;
	virtual bool AbortUse() override;
	virtual bool HandleInvert() override;
	virtual bool HandleFlip(EAxis::Type FlipAxis) override;
	virtual bool HandleControlKey(bool pressed) override;
	virtual bool HandleMouseUp() override { return true; }
	virtual bool ShowSnapCursorAffordances() override { return true; }
	virtual bool HandleOffset(const FVector2D& ViewSpaceDirection) override;

	void SetInstanceWidth(const float InWidth);
	void SetInstanceHeight(const float InHeight);
	void SetInstanceBottomOffset(const float InBottomOffset);
	float GetInstanceWidth() const;
	float GetInstanceHeight() const;
	float GetInstanceBottomOffset() const;

protected:
	virtual void OnAssemblyChanged() override;

	bool GetPortalCreationDeltas(TArray<FDeltaPtr>& OutDeltas);
	bool CalculateNativeSize();

	int32 CurTargetPlaneID;
	bool bUseBottomOffset;
	FVector WorldPos;
	FVector2D RelativePos;
	FQuat WorldRot, RelativeRot;

	bool Active, Inverted;
	bool bValidPortalConfig;
	TArray<FDeltaPtr> Deltas;

	FVector InstanceStampSize;
	float InstanceBottomOffset;
	bool bWasShowingSnapCursor;
	int32 TargetSpanIndex = 0;
};

UCLASS()
class MODUMATE_API UDoorTool : public UPortalToolBase
{
	GENERATED_BODY()

public:
	UDoorTool();

	virtual EToolMode GetToolMode() override { return EToolMode::VE_DOOR; }
};

UCLASS()
class MODUMATE_API UWindowTool : public UPortalToolBase
{
	GENERATED_BODY()

public:
	UWindowTool();

	virtual EToolMode GetToolMode() override { return EToolMode::VE_WINDOW; }
};
