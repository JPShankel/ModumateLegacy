// Copyright 2019 Modumate, Inc. All Rights Reserved.
#pragma once

#include "ToolsAndAdjustments/Common/EditModelToolBase.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"
#include "UnrealClasses/DynamicMeshActor.h"
#include "Database/ModumateArchitecturalMaterial.h"

#include "EditModelMetaPlaneTool.generated.h"

class AEditModelGameMode_CPP;
class AEditModelGameState_CPP;
class ALineActor;

UCLASS()
class MODUMATE_API UMetaPlaneTool : public UEditModelToolBase
{
	GENERATED_BODY()

protected:
	enum EState
	{
		Neutral = 0,
		NewSegmentPending,
	};
	EState State;

	TWeakObjectPtr<AEditModelGameMode_CPP> GameMode;
	TWeakObjectPtr<AEditModelGameState_CPP> GameState;

	EMouseMode OriginalMouseMode;
	int32 PendingSegmentID;
	TWeakObjectPtr<ADynamicMeshActor> PendingPlane;
	FVector AnchorPointDegree;
	TArray<FVector> PendingPlanePoints, SketchPlanePoints;
	FArchitecturalMaterial PendingPlaneMaterial;
	bool bPendingSegmentValid;
	bool bPendingPlaneValid;
	FPlane PendingPlaneGeom;
	float MinPlaneSize;
	float PendingPlaneAlpha;

	TArray<int32> NewObjIDs;

public:
	UMetaPlaneTool(const FObjectInitializer& ObjectInitializer);

	virtual EToolMode GetToolMode() override { return EToolMode::VE_METAPLANE; }
	virtual bool Activate() override;
	virtual bool HandleInputNumber(double n) override;
	virtual bool Deactivate() override;
	virtual bool BeginUse() override;
	virtual bool EnterNextStage() override;
	virtual bool FrameUpdate() override;
	virtual bool EndUse() override;
	virtual bool AbortUse() override;
	virtual void SetAxisConstraint(EAxisConstraint AxisConstraint) override;

protected:
	virtual bool MakeObject(const FVector &Location, TArray<int32> &OutNewObjIDs);
	void UpdatePendingPlane();
	bool ConstrainHitPoint(FVector &hitPoint);
};
