// Copyright 2018 Modumate, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/GameStateBase.h"

#include "DocumentManagement/ModumateDocument.h"

#include "EditModelGameState_CPP.generated.h"

UCLASS(config=Game)
class MODUMATE_API AEditModelGameState_CPP : public AGameStateBase
{
	GENERATED_BODY()

public:
	void InitDocument();

	UPROPERTY()
	UModumateDocument* Document;
};
