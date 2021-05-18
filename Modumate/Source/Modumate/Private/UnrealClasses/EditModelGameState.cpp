// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "UnrealClasses/EditModelGameState.h"

void AEditModelGameState::InitDocument()
{
	if (ensure(Document == nullptr))
	{
		Document = NewObject<UModumateDocument>(this);
	}
}

void AEditModelGameState::ReceiveServerDeltas_Implementation(const FDeltasRecord& Deltas)
{
	UWorld* world = GetWorld();
	if (!ensure(world && Document))
	{
		return;
	}

	Document->ApplyRemoteDeltas(Deltas, world);
}
