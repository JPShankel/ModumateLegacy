// Fill out your copyright notice in the Description page of Project Settings.

#include "UnrealClasses/MOIGroupActor_CPP.h"
#include "DocumentManagement/ModumateObjectInstance.h"
#include "Algo/Transform.h"

using namespace Modumate;

// Sets default values
AMOIGroupActor_CPP::AMOIGroupActor_CPP()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

TArray<FVector> AMOIGroupActor_CPP::GetMemberLocations()
{
	TArray<FVector> ret;
	Algo::Transform(MOI->GetChildObjects(),ret,[](const FModumateObjectInstance *ob){return ob->GetObjectLocation();});
	return ret;
}

// Called when the game starts or when spawned
void AMOIGroupActor_CPP::BeginPlay()
{
	Super::BeginPlay();

}

// Called every frame
void AMOIGroupActor_CPP::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

