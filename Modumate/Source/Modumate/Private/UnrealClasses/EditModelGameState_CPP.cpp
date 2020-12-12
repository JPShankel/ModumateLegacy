// Copyright 2018 Modumate, Inc. All Rights Reserved.

#include "UnrealClasses/EditModelGameState_CPP.h"

void AEditModelGameState_CPP::InitDocument()
{
	if (ensure(Document == nullptr))
	{
		Document = NewObject<UModumateDocument>();
	}
}
