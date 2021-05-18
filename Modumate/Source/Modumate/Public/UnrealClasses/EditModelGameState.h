// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/GameStateBase.h"

#include "DocumentManagement/ModumateDocument.h"

#include "EditModelGameState.generated.h"

UCLASS(config=Game)
class MODUMATE_API AEditModelGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	void InitDocument();

	UPROPERTY()
	UModumateDocument* Document;

	UFUNCTION(NetMulticast, Reliable)
	void ReceiveServerDeltas(const FDeltasRecord& Deltas);
};
