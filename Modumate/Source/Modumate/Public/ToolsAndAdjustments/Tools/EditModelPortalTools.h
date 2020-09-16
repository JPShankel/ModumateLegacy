// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once
#include "ToolsAndAdjustments/Common/EditModelToolBase.h"

#include "EditModelPortalTools.generated.h"

class APortalFrameActor_CPP;
class AEditModelPlayerController_CPP;
class ACompoundMeshActor;
class FModumateDocument;

UCLASS()
class MODUMATE_API UPortalToolBase : public UEditModelToolBase
{
	GENERATED_BODY()

protected:
	UPROPERTY()
	ACompoundMeshActor * CursorActor;

	FModumateDocument *Document;
	int32 HostID;
	bool bHasBoringError;
	bool bUseFixedOffset;
	FVector WorldPos;
	FVector2D RelativePos;
	FQuat WorldRot, RelativeRot;

	bool Active,Inverted;
	bool bValidPortalConfig;

	void SetupCursor();

public:
	UPortalToolBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

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
	virtual bool HandleControlKey(bool pressed) override;
	virtual bool HandleMouseUp() override { return true; }
	virtual bool ShowSnapCursorAffordances() override { return true; }
	virtual void SetAssemblyKey(const FBIMKey& InAssemblyKey) override;
};

UCLASS()
class MODUMATE_API UDoorTool : public UPortalToolBase
{
	GENERATED_BODY()

public:
	UDoorTool(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get())
		: Super(ObjectInitializer)
	{ }

	virtual EToolMode GetToolMode() override { return EToolMode::VE_DOOR; }
};

UCLASS()
class MODUMATE_API UWindowTool : public UPortalToolBase
{
	GENERATED_BODY()

public:
	UWindowTool(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get())
		: Super(ObjectInitializer)
	{ }

	virtual EToolMode GetToolMode() override { return EToolMode::VE_WINDOW; }
};
