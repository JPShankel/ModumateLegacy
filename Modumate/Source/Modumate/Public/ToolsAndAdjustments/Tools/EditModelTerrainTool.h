// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "ToolsAndAdjustments/Common/EditModelToolBase.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "UnrealClasses/DynamicMeshActor.h"
#include "Graph/Graph2D.h"

#include "EditModelTerrainTool.generated.h"

class AMOITerrain;

UCLASS()
class MODUMATE_API UTerrainTool : public UEditModelToolBase
{
	GENERATED_BODY()

public:
	virtual EToolMode GetToolMode() override { return EToolMode::VE_TERRAIN; }

	virtual bool Activate() override;
	virtual bool Deactivate() override;
 	virtual bool BeginUse() override;
 	virtual bool EndUse() override;
 	virtual bool AbortUse() override;
 	virtual bool FrameUpdate() override;
 	virtual bool EnterNextStage() override;
	virtual bool PostEndOrAbort() override;

	virtual bool HasDimensionActor() override { return true; }
	virtual bool HandleInputNumber(double n) override;

protected:
	bool AddFirstEdge(FVector Point1, FVector Point2);
	bool AddNewEdge(FVector Point1, FVector Point2);
	FString GetNextName() const;
	static FVector2D ProjectToPlane(FVector Origin, FVector Point);

	enum EState { Idle, AddEdge };
	EState State = Idle;

	EMouseMode OriginalMouseMode;
	FVector CurrentPoint;
	TSharedPtr<FGraph2D> TerrainGraph;
	TArray<FVector> Points;
	float ZHeight = 0.0f;

	static constexpr float CloseLoopEpsilon = 15.0f;
};
