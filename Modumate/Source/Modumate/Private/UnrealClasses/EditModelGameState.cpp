// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "UnrealClasses/EditModelGameState.h"

void AEditModelGameState::InitDocument()
{
	if (ensure(Document == nullptr))
	{
		Document = NewObject<UModumateDocument>();
	}
}
