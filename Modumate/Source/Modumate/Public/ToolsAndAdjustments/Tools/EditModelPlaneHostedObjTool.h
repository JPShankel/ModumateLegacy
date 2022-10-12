// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once
#include "ToolsAndAdjustments/Tools/EditModelRectangleTool.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "BIMKernel/AssemblySpec/BIMAssemblySpec.h"

#include "EditModelPlaneHostedObjTool.generated.h"

class ADynamicMeshActor;
class UModumateDocument;
class AModumateObjectInstance;

UCLASS()
class MODUMATE_API UPlaneHostedObjTool : public URectangleTool
{
	GENERATED_BODY()

public:
	UPlaneHostedObjTool();

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
	virtual bool HandleOffset(const FVector2D& ViewSpaceDirection) override;
	virtual void CommitSpanEdit() override;
	virtual void CancelSpanEdit() override;

protected:
	virtual void OnAssemblyChanged() override;
	virtual void OnCreateObjectModeChanged() override;

	bool GetObjectCreationDeltas(const TArray<int32>& InTargetFaceIDs, int32& NewID, TArray<FDeltaPtr>& OutDeltaPtrs, bool bSplitFaces);

	virtual bool MakeObject(const FVector& Location) override;
	virtual bool ValidatePlaneTarget(const AModumateObjectInstance* PlaneTarget);

	bool IsTargetFacingDown();
	float GetDefaultJustificationValue();
	bool GetAppliedInversionValue();
	void ResetSpanIDs();
	void DrawAffordanceForPlaneMOI(const AModumateObjectInstance* PlaneMOI, float Interval);

	bool bInverted=false;
	bool bRequireHoverMetaPlane=false;
	EObjectType ObjectType;
	int32 LastValidTargetID=0;
	bool bWasShowingSnapCursor=false;
	int32 TargetSpanIndex=0;
	TArray<int32> PreviewSpanGraphMemberIDs;
};

UCLASS()
class MODUMATE_API UWallTool : public UPlaneHostedObjTool
{
	GENERATED_BODY()

public:
	UWallTool();

	virtual EToolMode GetToolMode() override { return EToolMode::VE_WALL; }
};

UCLASS()
class MODUMATE_API UFloorTool : public UPlaneHostedObjTool
{
	GENERATED_BODY()

public:
	UFloorTool();

	virtual EToolMode GetToolMode() override { return EToolMode::VE_FLOOR; }
};

UCLASS()
class MODUMATE_API URoofFaceTool : public UPlaneHostedObjTool
{
	GENERATED_BODY()

public:
	URoofFaceTool();

	virtual EToolMode GetToolMode() override { return EToolMode::VE_ROOF_FACE; }

};

UCLASS()
class MODUMATE_API UCeilingTool : public UPlaneHostedObjTool
{
	GENERATED_BODY()

public:
	UCeilingTool();

	virtual EToolMode GetToolMode() override { return EToolMode::VE_CEILING; }
};

UCLASS()
class MODUMATE_API UCountertopTool : public UPlaneHostedObjTool
{
	GENERATED_BODY()

public:
	UCountertopTool();

	virtual EToolMode GetToolMode() override { return EToolMode::VE_COUNTERTOP; }
};

UCLASS()
class MODUMATE_API UPanelTool : public UPlaneHostedObjTool
{
	GENERATED_BODY()

public:
	UPanelTool();

	virtual EToolMode GetToolMode() override { return EToolMode::VE_PANEL; }
};
