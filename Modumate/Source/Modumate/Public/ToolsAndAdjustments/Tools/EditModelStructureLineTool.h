// Copyright 2020 Modumate, Inc. All Rights Reserved.

#pragma once
#include "ToolsAndAdjustments/Common/EditModelToolBase.h"
#include "ToolsAndAdjustments/Common/ModumateSnappedCursor.h"
#include "BIMKernel/BIMAssemblySpec.h"
#include "EditModelStructureLineTool.generated.h"

class AEditModelGameMode_CPP;
class AEditModelGameState_CPP;
class AEditModelPlayerState_CPP;
class ADynamicMeshActor;
class ALineActor;

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
	virtual void SetCreateObjectMode(EToolCreateObjectMode InCreateObjectMode) override;
	virtual void SetAssemblyKey(const FBIMKey& InAssemblyKey) override;

protected:
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
	int32 PendingSegmentID;
	TWeakObjectPtr<ADynamicMeshActor> PendingObjMesh;
	TWeakObjectPtr<AEditModelGameMode_CPP> GameMode;
	TWeakObjectPtr<AEditModelGameState_CPP> GameState;
	FVector LineStartPos, LineEndPos, LineDir, ObjNormal, ObjUp;
};
