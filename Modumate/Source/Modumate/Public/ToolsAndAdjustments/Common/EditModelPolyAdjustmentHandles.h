// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include "ToolsAndAdjustments/Common/AdjustmentHandleActor.h"

#include "EditModelPolyAdjustmentHandles.generated.h"


UCLASS()
class MODUMATE_API AAdjustPolyPointHandle : public AAdjustmentHandleActor
{
	GENERATED_BODY()

public:
	AAdjustPolyPointHandle(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual bool BeginUse() override;
	virtual bool UpdateUse() override;
	virtual FVector GetHandlePosition() const override;
	virtual FVector GetHandleDirection() const override;
	virtual bool HandleInputNumber(float number) override;

	void SetAdjustPolyEdge(bool bInAdjustPolyEdge);

protected:
	virtual bool GetHandleWidgetStyle(const USlateWidgetStyleAsset*& OutButtonStyle, FVector2D &OutWidgetSize, FVector2D &OutMainButtonOffset) const override;

	bool bAdjustPolyEdge;
	FPlane PolyPlane;
	FVector OriginalDirection;
	TArray<FVector> OriginalPolyPoints;
	TArray<FVector> LastValidPolyPoints;
	FVector AnchorLoc;
};

UCLASS()
class MODUMATE_API AAdjustPolyExtrusionHandle : public AAdjustmentHandleActor
{
	GENERATED_BODY()

public:
	virtual bool BeginUse() override;
	virtual bool UpdateUse() override;
	virtual FVector GetHandlePosition() const override;
	virtual FVector GetHandleDirection() const override;
	virtual bool HandleInputNumber(float number) override;

protected:
	virtual bool GetHandleWidgetStyle(const USlateWidgetStyleAsset*& OutButtonStyle, FVector2D &OutWidgetSize, FVector2D &OutMainButtonOffset) const override;

	TArray<FVector> OriginalControlPoints;
	FPlane OriginalPlane;
	float OriginalExtrusion;
	float LastValidExtrusion;
	FVector AnchorLoc;
};

UCLASS()
class MODUMATE_API AAdjustInvertHandle : public AAdjustmentHandleActor
{
	GENERATED_BODY()

public:
	virtual bool BeginUse() override;
	virtual FVector GetHandlePosition() const override;
	virtual FVector GetHandleDirection() const override;

protected:
	virtual bool GetHandleWidgetStyle(const USlateWidgetStyleAsset*& OutButtonStyle, FVector2D &OutWidgetSize, FVector2D &OutMainButtonOffset) const override;

	const static float DesiredWorldDist;
	const static float MaxScreenDist;
};

UCLASS()
class MODUMATE_API AJustificationHandle : public AAdjustmentHandleActor
{
	GENERATED_BODY()

public:
	virtual bool BeginUse() override;
	virtual void Tick(float DeltaTime) override;
	virtual FVector GetHandlePosition() const override;
	virtual FVector GetHandleDirection() const override;

	void SetJustification(float InJustificationValue);

protected:
	virtual void SetEnabled(bool bNewEnabled) override;
	virtual bool GetHandleWidgetStyle(const USlateWidgetStyleAsset*& OutButtonStyle, FVector2D &OutWidgetSize, FVector2D &OutMainButtonOffset) const override;

	const static float DesiredWorldDist;
	const static float MaxScreenDist;

	float JustificationValue = 0.f;
	bool bRootExpanded = false;
};
