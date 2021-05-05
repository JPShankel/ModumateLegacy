// Copyright 2019 Modumate, Inc. All Rights Reserved.
#pragma once

#include "ToolsAndAdjustments/Common/EditModelToolBase.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "UnrealClasses/DynamicMeshActor.h"

#include "EditModelJoinTool.generated.h"

class AEditModelGameMode;
class AEditModelGameState;
class ALineActor;

UCLASS()
class MODUMATE_API UJoinTool : public UEditModelToolBase
{
	GENERATED_BODY()

public:
	UJoinTool();

	virtual EToolMode GetToolMode() override { return EToolMode::VE_JOIN; }
	virtual bool Activate() override;
	virtual bool Deactivate() override;

	virtual bool BeginUse() override;
	virtual bool HandleMouseUp() override;
	virtual bool EnterNextStage() override;
	virtual bool FrameUpdate() override;
	virtual bool EndUse() override;
	virtual bool AbortUse() override;

private:
	bool UpdateSelectedPlanes();
		
private:

	EObjectType ActiveObjectType = EObjectType::OTUnknown;

	FPlane ActivePlane;
	TPair<FVector, FVector> ActiveEdge;

	TSet<int32> PendingObjectIDs;
	// Objects that are selectable for join given the starting object
	TSet<int32> FrontierObjectIDs;
};
