// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once
#include "BIMKernel/AssemblySpec/BIMAssemblySpec.h"
#include "ToolsAndAdjustments/Common/EditModelToolBase.h"

#include "EditModelFFETool.generated.h"

class ACompoundMeshActor;

UCLASS()
class MODUMATE_API UFFETool : public UEditModelToolBase
{
	GENERATED_BODY()

public:
	UFFETool();

	virtual EToolMode GetToolMode() override { return EToolMode::VE_PLACEOBJECT; }
	virtual bool Activate() override;
	virtual bool ScrollToolOption(int32 dir) override;
	virtual bool Deactivate() override;

	virtual bool FrameUpdate() override;
	virtual bool BeginUse() override;

protected:

	void ResetState();
	bool GetObjectCreationDeltas(const int32 InLastParentID, const FVector& InLocation, const FQuat& InRotation, TArray<FDeltaPtr>& OutDeltaPtrs);

	int32 LastParentID = MOD_ID_NONE;
	FVector TargetLocation = FVector::ZeroVector;
	FQuat TargetRotation = FQuat::Identity;
};
