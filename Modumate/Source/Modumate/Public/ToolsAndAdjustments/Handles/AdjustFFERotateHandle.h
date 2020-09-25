// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "ToolsAndAdjustments/Common/AdjustmentHandleActor.h"

#include "AdjustFFERotateHandle.generated.h"

class ALineActor;

UCLASS()
class MODUMATE_API AAdjustFFERotateHandle : public AAdjustmentHandleActor
{
	GENERATED_BODY()

public:
	AAdjustFFERotateHandle(const FObjectInitializer &ObjectInitializer = FObjectInitializer::Get());

	virtual bool BeginUse() override;
	virtual bool UpdateUse() override;
	virtual void PostEndOrAbort() override;
	virtual FVector GetHandlePosition() const override;
	virtual FVector GetHandleDirection() const override;
	virtual bool HandleInputNumber(float number) override;
	FQuat GetAnchorQuatFromCursor();

	virtual bool HasDimensionActor() override;
	virtual bool HasDistanceTextInput() override;

protected:
	virtual void Initialize() override;
	virtual bool GetHandleWidgetStyle(const USlateWidgetStyleAsset*& OutButtonStyle, FVector2D &OutWidgetSize, FVector2D &OutMainButtonOffset) const override;

	bool GetTransform(FTransform &OutTransform);

	FVector AssemblyNormal;
	FVector AssemblyTangent;
	FVector LocalHandlePos;

	FVector AnchorDirLocation; // a proxy location that defines the starting direction
	FVector AnchorLoc; // pivot of the rotation handle
	FQuat OriginalRotation;
	bool bClockwise = true;

	// The tool's pending state is represented by a rotation from the base line to the rotation line
	UPROPERTY()
	ALineActor* BaseLine;

	UPROPERTY()
	ALineActor* RotationLine;
};
