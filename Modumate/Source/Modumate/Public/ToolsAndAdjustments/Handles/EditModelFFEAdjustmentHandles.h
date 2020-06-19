// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include "ToolsAndAdjustments/Common/AdjustmentHandleActor.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "UnrealClasses/DynamicMeshActor.h"
#include "ToolsAndAdjustments/Handles/EditModelPortalAdjustmentHandles.h"

#include "EditModelFFEAdjustmentHandles.generated.h"


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

UCLASS()
class MODUMATE_API AAdjustFFEInvertHandle : public AAdjustmentHandleActor
{
	GENERATED_BODY()

public:
	virtual bool BeginUse() override;
	virtual FVector GetHandlePosition() const override;
	virtual FVector GetHandleDirection() const override;

protected:
	virtual bool GetHandleWidgetStyle(const USlateWidgetStyleAsset*& OutButtonStyle, FVector2D &OutWidgetSize, FVector2D &OutMainButtonOffset) const override;
};
