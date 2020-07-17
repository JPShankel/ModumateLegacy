// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "ToolsAndAdjustments/Common/EditModelToolBase.h"

#include "DocumentManagement/ModumateDelta.h"
#include "ToolsAndAdjustments/Common/ModumateSnappedCursor.h"

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

	FColor AffordanceLineColor = FColor::Orange;
	float AffordanceLineThickness = 4.0f;
	float AffordanceLineInterval = 8.0f;

protected:
	bool CreateGraphFromFaceTarget(TArray<TSharedPtr<Modumate::FDelta>> &OutDeltas);
	void ResetTarget();

	UPROPERTY()
	class AEditModelGameState_CPP* GameState;

	UPROPERTY()
	class UDimensionManager* DimensionManager;

	class Modumate::FModumateObjectInstance *LastHostTarget = nullptr;
	class Modumate::FModumateObjectInstance *LastGraphTarget = nullptr;

	UPROPERTY()
	AActor* LastHitHostActor;

	FVector LastValidHitLocation;
	FVector LastValidHitNormal;
	int32 LastValidFaceIndex;
	TArray<int32> LastCornerIndices;
	TArray<FVector> LastCornerPositions;
	FPlane LastTargetFacePlane;
	EMouseMode OriginalMouseMode;
	int32 PendingSegmentID;
};
