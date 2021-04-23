// Copyright 2021 Modumate, Inc. All Rights Reserved.

#include "UnrealClasses/AxesActor.h"
#include "UnrealClasses/EditModelPlayerController.h"


// Sets default values
AAxesActor::AAxesActor()
	: Controller(nullptr)
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void AAxesActor::BeginPlay()
{
	Super::BeginPlay();

#if !UE_SERVER
	// Controller needs SkyActor for time of day changes in ViewMenu to work
	Controller = GetWorld()->GetFirstPlayerController<AEditModelPlayerController>();
	if (ensureAlways(Controller))
	{
		Controller->AxesActor = this;
	}
#endif
}

// Called every frame
void AAxesActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// TODO move BP tick logic here
}