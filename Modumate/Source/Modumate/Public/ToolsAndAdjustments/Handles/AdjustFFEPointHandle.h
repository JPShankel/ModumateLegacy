// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "ToolsAndAdjustments/Common/AdjustmentHandleActor.h"

#include "AdjustFFEPointHandle.generated.h"

UCLASS()
class MODUMATE_API AAdjustFFEPointHandle : public AAdjustmentHandleActor
{
	GENERATED_BODY()

public:
	AAdjustFFEPointHandle(const FObjectInitializer &ObjectInitializer = FObjectInitializer::Get());

	virtual bool BeginUse() override;
	virtual bool UpdateUse() override;
	virtual void EndUse() override;
	virtual void AbortUse() override;
	virtual FVector GetHandlePosition() const override;
	virtual bool HandleInputNumber(float number) override;

protected:
	virtual void Initialize() override;
	virtual bool GetHandleWidgetStyle(const USlateWidgetStyleAsset*& OutButtonStyle, FVector2D &OutWidgetSize, FVector2D &OutMainButtonOffset) const override;

	FVector AssemblyNormal;
	FVector AssemblyTangent;
	FVector LocalHandlePos;

	FVector AnchorLoc;
	FVector OriginalObjectLoc;
	FQuat OriginalObjectRot;
	FVector OriginalHandleLoc;
		
	TArray<FVector> LastValidPendingCPLocations;
	FVector MeshExtent;

	bool UpdateTarget();
};
