// Copyright 2021 Modumate, Inc. All Rights Reserved.
#include "UnrealClasses/ModumateCapability.h"
#include "UnrealClasses/EditModelPlayerController.h"


AModumateCapability::AModumateCapability() :
	AActor()
{

}

AModumateCapability::~AModumateCapability()
{

}

void AModumateCapability::BeginPlay()
{
	Super::BeginPlay();

	auto playerController = Cast<AEditModelPlayerController>(GetOwner());
	if (playerController && playerController->IsLocalController())
	{
		playerController->CapabilityReady(this);
	}
}
