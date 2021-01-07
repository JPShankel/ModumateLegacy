// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once
#include "ToolsAndAdjustments/Tools/EditModelMetaPlaneTool.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "BIMKernel/Core/BIMKey.h"
#include "BIMKernel/AssemblySpec/BIMAssemblySpec.h"

#include "EditModelPlaneHostedObjTool.generated.h"

class ADynamicMeshActor;
class UModumateDocument;
class AModumateObjectInstance;

UCLASS()
class MODUMATE_API UPlaneHostedObjTool : public UMetaPlaneTool
{
	GENERATED_BODY()

public:

	UPlaneHostedObjTool(const FObjectInitializer& ObjectInitializer);

	virtual bool Activate() override;
	virtual bool HandleInputNumber(double n) override;
	virtual bool Deactivate() override;
	virtual bool BeginUse() override;
	virtual bool EnterNextStage() override;
	virtual bool FrameUpdate() override;
	virtual bool EndUse() override;
	virtual bool AbortUse() override;
	virtual bool HandleInvert() override;
	virtual bool HandleFlip(EAxis::Type FlipAxis) override;
	virtual bool HandleAdjustJustification(const FVector2D& ViewSpaceDirection) override;

	void SetInstanceJustification(const float InJustification);
	float GetInstanceJustification() const;

protected:
	virtual void OnAssemblyChanged() override;

	FDeltaPtr GetObjectCreationDelta(const TArray<int32>& TargetFaceIDs);

	virtual bool MakeObject(const FVector& Location) override;
	virtual bool ValidatePlaneTarget(const AModumateObjectInstance* PlaneTarget);

	bool IsTargetFacingDown();
	float GetDefaultJustificationValue();
	bool GetAppliedInversionValue();

	bool bInverted;
	bool bRequireHoverMetaPlane;
	EObjectType ObjectType;
	int32 LastValidTargetID;
	bool bWasShowingSnapCursor;

	float InstanceJustification;
};

UCLASS()
class MODUMATE_API UWallTool : public UPlaneHostedObjTool
{
	GENERATED_BODY()

public:
	UWallTool(const FObjectInitializer& ObjectInitializer);

	virtual EToolMode GetToolMode() override { return EToolMode::VE_WALL; }
};

UCLASS()
class MODUMATE_API UFloorTool : public UPlaneHostedObjTool
{
	GENERATED_BODY()

public:
	UFloorTool(const FObjectInitializer& ObjectInitializer);

	virtual EToolMode GetToolMode() override { return EToolMode::VE_FLOOR; }
};

UCLASS()
class MODUMATE_API URoofFaceTool : public UPlaneHostedObjTool
{
	GENERATED_BODY()

public:
	URoofFaceTool(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual EToolMode GetToolMode() override { return EToolMode::VE_ROOF_FACE; }

};

UCLASS()
class MODUMATE_API UCeilingTool : public UPlaneHostedObjTool
{
	GENERATED_BODY()

public:
	UCeilingTool(const FObjectInitializer& ObjectInitializer);

	virtual EToolMode GetToolMode() override { return EToolMode::VE_CEILING; }
};

UCLASS()
class MODUMATE_API UCountertopTool : public UPlaneHostedObjTool
{
	GENERATED_BODY()

public:
	UCountertopTool(const FObjectInitializer& ObjectInitializer);

	virtual EToolMode GetToolMode() override { return EToolMode::VE_COUNTERTOP; }
};

UCLASS()
class MODUMATE_API UPanelTool : public UPlaneHostedObjTool
{
	GENERATED_BODY()

public:
	UPanelTool(const FObjectInitializer& ObjectInitializer);

	virtual EToolMode GetToolMode() override { return EToolMode::VE_PANEL; }
};
