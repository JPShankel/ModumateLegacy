// Copyright 2019 Modumate, Inc. All Rights Reserved.
#pragma once

#include "ToolsAndAdjustments/Common/EditModelToolBase.h"
#include "ToolsAndAdjustments/Tools/EditModelSelectTool.h"
#include "UnrealClasses/EditModelGameState_CPP.h"
#include "UnrealClasses/EditModelPlayerController_CPP.h"

#include "EditModelFinishTool.generated.h"

UCLASS()
class MODUMATE_API UFinishTool : public UEditModelToolBase
{
	GENERATED_BODY()

private:
	class Modumate::FModumateObjectInstance *LastValidTarget = nullptr;
	TWeakObjectPtr<AActor> LastValidHitActor = nullptr;
	FVector LastValidHitLocation = FVector::ZeroVector;
	FVector LastValidHitNormal = FVector::ZeroVector;
	int32 LastValidFaceIndex = INDEX_NONE;
	TArray<FVector> LastCornerPoints;
	EMouseMode OriginalMouseMode = EMouseMode::Object;
	FModumateObjectAssembly FinishAssembly = FModumateObjectAssembly();

public:
	UFinishTool(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual EToolMode GetToolMode() override { return EToolMode::VE_FINISH; }
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
};
