// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MOIGroupActor_CPP.generated.h"

class FModumateObjectInstance;

UCLASS()
class MODUMATE_API AMOIGroupActor_CPP : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AMOIGroupActor_CPP();

	FModumateObjectInstance *MOI;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable)
	TArray<FVector> GetMemberLocations();




};
