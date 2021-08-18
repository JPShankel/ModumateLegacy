// Copyright 2021 Modumate, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"

#include "ModumateCapability.generated.h"


UCLASS()
class MODUMATE_API AModumateCapability : public AActor
{
	GENERATED_BODY()

public:
	AModumateCapability();

	virtual ~AModumateCapability();
	virtual void BeginPlay() override;
};
