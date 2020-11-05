// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once
#include "ToolsAndAdjustments/Common/EditModelToolBase.h"
#include "ToolsAndAdjustments/Common/ModumateSnappedCursor.h"
#include "BIMKernel/BIMAssemblySpec.h"
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
	virtual bool HasDimensionActor() override { return true; }

protected:
	virtual void OnCreateObjectModeChanged() override;
	virtual void OnAssemblyChanged() override;

	void SetTargetID(int32 NewTargetID);
	bool SetStructureLineHidden(int32 StructureLineID, bool bHidden);
	bool UpdatePreviewStructureLine();
	bool MakeStructureLine(int32 TargetEdgeID = MOD_ID_NONE);
	void ResetState();

	bool bHaveSetUpGeometry;
	bool bWasShowingSnapCursor;
	EMouseMode OriginalMouseMode;
	bool bWantedVerticalSnap;
	int32 LastValidTargetID;
	int32 LastTargetStructureLineID;
	FBIMAssemblySpec ObjAssembly;

	UPROPERTY()
	class ADynamicMeshActor* PendingObjMesh;

	UPROPERTY()
	class AEditModelGameMode_CPP* GameMode;

	FVector LineStartPos, LineEndPos, LineDir, ObjNormal, ObjUp;
};

UCLASS()
class MODUMATE_API UMullionTool : public UStructureLineTool
{
	GENERATED_BODY()

public:
	UMullionTool(const FObjectInitializer& ObjectInitializer);
	virtual EToolMode GetToolMode() override { return EToolMode::VE_MULLION; }
};
