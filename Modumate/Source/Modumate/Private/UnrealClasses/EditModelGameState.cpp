// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "UnrealClasses/EditModelGameState.h"

void AEditModelGameState::InitDocument(const FString& LocalUserID, int32 LocalUserIdx)
{
	if (ensure(Document == nullptr))
	{
		Document = NewObject<UModumateDocument>(this);
		Document->SetLocalUserInfo(LocalUserID, LocalUserIdx);
	}
}

// RPC from the server to every client's copy of the global GameState
void AEditModelGameState::BroadcastServerDeltas_Implementation(const FDeltasRecord& Deltas)
{
	UWorld* world = GetWorld();
	if (ensure(world && Document))
	{
		Document->ApplyRemoteDeltas(Deltas, world, false);
	}
}
