// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "ToolsAndAdjustments/Common/EditModelToolBase.h"
#include "ToolsAndAdjustments/Common/ModumateSnappedCursor.h"
#include "Graph/Graph2D.h"

#include "EditModelSurfaceGraphTool.generated.h"

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
	virtual TArray<EEditViewModes> GetRequiredEditModes() const override;

	virtual bool HasDimensionActor() { return true; }

protected:
	bool InitializeSegment();
	bool CreateGraphFromFaceTarget(int32& OutSurfaceGraphID);
	bool AddEdge(FVector StartPos, FVector EndPos);
	void ResetTarget();

	class FModumateObjectInstance *HostTarget;
	class FModumateObjectInstance *GraphTarget;
	class FModumateObjectInstance *GraphElementTarget;

	UPROPERTY()
	AActor* HitHostActor;

	FVector HitLocation, HitNormal;
	int32 HitFaceIndex;
	TArray<int32> HostCornerIndices;
	TArray<FVector> HostCornerPositions;
	FVector TargetFaceOrigin, TargetFaceNormal, TargetFaceAxisX, TargetFaceAxisY;
	EMouseMode OriginalMouseMode;
	TSharedPtr<Modumate::FGraph2D> TargetSurfaceGraph;
};
