// Fill out your copyright notice in the Description page of Project Settings.

#include "UnrealClasses/MOIGroupActor.h"
#include "Objects/ModumateObjectInstance.h"
#include "Algo/Transform.h"



// Sets default values
AMOIGroupActor::AMOIGroupActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

TArray<FVector> AMOIGroupActor::GetMemberLocations()
{
	TArray<FVector> ret;
	Algo::Transform(MOI->GetChildObjects(),ret,[](const AModumateObjectInstance *ob){return ob->GetLocation();});
	return ret;
}

// Called when the game starts or when spawned
void AMOIGroupActor::BeginPlay()
{
	Super::BeginPlay();

}

// Called every frame
void AMOIGroupActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

