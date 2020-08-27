// Copyright 2019 Modumate, Inc. All Rights Reserved.

#pragma once
#include "ToolsAndAdjustments/Common/EditModelToolBase.h"
#include "ToolsAndAdjustments/Common/ModumateSnappedCursor.h"
#include "BIMKernel/BIMAssemblySpec.h"

#include "EditModelStairTool.generated.h"

class AEditModelGameMode_CPP;
class AEditModelGameState_CPP;
class AEditModelPlayerState_CPP;
class ADynamicMeshActor;
class ALineActor;
class FModumateObjectInstance;


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
	virtual void SetAssemblyKey(const FName &InAssemblyKey) override;

protected:
	enum EState
	{
		Starting,
		RunPending,
		RisePending,
		WidthPending
	};

	bool UpdatePreviewStairs();
	bool MakeStairs(int32 &RefParentPlaneID, int32 &OutStairsID);
	bool ValidatePlaneTarget(const FModumateObjectInstance *PlaneTarget);
	void MakePendingSegment(TWeakObjectPtr<ALineActor> &TargetSegment, const FVector &StartingPoint, const FColor &SegmentColor);
	void MakePendingSegment(int32 &TargetSegmentID, const FVector &StartingPoint, const FColor &SegmentColor);
	void ResetState();

	EState CurrentState;
	bool bWasShowingSnapCursor;
	EMouseMode OriginalMouseMode;
	bool bWantedVerticalSnap;
	int32 LastValidTargetID;
	FBIMAssemblySpec ObjAssembly;
	int32 RunSegmentID, RiseSegmentID, WidthSegmentID;
	TWeakObjectPtr<ADynamicMeshActor> PendingObjMesh;
	TWeakObjectPtr<AEditModelGameMode_CPP> GameMode;
	TWeakObjectPtr<AEditModelGameState_CPP> GameState;
	FVector RunStartPos, RiseStartPos, RiseEndPos, WidthEndPos;
	FVector RunDir, WidthDir;
	float DesiredTreadDepth, CurrentTreadDepth, CurrentRiserHeight, CurrentWidth;
	int32 DesiredTreadNum, CurrentTreadNum, CurrentRiserNum;
	bool bWantStartRiser, bWantEndRiser, bFixTreadDepth;

	TArray<TArray<FVector>> CachedTreadPolys;
	TArray<TArray<FVector>> CachedRiserPolys;
	TArray<FVector> CachedRiserNormals;

	FColor TargetPlaneDotColor = FColor::Orange;
	float TargetPlaneDotThickness = 4.0f;
	float TargetPlaneDotInterval = 8.0f;
	FColor RunSegmentColor, RiseSegmentColor, WidthSegmentColor;
};
