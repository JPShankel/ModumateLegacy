// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "ToolsAndAdjustments/Common/EditModelToolBase.h"
#include "ToolsAndAdjustments/Common/ModumateSnappedCursor.h"
#include "Graph/Graph2D.h"

#include "EditModelSurfaceGraphTool.generated.h"

class FModumateObjectInstance;

UCLASS()
class MODUMATE_API USurfaceGraphTool : public UEditModelToolBase
{
	GENERATED_BODY()

public:
	USurfaceGraphTool(const FObjectInitializer& ObjectInitializer);

	virtual EToolMode GetToolMode() override { return EToolMode::VE_SURFACEGRAPH; }
	virtual bool Activate() override;
	virtual bool Deactivate() override;
	virtual bool BeginUse() override;
	virtual bool EnterNextStage() override;
	virtual bool EndUse() override;
	virtual bool AbortUse() override;
	virtual bool FrameUpdate() override;
	virtual bool HandleInputNumber(double n) override;

	virtual bool HasDimensionActor() { return true; }

protected:
	bool UpdateTarget(const FModumateObjectInstance* HitObject, const FVector& HitLocation, const FVector& HitNormal);
	bool InitializeSegment();
	bool CompleteSegment();
	bool CreateGraphFromFaceTarget(int32& NextID, int32& OutSurfaceGraphID, int32& OutRootPolyID, TArray<FDeltaPtr>& OutDeltas);
	bool AddEdge(FVector StartPos, FVector EndPos);
	void ResetTarget();

	const FModumateObjectInstance* HitGraphHostMOI;
	const FModumateObjectInstance* HitGraphMOI;
	TSharedPtr<Modumate::FGraph2D> HitSurfaceGraph;
	const FModumateObjectInstance* HitGraphElementMOI;
	TArray<const FModumateObjectInstance*> HitAdjacentGraphMOIs;

	FVector HitLocation;
	int32 HitFaceIndex;
	FTransform HitFaceOrigin;
	TArray<FVector> HitFacePoints;

	const FModumateObjectInstance* TargetGraphMOI;
	TSharedPtr<Modumate::FGraph2D> TargetSurfaceGraph;
	TArray<const FModumateObjectInstance*> TargetAdjacentGraphMOIs;

	EMouseMode OriginalMouseMode;
};
