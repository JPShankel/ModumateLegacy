// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include "ToolsAndAdjustments/Common/AdjustmentHandleActor.h"

#include "EditModelPortalAdjustmentHandles.generated.h"


UCLASS()
class MODUMATE_API AAdjustPortalInvertHandle : public AAdjustmentHandleActor
{
	GENERATED_BODY()

public:
	virtual bool BeginUse() override;
	virtual FVector GetHandlePosition() const override;
	virtual FVector GetHandleDirection() const override;
	void SetTransvert(bool bInShouldTransvert);

protected:
	virtual bool GetHandleWidgetStyle(const USlateWidgetStyleAsset*& OutButtonStyle, FVector2D &OutWidgetSize, FVector2D &OutMainButtonOffset) const override;
	bool GetBottomCorners(FVector &OutCorner0, FVector &OutCorner1) const;

	bool bShouldTransvert = false;
};

UCLASS()
class MODUMATE_API AAdjustPortalPointHandle : public AAdjustmentHandleActor
{
	GENERATED_BODY()

public:
	virtual bool BeginUse() override;
	virtual bool UpdateUse() override;
	virtual void Tick(float DeltaTime) override;
	virtual void EndUse() override;
	virtual FVector GetHandlePosition() const override;
	virtual bool HandleInputNumber(float number) override;

protected:
	virtual bool GetHandleWidgetStyle(const USlateWidgetStyleAsset*& OutButtonStyle, FVector2D &OutWidgetSize, FVector2D &OutMainButtonOffset) const override;

	TArray<int32> CP;
	TArray<FVector> OriginalP;
	FVector AnchorLoc;
	FVector OrginalLoc;
	FVector OriginalLocHandle;
	TArray<FVector> LastValidPendingCPLocations;
};
