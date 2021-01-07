// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once

#include "DocumentManagement/DocumentDelta.h"
#include "ToolsAndAdjustments/Common/EditModelToolBase.h"
#include "ToolsAndAdjustments/Common/ModumateSnappedCursor.h"
#include "BIMKernel/AssemblySpec/BIMAssemblySpec.h"
#include "EditModelStructureLineTool.generated.h"

UCLASS()
class MODUMATE_API UStructureLineTool : public UEditModelToolBase
{
	GENERATED_BODY()

public:
	UStructureLineTool(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual EToolMode GetToolMode() override { return EToolMode::VE_STRUCTURELINE; }
	virtual bool Activate() override;
	virtual bool HandleInputNumber(double n) override;
	virtual bool Deactivate() override;
	virtual bool BeginUse() override;
	virtual bool EnterNextStage() override;
	virtual bool FrameUpdate() override;
	virtual bool EndUse() override;
	virtual bool AbortUse() override;
	virtual bool HandleFlip(EAxis::Type FlipAxis) override;
	virtual bool HandleAdjustJustification(const FVector2D& ViewSpaceDirection) override;
	virtual bool HasDimensionActor() override { return true; }

protected:
	virtual void OnCreateObjectModeChanged() override;

	void SetTargetID(int32 NewTargetID);
	bool SetStructureLineHidden(int32 StructureLineID, bool bHidden);
	bool MakeStructureLine(int32 TargetEdgeID = MOD_ID_NONE);
	void ResetState();

	bool GetObjectCreationDeltas(const TArray<int32>& InTargetEdgeIDs, TArray<FDeltaPtr>& OutDeltaPtrs, bool bSplitFaces);

	bool bWasShowingSnapCursor;
	EMouseMode OriginalMouseMode;
	bool bWantedVerticalSnap;
	int32 LastValidTargetID;
	int32 LastTargetStructureLineID;

	FVector LineStartPos, LineEndPos, LineDir, ObjNormal, ObjUp;
};

UCLASS()
class MODUMATE_API UMullionTool : public UStructureLineTool
{
	GENERATED_BODY()

public:
	virtual EToolMode GetToolMode() override { return EToolMode::VE_MULLION; }
};
