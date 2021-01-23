// Fill out your copyright notice in the Description page of Project Settings.

#include "UnrealClasses/PortalFrameActor.h"


// Sets default values
APortalFrameActor::APortalFrameActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void APortalFrameActor::BeginPlay()
{
	Super::BeginPlay();

}

// Called every frame
void APortalFrameActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

