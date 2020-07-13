// Copyright 2020 Modumate, Inc. All Rights Reserved.
#pragma once

#include "ToolsAndAdjustments/Common/EditModelToolBase.h"
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
	void ResetTarget();

	class Modumate::FModumateObjectInstance *LastHostTarget = nullptr;
	class Modumate::FModumateObjectInstance *LastGraphTarget = nullptr;
	TWeakObjectPtr<AActor> LastHitHostActor = nullptr;
	FVector LastValidHitLocation = FVector::ZeroVector;
	FVector LastValidHitNormal = FVector::ZeroVector;
	int32 LastValidFaceIndex = INDEX_NONE;
	TArray<int32> LastCornerIndices;
	EMouseMode OriginalMouseMode = EMouseMode::Object;
};
