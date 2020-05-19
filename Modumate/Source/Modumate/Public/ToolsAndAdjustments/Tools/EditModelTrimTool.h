// Copyright 2019 Modumate, Inc. All Rights Reserved.
#pragma once

#include "ToolsAndAdjustments/Common/EditModelToolBase.h"
#include "Engine/Engine.h"
#include "Database/ModumateObjectAssembly.h"
#include "ModumateCore/ModumateObjectStatics.h"
#include "UnrealClasses/EditModelPlayerState_CPP.h"

#include "EditModelTrimTool.generated.h"

class ADynamicMeshActor;

UCLASS()
class MODUMATE_API UTrimTool : public UEditModelToolBase
{
	GENERATED_BODY()

private:
	bool bInverted;
	class Modumate::FModumateObjectInstance *CurrentTarget;
	TArray<class Modumate::FModumateObjectInstance *> CurrentTargetChildren;
	TWeakObjectPtr<AActor> CurrentHitActor;
	int32 CurrentStartIndex, CurrentEndIndex, CurrentMountIndex;
	float CurrentStartAlongEdge, CurrentEndAlongEdge;
	bool bCurrentLengthsArePCT;
	FVector CurrentEdgeStart, CurrentEdgeEnd;
	ETrimMiterOptions MiterOptionStart, MiterOptionEnd;
	EMouseMode OriginalMouseMode;
	FModumateObjectAssembly TrimAssembly;
	TWeakObjectPtr<ADynamicMeshActor> PendingTrimActor;

	void ResetTarget();
	bool HasValidTarget() const;
	void OnAssemblySet();
	bool ConstrainTargetToSolidEdge(float targetPosAlongEdge, int32 startIndex,int32 endIndex);

public:
	UTrimTool(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual EToolMode GetToolMode() override { return EToolMode::VE_TRIM; }
	virtual bool Activate() override;
	virtual bool HandleInputNumber(double n) override;
	virtual bool Deactivate() override;
	virtual bool BeginUse() override;
	virtual bool FrameUpdate() override;
	virtual bool EndUse() override;
	virtual bool AbortUse() override;
	virtual bool HandleSpacebar() override;
	virtual void SetAssembly(const FShoppingItem &key) override;

	FColor AffordanceLineColor = FColor::Orange;
	float AffordanceLineInterval = 4.0f;
};
