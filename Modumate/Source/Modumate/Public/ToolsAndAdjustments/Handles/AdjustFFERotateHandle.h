// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "ToolsAndAdjustments/Common/AdjustmentHandleActor.h"

#include "AdjustFFERotateHandle.generated.h"

UCLASS()
class MODUMATE_API AAdjustFFERotateHandle : public AAdjustmentHandleActor
{
	GENERATED_BODY()

public:
	virtual bool BeginUse() override;
	virtual bool UpdateUse() override;
	virtual FVector GetHandlePosition() const override;
	virtual FVector GetHandleDirection() const override;
	virtual bool HandleInputNumber(float number) override;
	FQuat GetAnchorQuatFromCursor();

protected:
	virtual void Initialize() override;
	virtual bool GetHandleWidgetStyle(const USlateWidgetStyleAsset*& OutButtonStyle, FVector2D &OutWidgetSize, FVector2D &OutMainButtonOffset) const override;

	FVector AssemblyNormal;
	FVector AssemblyTangent;
	FVector LocalHandlePos;

	FVector AnchorDirLocation; // a proxy location that defines the starting direction
	FVector AnchorLoc; // pivot of the rotation handle
	FQuat OriginalRotation;
	bool bClockwise = true;
};
