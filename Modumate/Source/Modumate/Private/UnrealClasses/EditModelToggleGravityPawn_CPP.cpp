// Fill out your copyright notice in the Description page of Project Settings.

#include "EditModelToggleGravityPawn_CPP.h"

// Sets default values
AEditModelToggleGravityPawn_CPP::AEditModelToggleGravityPawn_CPP()
{
 	// Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void AEditModelToggleGravityPawn_CPP::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void AEditModelToggleGravityPawn_CPP::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}
