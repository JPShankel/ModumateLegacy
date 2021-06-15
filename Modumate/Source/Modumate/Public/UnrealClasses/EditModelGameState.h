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
	void InitDocument(const FString& LocalUserID, int32 LocalUserIdx);

	UPROPERTY()
	UModumateDocument* Document;

	UFUNCTION(NetMulticast, Reliable)
	void BroadcastServerDeltas(const FDeltasRecord& Deltas);

	UFUNCTION(NetMulticast, Reliable)
	void BroadcastUndo(const FString& UndoingUserID, const TArray<uint32>& UndoRecordHashes);
};
