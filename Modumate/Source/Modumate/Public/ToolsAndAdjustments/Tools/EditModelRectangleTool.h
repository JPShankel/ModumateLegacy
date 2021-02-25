// Copyright 2019 Modumate, Inc. All Rights Reserved.
#pragma once

#include "ToolsAndAdjustments/Common/EditModelToolBase.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "UnrealClasses/DynamicMeshActor.h"
#include "Database/ModumateArchitecturalMaterial.h"

#include "EditModelRectangleTool.generated.h"

class AEditModelGameMode;
class AEditModelGameState;
class ALineActor;

UCLASS()
class MODUMATE_API URectangleTool : public UEditModelToolBase
{
	GENERATED_BODY()

protected:
	enum EState
	{
		Neutral = 0,
		NewSegmentPending,
		NewPlanePending,
	};
	EState State;

	TWeakObjectPtr<AEditModelGameMode> GameMode;

	EMouseMode OriginalMouseMode;
	TArray<FVector> PendingPlanePoints;
	FVector PlaneBaseStart, PlaneBaseEnd; // for rectangle tool, base points are a line segment that the rectangle extrudes from
	FArchitecturalMaterial PendingPlaneMaterial;
	bool bPendingPlaneValid;
	bool bExtrudingPlaneFromEdge;
	FPlane PendingPlaneGeom;
	float MinPlaneSize;

	virtual float GetDefaultPlaneHeight() const;

public:
	URectangleTool(const FObjectInitializer& ObjectInitializer);

	virtual EToolMode GetToolMode() override { return EToolMode::VE_RECTANGLE; }
	virtual bool Activate() override;
	virtual bool HandleInputNumber(double n) override;
	virtual bool Deactivate() override;
	virtual bool BeginUse() override;
	virtual bool EnterNextStage() override;
	virtual bool FrameUpdate() override;
	virtual bool EndUse() override;
	virtual bool AbortUse() override;
	virtual bool PostEndOrAbort() override;

	virtual bool HasDimensionActor() { return true; }

protected:
	bool GetMetaObjectCreationDeltas(const FVector& Location, bool bSplitAndUpdateFaces,
		FVector& OutConstrainedLocation, TArray<FDeltaPtr>& OutDeltaPtrs);

	virtual bool MakeObject(const FVector& Location);
	virtual bool UpdatePreview();

	void UpdatePendingPlane();
	bool ConstrainHitPoint(FVector &hitPoint);

	TArray<int32> CurAddedEdgeIDs, CurAddedFaceIDs;
};
