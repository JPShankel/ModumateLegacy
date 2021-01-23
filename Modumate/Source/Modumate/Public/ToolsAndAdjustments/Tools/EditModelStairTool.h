// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once
#include "ToolsAndAdjustments/Common/EditModelToolBase.h"
#include "ToolsAndAdjustments/Common/ModumateSnappedCursor.h"
#include "BIMKernel/AssemblySpec/BIMAssemblySpec.h"

#include "EditModelStairTool.generated.h"

class AEditModelGameMode;
class AEditModelGameState;
class AEditModelPlayerState;
class ADynamicMeshActor;
class ALineActor;
class AModumateObjectInstance;


UCLASS()
class MODUMATE_API UStairTool : public UEditModelToolBase
{
	GENERATED_BODY()

public:
	UStairTool(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual EToolMode GetToolMode() override { return EToolMode::VE_STAIR; }
	virtual bool Activate() override;
	virtual bool HandleInputNumber(double n) override;
	virtual bool Deactivate() override;
	virtual bool BeginUse() override;
	virtual bool EnterNextStage() override;
	virtual bool FrameUpdate() override;
	virtual bool EndUse() override;
	virtual bool AbortUse() override;

protected:
	enum EState
	{
		Starting,
		RunPending,
		RisePending,
		WidthPending
	};

	virtual void OnAssemblyChanged() override;

	bool UpdatePreviewStairs();
	bool MakeStairs();
	bool ValidatePlaneTarget(const AModumateObjectInstance *PlaneTarget);
	void MakePendingSegment(int32 &TargetSegmentID, const FVector &StartingPoint);
	void ResetState();

	EState CurrentState;
	bool bWasShowingSnapCursor;
	EMouseMode OriginalMouseMode;
	bool bWantedVerticalSnap;
	int32 LastValidTargetID;
	FBIMAssemblySpec ObjAssembly;
	int32 RunSegmentID, RiseSegmentID, WidthSegmentID;
	TWeakObjectPtr<ADynamicMeshActor> PendingObjMesh;
	TWeakObjectPtr<AEditModelGameMode> GameMode;
	TWeakObjectPtr<AEditModelGameState> GameState;
	FVector RunStartPos, RiseStartPos, RiseEndPos, WidthEndPos;
	FVector RunDir, WidthDir;
	float DesiredTreadDepth, CurrentTreadDepth, CurrentRiserHeight, CurrentWidth;
	int32 DesiredTreadNum, CurrentTreadNum, CurrentRiserNum;
	bool bWantStartRiser, bWantEndRiser, bFixTreadDepth;

	TArray<TArray<FVector>> CachedTreadPolys;
	TArray<TArray<FVector>> CachedRiserPolys;
	TArray<FVector> CachedRiserNormals;

	FColor SegmentColor;
};
