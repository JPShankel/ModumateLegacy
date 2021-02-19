// Copyright 2021 Modumate, Inc. All Rights Reserved.
#pragma once

#include "ToolsAndAdjustments/Common/EditModelToolBase.h"
#include "UnrealClasses/EditModelPlayerState.h"
#include "UnrealClasses/DynamicMeshActor.h"

#include "EditModelPasteTool.generated.h"

class AEditModelGameMode;
class AEditModelGameState;
class ALineActor;

struct FMOIDocumentRecordV4;
using FMOIDocumentRecord = FMOIDocumentRecordV4;

UCLASS()
class MODUMATE_API UPasteTool : public UEditModelToolBase
{
	GENERATED_BODY()

public:
	UPasteTool(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

public:
	virtual EToolMode GetToolMode() override { return EToolMode::VE_PASTE; }

	virtual bool Activate() override;
	virtual bool FrameUpdate() override;
	virtual bool EnterNextStage() override;

private:
	FMOIDocumentRecord CurrentRecord;

	TArray<FDeltaPtr> CurDeltas;

	FVector PasteOrigin;
};
